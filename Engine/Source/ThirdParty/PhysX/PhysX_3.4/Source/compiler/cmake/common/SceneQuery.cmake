#
# Build SceneQuery common
#

ADD_LIBRARY(SceneQuery STATIC 
	${LL_SOURCE_DIR}/SqAABBPruner.cpp
	${LL_SOURCE_DIR}/SqAABBTree.cpp
	${LL_SOURCE_DIR}/SqAABBTreeUpdateMap.cpp
	${LL_SOURCE_DIR}/SqBucketPruner.cpp
	${LL_SOURCE_DIR}/SqExtendedBucketPruner.cpp	
	${LL_SOURCE_DIR}/SqPruningPool.cpp
	${LL_SOURCE_DIR}/SqSceneQueryManager.cpp
	${LL_SOURCE_DIR}/SqBounds.cpp
	${LL_SOURCE_DIR}/SqMetaData.cpp
	${LL_SOURCE_DIR}/SqPruningStructure.cpp
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(SceneQuery 
	PRIVATE ${SCENEQUERY_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/pvd
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
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

	PRIVATE ${PHYSX_SOURCE_DIR}/SceneQuery/include
	PRIVATE ${PHYSX_SOURCE_DIR}/SimulationController/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysX/src
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysX/src/buffering
)

# No linked libraries

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(SceneQuery 

	# Common to all configurations
	PRIVATE ${SCENEQUERY_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(SceneQuery PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "SceneQuery${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "SceneQuery${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "SceneQuery${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "SceneQuery${CMAKE_RELEASE_POSTFIX}"
)