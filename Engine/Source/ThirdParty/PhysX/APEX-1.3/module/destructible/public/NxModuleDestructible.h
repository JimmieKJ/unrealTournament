// This code contains NVIDIA Confidential Information and is disclosed to you
// under a form of NVIDIA software license agreement provided separately to you.
//
// Notice
// NVIDIA Corporation and its licensors retain all intellectual property and
// proprietary rights in and to this software and related documentation and
// any modifications thereto. Any use, reproduction, disclosure, or
// distribution of this software and related documentation without an express
// license agreement from NVIDIA Corporation is strictly prohibited.
//
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2008-2014 NVIDIA Corporation. All rights reserved.

#ifndef NX_MODULE_DESTRUCTIBLE_H
#define NX_MODULE_DESTRUCTIBLE_H

#include "foundation/Px.h"
#include "foundation/PxBounds3.h"
#include "NxModule.h"

#if (NX_SDK_VERSION_MAJOR == 2)
class NxActor;
class NxActorDesc;
class NxScene;
#endif

#ifndef APEX_RUNTIME_FRACTURE
#if (NX_SDK_VERSION_MAJOR == 3)
#define APEX_RUNTIME_FRACTURE 1
#else
#define APEX_RUNTIME_FRACTURE 0
#endif
#else
#undef APEX_RUNTIME_FRACTURE
#define APEX_RUNTIME_FRACTURE 0
#endif

#ifdef PX_ANDROID
#undef APEX_RUNTIME_FRACTURE
#define APEX_RUNTIME_FRACTURE 0
#endif

namespace physx
{

#if (NX_SDK_VERSION_MAJOR == 3)
class PxRigidActor;
class PxRigidDynamic;
class PxScene;
#endif

namespace apex
{

PX_PUSH_PACK_DEFAULT

class NxDestructibleAsset;
class NxDestructibleAssetAuthoring;
class NxDestructibleActor;
class NxDestructibleStructure;
class NxDestructibleActorJoint;
class NxDestructibleChunkDesc;
class NxDestructibleActorDesc;
class NxDestructibleActorJointDesc;

#if (APEX_USE_GRB) || defined(DOXYGEN)
struct NxDestructibleGrbStats
{
	physx::PxU32 numPairs;
	physx::PxU32 numActors;
	physx::PxU32 numConvexes;
	physx::PxU32 numContacts;

	NxDestructibleGrbStats():
		numPairs(0),
		numActors(0),
		numConvexes(0),
		numContacts(0)
	{
	}
};
#endif // #if APEX_USE_GRB

/**
	Flags returned by an NxUserChunkReport
*/
struct NxApexChunkFlag
{
	enum Enum
	{
		/** The chunk is dynamic */
		DYNAMIC							=	1 << 0,

		/** The chunk has environmental support, so will remain kinematic until fractured */
		EXTERNALLY_SUPPORTED			=	1 << 1,

		/** The chunk has been fractured */
		FRACTURED						=	1 << 2,

		/** The chunk has been destroyed because the NxActor FIFO was full */
		DESTROYED_FIFO_FULL				=	1 << 3,

		/** The chunk has been destroyed because it has exceeded the maximum debris lifetime */
		DESTROYED_TIMED_OUT				=	1 << 4,

		/** The chunk has been destroyed because it has exceeded the maximum debris distance */
		DESTROYED_EXCEEDED_MAX_DISTANCE	=	1 << 5,

		/** The destroyed chunk has generated crumble particles */
		DESTROYED_CRUMBLED				=	1 << 6,

		/** The destroyed chunk has moved beyond the destructible actor's valid bounds. */
		DESTROYED_LEFT_VALID_BOUNDS		=	1 << 7
	};
};

/**
	Per-chunk data returned in NxApexDamageEventReportData
*/
struct NxApexChunkData
{
	/** The index of the chunk within the destructible asset */
	physx::PxU32			index;

	/** The hierarchy depth of the chunk */
	physx::PxU32			depth;

	/**
		The chunk's axis-aligned bounding box, in world space.
	*/
	physx::PxBounds3		worldBounds;

	/**
		How much damage the chunk has taken
	*/
	physx::PxF32			damage;

