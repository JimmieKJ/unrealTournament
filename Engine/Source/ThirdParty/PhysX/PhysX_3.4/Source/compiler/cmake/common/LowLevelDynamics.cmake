#
# Build LowLevelDynamics common
#

ADD_LIBRARY(LowLevelDynamics STATIC 
	${LL_SOURCE_DIR}/DyArticulation.cpp
	${LL_SOURCE_DIR}/DyArticulationContactPrep.cpp
	${LL_SOURCE_DIR}/DyArticulationContactPrepPF.cpp
	${LL_SOURCE_DIR}/DyArticulationHelper.cpp
	${LL_SOURCE_DIR}/DyArticulationScalar.cpp
	${LL_SOURCE_DIR}/DyArticulationSIMD.cpp
	${LL_SOURCE_DIR}/DyConstraintPartition.cpp
	${LL_SOURCE_DIR}/DyConstraintSetup.cpp
	${LL_SOURCE_DIR}/DyConstraintSetupBlock.cpp
	${LL_SOURCE_DIR}/DyContactPrep.cpp
	${LL_SOURCE_DIR}/DyContactPrep4.cpp
	${LL_SOURCE_DIR}/DyContactPrep4PF.cpp
	${LL_SOURCE_DIR}/DyContactPrepPF.cpp
	${LL_SOURCE_DIR}/DyDynamics.cpp
	${LL_SOURCE_DIR}/DyFrictionCorrelation.cpp
	${LL_SOURCE_DIR}/DyRigidBodyToSolverBody.cpp
	${LL_SOURCE_DIR}/DySolverConstraints.cpp
	${LL_SOURCE_DIR}/DySolverConstraintsBlock.cpp
	${LL_SOURCE_DIR}/DySolverControl.cpp
	${LL_SOURCE_DIR}/DySolverControlPF.cpp
	${LL_SOURCE_DIR}/DySolverPFConstraints.cpp
	${LL_SOURCE_DIR}/DySolverPFConstraintsBlock.cpp
	${LL_SOURCE_DIR}/DyThreadContext.cpp
	${LL_SOURCE_DIR}/DyThresholdTable.cpp
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(LowLevelDynamics 
	PRIVATE ${LOWLEVELDYNAMICS_PLATFORM_INCLUDES}
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/include
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/utils
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/software/include

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelDynamics/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelDynamics/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(LowLevelDynamics 

	# Common to all configurations
	PRIVATE ${LOWLEVELDYNAMICS_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(LowLevelDynamics PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "LowLevelDynamics${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "LowLevelDynamics${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "LowLevelDynamics${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "LowLevelDynamics${CMAKE_RELEASE_POSTFIX}"
)