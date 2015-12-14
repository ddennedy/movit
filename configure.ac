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

# Needed for unit tests and the demo app. We prefer SDL2 if possible,
# but can also use classic SDL.
with_SDL2=no
with_demo_app=yes
PKG_CHECK_MODULES([SDL2], [sdl2], [with_SDL2=yes], [
  PKG_CHECK_MODULES([SDL], [sdl])
])

# These are only needed for the demo app.
if test $with_SDL2 = "yes"; then
  PKG_CHECK_MODULES([SDL2_image], [SDL2_image], [], [with_demo_app=no; AC_MSG_WARN([SDL2_image not found, demo program will not be built])])
else
  PKG_CHECK_MODULES([SDL_image], [SDL_image], [], [with_demo_app=no; AC_MSG_WARN([SDL_image not found, demo program will not be built])])
fi
PKG_CHECK_MODULES([libpng], [libpng], [], [with_demo_app=no; AC_MSG_WARN([libpng not found, demo program will not be built])])

AC_SUBST([with_demo_app])
AC_SUBST([with_SDL2])

with_coverage=no
AC_ARG_ENABLE([coverage], [  --enable-coverage       build with information needed to compute test coverage], [with_coverage=yes])
AC_SUBST([with_coverage])

AC_CONFIG_FILES([Makefile movit.pc])
AC_OUTPUT
