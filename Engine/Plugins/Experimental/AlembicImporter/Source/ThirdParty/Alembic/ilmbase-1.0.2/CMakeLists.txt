CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

PROJECT( ilmbase )

# define all search paths
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/Half")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/Iex")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/IlmThread")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/Imath")
IF( "${CMAKE_SYSTEM_NAME}" MATCHES "Windows" )
	INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/config.windows")
ELSE()
	INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/config")
ENDIF()

# gather all sources
FILE(GLOB ilmbase_sources 
	${CMAKE_CURRENT_SOURCE_DIR}/Half/* 
	${CMAKE_CURRENT_SOURCE_DIR}/Iex/* 
	${CMAKE_CURRENT_SOURCE_DIR}/IlmThread/* 
	${CMAKE_CURRENT_SOURCE_DIR}/Imath/*
)

# [mkonyk] For current platform, remove platform-specific files responsible for other platforms.
IF( WIN32 )
	# We need to exclude all files in ILM Thread which have POSIX postfix on WIN systems.
	FILE(GLOB ilmbase_sources_posix "${CMAKE_CURRENT_SOURCE_DIR}/IlmThread/*Posix.*")
	# Remove all detected POSIX postfix'ed files from the general list.
	LIST(REMOVE_ITEM ilmbase_sources ${ilmbase_sources_posix})
ELSEIF( UNIX )
	# We need to exclude all files in ILM Thread which have Win32 postfix on POSIX systems.
	FILE(GLOB ilmbase_sources_win "${CMAKE_CURRENT_SOURCE_DIR}/IlmThread/*Win32.*")
	# Remove all detected Win32 postfix'ed files from the general list.
	LIST(REMOVE_ITEM ilmbase_sources ${ilmbase_sources_win})
ENDIF()

# ilmbase
ADD_EXECUTABLE(ilmbase_toFloat ${CMAKE_CURRENT_SOURCE_DIR}/Half/toFloat/toFloat.cpp)
ADD_EXECUTABLE(ilmbase_eLut ${CMAKE_CURRENT_SOURCE_DIR}/Half/eLut/eLut.cpp)

set_property(TARGET "ilmbase_toFloat" PROPERTY FOLDER "${IlmBaseFolder}")
set_property(TARGET "ilmbase_eLut" PROPERTY FOLDER "${IlmBaseFolder}")

GET_TARGET_PROPERTY(ilmbase_toFloat_EXE ilmbase_toFloat LOCATION)
GET_TARGET_PROPERTY(ilmbase_eLut_EXE ilmbase_eLut LOCATION)
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/Half/toFloat.h
	COMMAND ${ilmbase_toFloat_EXE} > ${CMAKE_CURRENT_SOURCE_DIR}/Half/toFloat.h
	DEPENDS ilmbase_toFloat
)
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/Half/eLut.h
	COMMAND ${ilmbase_eLut_EXE} > ${CMAKE_CURRENT_SOURCE_DIR}/Half/eLut.h
	DEPENDS ilmbase_toFloat
)
ADD_LIBRARY(ilmbase STATIC 
	${ilmbase_sources}
	${CMAKE_CURRENT_SOURCE_DIR}/Half/toFloat.h
	${CMAKE_CURRENT_SOURCE_DIR}/Half/eLut.h
)
set_property(TARGET "ilmbase" PROPERTY FOLDER "${IlmBaseFolder}")


ADD_DEPENDENCIES(ilmbase ilmbase_toFloat)
ADD_DEPENDENCIES(ilmbase ilmbase_eLut)