	/**
		Several flags holding chunk information, see NxApexChunkFlag.
	*/
	physx::PxU32			flags;
};

/**
	Per-actor damage event data returned by an NxUserChunkReport
*/
struct NxApexDamageEventReportData
{
	/**
		The NxDestructibleActor instance that these chunks belong to
	*/
	NxDestructibleActor*	destructible;

	/**
		Damage event hitDirection in world space.
	*/
	physx::PxVec3			hitDirection;

	/**
		The axis-aligned bounding box of all chunk fractures caused by this damage event,
		which have flags that overlap the module's chunkReportBitMask (see NxModuleDestructible::setChunkReportBitMask).
	*/
	physx::PxBounds3		worldBounds;

	/**
		Total number of fracture events caused by this damage event,
		which have flags that overlap the module's chunkReportBitMask (see NxModuleDestructible::setChunkReportBitMask).
	*/
	physx::PxU32			totalNumberOfFractureEvents;

	/**
		Min and max depth of chunk fracture events caused by this damage event,
		which have flags that overlap the module's chunkReportBitMask (see NxModuleDestructible::setChunkReportBitMask).
	*/
	physx::PxU16			minDepth;
	physx::PxU16			maxDepth;

	/**
		Array of chunk fracture event data for all chunks above a size threshold, which have flags that overlap the
		module's chunkReportBitMask (see NxModuleDestructible::setChunkReportBitMask).  Currently
		the size cutoff is determined by the NxModuleDestructible's chunkReportMaxFractureEventDepth (See
		NxModuleDestructible::setChunkReportMaxFractureEventDepth).  All chunks up to that depth, but no deeper,
		are reported in this list.  The size of this array is given by fractureEventListSize.  fractureEventList may
		be NULL if fractureEventListSize = 0.
	*/
	const NxApexChunkData* 	fractureEventList;

	/**
		Size of the fractureEventList array.  This may be less than totalNumberOfFractureEvents
		if some of the fracture events involve chunks which do not meet the size criterion
		described in the notes for the fractureEventList array.
	*/
	physx::PxU32			fractureEventListSize;

	/**
		Other PhysX actor that caused damage to NxApexDamageEventReportData.
	*/
#if NX_SDK_VERSION_MAJOR == 2
	NxActor const*			impactDamageActor;
#elif NX_SDK_VERSION_MAJOR == 3
	physx::PxActor const*	impactDamageActor;
#endif

	/**
		Impact damage position in world-space.
	*/
	physx::PxVec3			hitPosition;

	/**
		User data from applyDamage or applyRadiusDamage.
	*/
	void*					appliedDamageUserData;
};


/**
	An event structure for an optional chunk event buffer.  Contains a chunk index and an event field.

	Note: currently the only chunk state event is for visibility changes, so the VisibilityChanged bit will always be set when this struct is used.
*/
struct NxDestructibleChunkEvent
{
	enum EventMask
	{
		VisibilityChanged	=	(1 << 0),
		ChunkVisible	=		(1 << 1)
	};

	physx::PxU16	chunkIndex;
	physx::PxU16	event;
};


/**
	Chunk state event data pushed to the user, if the user requests it via NxModuleDestructible::setChunkReportSendChunkStateEvents.
*/
struct NxApexChunkStateEventData
{
	/**
		The NxDestructibleActor instance that these chunks belong to
	*/
	NxDestructibleActor*			destructible;

	/**
		Array of chunk state event data collected since the last NxUserChunkReport::onStateChangeNotify call.
	*/
	const NxDestructibleChunkEvent* stateEventList;

	/**
		Size of the stateEventList array
	*/
	physx::PxU32					stateEventListSize;
};


/**
	NxUserChunkReport - API for a user-defined callback to get information about fractured or damaged chunks
*/
class NxUserChunkReport
{
public:
	/**
		User implementation of NxUserChunkReport must overload this function.
		This function will get called when a chunk is fractured or destroyed.
		See the definition of NxApexDamageEventReportData for the information provided
		to the function.
	*/
	virtual void	onDamageNotify(const NxApexDamageEventReportData& damageEvent) = 0;

