#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <assert.h>

#include <ares.h>

#include <php.h>

#include "dnscache.h"

#include "lruc.h"
#include "subhook/subhook.h"

#include "inject.h"

struct gethostbyname_data {
	struct hostent hostent_space;
	in_addr_t resolved_addr;
	char *resolved_addr_p[2];
	char addr_name[1024 * 8];
};

struct addrinfo_data {
	struct addrinfo addrinfo_space;
	struct sockaddr_storage sockaddr_space;
	char addr_name[256];
};

struct ares_ctx {
	int status;
	struct in_addr *addr;
};

#define ARES_STATUS_RET_MASK 0xffff

#define ARES_STATUS_SET_RET(ctx, ret) (ctx)->status |= (ret & ARES_STATUS_RET_MASK);
#define ARES_STATUS_GET_RET(ctx) ((ctx)->status & ARES_STATUS_RET_MASK)
#define ARES_STATUS_SET_COMPLETED(ctx) (ctx)->status |= 1 << 16;
#define ARES_STATUS_IS_COMPLETED(ctx) ((ctx)->status & (1 << 16))

static lruc *cache = NULL;

static int looks_like_numeric_ipv6(const char *node)
{
	if(!strchr(node, ':')) return 0;
	const char* p= node;
	while(1) switch(*(p++)) {
		case 0: return 1;
		case ':': case '.':
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			break;
		default: return 0;
	}
}

void proxy_freeaddrinfo(struct addrinfo *res) {
	//PFUNC();
	free(res);
}

/*static void gethostbyname_data_setstring(struct gethostbyname_data* data, char* name) {
	snprintf(data->addr_name, sizeof(data->addr_name), "%s", name);
	data->hostent_space.h_name = data->addr_name;
}

extern ip_type4 hostsreader_get_numeric_ip_for_name(const char* name);
struct hostent *my_gethostbyname(const char *name, struct gethostbyname_data* data) {
	//PFUNC();
	char buff[256];

	data->resolved_addr_p[0] = (char *) &data->resolved_addr;
	data->resolved_addr_p[1] = NULL;

	data->hostent_space.h_addr_list = data->resolved_addr_p;
	// let aliases point to the NULL member, mimicking an empty list.
	data->hostent_space.h_aliases = &data->resolved_addr_p[1];

	data->resolved_addr = 0;
	data->hostent_space.h_addrtype = AF_INET;
	data->hostent_space.h_length = sizeof(in_addr_t);

	gethostname(buff, sizeof(buff));

	if(!strcmp(buff, name)) {
		data->resolved_addr = inet_addr(buff);
		if(data->resolved_addr == (in_addr_t) (-1))
			data->resolved_addr = (in_addr_t) (ip_type_localhost.addr.v4.as_int);
		goto retname;
	}

	// this iterates over the "known hosts" db, usually /etc/hosts
	ip_type4 hdb_res = hostsreader_get_numeric_ip_for_name(name);
	if(hdb_res.as_int != ip_type_invalid.addr.v4.as_int) {
		data->resolved_addr = hdb_res.as_int;
		goto retname;
	}

	data->resolved_addr = at_get_ip_for_host((char*) name, strlen(name)).as_int;
	if(data->resolved_addr == (in_addr_t) ip_type_invalid.addr.v4.as_int) return NULL;

	retname:

	gethostbyname_data_setstring(data, (char*) name);

	//PDEBUG("return hostent space\n");

	return &data->hostent_space;
}*/

static struct gethostbyname_data ghbndata;
/*
 * Overrides real gethostbyname
 * */
struct hostent *proxy_gethostbyname(const char *name) {
    printf("gethostbyname: %s\n", name);
    // Load real gethostbyname function
    static struct hostent *(*gethostbyname_real) (const char *) = NULL;
    if (!gethostbyname_real)
	gethostbyname_real = dlsym(RTLD_NEXT, "gethostbyname");

    struct hostent *ret = gethostbyname_real(name);
    return ret;
}


/*
 * Overrides real gethostbyname2
 * */
struct hostent *proxy_gethostbyname2(const char *name, int af)
{
    printf("gethostbyname2: %s\n", name);
    // Load real gethostbyname2 function
    static struct hostent *(*gethostbyname2_real) (const char *, int) =
	NULL;
    if (!gethostbyname2_real)
	gethostbyname2_real = dlsym(RTLD_NEXT, "gethostbyname2");

