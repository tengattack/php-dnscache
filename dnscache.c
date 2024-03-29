// include the PHP API itself
#include <php.h>
// then include the header of your extension
#include "dnscache.h"
#include "inject.h"

ZEND_DECLARE_MODULE_GLOBALS(dnscache);

// Declare argument info structures
ZEND_BEGIN_ARG_INFO_EX(arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()

// register our function to the PHP API
// so that PHP knows, which functions are in this module
zend_function_entry dnscache_functions[] = {
    PHP_FE(dnscache_clear, arginfo_void)
    {NULL, NULL, NULL}
};

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("dnscache.dns_timeout_ms", "1000", PHP_INI_ALL,
                      OnUpdateLong, dns_timeout_ms, zend_dnscache_globals, dnscache_globals)
    STD_PHP_INI_ENTRY("dnscache.cache_size", "2048", PHP_INI_ALL,
                      OnUpdateLong, cache_size, zend_dnscache_globals, dnscache_globals)
    //STD_PHP_INI_ENTRY("dnscache.avg_size", "4", PHP_INI_ALL,
    //                  OnUpdateLong, avg_size, zend_dnscache_globals, dnscache_globals)
    STD_PHP_INI_ENTRY("dnscache.ttl", "20000", PHP_INI_ALL,
                      OnUpdateLong, ttl, zend_dnscache_globals, dnscache_globals)
PHP_INI_END()

static PHP_MINIT_FUNCTION(dnscache) {
    REGISTER_INI_ENTRIES();

    dnscache_init();

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(dnscache) {
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_GINIT_FUNCTION(dnscache) {
#if defined(COMPILE_DL_ASTKIT) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    dnscache_globals->dns_timeout_ms = 0;
    dnscache_globals->cache_size = 0;
    //dnscache_globals->avg_size = 0;
    dnscache_globals->ttl = 0;
}

static PHP_GSHUTDOWN_FUNCTION(dnscache) {
    dnscache_deinit();
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
    NULL, /* MINFO */
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
    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }
    // TODO: clear dnscache
    php_printf("TODO: clear dnscache\n");
}
