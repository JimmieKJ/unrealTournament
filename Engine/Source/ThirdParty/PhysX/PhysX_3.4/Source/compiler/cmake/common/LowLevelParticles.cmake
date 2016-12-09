#
# Build LowLevelParticles common
#

ADD_LIBRARY(LowLevelParticles STATIC 
	${LL_SOURCE_DIR}/PtBatcher.cpp
	${LL_SOURCE_DIR}/PtBodyTransformVault.cpp
	${LL_SOURCE_DIR}/PtCollision.cpp
	${LL_SOURCE_DIR}/PtCollisionBox.cpp
	${LL_SOURCE_DIR}/PtCollisionCapsule.cpp
	${LL_SOURCE_DIR}/PtCollisionConvex.cpp
	${LL_SOURCE_DIR}/PtCollisionMesh.cpp
	${LL_SOURCE_DIR}/PtCollisionPlane.cpp
	${LL_SOURCE_DIR}/PtCollisionSphere.cpp
	${LL_SOURCE_DIR}/PtContextCpu.cpp
	${LL_SOURCE_DIR}/PtDynamics.cpp
	${LL_SOURCE_DIR}/PtParticleData.cpp
	${LL_SOURCE_DIR}/PtParticleShapeCpu.cpp
	${LL_SOURCE_DIR}/PtParticleSystemSimCpu.cpp
	${LL_SOURCE_DIR}/PtSpatialHash.cpp
	${LL_SOURCE_DIR}/PtSpatialLocalHash.cpp
	
	${LOWLEVELPARTICLES_PLATFORM_SRC_FILES}
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(LowLevelParticles 
	PRIVATE ${LOWLEVELPARTICLES_PLATFORM_INCLUDES}
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelParticles/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelParticles/src
	
)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(LowLevelParticles 

	# Common to all configurations
	PRIVATE ${LOWLEVELPARTICLES_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(LowLevelParticles PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "LowLevelParticles${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "LowLevelParticles${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "LowLevelParticles${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "LowLevelParticles${CMAKE_RELEASE_POSTFIX}"
)