	/**
		User implementation of NxUserChunkReport must overload this function.
		This function gets called when chunk visibility changes occur, if the user has selected
		this option via NxModuleDestructible::setChunkReportSendChunkStateEvents.
		See the definition of NxApexChunkStateEventData for the information provided
		to the function.
	*/
	virtual void	onStateChangeNotify(const NxApexChunkStateEventData& visibilityEvent) = 0;

protected:
	virtual			~NxUserChunkReport() {}
};


/**
	Particle buffer data returned with NxUserChunkParticleReport
*/
struct NxApexChunkParticleReportData
{
	/** Position buffer.  The length of this buffer is given by positionCount. */
	const physx::PxVec3* positions;

	/** The array length of the positions buffer. */
	physx::PxU32 positionCount;

	/**
		Velocity buffer.  The length of this buffer is given by velocityCount.
		N.B.:  The velocity buffer might not have the same length as the positions buffer.
			It will be one of three lengths:
			velocityCount = 0: There is no velocity data with these particles.
			velocityCount = 1: All of the particle velocities are the same, given by *velocities.
			velocityCount = positionCount: Each particle velocity is given.
	*/
	const physx::PxVec3* velocities;

	/** The array length of the velocities buffer.  (See the description above.)*/
	physx::PxU32 velocityCount;
};

/**
	NxUserChunkParticleReport - API for a user-defined callback to get particle buffer data when a chunk crumbles or emits dust particles.
	Note - only crumble emitters are implemented in 1.2.0
*/
class NxUserChunkParticleReport
{
public:
	/**
		User implementation of NxUserChunkParticleReport must overload this function.
		This function will get called when an NxDestructibleActor generates crumble particle or
		dust particle effect data.  Crumble particles are generated whenever a chunk is destroyed completely and
		the NxDestructibleActorFlag CRUMBLE_SMALLEST_CHUNKS is set.  Dust line particles are
		generated along chunk boundaries whenever they are fractured free from other chunks.

		Note - only crumble emitters are implemented in 1.2.0
	*/
	virtual void	onParticleEmission(const NxApexChunkParticleReportData& particleData) = 0;

protected:
	virtual			~NxUserChunkParticleReport() {}
};


/**
	NxUserDestructiblePhysXActorReport - API for user-defined callbacks giving notification of PhysX actor creation and release from the
	destruction module.
*/
class NxUserDestructiblePhysXActorReport
{
public:
#if NX_SDK_VERSION_MAJOR == 2
	/** Called immediately after an NxActor is created in the Destruction module. */
	virtual void	onPhysXActorCreate(const NxActor& actor) = 0;
#elif NX_SDK_VERSION_MAJOR == 3
	/** Called immediately after a PxActor is created in the Destruction module. */
	virtual void	onPhysXActorCreate(const physx::PxActor& actor) = 0;
#endif

#if NX_SDK_VERSION_MAJOR == 2
	/** Called immediately before an NxActor is released in the Destruction module. */
	virtual void	onPhysXActorRelease(const NxActor& actor) = 0;
#elif NX_SDK_VERSION_MAJOR == 3
	/** Called immediately before a PxActor is released in the Destruction module. */
	virtual void	onPhysXActorRelease(const physx::PxActor& actor) = 0;
#endif

protected:
	virtual		~NxUserDestructiblePhysXActorReport() {}
};


/**
	Destructible module constants
*/
struct NxModuleDestructibleConst
{
	enum Enum
	{
		/**
			When a function returns a chunk index, or takes a chunk index as a parameter, this
			value indicates "no chunk."
		*/
		INVALID_CHUNK_INDEX	= -1
	};
};

/**
	Render mesh distinction, skinned vs. static
*/
struct NxDestructibleActorMeshType
{
	enum Enum
	{
		Skinned,
		Static,
		Count
	};
};

/**
	The core data of Damage Event. It is used for sync damage coloring.
*/
struct NxDamageEventCoreData
{
	physx::PxI32	chunkIndexInAsset;	// Which chunk the damage is being applied to (non-range based damage)
	physx::PxVec3	position;			// The position, in world space, where the damage event is originating from.
	physx::PxF32	damage;				// The amount of damage
	physx::PxF32	radius;				// The radius of the damage, if this is a range based damage event.
};

/**
	Extended damage event data, used for impact damage reports
*/
struct NxImpactDamageEventData : public NxDamageEventCoreData
{
	physx::NxDestructibleActor*	destructible;	// The destructible hit by the impacting actor
	physx::PxVec3				direction;	// The position, in world space, from which the damage is applied
#if NX_SDK_VERSION_MAJOR == 2
	NxActor const*				impactDamageActor;	// Other PhysX actor that caused damage
#elif NX_SDK_VERSION_MAJOR == 3
	physx::PxActor const*		impactDamageActor;	// Other PhysX actor that caused damage
#endif
};

/**
	NxUserImpactDamageReport - API for a user-defined callback to get a buffer of impact damage events
*/
class NxUserImpactDamageReport
{
public:
	/**
		User implementation of NxUserImpactDamageReport must overload this function.
		If an instance of this object is passed to NxModuleDestructible::setImpactDamageReportCallback,
		this function will get called once during ApexScene::fetchResults, passing back an array of
		NxImpactDamageEventData reporting all impact damage events.
	*/
	virtual void	onImpactDamageNotify(const NxImpactDamageEventData* buffer, physx::PxU32 bufferSize) = 0;
};

/*** Sync-able Callback Class ***/
template<typename NxApexDestructibleSyncHeader>
class NxUserDestructibleSyncHandler
{
public:
	virtual void	onWriteBegin(NxApexDestructibleSyncHeader *& bufferStart, physx::PxU32 bufferSize) = 0;
	virtual void	onWriteDone(physx::PxU32 headerCount) = 0;
	virtual void	onPreProcessReadBegin(NxApexDestructibleSyncHeader *& bufferStart, physx::PxU32 & bufferSize, bool & continuePointerSwizzling) = 0;
	virtual void	onPreProcessReadDone(physx::PxU32 headerCount) = 0;
	virtual void	onReadBegin(const NxApexDestructibleSyncHeader *& bufferStart) = 0;
	virtual void	onReadDone(const char * debugMessage) = 0;
protected:
	virtual ~NxUserDestructibleSyncHandler() {}
};

/*** Sync-able Damage Data ***/
struct NxApexDamageEventUnit;
struct NxApexDamageEventHeader
{
	physx::PxU32					userActorID;
	physx::PxU32					damageEventCount;
	NxApexDamageEventUnit *			damageEventBufferStart;
	NxApexDamageEventHeader *		next;
};

struct NxApexDamageEventUnit
{
	physx::PxU32	chunkIndex;
	physx::PxU32	damageEventFlags;
	physx::PxF32	damage;
	physx::PxF32	momentum;
	physx::PxF32	radius;
	physx::PxVec3	position;
	physx::PxVec3	direction;
};

/*** Sync-able Fracture Data ***/
struct NxApexFractureEventUnit;
struct NxApexFractureEventHeader
{
    physx::PxU32					userActorID;
    physx::PxU32					fractureEventCount;
    NxApexFractureEventUnit *		fractureEventBufferStart;
    NxApexFractureEventHeader *		next;
};

struct NxApexFractureEventUnit
{
    physx::PxU32	chunkIndex;
    physx::PxU32	fractureEventFlags;
	physx::PxVec3	position;
	physx::PxVec3	direction;
	physx::PxVec3	impulse;
};

/*** Sync-able Transform Data ***/
struct NxApexChunkTransformUnit;
struct NxApexChunkTransformHeader
{
    physx::PxU32					userActorID;
    physx::PxU32					chunkTransformCount;
    NxApexChunkTransformUnit *		chunkTransformBufferStart;
    NxApexChunkTransformHeader *	next;
};

struct NxApexChunkTransformUnit
{
	physx::PxU32	chunkIndex;
	physx::PxVec3	chunkPosition;
    physx::PxQuat	chunkOrientation;
};

/**
	Descriptor used to create the Destructible APEX module.
*/
class NxModuleDestructible : public NxModule
{
public:
	/** Object creation */

