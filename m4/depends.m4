dnl From Benedikt Meurer (benedikt.meurer@unix-ag.uni-siegen.de)
dnl
dnl

AC_DEFUN([BM_DEPEND],
[
  PKG_CHECK_MODULES([$1], [$2 >= $3])
  $1_REQUIRED_VERSION=$3
  AC_SUBST($1_REQUIRED_VERSION)
])
