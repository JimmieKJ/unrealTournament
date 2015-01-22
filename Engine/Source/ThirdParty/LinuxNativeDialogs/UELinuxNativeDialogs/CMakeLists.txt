project(UELinuxFileDialogs)

cmake_minimum_required(VERSION 2.8.8)

find_package(PkgConfig REQUIRED)
find_package(Qt4 QUIET COMPONENTS QtGui)
find_package(Qt5Widgets QUIET)
find_package(GTK2 QUIET)
pkg_check_modules(GTK3 gtk+-3.0)
pkg_check_modules(SDL2 sdl2 QUIET)

if(SDL2_FOUND)
  find_package(OpenGL REQUIRED)
endif(SDL2_FOUND)

if(SDL2_FOUND)
    message(STATUS "SDL2 found")
endif()

macro(add_sdl_test name linkto)
  if(SDL2_FOUND)
    message("SDL2 found ${SDL2_LIBRARIES}")
    add_executable(${name} src/sdl2test.cpp)
    target_link_libraries(${name} ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${linkto})
  endif()
endmacro()

include_directories(include)

if(QT4_FOUND)
  message(STATUS "Qt4 found")

  add_library(qt4dialog SHARED src/qtdialog.cpp)
  target_link_libraries(qt4dialog Qt4::QtGui)

  add_executable(qt4test src/dialogtest.cpp)
  target_link_libraries(qt4test qt4dialog)
  add_sdl_test(qt4sdltest qt4dialog)

endif()


if(Qt5Widgets_FOUND)
  message(STATUS "Qt5 found")

  add_library(qt5dialog SHARED src/qtdialog.cpp)
  target_link_libraries(qt5dialog Qt5::Widgets)

  add_executable(qt5test src/dialogtest.cpp)
  target_link_libraries(qt5test qt5dialog)
  add_sdl_test(qt5sdltest qt5dialog)

endif()


if(GTK2_FOUND)
  message(STATUS "GTK2 found")

  add_library(gtk2dialog SHARED src/gtkdialog.c)
  set_property( TARGET gtk2dialog APPEND PROPERTY INCLUDE_DIRECTORIES ${GTK2_INCLUDE_DIRS})
  target_link_libraries(gtk2dialog ${GTK2_LIBRARIES})

  add_executable(gtk2test src/dialogtest.cpp)
  target_link_libraries(gtk2test gtk2dialog)
  add_sdl_test(gtk2sdltest gtk2dialog)

endif()

if(GTK3_FOUND)
  message(STATUS "GTK3 found")

  add_library(gtk3dialog SHARED src/gtkdialog.c)
  set_property( TARGET gtk3dialog APPEND PROPERTY INCLUDE_DIRECTORIES ${GTK3_INCLUDE_DIRS})
  target_link_libraries(gtk3dialog ${GTK3_LIBRARIES})

  add_executable(gtk3test src/dialogtest.cpp)
  target_link_libraries(gtk3test gtk3dialog)
  add_sdl_test(gtk3sdltest gtk3dialog)

endif()