    // Call real gethostbyname2 with resolved alias from HOSTALIASES variable
    const char *alias = NULL; // resolveAlias(name, buf, BUFLEN);
    struct hostent *ret = gethostbyname2_real(alias ? alias : name, af);
    return ret;
}

/*
 * Overrides real gethostbyname_r
 */
int
proxy_gethostbyname_r(const char *name,
		struct hostent *ret, char *buf, size_t buflen,
		struct hostent **result, int *h_errnop)
{
    printf("gethostbyname_r: %s\n", name);
    // Load real gethostbyname_r function
    static int (*gethostbyname_r_real) (const char *,
					struct hostent *, char *, size_t,
					struct hostent **, int *) = NULL;
    if (!gethostbyname_r_real)
	gethostbyname_r_real = dlsym(RTLD_NEXT, "gethostbyname_r");

    // Call real gethostbyname_r with resolved alias from HOSTALIASES variable
    const char *alias = NULL; //resolveAlias(name, buf, BUFLEN);
    int mret =
	gethostbyname_r_real(alias ? alias : name, ret, buf, buflen,
			     result,
			     h_errnop);
    return mret;
}

/*
 * Overrides real gethostbyname2_r
 */
int
proxy_gethostbyname2_r(const char *name, int af,
		 struct hostent *ret, char *buf, size_t buflen,
		 struct hostent **result, int *h_errnop)
{
    printf("gethostbyname2_r: %s\n", name);
    // Load real gethostbyname2_r function
    static int (*gethostbyname2_r_real) (const char *, int af,
					 struct hostent *, char *, size_t,
					 struct hostent **, int *) = NULL;
    if (!gethostbyname2_r_real)
	gethostbyname2_r_real = dlsym(RTLD_NEXT, "gethostbyname2_r");

    // Call real gethostbyname2_r with resolved alias from HOSTALIASES variable
    const char *alias = NULL; //resolveAlias(name, buf, BUFLEN);
    int mret =
	gethostbyname2_r_real(alias ? alias : name, af, ret, buf, buflen,
			      result,
			      h_errnop);
    return mret;
}

static int my_inet_aton(const char *node, struct addrinfo_data* space)
{
	int ret;
	((struct sockaddr_in *) &space->sockaddr_space)->sin_family = AF_INET;
	ret = inet_aton(node, &((struct sockaddr_in *) &space->sockaddr_space)->sin_addr);
	if(ret || !looks_like_numeric_ipv6(node)) return ret;
	ret = inet_pton(AF_INET6, node, &((struct sockaddr_in6 *) &space->sockaddr_space)->sin6_addr);
	if(ret) ((struct sockaddr_in6 *) &space->sockaddr_space)->sin6_family = AF_INET6;
	return ret;
}

void dnscache_ares_callback(void* arg, int status, int timeouts, struct hostent* host)
{
    struct ares_ctx *ctx = (struct ares_ctx *)arg;

    ARES_STATUS_SET_COMPLETED(ctx);

    if (status != ARES_SUCCESS) {
        //fprintf(stderr, "Error: %s\n", ares_strerror(status));
        switch (status) {
        case ARES_ENOTFOUND:
            ARES_STATUS_SET_RET(ctx, HOST_NOT_FOUND);
            break;
        case ARES_ENODATA:
            ARES_STATUS_SET_RET(ctx, NO_DATA);
            break;
        default:
            ARES_STATUS_SET_RET(ctx, NO_RECOVERY);
            break;
        }
        return;
    }

    // char ip[INET6_ADDRSTRLEN];
    int idx;
    for (idx = 0; host->h_addr_list[idx]; idx++) {
        //inet_ntop(host->h_addrtype, host->h_addr_list[idx], ip, sizeof(ip));
        //printf("dns result: %s\n", ip);
        *ctx->addr = *(struct in_addr *)host->h_addr_list[idx];
        ARES_STATUS_SET_RET(ctx, 0);
        return;
    }

    ARES_STATUS_SET_RET(ctx, TRY_AGAIN);
}