	/**
		Create an NxDestructibleActorJoint from the descriptor.  (See NxDestructibleActorJoint and
		NxDestructibleActorJointDesc.)
		This module will own the NxDestructibleActorJoint, so that any NxDestructibleAsset created by it will
		be released when this module is released.  You may also call the NxDestructibleActorJoint's release()
		method, as long as the NxDestructibleActorJoint is still valid in the scene. (See isDestructibleActorJointActive())
	*/
	virtual NxDestructibleActorJoint*		createDestructibleActorJoint(const NxDestructibleActorJointDesc& desc, NxApexScene& scene) = 0;

	/**
	    Query the module on the validity of a created NxDestructibleActorJoint.
	    A NxDestructibleActorJoint will no longer be valid when it is destroyed in the scene, in which case the module calls the NxDestructibleActorJoint's release().
	    Therefore, this NxDestructibleActorJoint should no longer be referenced if it is not valid.
	*/
	virtual bool                            isDestructibleActorJointActive(const NxDestructibleActorJoint* candidateJoint, NxApexScene& apexScene) const = 0;

	/** LOD */

	/**
		The maximum number of allowed NxActors which represent dynamic groups of chunks.  If a fracturing
		event would cause more NxActors to be created, then some NxActors are released and the chunks they
		represent destroyed.  The NxActors released to make room for others depends on the priority mode.
		If sorting by benefit (see setSortByBenefit), then chunks with the lowest LOD benefit are released
		first.  Otherwise, oldest chunks are released first.
	*/
	virtual void							setMaxDynamicChunkIslandCount(physx::PxU32 maxCount) = 0;

