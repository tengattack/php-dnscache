#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define USE_CARES 1

// https://stackoverflow.com/questions/3632160/how-to-make-a-php-extension
// we define Module constants
#define PHP_DNSCACHE_EXTNAME "dnscache"
#define PHP_DNSCACHE_VERSION "0.0.3"

ZEND_BEGIN_MODULE_GLOBALS(dnscache)
    int cache_size;
    //int avg_size;
    int ttl;
#ifdef USE_CARES
    int dns_timeout;
#endif
ZEND_END_MODULE_GLOBALS(dnscache)

ZEND_EXTERN_MODULE_GLOBALS(dnscache)

#define DNSCACHEG(v) ZEND_MODULE_GLOBALS_ACCESSOR(dnscache, v)

// then we declare the function to be exported
PHP_FUNCTION(dnscache_clear);
