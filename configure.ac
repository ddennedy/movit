AC_INIT(movit, git)

AC_CONFIG_SRCDIR(effect.cpp)

AC_PROG_CXX
PKG_CHECK_MODULES([Eigen3], [eigen3])
PKG_CHECK_MODULES([GLEW], [glew])

# These are only needed for the demo app.
PKG_CHECK_MODULES([SDL], [sdl])
PKG_CHECK_MODULES([SDL_image], [SDL_image])
PKG_CHECK_MODULES([libpng], [libpng12])

AC_CONFIG_FILES([Makefile movit.pc])
AC_OUTPUT
