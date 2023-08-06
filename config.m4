dnl config.m4
PHP_ARG_WITH(dnscache)

if test "$PHP_DNSCACHE" != "no"; then
  PHP_NEW_EXTENSION(dnscache, dnscache.c inject.c lruc.c subhook/subhook.c, $ext_shared)

  DNSCACHE_SHARED_LIBADD="-L$ext_srcdir/c-ares/src/lib/.libs -lcares"
  PHP_ADD_INCLUDE($ext_srcdir/c-ares/include)

  PHP_SUBST(DNSCACHE_SHARED_LIBADD)
fi
