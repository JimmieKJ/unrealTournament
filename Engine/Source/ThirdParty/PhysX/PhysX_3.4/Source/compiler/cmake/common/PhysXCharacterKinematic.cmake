#
# Build PhysXCharacterKinematic common
#

ADD_LIBRARY(PhysXCharacterKinematic STATIC 
	${LL_SOURCE_DIR}/CctBoxController.cpp
	${LL_SOURCE_DIR}/CctCapsuleController.cpp
	${LL_SOURCE_DIR}/CctCharacterController.cpp
	${LL_SOURCE_DIR}/CctCharacterControllerCallbacks.cpp
	${LL_SOURCE_DIR}/CctCharacterControllerManager.cpp
	${LL_SOURCE_DIR}/CctController.cpp
	${LL_SOURCE_DIR}/CctObstacleContext.cpp
	${LL_SOURCE_DIR}/CctSweptBox.cpp
	${LL_SOURCE_DIR}/CctSweptCapsule.cpp
	${LL_SOURCE_DIR}/CctSweptVolume.cpp
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(PhysXCharacterKinematic 
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/characterkinematic
	PRIVATE ${PHYSX_ROOT_DIR}/Include/extensions
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils
	
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/common
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/distance
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/gjk
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/intersection
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/pcm
)

TARGET_COMPILE_DEFINITIONS(PhysXCharacterKinematic 

	# Common to all configurations
	PRIVATE ${PHYSXCHARACTERKINEMATICS_COMPILE_DEFS}
)


# Add linked libraries
TARGET_LINK_LIBRARIES(PhysXCharacterKinematic PUBLIC PhysXCommon PhysXExtensions PxFoundation)

SET_TARGET_PROPERTIES(PhysXCharacterKinematic PROPERTIES
	OUTPUT_NAME PhysX3CharacterKinematic
)

SET_TARGET_PROPERTIES(PhysXCharacterKinematic PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "PhysX3CharacterKinematic${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "PhysX3CharacterKinematic${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "PhysX3CharacterKinematic${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "PhysX3CharacterKinematic${CMAKE_RELEASE_POSTFIX}"
)