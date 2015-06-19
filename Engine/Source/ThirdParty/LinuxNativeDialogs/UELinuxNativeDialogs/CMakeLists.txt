project(LinuxFileDialogs)

cmake_minimum_required(VERSION 2.8.8)


set(CMAKE_CXX_FLAGS "-std=c++11")

find_package(Qt4 QUIET COMPONENTS QtGui)
find_package(Qt5Widgets QUIET)
find_package(GTK2 QUIET)
find_package(SDL2 QUIET)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 gtk+-3.0)

if(SDL2_FOUND)
	message(STATUS "SDL2 found")
	find_package(OpenGL REQUIRED)
endif()

macro(add_sdl_test name linkto)
  if(SDL2_FOUND)
    add_executable(${name} src/sdl2test.cpp)

    target_link_libraries(${name} ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${linkto})
  endif()
endmacro()

include_directories(include)

if(QT4_FOUND)
message(STATUS "Qt4 found")

add_library(LND-qt4 SHARED src/qtdialog.cpp)

target_link_libraries(LND-qt4 Qt4::QtGui)

# uncomment if developing, not needed for production
#add_executable(qt4test src/dialogtest.cpp)
#target_link_libraries(qt4test LND-qt4)
#add_sdl_test(qt4sdltest LND-qt4)

endif()

if(Qt5Widgets_FOUND)
message(STATUS "Qt5 found")

add_library(LND-qt5 SHARED src/qtdialog.cpp)

target_link_libraries(LND-qt5 Qt5::Widgets)

# uncomment if developing, not needed for production
#add_executable(qt5test src/dialogtest.cpp)
#target_link_libraries(qt5test LND-qt5)
#add_sdl_test(qt5sdltest LND-qt5)

endif()

if(GTK2_FOUND)
message(STATUS "GTK2 found")

include_directories(${GTK2_INCLUDE_DIRS})
add_library(LND-gtk2 SHARED src/gtkdialog.c)
target_link_libraries(LND-gtk2 ${GTK2_LIBRARIES})

# uncomment if developing, not needed for production
#add_executable(gtk2test src/dialogtest.cpp)
#target_link_libraries(gtk2test LND-gtk2)
#add_sdl_test(gtk2sdltest LND-gtk2)

endif()

if(GTK3_FOUND)
message(STATUS "GTK3 found")

include_directories(${GTK3_INCLUDE_DIRS})
add_library(LND-gtk3 SHARED src/gtkdialog.c)
target_link_libraries(LND-gtk3 ${GTK3_LIBRARIES})

# uncomment if developing, not needed for production
#add_executable(gtk3test src/dialogtest.cpp)
#target_link_libraries(gtk3test LND-gtk3)
#add_sdl_test(gtk3sdltest LND-gtk3)

endif()

add_library(LND SHARED src/dialog.cpp)
target_link_libraries(LND ${CMAKE_DL_LIBS})

# uncomment if developing, not needed for production
#add_executable(dialogtest src/dialogtest.cpp)
#target_link_libraries(dialogtest LND)