	/**
		The maximum number of allowed visible chunks in the scene.  If a fracturing
		event would cause more chunks to be created, then NxActors are released to make room for the
		created chunks.  The priority for released NxActors is the same as described in
		setMaxDynamicChunkIslandCount.
	*/
	virtual void							setMaxChunkCount(physx::PxU32 maxCount) = 0;

	/**
		Instead of keeping the youngest NxActors, keep the greatest benefit NxActors if sortByBenefit is true.
		By default, this module does not sort by benefit.  That is, oldest NxActors are released first.
	*/
	virtual void							setSortByBenefit(bool sortByBenefit) = 0;

	/**
		Deprecated
	*/
	virtual void                            setValidBoundsPadding(physx::PxF32 pad) = 0;

	/**
		Effectively eliminates the deeper level (smaller) chunks from NxDestructibleAssets (see
		NxDestructibleAsset).  If maxChunkDepthOffset = 0, all chunks can be fractured.  If maxChunkDepthOffset = 1,
		the depest level (smallest) chunks are eliminated, etc.  This prevents too many chunks from being
		formed.  In other words, the higher maxChunkDepthOffset, the lower the "level of detail."
	*/
	virtual void							setMaxChunkDepthOffset(physx::PxU32 maxChunkDepthOffset) = 0;

	/**
		Every destructible asset defines a min and max lifetime, and maximum separation distance for its chunks.
		Chunk islands are destroyed after this time or separation from their origins.  This parameter sets the
		lifetimes and max separations within their min-max ranges.  The valid range is [0,1].  Default is 0.5.
	*/
	virtual void							setMaxChunkSeparationLOD(physx::PxF32 separationLOD) = 0;


	/** General */
	/**
		Sets the user chunk fracture/destroy callback. See NxUserChunkReport.
		Set to NULL (the default) to disable.  APEX does not take ownership of the report object.
	*/
	virtual void							setChunkReport(NxUserChunkReport* chunkReport) = 0;

	/**
		Sets the user impact damage report callback.  See NxUserImpactDamageReport.
		Set to NULL (the default) to disable.  APEX does not take ownership of the report object.
	*/
	virtual void							setImpactDamageReportCallback(NxUserImpactDamageReport* impactDamageReport) = 0;

	/**
		Set a bit mask of flags (see NxApexChunkFlag) for the fracture/destroy callback (See setChunkReport.)
		Fracture events with flags that overlap this mask will contribute to the NxApexDamageEventReportData.
	*/
	virtual void							setChunkReportBitMask(physx::PxU32 chunkReportBitMask) = 0;

	/**
		Sets the user callback to receive PhysX actor create/release notifications for destruction-associated PhysX actors.
		Set to NULL (the default) to disable.
	*/
	virtual void							setDestructiblePhysXActorReport(NxUserDestructiblePhysXActorReport* destructiblePhysXActorReport) = 0;

	/**
		Set the maximum depth at which individual chunk fracture events will be reported in an NxApexDamageEventReportData's
		fractureEventList through the NxUserChunkReport.  Fracture events deeper than this will still contribute to the
		NxApexDamageEventReportData's worldBounds and totalNumberOfFractureEvents.
	*/
	virtual void							setChunkReportMaxFractureEventDepth(physx::PxU32 chunkReportMaxFractureEventDepth) = 0;

