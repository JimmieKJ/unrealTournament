#
# Build SimulationController common
#

ADD_LIBRARY(SimulationController STATIC 
	${LL_SOURCE_DIR}/ScActorCore.cpp
	${LL_SOURCE_DIR}/ScActorSim.cpp
	${LL_SOURCE_DIR}/ScArticulationCore.cpp
	${LL_SOURCE_DIR}/ScArticulationJointCore.cpp
	${LL_SOURCE_DIR}/ScArticulationJointSim.cpp
	${LL_SOURCE_DIR}/ScArticulationSim.cpp
	${LL_SOURCE_DIR}/ScBodyCore.cpp
	${LL_SOURCE_DIR}/ScBodyCoreKinematic.cpp
	${LL_SOURCE_DIR}/ScBodySim.cpp
	${LL_SOURCE_DIR}/ScConstraintCore.cpp
	${LL_SOURCE_DIR}/ScConstraintGroupNode.cpp
	${LL_SOURCE_DIR}/ScConstraintInteraction.cpp
	${LL_SOURCE_DIR}/ScConstraintProjectionManager.cpp
	${LL_SOURCE_DIR}/ScConstraintProjectionTree.cpp
	${LL_SOURCE_DIR}/ScConstraintSim.cpp
	${LL_SOURCE_DIR}/ScElementInteractionMarker.cpp
	${LL_SOURCE_DIR}/ScElementSim.cpp
	${LL_SOURCE_DIR}/ScInteraction.cpp
	${LL_SOURCE_DIR}/ScIterators.cpp	
	${LL_SOURCE_DIR}/ScMaterialCore.cpp
	${LL_SOURCE_DIR}/ScMetaData.cpp
	${LL_SOURCE_DIR}/ScNPhaseCore.cpp
	${LL_SOURCE_DIR}/ScPhysics.cpp
	${LL_SOURCE_DIR}/ScRigidCore.cpp
	${LL_SOURCE_DIR}/ScRigidSim.cpp
	${LL_SOURCE_DIR}/ScScene.cpp
	${LL_SOURCE_DIR}/ScShapeCore.cpp
	${LL_SOURCE_DIR}/ScShapeInteraction.cpp
	${LL_SOURCE_DIR}/ScShapeSim.cpp
	${LL_SOURCE_DIR}/ScSimStats.cpp
	${LL_SOURCE_DIR}/ScStaticCore.cpp
	${LL_SOURCE_DIR}/ScStaticSim.cpp
	${LL_SOURCE_DIR}/ScTriggerInteraction.cpp
	${LL_SOURCE_DIR}/cloth/ScClothCore.cpp
	${LL_SOURCE_DIR}/cloth/ScClothFabricCore.cpp
	${LL_SOURCE_DIR}/cloth/ScClothShape.cpp
	${LL_SOURCE_DIR}/cloth/ScClothSim.cpp
	${LL_SOURCE_DIR}/particles/ScParticleBodyInteraction.cpp
	${LL_SOURCE_DIR}/particles/ScParticlePacketShape.cpp
	${LL_SOURCE_DIR}/particles/ScParticleSystemCore.cpp
	${LL_SOURCE_DIR}/particles/ScParticleSystemSim.cpp
	
	${LL_SOURCE_DIR}/ScSimulationController.cpp
	${LL_SOURCE_DIR}/ScSqBoundsManager.cpp
	

)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(SimulationController 
	PRIVATE ${SIMULATIONCONTROLLER_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/pvd
	PRIVATE ${PHYSX_ROOT_DIR}/Include/particles
	PRIVATE ${PHYSX_ROOT_DIR}/Include/cloth
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils
	
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
	
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
	
	PRIVATE ${PHYSX_SOURCE_DIR}/SimulationController/include
	PRIVATE ${PHYSX_SOURCE_DIR}/SimulationController/src
	PRIVATE ${PHYSX_SOURCE_DIR}/SimulationController/src/particles
	PRIVATE ${PHYSX_SOURCE_DIR}/SimulationController/src/cloth
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/collision
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/utils
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/software/include

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelDynamics/include

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelCloth/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelAABB/include
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelParticles/include
	
)

# No linked libraries

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(SimulationController 

	# Common to all configurations
	PRIVATE ${SIMULATIONCONTROLLER_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(SimulationController PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "SimulationController${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "SimulationController${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "SimulationController${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "SimulationController${CMAKE_RELEASE_POSTFIX}"
)