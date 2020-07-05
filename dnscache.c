// include the PHP API itself
#include <php.h>
#include <ext/standard/info.h>
// then include the header of your extension
#include "dnscache.h"
#include "inject.h"

#ifdef USE_CARES
#include <ares.h>
#endif

ZEND_DECLARE_MODULE_GLOBALS(dnscache);

// register our function to the PHP API 
// so that PHP knows, which functions are in this module
zend_function_entry dnscache_functions[] = {
    PHP_FE(dnscache_clear, NULL)
    {NULL, NULL, NULL}
};

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("dnscache.cache_size", "2048", PHP_INI_ALL,
                      OnUpdateLong, cache_size, zend_dnscache_globals, dnscache_globals)
    //STD_PHP_INI_ENTRY("dnscache.avg_size", "4", PHP_INI_ALL,
    //                  OnUpdateLong, avg_size, zend_dnscache_globals, dnscache_globals)
    STD_PHP_INI_ENTRY("dnscache.ttl", "20000", PHP_INI_ALL,
                      OnUpdateLong, ttl, zend_dnscache_globals, dnscache_globals)
#ifdef USE_CARES
    STD_PHP_INI_ENTRY("dnscache.dns_timeout", "5000", PHP_INI_ALL,
                      OnUpdateLong, dns_timeout, zend_dnscache_globals, dnscache_globals)
#endif
PHP_INI_END()

static PHP_MINIT_FUNCTION(dnscache) {
    REGISTER_INI_ENTRIES();

    dnscache_init();

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(dnscache)
{
    php_info_print_table_start();
#ifdef USE_CARES
    php_info_print_table_header(2, "dnscache cares", "enabled");
    php_info_print_table_row(2, "dnscache cares version", ares_version(NULL));
#else
    php_info_print_table_header(2, "dnscache cares", "disabled");
#endif
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}

static PHP_MSHUTDOWN_FUNCTION(dnscache) {
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_GINIT_FUNCTION(dnscache) {
#if defined(COMPILE_DL_ASTKIT) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    dnscache_globals->cache_size = 0;
    //dnscache_globals->avg_size = 0;
    dnscache_globals->ttl = 0;

#ifdef USE_CARES
    dnscache_globals->dns_timeout = 0;

    int ares_status = ares_library_init(ARES_LIB_INIT_ALL);
    if (ares_status != ARES_SUCCESS) {
        fprintf(stderr, "ares_library_init: %s\n", ares_strerror(ares_status));
    }
#endif
}

static PHP_GSHUTDOWN_FUNCTION(dnscache) {
    dnscache_deinit();
#ifdef USE_CARES
    ares_library_cleanup();
#endif
}

// some pieces of information about our module
zend_module_entry dnscache_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_DNSCACHE_EXTNAME,
    dnscache_functions,
    PHP_MINIT(dnscache),
    PHP_MSHUTDOWN(dnscache),
    NULL, /* RINIT */
    NULL, /* RSHUTDOWN */
    PHP_MINFO(dnscache),
    PHP_DNSCACHE_VERSION,
    PHP_MODULE_GLOBALS(dnscache),
    PHP_GINIT(dnscache),
    PHP_GSHUTDOWN(dnscache),
    NULL, /* RPOSTSHUTDOWN */
    STANDARD_MODULE_PROPERTIES_EX
};

//#ifdef COMPILE_DL_DNSCACHE
// use a macro to output additional C code, to make ext dynamically loadable
ZEND_GET_MODULE(dnscache)
//#endif

// Finally, we implement our "Hello World" function
// this function will be made available to PHP
// and prints to PHP stdout using printf
PHP_FUNCTION(dnscache_clear) {
    // TODO: clear dnscache
    php_printf("TODO: clear dnscache\n");
}
