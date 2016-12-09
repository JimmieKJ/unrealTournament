#
# Build LowLevel common
#

ADD_LIBRARY(LowLevel STATIC 
	${LL_SOURCE_DIR}/API/src/px_globals.cpp
	${LL_SOURCE_DIR}/common/src/collision/PxcContact.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcContactCache.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcContactMethodImpl.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcMaterialHeightField.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcMaterialMesh.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcMaterialMethodImpl.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcMaterialShape.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcNpBatch.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcNpCacheStreamPair.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcNpContactPrepShared.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcNpMemBlockPool.cpp
	${LL_SOURCE_DIR}/common/src/pipeline/PxcNpThreadContext.cpp
	${LL_SOURCE_DIR}/software/src/PxsCCD.cpp
	${LL_SOURCE_DIR}/software/src/PxsContactManager.cpp
	${LL_SOURCE_DIR}/software/src/PxsContext.cpp
	${LL_SOURCE_DIR}/software/src/PxsMaterialCombiner.cpp

	${LL_SOURCE_DIR}/software/src/PxsDefaultMemoryManager.cpp
	${LL_SOURCE_DIR}/software/src/PxsIslandSim.cpp
	${LL_SOURCE_DIR}/software/src/PxsNphaseImplementationContext.cpp
	${LL_SOURCE_DIR}/software/src/PxsSimpleIslandManager.cpp
	
)

# Target specific compile options


TARGET_INCLUDE_DIRECTORIES(LowLevel 
	PRIVATE ${LOWLEVEL_PLATFORM_INCLUDES}
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/include
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/common
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/distance
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/sweep
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/gjk
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/intersection
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/pcm
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/ccd
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/collision
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/utils
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/software/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelDynamics/include

)

TARGET_COMPILE_DEFINITIONS(LowLevel 
	PRIVATE ${LOWLEVEL_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(LowLevel PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "LowLevel${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "LowLevel${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "LowLevel${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "LowLevel${CMAKE_RELEASE_POSTFIX}"
)