	/**
		Set whether or not the NxUserChunkReport::onStateChangeNotify interface will be used to deliver visibility
		change data to the user.

		Default = false.
	*/
	virtual void							setChunkReportSendChunkStateEvents(bool chunkReportSendChunkStateEvents) = 0;

	/**
		Sets the user chunk crumble particle buffer callback. See NxUserChunkParticleReport.
		Set to NULL (the default) to disable.  APEX does not take ownership of the report object.
	*/
	virtual void							setChunkCrumbleReport(NxUserChunkParticleReport* chunkCrumbleReport) = 0;

	/**
		Sets the user chunk dust particle buffer callback. See NxUserChunkParticleReport.
		Set to NULL (the default) to disable.  APEX does not take ownership of the report object.

		Note - this is a placeholder API.  The dust feature is disabled in 1.2.0.
	*/
	virtual void							setChunkDustReport(NxUserChunkParticleReport* chunkDustReport) = 0;

#if (NX_SDK_VERSION_MAJOR == 2) || defined(DOXYGEN)
	/**
		PhysX SDK 2.8.X.
		Allows the user to specify an alternative PhysX scene to use for world support calculations.  If NULL (the default),
		the PhysX scene associated with the NxApexScene will be used.
	*/
	virtual void							setWorldSupportPhysXScene(NxApexScene& apexScene, NxScene* physxScene) = 0;
#endif
#if (NX_SDK_VERSION_MAJOR == 3) || defined(DOXYGEN)
	/**
		PhysX SDK 3.X.
		Allows the user to specify an alternative PhysX scene to use for world support calculations.  If NULL (the default),
		the PhysX scene associated with the NxApexScene will be used.
	*/
	virtual void							setWorldSupportPhysXScene(NxApexScene& apexScene, PxScene* physxScene) = 0;
#endif

#if (NX_SDK_VERSION_MAJOR == 2) || defined(DOXYGEN)
	/**
		PhysX SDK 2.8.X.
		Returns true iff the NxActor was created by the Destructible module.  If true, the user must NOT destroy the actor.
	*/
	virtual bool							owns(const NxActor* actor) const = 0;
#endif
#if (NX_SDK_VERSION_MAJOR == 3) || defined(DOXYGEN)
	/**
		PhysX SDK 3.X.
		Returns true iff the PxRigidActor was created by the Destructible module.  If true, the user must NOT destroy the actor.
	*/
	virtual bool							owns(const PxRigidActor* actor) const = 0;

#if APEX_RUNTIME_FRACTURE
	/**
		PhysX SDK 3.X.
		Returns true iff the PxShape was created by the runtime fracture system.
	*/
	virtual bool							isRuntimeFractureShape(const PxShape& shape) const = 0;
#endif
#endif

#if (NX_SDK_VERSION_MAJOR == 2) || defined(DOXYGEN)
	/**
		PhysX SDK 2.8.X.
		Given an NxShape, returns the NxDestructibleActor and chunkIndex which belong to it,
		if the shape's NxActor is owned by the Destructible module (see the 'owns' function, above).  Otherwise returns NULL.
		Useful when a scene query such as a raycast returns a shape.
	*/
	virtual NxDestructibleActor*			getDestructibleAndChunk(const NxShape* shape, physx::PxI32* chunkIndex = NULL) const = 0;
#endif
#if (NX_SDK_VERSION_MAJOR == 3) || defined(DOXYGEN)
	/**
		PhysX SDK 3.X.
		Given an PxShape, returns the NxDestructibleActor and chunkIndex which belong to it,
		if the shape's NxActor is owned by the Destructible module (see the 'owns' function, above).  Otherwise returns NULL.
		Useful when a scene query such as a raycast returns a shape.
	*/
	virtual NxDestructibleActor*			getDestructibleAndChunk(const PxShape* shape, physx::PxI32* chunkIndex = NULL) const = 0;
#endif