int my_gethostbyname_ares(const char *host, struct in_addr *addr) {
    ares_channel channel;
    int status;
    struct ares_ctx ctx;
    struct ares_options options;
    int optmask = ARES_OPT_TIMEOUTMS;

    memset(&ctx, 0, sizeof(struct ares_ctx));
    ctx.addr = addr;
    options.timeout = DNSCACHEG(dns_timeout_ms);

    status = ares_init_options(&channel, &options, optmask);
    if (status != ARES_SUCCESS) {
        //fprintf(stderr, "ares_init_options: %s\n", ares_strerror(status));
        return NO_RECOVERY;
    }
    ares_gethostbyname(channel, host, AF_INET, dnscache_ares_callback, &ctx);

    struct pollfd fds[ARES_GETSOCK_MAXNUM];
    int           socks[ARES_GETSOCK_MAXNUM];
    int nfds, num;
    int timeout;
    int i;
    int bitmask;

    while (!ARES_STATUS_IS_COMPLETED(&ctx)) {
        num = 0;
        bitmask = ares_getsock(channel, socks, ARES_GETSOCK_MAXNUM);
        for (i = 0; i < ARES_GETSOCK_MAXNUM; i++) {
            fds[i].events  = 0;
            fds[i].revents = 0;
            if (ARES_GETSOCK_READABLE(bitmask, i)) {
                fds[i].fd = socks[i];
                fds[i].events |= POLLIN;
                fds[i].events |= POLLPRI;
            }
            if (ARES_GETSOCK_WRITABLE(bitmask, i)) {
                fds[i].fd = socks[i];
                fds[i].events |= POLLOUT;
            }
            if (fds[i].events != 0) {
                num++;
            } else {
                break;
            }
        }

        if (num == 0)
            break;

        struct timeval *tvp, tv;
        tvp = ares_timeout(channel, NULL, &tv);
        timeout = (tvp->tv_sec * 1000) + (tvp->tv_usec / 1000);
        //printf("timeout: %dms\n", timeout);

        nfds = poll(fds, num, timeout);
        if (nfds <= 0) {
            if (nfds == 0) {
                //fprintf(stderr, "ares dns resolve error: timeout\n");
            } else {
                fprintf(stderr, "ares dns resolve error: %s\n", ares_strerror(errno));
            }
            break;
        }

        for (i = 0; i < num; i++) {
            ares_process_fd(channel,
                            fds[i].revents & (POLLIN | POLLPRI) ? fds[i].fd : ARES_SOCKET_BAD,
                            fds[i].revents & POLLOUT ? fds[i].fd : ARES_SOCKET_BAD);
        }
    }

    ares_destroy(channel);

    if (!ARES_STATUS_IS_COMPLETED(&ctx)) {
        return NO_RECOVERY;
    }

    return ARES_STATUS_GET_RET(&ctx);
}

int my_gethostbyname(const char *host, struct in_addr *addr) {
    struct hostent he, *result;
    int herr, ret, bufsz = 512;
    char *buff = NULL;
    do {
        char *new_buff = (char *)realloc(buff, bufsz);
        if (new_buff == NULL) {
            free(buff);
            return ENOMEM;
        }
        buff = new_buff;
        ret = gethostbyname2_r(host, AF_INET, &he, buff, bufsz, &result, &herr);
        //printf("gethostbyname2_r ret: %d herr: %d\n", ret, herr);
        bufsz *= 2;
    } while (ret == ERANGE);

    if (ret == 0 && result != NULL)
        *addr = *(struct in_addr *)he.h_addr;
    else if (result != &he)
        ret = herr;
    free(buff);
    return ret;
}

