dnl config.m4
PHP_ARG_WITH(dnscache)

if test "$PHP_DNSCACHE" != "no"; then
  PHP_NEW_EXTENSION(dnscache, dnscache.c inject.c lruc.c subhook/subhook.c, $ext_shared)
fi