	/**
		Applies damage to all NxDestructibleActors within the given radius in the apex scene.
			damage = the amount of damage at the damage point
			momentum = the magnitude of the impulse to transfer to each chunk
			position = the damage location
			radius = distance from damage location at which chunks will be affected
			falloff = whether or not to decrease damage with distance from the damage location.  If true,
				damage will decrease linearly from the full damage (at zero distance) to zero damage (at radius).
				If false, full damage is applied to all chunks within the radius.
	*/
	virtual void							applyRadiusDamage(NxApexScene& scene, physx::PxF32 damage, physx::PxF32 momentum,
	        const physx::PxVec3& position, physx::PxF32 radius, bool falloff) = 0;

	/**
		Lets the user throttle the number of SDK actor creates per frame (per scene) due to destruction, as this can be quite costly.
		The default is 0xffffffff (unlimited).
	*/
	virtual void							setMaxActorCreatesPerFrame(physx::PxU32 maxActorsPerFrame) = 0;

	/**
		Lets the user throttle the number of fractures processed per frame (per scene) due to destruction, as this can be quite costly.
		The default is 0xffffffff (unlimited).
	*/
	virtual void							setMaxFracturesProcessedPerFrame(physx::PxU32 maxFracturesProcessedPerFrame) = 0;

    /**
        Set the callback pointers from which APEX will use to return sync-able data.
    */
	virtual bool                            setSyncParams(NxUserDestructibleSyncHandler<NxApexDamageEventHeader> * userDamageEventHandler, NxUserDestructibleSyncHandler<NxApexFractureEventHeader> * userFractureEventHandler, NxUserDestructibleSyncHandler<NxApexChunkTransformHeader> * userChunkMotionHandler) = 0;

#if (NX_SDK_VERSION_MAJOR == 2) || defined(DOXYGEN)
	/**
		PhysX SDK 2.8.X only.
		Create a rigid body for the apex scene, guaranteed to have two-way interaction with destructible chunks.
		If using GRBs, the rigid body will be a GRB.  Note: only a subset of the NxActor interface is implemented.
	*/
	virtual	NxActor*						createTwoWayRb(const NxActorDesc& desc, NxApexScene& apexScene) = 0;

	/**
		PhysX SDK 2.8.X only.
		Release a rigid body created by createTwoWayRb.  If the NxActor was not created by createTwoWayRb, this function returns
		false and nothing is done.  Note: all NxActors created by createTwoWayRb, which have not been released by releaseTwoRayRb,
		will be released when this module is released.
		Returns true if successful.
	*/
	virtual bool							releaseTwoWayRb(NxActor& actor) = 0;
#else

	/**
		Create a rigid body for the apex scene, guaranteed to have two-way interaction with destructible chunks.
		If using GRBs, the rigid body will be a GRB.  Note: only a subset of the NxActor interface is implemented.
	*/
	virtual physx::PxRigidDynamic *			createTwoWayRb(const physx::PxTransform & transform, NxApexScene& apexScene) = 0;
	
	/**
		Add a rigid body for the apex scene, guaranteed to have two-way interaction with destructible chunks.
		If using GRBs, the rigid body will be a GRB.  Note: only a subset of the NxActor interface is implemented.
	*/

	virtual physx::PxRigidDynamic *			addTwoWayRb(physx::PxRigidDynamic * actorExternal, NxApexScene& apexScene) = 0;
	
	/**
		Release a rigid body created by createTwoWayRb.  If the NxActor was not created by createTwoWayRb, this function returns
		false and nothing is done.  Note: all NxActors created by createTwoWayRb, which have not been released by releaseTwoRayRb,
		will be released when this module is released.
		Returns true if successful.
	*/

	virtual bool							releaseTwoWayRb(physx::PxRigidDynamic& actor) = 0;
#endif

#if APEX_USE_GRB || defined (DOXYGEN)
	/**
		The following functions are for GPU rigid body settings
	*/

	/**
		The grid size to use for the triangle mesh acceleration structure.

		The GPU collision detection system uses a uniform grid to accelerate static triangle mesh collision. The meshCellSize controls
		the size of the cells in the uniform grid. When static triangle meshes are created, they are voxelized into the uniform grid.
		If meshCellSize is set too low then each triangle will intersect with too many grid cells, reducing performance. 
		If the value is set too high then each grid cell will contain too many triangles, reducing performance. Dynamic shapes (such
		as the dynamic convex debris created by APEX destruction) do not use the uniform grid structure, so they should provide good
		performance at any size.
	*/
	virtual void							setGrbMeshCellSize(float cellSize) = 0;

