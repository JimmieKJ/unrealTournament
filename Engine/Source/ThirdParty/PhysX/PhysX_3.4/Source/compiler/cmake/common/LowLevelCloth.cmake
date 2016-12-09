#
# Build LowLevelCloth common
#

ADD_LIBRARY(LowLevelCloth STATIC 
	${LOWLEVELCLOTH_PLATFORM_SOURCE_FILES}

	${LL_SOURCE_DIR}/Allocator.cpp
	${LL_SOURCE_DIR}/Factory.cpp
	${LL_SOURCE_DIR}/PhaseConfig.cpp
	${LL_SOURCE_DIR}/SwCloth.cpp
	${LL_SOURCE_DIR}/SwClothData.cpp
	${LL_SOURCE_DIR}/SwCollision.cpp
	${LL_SOURCE_DIR}/SwFabric.cpp
	${LL_SOURCE_DIR}/SwFactory.cpp
	${LL_SOURCE_DIR}/SwInterCollision.cpp
	${LL_SOURCE_DIR}/SwSelfCollision.cpp
	${LL_SOURCE_DIR}/SwSolver.cpp
	${LL_SOURCE_DIR}/SwSolverKernel.cpp
	${LL_SOURCE_DIR}/TripletScheduler.cpp
	

# These appear to be only used for one platform, NEON	
#	${LL_SOURCE_DIR}/neon/NeonCollision.cpp
#	${LL_SOURCE_DIR}/neon/NeonSelfCollision.cpp
#	${LL_SOURCE_DIR}/neon/NeonSolverKernel.cpp


)


# Target specific compile options


TARGET_INCLUDE_DIRECTORIES(LowLevelCloth 
	PRIVATE ${LOWLEVELCLOTH_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelCloth/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelCloth/src
	
	PRIVATE ${NVSIMD_INCLUDE_DIR}/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/utils
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelAABB/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelAABB/src
)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(LowLevelCloth 

	# Common to all configurations
	PRIVATE ${LOWLEVELCLOTH_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(LowLevelCloth PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "LowLevelCloth${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "LowLevelCloth${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "LowLevelCloth${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "LowLevelCloth${CMAKE_RELEASE_POSTFIX}"
)