int proxy_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
	//struct gethostbyname_data ghdata;
	struct addrinfo_data *space;
	struct servent *se = NULL;
	struct hostent *hp = NULL;
	struct servent se_buf;
	struct addrinfo *p;
	struct in_addr *addr = NULL;
	char buf[1024];
        int err;
	int port, af = AF_INET;
	//PFUNC();

	//printf("proxy_getaddrinfo node %s service %s\n", node, service);
	space = calloc(1, sizeof(struct addrinfo_data));
	if(!space) goto err1;

	if(node && !my_inet_aton(node, space)) {
		/* some folks (nmap) use getaddrinfo() with AI_NUMERICHOST to check whether a string
		   containing a numeric ip was passed. we must return failure in that case. */
		if(hints && (hints->ai_flags & AI_NUMERICHOST)) {
			free(space);
			return EAI_NONAME;
		}
		int key_len = strlen(node);
                err = lruc_get(cache, (void *)node, key_len, (void **)(&addr));
                if (!err && addr) {
			//printf("proxy_getaddrinfo node %s service %s (cached)\n", node, service);
			memcpy(&((struct sockaddr_in *) &space->sockaddr_space)->sin_addr,
		       		addr, sizeof(struct in_addr));
                } else {
			//printf("proxy_getaddrinfo node %s service %s\n", node, service);
			//hp = gethostbyname(node);
			//if(hp) {
			//	memcpy(&((struct sockaddr_in *) &space->sockaddr_space)->sin_addr,
			//       		*(hp->h_addr_list), sizeof(in_addr_t));
			//	memcpy(addr_list_cache, *(hp->h_addr_list), sizeof(in_addr_t));
			addr = malloc(sizeof(struct in_addr));
			if (!addr) goto err1;
			err = my_gethostbyname_ares(node, addr);
			//printf("gethostbyname error: %d\n", err);
			if (!err) {
				memcpy(&((struct sockaddr_in *) &space->sockaddr_space)->sin_addr,
						addr, sizeof(struct in_addr));
                                char *node_cache = strdup(node);
				if (!node_cache) {
					free(addr);
					// PASS
				} else {
					err = lruc_set(cache, node_cache, key_len, addr, sizeof(struct in_addr));
					if (err) {
						free(node_cache);
						free(addr);
						// PASS
                                	}
				}
                	} else {
				free(addr);
				goto err2;
			}
                }
	} else if(node) {
		af = ((struct sockaddr_in *) &space->sockaddr_space)->sin_family;
	} else if(!node && !(hints->ai_flags & AI_PASSIVE)) {
		af = ((struct sockaddr_in *) &space->sockaddr_space)->sin_family = AF_INET;
		memcpy(&((struct sockaddr_in *) &space->sockaddr_space)->sin_addr,
		       (char[]){127,0,0,1}, 4);
	}
	if(service) /*my*/getservbyname_r(service, NULL, &se_buf, buf, sizeof(buf), &se);

	port = se ? se->s_port : htons(atoi(service ? service : "0"));
	if(af == AF_INET)
		((struct sockaddr_in *) &space->sockaddr_space)->sin_port = port;
	else
		((struct sockaddr_in6 *) &space->sockaddr_space)->sin6_port = port;

	*res = p = &space->addrinfo_space;
	assert((size_t)p == (size_t) space);

	p->ai_addr = (void*) &space->sockaddr_space;
	if(node)
		snprintf(space->addr_name, sizeof(space->addr_name), "%s", node);
	p->ai_canonname = space->addr_name;
	p->ai_next = NULL;
	p->ai_family = space->sockaddr_space.ss_family = af;
	p->ai_addrlen = af == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

	if(hints) {
		p->ai_socktype = hints->ai_socktype;
		p->ai_flags = hints->ai_flags;
		p->ai_protocol = hints->ai_protocol;
	} else {
#ifndef AI_V4MAPPED
#define AI_V4MAPPED 0
#endif
		p->ai_flags = (AI_V4MAPPED | AI_ADDRCONFIG);
	}

	goto out;
	err2:
	free(space);
	err1:
	return 1;
	out:
	return 0;
}

subhook_t getaddrinfo_hook;
subhook_t freeaddrinfo_hook;

#define AVG_SIZE    sizeof(struct in_addr)   // or sizeof(in_addr_t) 4

//__attribute__((constructor))
//static void ctor(void) {
void dnscache_init() {
    //printf("cache_size: %d, avg_size: %d, ttl: %d\n", DNSCACHEG(cache_size), DNSCACHEG(avg_size), DNSCACHEG(ttl));
    if (cache) {
        return;
    }

    cache = lruc_new(DNSCACHEG(cache_size), AVG_SIZE, DNSCACHEG(ttl));
    if (cache == NULL) {
        perror("Failed to init dnscache");
    }

    int status = ares_library_init(ARES_LIB_INIT_ALL);
    if (status != ARES_SUCCESS) {
        fprintf(stderr, "ares_library_init: %s\n", ares_strerror(status));
    }

    //patch("getaddrinfo", &proxy_getaddrinfo);
    //patch("freeaddrinfo", &proxy_freeaddrinfo);

    /* Create a hook that will redirect all foo() calls to to my_foo(). */
    getaddrinfo_hook = subhook_new((void *)getaddrinfo, (void *)proxy_getaddrinfo, 0);
    freeaddrinfo_hook = subhook_new((void *)freeaddrinfo, (void *)proxy_freeaddrinfo, 0);

    subhook_install(getaddrinfo_hook);
    subhook_install(freeaddrinfo_hook);
}

void dnscache_deinit() {
    /* Remove the hook and free memory when you're done. */
    subhook_remove(getaddrinfo_hook);
    subhook_free(getaddrinfo_hook);
    subhook_remove(freeaddrinfo_hook);
    subhook_free(freeaddrinfo_hook);

    ares_library_cleanup();

    lruc_free(cache);
    cache = NULL;
}