	/**
		The maximum linear acceleration which a GRB will be given.  This may also be set in the gpuRigidBodySettings parameters
		in NxModuleDestructible::init(params).
	*/
	virtual void							setGrbMaxLinAcceleration(float maxLinAcceleration) = 0;

	/**
		Enable or disable GRB simulation in the given NxApexScene.
		This module also allows GRB simulation to be controlled after scene creation using the NxMo
	*/
	virtual void							setGrbSimulationEnabled(NxApexScene& apexScene, bool enabled) = 0;

	/**
		Query the state of GRB simulation in the given NxApexScene.
		Note: even if GRB simulation is attempted to be enabled using the NxApexSceneDesc or the setGrbSimulationEnabled call,
		above, GRBs will only be enabled if a valid GPU device ordinal is set using the gpuRigidBodySettings.gpuDeviceOrdinal
		in the NxModuleDestructible::init(params) function.  This function returns true if both of these conditions are met.
	*/
	virtual bool							isGrbSimulationEnabled(const NxApexScene& apexScene) const = 0;
#endif

	/** The following functions control the use of legacy behavior. */

	/** 
		By default, destruction damage application (either point or radius damage) now use the exact collision bounds
		of the chunk to determine damage application.  Before, bounding spheres were used on "leaf" chunks (deepest in the
		fracture hierarchy) and only the bounding sphere center (effectively radius 0) was used on all chunks higher in the
		hierarchy.  This led to inconsistent behavior when damaging destructibles at various levels of detail.

		If true is passed into this function, the legacy behavior is restored.
	*/
	virtual void							setUseLegacyChunkBoundsTesting(bool useLegacyChunkBoundsTesting) = 0;

	/** 
		By default, the BehaviorGroup fields damageSpread.minimumRadius, damageSpread.radiusMultiplier, and
		damageSpread.falloffExponent are used for point and radius damage.  Impact damage still uses damageToRadius,
		but this radius does not scale with the size of the destructible.
		
		The old, deprecated behavior was to base damage spread upon the damageToRadius field of the BehaviorGroup used
		by the chunk.  Specifically, damage would spread throughout a damage radius calculated as
		radius = damageToRadius * (damage / damageThreshold) * (size of destructible).

		If true is passed into this function, the legacy behavior is restored.
	*/
	virtual void							setUseLegacyDamageRadiusSpread(bool useLegacyDamageRadiusSpread) = 0;

	/**
		Sets mass scaling properties in a given apex scene.

		massScale and scaledMassExponent are used for scaling dynamic chunk masses.
		The 'real' mass m of a chunk is calculated by the destructible actor's density multiplied by
		the total volume of the chunk's (scaled) collision shapes.  The mass used in
		the simulation is massScale*pow(m/massScale,scaledMassExponent).  Values less than 1 have the
		effect of reducing the ratio of different masses.  The closer scaledMassExponent is to zero, the
		more the ratio will be 'flattened.'  This helps PhysX converge when there is a very large number
		of interacting rigid bodies (such as a pile of destructible chunks).

		massScale valid range:  (0,infinity).  Default = 1.0.
		scaledMassExponent valid range: [0,1].  Default = 0.5.

		Returns true iff the parameters are valid and the apexScene is valid.
	*/
	virtual bool							setMassScaling(physx::PxF32 massScale, physx::PxF32 scaledMassExponent, NxApexScene& apexScene) = 0;

	/**
		If a static PhysX actor is about to be removed, or has just been added, to the scene, then world support may change.
		Use this function to notify the destruction module that it may need to reconsider world support in the given volumes.
		This may be called multiple times, and the bounds list will accumulate.  This list will be processed upon the next APEX
		simulate call, and cleared.
	*/
	virtual void							invalidateBounds(const physx::PxBounds3* bounds, physx::PxU32 boundsCount, NxApexScene& apexScene) = 0;

protected:
	virtual									~NxModuleDestructible() {}
};


#if !defined(_USRDLL)
void instantiateModuleDestructible();
#endif

PX_POP_PACK

}
} // end namespace physx::apex

#endif // NX_MODULE_DESTRUCTIBLE_H
