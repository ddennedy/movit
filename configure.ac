AC_CONFIG_MACRO_DIR([m4])
AC_INIT(movit, git)
LT_INIT
PKG_PROG_PKG_CONFIG

AC_CONFIG_SRCDIR(effect.cpp)
AC_CONFIG_AUX_DIR(.)

AC_PROG_CC
AC_PROG_CXX
PKG_CHECK_MODULES([Eigen3], [eigen3])
PKG_CHECK_MODULES([epoxy], [epoxy])
PKG_CHECK_MODULES([FFTW3], [fftw3])

CXXFLAGS="$CXXFLAGS -std=gnu++11"

# Needed for unit tests and the demo app.
with_demo_app=yes
PKG_CHECK_MODULES([SDL2], [sdl2])

# This is only needed for the demo app.
PKG_CHECK_MODULES([SDL2_image], [SDL2_image], [], [with_demo_app=no; AC_MSG_WARN([SDL2_image not found, demo program will not be built])])
PKG_CHECK_MODULES([libpng], [libpng], [], [with_demo_app=no; AC_MSG_WARN([libpng not found, demo program will not be built])])

# This is only needed for microbenchmarks, so optional.
PKG_CHECK_MODULES([benchmark], [benchmark], [with_benchmark=yes], [with_benchmark=no; AC_MSG_WARN([Google microbenchmark framework not found, microbenchmarks will not be built])])

AC_SUBST([with_demo_app])
AC_SUBST([with_benchmark])

with_coverage=no
AC_ARG_ENABLE([coverage], [  --enable-coverage       build with information needed to compute test coverage], [with_coverage=yes])
AC_SUBST([with_coverage])

AC_CONFIG_FILES([Makefile movit.pc])
AC_OUTPUT
