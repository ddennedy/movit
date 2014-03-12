AC_CONFIG_MACRO_DIR([m4])
AC_INIT(movit, git)
LT_INIT

AC_CONFIG_SRCDIR(effect.cpp)

AC_PROG_CC
AC_PROG_CXX
PKG_CHECK_MODULES([Eigen3], [eigen3])
PKG_CHECK_MODULES([GLEW], [glew])

# Needed for unit tests and the demo app.
PKG_CHECK_MODULES([SDL], [sdl])

# These are only needed for the demo app.
with_demo_app=yes
PKG_CHECK_MODULES([SDL_image], [SDL_image], [], [with_demo_app=no; AC_MSG_WARN([SDL_image not found, demo program will not be built])])
PKG_CHECK_MODULES([libpng], [libpng12], [], [with_demo_app=no; AC_MSG_WARN([libpng12 not found, demo program will not be built])])

AC_SUBST([with_demo_app])

with_coverage=no
AC_ARG_ENABLE([coverage], [  --enable-coverage       build with information needed to compute test coverage], [with_coverage=yes])
AC_SUBST([with_coverage])

AC_CONFIG_FILES([Makefile movit.pc])
AC_OUTPUT
