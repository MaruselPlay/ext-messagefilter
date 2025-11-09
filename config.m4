PHP_ARG_ENABLE([messagefilter],
  [whether to enable messagefilter support],
  [AS_HELP_STRING([--enable-messagefilter],
    [Enable messagefilter support])],
  [no])

if test "$PHP_MESSAGEFILTER" != "no"; then
  AC_DEFINE(HAVE_MESSAGEFILTER, 1, [ Have messagefilter support ])
  PHP_NEW_EXTENSION(messagefilter, messagefilter.c, $ext_shared)
fi