#
# Build LowLevelCloth
#

SET(GW_DEPS_ROOT $ENV{GW_DEPS_ROOT})
FIND_PACKAGE(PxShared REQUIRED)
FIND_PACKAGE(NvSimd REQUIRED)

SET(PHYSX_SOURCE_DIR ${PROJECT_SOURCE_DIR}/../../../)

SET(LL_SOURCE_DIR ${PHYSX_SOURCE_DIR}/LowLevelCloth/src)

SET(LOWLEVELCLOTH_PLATFORM_INCLUDES
	${PHYSX_SOURCE_DIR}/LowLevelAABB/windows/include

)

SET(LOWLEVELCLOTH_PLATFORM_SOURCE_FILES
	${LL_SOURCE_DIR}/avx/SwSolveConstraints.cpp	
)

SET(LOWLEVELCLOTH_COMPILE_DEFS

	# Common to all configurations
	${PHYSX_WINDOWS_COMPILE_DEFS};PX_PHYSX_STATIC_LIB

	$<$<CONFIG:debug>:${PHYSX_WINDOWS_DEBUG_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=DEBUG;>
	$<$<CONFIG:checked>:${PHYSX_WINDOWS_CHECKED_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=CHECKED;>
	$<$<CONFIG:profile>:${PHYSX_WINDOWS_PROFILE_COMPILE_DEFS};PX_PHYSX_DLL_NAME_POSTFIX=PROFILE;>
	$<$<CONFIG:release>:${PHYSX_WINDOWS_RELEASE_COMPILE_DEFS};>
)

# include common low level cloth settings
INCLUDE(../common/LowLevelCloth.cmake)


#cl.exe /c /Zi /Ox /MT /arch:AVX /Fd$(TargetDir)\$(TargetName).pdb /Fo./x64/LowLevelCloth/debug/avx/SwSolveConstraints.obj ..\..\LowLevelCloth\src\avx\SwSolveConstraints.cpp	
SET_SOURCE_FILES_PROPERTIES(${LL_SOURCE_DIR}/avx/SwSolveConstraints.cpp PROPERTIES COMPILE_FLAGS "/arch:AVX") # Removed all flags except arch, should be handled on higher level.
