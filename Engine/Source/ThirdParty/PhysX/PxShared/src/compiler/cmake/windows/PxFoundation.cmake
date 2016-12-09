#
# Build PxFoundation
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})

SET(PXSHARED_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../../src)

SET(LL_SOURCE_DIR ${PXSHARED_SOURCE_DIR}/foundation)

SET(PXFOUNDATION_LIBTYPE SHARED)

SET(PXFOUNDATION_PLATFORM_FILES
	${LL_SOURCE_DIR}/src/windows/PsWindowsAtomic.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsCpu.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsFPU.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsMutex.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsPrintString.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsSList.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsSocket.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsSync.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsThread.cpp
	${LL_SOURCE_DIR}/src/windows/PsWindowsTime.cpp
	${PXSHARED_SOURCE_DIR}/compiler/resource_${LIBPATH_SUFFIX}/PxFoundation.rc
)

SET(PXFOUNDATION_PLATFORM_INCLUDES
	${LL_SOURCE_DIR}/include/windows
)

SET(PXFOUNDATION_COMPILE_DEFS
	# Common to all configurations
	${PXSHARED_WINDOWS_COMPILE_DEFS};PX_FOUNDATION_DLL=1;

	$<$<CONFIG:debug>:${PXSHARED_WINDOWS_DEBUG_COMPILE_DEFS};>
	$<$<CONFIG:checked>:${PXSHARED_WINDOWS_CHECKED_COMPILE_DEFS};>
	$<$<CONFIG:profile>:${PXSHARED_WINDOWS_PROFILE_COMPILE_DEFS};>
	$<$<CONFIG:release>:${PXSHARED_WINDOWS_RELEASE_COMPILE_DEFS};>
)
	
# include PxFoundation common
INCLUDE(../common/PxFoundation.cmake)