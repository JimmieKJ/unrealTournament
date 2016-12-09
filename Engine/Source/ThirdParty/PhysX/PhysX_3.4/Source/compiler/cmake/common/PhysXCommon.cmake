#
# Build PhysXCommon common
#

IF (NOT "HTML5" IN_LIST PLATFORM_LIST)
	SET(PXCOMMON_BVH4_FILES
		${GU_SOURCE_DIR}/src/mesh/GuBV4_AABBSweep.cpp
		${GU_SOURCE_DIR}/src/mesh/GuBV4_BoxOverlap.cpp
		${GU_SOURCE_DIR}/src/mesh/GuBV4_CapsuleSweep.cpp
		${GU_SOURCE_DIR}/src/mesh/GuBV4_CapsuleSweepAA.cpp
		${GU_SOURCE_DIR}/src/mesh/GuBV4_OBBSweep.cpp
		${GU_SOURCE_DIR}/src/mesh/GuBV4_Raycast.cpp
		${GU_SOURCE_DIR}/src/mesh/GuBV4_SphereOverlap.cpp
		${GU_SOURCE_DIR}/src/mesh/GuBV4_SphereSweep.cpp
		${GU_SOURCE_DIR}/src/mesh/GuMidphaseBV4.cpp
	)
ENDIF()

# Add all source files individually, don't glob.
ADD_LIBRARY(PhysXCommon ${PXCOMMON_LIBTYPE} 
	${COMMON_SRC_DIR}/CmBoxPruning.cpp
	${COMMON_SRC_DIR}/CmCollection.cpp
	${COMMON_SRC_DIR}/CmMathUtils.cpp
	${COMMON_SRC_DIR}/CmPtrTable.cpp
	${COMMON_SRC_DIR}/CmRadixSort.cpp
	${COMMON_SRC_DIR}/CmRadixSortBuffered.cpp
	${COMMON_SRC_DIR}/CmRenderOutput.cpp
	${COMMON_SRC_DIR}/CmVisualization.cpp
	
	${PXCOMMON_PLATFORM_SRC_FILES}
	
	${GU_SOURCE_DIR}/src/GuBounds.cpp
	${GU_SOURCE_DIR}/src/GuBox.cpp
	${GU_SOURCE_DIR}/src/GuCapsule.cpp
	${GU_SOURCE_DIR}/src/GuCCTSweepTests.cpp
	${GU_SOURCE_DIR}/src/GuDebug.cpp
	${GU_SOURCE_DIR}/src/GuGeometryQuery.cpp
	${GU_SOURCE_DIR}/src/GuGeometryUnion.cpp
	${GU_SOURCE_DIR}/src/GuInternal.cpp
	${GU_SOURCE_DIR}/src/GuMeshFactory.cpp
	${GU_SOURCE_DIR}/src/GuMetaData.cpp
	${GU_SOURCE_DIR}/src/GuMTD.cpp
	${GU_SOURCE_DIR}/src/GuOverlapTests.cpp
	${GU_SOURCE_DIR}/src/GuRaycastTests.cpp
	${GU_SOURCE_DIR}/src/GuSerialize.cpp
	${GU_SOURCE_DIR}/src/GuSweepMTD.cpp
	${GU_SOURCE_DIR}/src/GuSweepSharedTests.cpp
	${GU_SOURCE_DIR}/src/GuSweepTests.cpp
	${GU_SOURCE_DIR}/src/ccd/GuCCDSweepConvexMesh.cpp
	${GU_SOURCE_DIR}/src/ccd/GuCCDSweepPrimitives.cpp
	${GU_SOURCE_DIR}/src/common/GuBarycentricCoordinates.cpp
	${GU_SOURCE_DIR}/src/common/GuSeparatingAxes.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactBoxBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleCapsule.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleConvex.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleMesh.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactConvexConvex.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactConvexMesh.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPlaneBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPlaneCapsule.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPlaneConvex.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPolygonPolygon.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereMesh.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSpherePlane.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereSphere.cpp
	${GU_SOURCE_DIR}/src/contact/GuFeatureCode.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactBoxHeightField.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactCapsuleHeightField.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactConvexHeightField.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactSphereHeightField.cpp
	${GU_SOURCE_DIR}/src/convex/GuBigConvexData.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexHelper.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexMesh.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexSupportTable.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexUtilsInternal.cpp
	${GU_SOURCE_DIR}/src/convex/GuHillClimbing.cpp
	${GU_SOURCE_DIR}/src/convex/GuShapeConvex.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistancePointBox.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistancePointTriangle.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentBox.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentSegment.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentTriangle.cpp
	${GU_SOURCE_DIR}/src/gjk/GuEPA.cpp
	${GU_SOURCE_DIR}/src/gjk/GuGJKSimplex.cpp
	${GU_SOURCE_DIR}/src/gjk/GuGJKTest.cpp
	${GU_SOURCE_DIR}/src/hf/GuHeightField.cpp
	${GU_SOURCE_DIR}/src/hf/GuHeightFieldUtil.cpp
	${GU_SOURCE_DIR}/src/hf/GuOverlapTestsHF.cpp
	${GU_SOURCE_DIR}/src/hf/GuSweepsHF.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionBoxBox.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionCapsuleTriangle.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionEdgeEdge.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayBox.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayCapsule.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRaySphere.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionSphereBox.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionTriangleBox.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4Build.cpp

	${PXCOMMON_BVH4_FILES}

	${GU_SOURCE_DIR}/src/mesh/GuMeshQuery.cpp
	${GU_SOURCE_DIR}/src/mesh/GuMidphaseRTree.cpp
	${GU_SOURCE_DIR}/src/mesh/GuOverlapTestsMesh.cpp
	${GU_SOURCE_DIR}/src/mesh/GuRTree.cpp
	${GU_SOURCE_DIR}/src/mesh/GuRTreeQueries.cpp
	${GU_SOURCE_DIR}/src/mesh/GuSweepsMesh.cpp
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMesh.cpp
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMeshBV4.cpp
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMeshRTree.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV32.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV32Build.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactBoxBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactBoxConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleHeightField.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleMesh.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexCommon.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexHeightField.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexMesh.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactGenBoxConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactGenSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactPlaneBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactPlaneCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactPlaneConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereHeightField.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereMesh.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSpherePlane.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereSphere.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMShapeConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMTriangleContactGen.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPersistentContactManifold.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxBox.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxSphere.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxTriangle_FeatureBased.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxTriangle_SAT.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleBox.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleCapsule.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleTriangle.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereSphere.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereTriangle.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepTriangleUtils.cpp
)

# Target specific compile options

TARGET_INCLUDE_DIRECTORIES(PhysXCommon 
	PRIVATE ${PXCOMMON_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PUBLIC ${PHYSX_ROOT_DIR}/Include
	PUBLIC ${PHYSX_ROOT_DIR}/Include/common
	PUBLIC ${PHYSX_ROOT_DIR}/Include/geometry
	PUBLIC ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
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
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include	
)

TARGET_COMPILE_DEFINITIONS(PhysXCommon 
	PRIVATE ${PXCOMMON_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(PhysXCommon PROPERTIES
	OUTPUT_NAME PhysX3Common
)

SET_TARGET_PROPERTIES(PhysXCommon PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "PhysX3Common${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "PhysX3Common${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "PhysX3Common${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "PhysX3Common${CMAKE_RELEASE_POSTFIX}"
)
