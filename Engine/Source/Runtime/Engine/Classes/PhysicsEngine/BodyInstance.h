// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysxUserData.h"
#include "CollisionQueryParams.h"
#include "BodyInstance.generated.h"

#define UE_WITH_PHYSICS (WITH_PHYSX || WITH_BOX2D)

struct FCollisionShape;
struct FConstraintInstance;
class UPhysicsConstraintComponent;

/** Delegate for applying custom physics forces upon the body. Can be passed to "AddCustomPhysics" so 
  * custom forces and torques can be calculated induvidually for every physics substep.
  * The function provides delta time for a physics step and pointer to body instance upon which forces must be added.
  * 
  * Do not expect this callback to be called from the main game thread! It may get called from a physics simulation thread. */
DECLARE_DELEGATE_TwoParams(FCalculateCustomPhysics, float, FBodyInstance*);

#if WITH_PHYSX
struct FShapeData;

namespace physx
{
	class PxRigidActor;
	class PxAggregate;
	class PxRigidBody;
	class PxRigidDynamic;
	class PxGeometry;
	class PxShape;
	class PxMaterial;
	class PxTransform;
	struct PxContactPair;
	struct PxFilterData;
}

#endif // WITH_PHYSX

UENUM(BlueprintType)
namespace EDOFMode
{
	enum Type
	{
		/*Inherits the degrees of freedom from the project settings.*/
		Default,
		/*Specifies which axis to freeze rotation and movement along.*/
		SixDOF,
		/*Allows 2D movement along the Y-Z plane.*/
		YZPlane,
		/*Allows 2D movement along the X-Z plane.*/
		XZPlane,
		/*Allows 2D movement along the X-Y plane.*/
		XYPlane,
		/*Allows 2D movement along the plane of a given normal*/
		CustomPlane,
		/*No constraints.*/
		None
	};
}

struct FCollisionNotifyInfo;
template <bool bCompileStatic> struct FInitBodiesHelper;

USTRUCT()
struct ENGINE_API FCollisionResponse
{
	GENERATED_USTRUCT_BODY()

	FCollisionResponse();
	FCollisionResponse(ECollisionResponse DefaultResponse);

	/** Set the response of a particular channel in the structure. */
	void SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse);

	/** Set all channels to the specified response */
	void SetAllChannels(ECollisionResponse NewResponse);

	/** Replace the channels matching the old response with the new response */
	void ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse);

	/** Returns the response set on the specified channel */
	ECollisionResponse GetResponse(ECollisionChannel Channel) const;
	const FCollisionResponseContainer& GetResponseContainer() const { return ResponseToChannels; }

	/** Set all channels from ChannelResponse Array **/
	void SetCollisionResponseContainer(const FCollisionResponseContainer& InResponseToChannels);
	void SetResponsesArray(const TArray<FResponseChannel>& InChannelResponses);
	void UpdateResponseContainerFromArray();

private:

#if 1// @hack until PostLoad is disabled for CDO of BP - WITH_EDITOR
	/** Anything that updates array does not have to be done outside of editor
	 *	because we won't save outside of editor
	 *	During runtime, important data is ResponseToChannel
	 *	That is the data we care during runtime. But that data won't be saved.
	 */
	bool RemoveReponseFromArray(ECollisionChannel Channel);
	bool AddReponseToArray(ECollisionChannel Channel, ECollisionResponse Response);
	void UpdateArrayFromResponseContainer();
#endif

	/** Types of objects that this physics objects will collide with. */
	// @todo : make this to be transient, so that it doesn't have to save anymore
	// we have to still load them until resave
	UPROPERTY(transient)
	struct FCollisionResponseContainer ResponseToChannels;

	/** Custom Channels for Responses */
	UPROPERTY()
	TArray<FResponseChannel> ResponseArray;

	friend struct FBodyInstance;
};

enum class BodyInstanceSceneState
{
	NotAdded,
	AwaitingAdd,
	Added,
	AwaitingRemove,
	Removed
};

/** Container for a physics representation of an object */
USTRUCT()
struct ENGINE_API FBodyInstance
{
	GENERATED_USTRUCT_BODY()

	/** 
	 *	Index of this BodyInstance within the SkeletalMeshComponent/PhysicsAsset. 
	 *	Is INDEX_NONE if a single body component
	 */
	int32 InstanceBodyIndex;

	/** When we are a body within a SkeletalMeshComponent, we cache the index of the bone we represent, to speed up sync'ing physics to anim. */
	int32 InstanceBoneIndex;

	/** Current scale of physics - used to know when and how physics must be rescaled to match current transform of OwnerComponent. */
	UPROPERTY()
	FVector Scale3D;

	/** Physics scene index for the synchronous scene. */
	int32 SceneIndexSync;

	/** Physics scene index for the asynchronous scene. */
	int32 SceneIndexAsync;

	/////////
	// COLLISION SETTINGS

	/** Types of objects that this physics objects will collide with. */
	// @todo : make this to be transient, so that it doesn't have to save anymore
	// we have to still load them until resave
	UPROPERTY() 
	struct FCollisionResponseContainer ResponseToChannels_DEPRECATED;

private:
	/** Collision Profile Name **/
	UPROPERTY(EditAnywhere, Category=Custom)
	FName CollisionProfileName;

	/** Type of Collision Enabled 
	 * 
	 *	No Collision				: No collision is performed against this neither trace or physics
	 *	No Physics Collision		: This body is only used for collision raycasts, sweeps and overlaps 
	 *	Collision Enabled			: This body is used for physics simulation and collision queries
	 */
	UPROPERTY(EditAnywhere, Category=Custom)
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled;

	/** Enum indicating what type of object this should be considered as when it moves */
	UPROPERTY(EditAnywhere, Category=Custom)
	TEnumAsByte<enum ECollisionChannel> ObjectType;

	/** Custom Channels for Responses*/
	UPROPERTY(EditAnywhere, Category=Custom) 
	struct FCollisionResponse CollisionResponses;

public:

	/** If true Continuous Collision Detection (CCD) will be used for this component */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Collision)
	uint32 bUseCCD:1;

	/**	Should 'Hit' events fire when this object collides during physics simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Collision, meta=(DisplayName="Simulation Generates Hit Events"))
	uint32 bNotifyRigidBodyCollision:1;

	/////////
	// SIM SETTINGS

	/** If true, this body will use simulation. If false, will be 'fixed' (ie kinematic) and move where it is told. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Physics)
	uint32 bSimulatePhysics:1;

	/** If true, mass will not be automatically computed and you must set it directly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Physics, meta=(DisplayName="Override"))
	uint32 bOverrideMass : 1;

	/**Mass of the body in KG. By default we compute this based on physical material and mass scale.
	*@see bOverrideMass to set this directly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bOverrideMass", ClampMin = "0.001", UIMin = "0.001"))
	float MassInKg;

	/** 'Drag' force added to reduce linear movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	float LinearDamping;

	/** 'Drag' force added to reduce angular movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	float AngularDamping;

	/** If object should have the force of gravity applied */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Physics)
	uint32 bEnableGravity : 1;

	/** If true and is attached to a parent, the two bodies will be joined into a single rigid body. Physical settings like collision profile and body settings are determined by the root */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics)
	uint32 bAutoWeld : 1;

	/** determines if the body is currently welded */
	uint32 bWelded : 1;

	/** If object should start awake, or if it should initially be sleeping */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bSimulatePhysics"))
	uint32 bStartAwake:1;

	/** If true, it will update mass when scale changes **/
	UPROPERTY()
	uint32 bUpdateMassWhenScaleChanges:1;

	/** When a Locked Axis Mode is selected, will lock translation on the specified axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta=(DisplayName = "Lock Axis Translation"))
	uint32 bLockTranslation : 1;
	
	/** When a Locked Axis Mode is selected, will lock rotation to the specified axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta=(DisplayName = "Lock Axis Rotation"))
	uint32 bLockRotation : 1;

	/** Lock translation along the X-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "X"))
	uint32 bLockXTranslation : 1;

	/** Lock translation along the Y-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Y"))
	uint32 bLockYTranslation : 1;

	/** Lock translation along the Z-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Z"))
	uint32 bLockZTranslation : 1;

	/** Lock rotation about the X-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "X"))
	uint32 bLockXRotation : 1;

	/** Lock rotation about the Y-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Y"))
	uint32 bLockYRotation : 1;

	/** Lock rotation about the Z-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Z"))
	uint32 bLockZRotation : 1;

	/** Locks physical movement along specified axis.*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Mode"))
	TEnumAsByte<EDOFMode::Type> DOFMode;

	/** Locks physical movement along a custom plane for a given normal.*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Plane Normal"))
	FVector CustomDOFPlaneNormal;

	/** User specified offset for the center of mass of this object, from the calculated location */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics, meta = (DisplayName = "Center Of Mass Offset"))
	FVector COMNudge;

	/** Per-instance scaling of mass */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics)
	float MassScale;

	/** The maximum angular velocity for this instance */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bSimulatePhysics"))
	float MaxAngularVelocity;


	/** Locks physical movement along axis. */
	void SetDOFLock(EDOFMode::Type NewDOFMode);

	FVector GetLockedAxis() const;
	void CreateDOFLock();

	static EDOFMode::Type ResolveDOFMode(EDOFMode::Type DOFMode);

	/** Constraint used to allow for easy DOF setup per bodyinstance */
	FConstraintInstance * DOFConstraint;

	/** The parent body that we are welded to*/
	FBodyInstance* WeldParent;

#if WITH_PHYSX
	/** Figures out the new FCollisionNotifyInfo needed for pending notification. It adds it, and then returns an array that goes from pair index to notify collision index */
	static TArray<int32> AddCollisionNotifyInfo(const FBodyInstance* Body0, const FBodyInstance* Body1, const physx::PxContactPair * Pairs, uint32 NumPairs, TArray<FCollisionNotifyInfo> & PendingNotifyInfos);

#endif

protected:

	/**
	 * If true, this body will be put into the asynchronous physics scene. If false, it will be put into the synchronous physics scene.
	 * If the body is static, it will be placed into both scenes regardless of the value of bUseAsyncScene.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Physics)
	uint32 bUseAsyncScene:1;

	/** Whether this body instance has its own custom MaxDepenetrationVelocity*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics)
	uint32 bOverrideMaxDepenetrationVelocity : 1;

	/** The maximum velocity used to depenetrate this object*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bOverrideMaxDepenetrationVelocity", ClampMin = "0.0", UIMin = "0.0"))
	float MaxDepenetrationVelocity;

	/** Whether this instance of the object has its own custom walkable slope override setting. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Physics)
	uint32 bOverrideWalkableSlopeOnInstance : 1;

	/**
	* Custom walkable slope override setting for this instance.
	* @see GetWalkableSlopeOverride(), SetWalkableSlopeOverride()
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bOverrideWalkableSlopeOnInstance"))
	struct FWalkableSlopeOverride WalkableSlopeOverride;

	/**	Allows you to override the PhysicalMaterial to use for simple collision on this body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Collision)
	class UPhysicalMaterial* PhysMaterialOverride;

public:
	/** The set of values used in considering when put this body to sleep. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Physics)
	ESleepFamily SleepFamily;

	/**	Influence of rigid body physics (blending) on the mesh's pose (0.0 == use only animation, 1.0 == use only physics) */
	/** Provide appropriate interface for doing this instead of allowing BlueprintReadWrite **/
	UPROPERTY()
	float PhysicsBlendWeight;

	/** This physics body's solver iteration count for position. Increasing this will be more CPU intensive, but better stabilized.  */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Physics)
	int32 PositionSolverIterationCount;

	/** This physics body's solver iteration count for velocity. Increasing this will be more CPU intensive, but better stabilized. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Physics)
	int32 VelocitySolverIterationCount;

public:

#if WITH_PHYSX
	/** Internal use. Physics-engine representation of this body in the synchronous scene. */
	physx::PxRigidActor* RigidActorSync;

	/** Internal use. Physics-engine representation of this body in the asynchronous scene. */
	physx::PxRigidActor* RigidActorAsync;

	/** Internal use. Physics-engine representation of a PxAggregate for this body, in case it has alot of shapes. */
	physx::PxAggregate* BodyAggregate;

	TSharedPtr<TArray<ANSICHAR>> CharDebugName;
#endif	//WITH_PHYSX

	/** Internal use. Physics-engine id of the actor used during serialization. Needs to be outside the ifdef for serialization purposes*/
	UPROPERTY()
	uint64 RigidActorSyncId;

	/** Internal use. Physics-engine id of the actor used during serialization.  Needs to be outside the ifdef for serialization purposes*/
	UPROPERTY()
	uint64 RigidActorAsyncId;

	/** Per instance data used to initialize dynamic instances */
	/** Initial physx velocity to apply to dynamic instances */
	FVector InitialLinearVelocity;

	/** When initializing dynamic instances their component or velocity can override the bStartAwake flag */
	bool bWokenExternally;

	/** PrimitiveComponent containing this body.   */
	TWeakObjectPtr<class UPrimitiveComponent> OwnerComponent;

	/** BodySetup pointer that this instance is initialized from */
	TWeakObjectPtr<class UBodySetup> BodySetup;

	/** Constructor **/
	FBodyInstance();

	/**  
	 * Update profile data if required
	 * 
	 * @param : bVerifyProfile - if true, it makes sure it has correct set up with current profile, if false, it overwrites from profile data
	 *								(for backward compatibility)
	 * 
	 **/
	void LoadProfileData(bool bVerifyProfile);

#if WITH_PHYSX
	typedef physx::PxAggregate* PhysXAggregateType;
#elif WITH_BOX2D
	typedef void* PhysXAggregateType;
#endif

#if UE_WITH_PHYSICS
	/** Initialise a single rigid body (this FBodyInstance) for the given body setup
	 *	@param Setup The setup to use to create the body
	 *	@param Transform Transform of the body
	 *	@param PrimComp The owning component
	 *	@param InRBScene The physics scene to place the body into
	 *	@param InAggregate An aggregate to place the body into
	 */
	void InitBody(class UBodySetup* Setup, const FTransform& Transform, class UPrimitiveComponent* PrimComp, class FPhysScene* InRBScene, PhysXAggregateType InAggregate = NULL);

	/** Validate a body transform, outputting debug info
	 *	@param Transform Transform to debug
	 *	@param DebugName Name of the instance for logging
	 *	@param Setup Body setup for this instance
	 */
	static bool ValidateTransform(const FTransform &Transform, const FString& DebugName, const UBodySetup* Setup);

	/** Standalone path to batch initialize large amounts of static bodies, which will be deferred till the next scene update for fast scene addition.
	 *	@param Bodies
	 *	@param Transforms
	 *	@param BodySetup
	 *	@param PrimitiveComp
	 *	@param InRBScene
	 *  @param PhysicsSerializer
	 */
	static void InitStaticBodies(TArray<FBodyInstance*>& Bodies, TArray<FTransform>& Transforms, class UBodySetup* BodySetup, class UPrimitiveComponent* PrimitiveComp, class FPhysScene* InRBScene, class UPhysicsSerializer* PhysicsSerializer);

	/** Obtains the appropriate PhysX scene lock for READING and executes the passed in lambda. */
	void ExecuteOnPhysicsReadOnly(TFunctionRef<void()> Func) const;

	/** Obtains the appropriate PhysX scene lock for WRITING and executes the passed in lambda. */
	void ExecuteOnPhysicsReadWrite(TFunctionRef<void()> Func) const;
	
#if WITH_PHYSX

	/** If scene type is not specified return the first scene used. */
	int32 GetSceneIndex(int32 SceneType = -1) const;


	/** Initialise dynamic properties for this instance when using PhysX - this must be done after scene addition.
	 */
	DEPRECATED(4.8, "Please call InitDynamicProperties_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void InitDynamicProperties()
	{
		InitDynamicProperties_AssumesLocked();
	}

	/** Initialise dynamic properties for this instance when using PhysX - this must be done after scene addition.
	 *  Note: This function is not thread safe. Make sure to obtain the appropriate PhysX scene locks before calling this function
	 */
	void InitDynamicProperties_AssumesLocked();

	/** Populate the filter data within the provided FShapeData with the correct filters for this instance
	 *	@param ShapeData ShapeData to populate
	 *	@param bForceSimpleAsComplex Whether to force simple colision as complex
	 */
	DEPRECATED(4.8, "Please call GetFilterData_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void GetFilterData(FShapeData& ShapeData, bool bForceSimpleAsComplex = false)
	{
		GetFilterData_AssumesLocked(ShapeData, bForceSimpleAsComplex);
	}

	/** Populate the filter data within the provided FShapeData with the correct filters for this instance
	 *	@param ShapeData ShapeData to populate
	 *	@param bForceSimpleAsComplex Whether to force simple colision as complex
	 *  Note: This function is not thread safe. Make sure to obtain the appropriate PhysX scene locks before calling this function
	 */
	void GetFilterData_AssumesLocked(FShapeData& ShapeData, bool bForceSimpleAsComplex = false);

	/** Set initialisation flags on a provided shape
	 *	@param UseCollisionEnabled Whether collision is enabled for the shape
	 *	@param PShape The shape to set the flags on
	 *	@param SceneType Which scene we are using
	 *	@param bUseComplexAsSimple Whether to use complex collision as simple
	 */
	DEPRECATED(4.8, "Please call SetShapeFlags_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void SetShapeFlags(TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, physx::PxShape* PShape, EPhysicsSceneType SceneType, const bool bUseComplexAsSimple = false)
	{
		SetShapeFlags_AssumesLocked(UseCollisionEnabled, PShape, SceneType, bUseComplexAsSimple);
	}

	/** Set initialisation flags on a provided shape
	 *	@param UseCollisionEnabled Whether collision is enabled for the shape
	 *	@param PShape The shape to set the flags on
	 *	@param SceneType Which scene we are using
	 *	@param bUseComplexAsSimple Whether to use complex collision as simple
	 *  Note: This function is not thread safe. Make sure to obtain the appropriate PhysX scene locks before calling this function
	 */
	void SetShapeFlags_AssumesLocked(TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, physx::PxShape* PShape, EPhysicsSceneType SceneType, const bool bUseComplexAsSimple = false);

	/** Populate the flag fields of the provided FShapeData with correct initialisation flags
	 *	@param ShapeData ShapeData to populate
	 *	@param UseCollisionEnabled Whether collision is enabled for this instance
	 *	@param bUseComplexAsSimple Whether to use complex collision as simple
	 */
	DEPRECATED(4.8, "Please call GetShapeFlags_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void GetShapeFlags(FShapeData& ShapeData, TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, const bool bUseComplexAsSimple = false)
	{
		GetShapeFlags_AssumesLocked(ShapeData, UseCollisionEnabled, bUseComplexAsSimple);
	}

	/** Populate the flag fields of the provided FShapeData with correct initialisation flags
	 *	@param ShapeData ShapeData to populate
	 *	@param UseCollisionEnabled Whether collision is enabled for this instance
	 *	@param bUseComplexAsSimple Whether to use complex collision as simple
	 *  Note: This function is not thread safe. Make sure to obtain the appropriate PhysX scene locks before calling this function
	 */
	void GetShapeFlags_AssumesLocked(FShapeData& ShapeData, TEnumAsByte<ECollisionEnabled::Type> UseCollisionEnabled, const bool bUseComplexAsSimple = false);

	/**
	 * Return the PxRigidActor from the given scene (see EPhysicsSceneType), if SceneType is in the range [0, PST_MAX).
	 * If SceneType < 0, the PST_Sync actor is returned if it is not NULL, otherwise the PST_Async actor is returned.
	 * Invalid scene types will cause NULL to be returned.
	 */
	DEPRECATED(4.8, "Please call GetPxRigidActor_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	physx::PxRigidActor* GetPxRigidActor(int32 SceneType = -1) const
	{
		return GetPxRigidActor_AssumesLocked(SceneType);
	}

	/**
	 * Return the PxRigidActor from the given scene (see EPhysicsSceneType), if SceneType is in the range [0, PST_MAX).
	 * If SceneType < 0, the PST_Sync actor is returned if it is not NULL, otherwise the PST_Async actor is returned.
	 * Invalid scene types will cause NULL to be returned.
	 * Note: Reading/writing from/to PxRigidActor is not thread safe. If you use the actor make sure to obtain the appropriate PhysX scene lock
	 */
	physx::PxRigidActor* GetPxRigidActor_AssumesLocked(int32 SceneType = -1) const;


	/** Return the PxRigidDynamic if it exists in one of the scenes (NULL otherwise).  Currently a PxRigidDynamic can exist in only one of the two scenes. */
	DEPRECATED(4.8, "Please call GetPxRigidDynamic_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	physx::PxRigidDynamic* GetPxRigidDynamic() const
	{
		return GetPxRigidDynamic_AssumesLocked();
	}

	/** Return the PxRigidDynamic if it exists in one of the scenes (NULL otherwise).  Currently a PxRigidDynamic can exist in only one of the two scenes.
	 *  Note: This function assumes the appropriate scene lock has been obtained.
	 *  @see ExecuteOnPxRigidDynamicReadOnly, ExecuteOnPxRigidDynamicReadWrite
	 */
	physx::PxRigidDynamic* GetPxRigidDynamic_AssumesLocked() const;

	/** Return the PxRigidBody if it exists in one of the scenes (NULL otherwise).  Currently a PxRigidBody can exist in only one of the two scenes. */
	DEPRECATED(4.8, "Please call GetPxRigidBody_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	physx::PxRigidBody* GetPxRigidBody() const
	{
		return GetPxRigidBody_AssumesLocked();
	}

	/** Return the PxRigidBody if it exists in one of the scenes (NULL otherwise).  Currently a PxRigidBody can exist in only one of the two scenes.
	 *  Note: This function assumes the appropriate scene lock has been obtained.
	 *  @see ExecuteOnPxRigidBodyReadOnly, ExecuteOnPxRigidBodyReadWrite
	 */
	physx::PxRigidBody* GetPxRigidBody_AssumesLocked() const;

	

	/** 
	 *	Utility to get all the shapes from a FBodyInstance 
	 *	Shapes belonging to sync actor are first, then async. Number of shapes belonging to sync actor is returned.
	 */
	DEPRECATED(4.8, "Please call GetAllShapes_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	TArray<physx::PxShape*> GetAllShapes(int32& OutNumSyncShapes) const
	{
		TArray<physx::PxShape*> Shapes;
		OutNumSyncShapes = GetAllShapes_AssumesLocked(Shapes);
		return Shapes;
	}

	/** 
	 *	Utility to get all the shapes from a FBodyInstance 
	 *	Shapes belonging to sync actor are first, then async. Number of shapes belonging to sync actor is returned.
	 *	NOTE: This function is not thread safe. You must hold the PhysX scene lock while calling it and reading/writing from the shapes
	 */
	int32 GetAllShapes_AssumesLocked(TArray<physx::PxShape*>& OutShapes) const;
#endif	//WITH_PHYSX

#endif	//UE_WITH_PHYSICS

	void TermBody();

	/** 
	 * Takes two body instances and welds them together to create a single simulated rigid body. Returns true if success.
	 */
	bool Weld(FBodyInstance* Body, const FTransform& RelativeTM);

	/** 
	 * Takes a welded body and unwelds it. This function does not create the new body, it only removes the old one */
	void UnWeld(FBodyInstance* Body);

	/**
	 * After adding/removing shapes call this function to update mass distribution etc... */
	void PostShapeChange();

	/**
	 * Update Body Scale
	 * @param inScale3D - new Scale3D. If that's different from previous Scale3D, it will update Body scale
	 * @return true if succeed
	 */
	bool UpdateBodyScale(const FVector& InScale3D);

	void UpdateTriMeshVertices(const TArray<FVector> & NewPositions);

	/** Returns the center of mass of this body (in world space) */
	FVector GetCOMPosition() const;

	/** Draws the center of mass as a wire star */
	void DrawCOMPosition(class FPrimitiveDrawInterface* PDI, float COMRenderSize, const FColor& COMRenderColor);

	/** Utility for copying properties from one BodyInstance to another. */
	void CopyBodyInstancePropertiesFrom(const FBodyInstance* FromInst);

	/** Find the correct PhysicalMaterial for simple geometry on this body */
	UPhysicalMaterial* GetSimplePhysicalMaterial() const;

	/** Find the correct PhysicalMaterial for simple geometry on a given body and owner. This is really for internal use during serialization */
	static UPhysicalMaterial* GetSimplePhysicalMaterial(const FBodyInstance* BodyInstance, TWeakObjectPtr<UPrimitiveComponent> Owner, TWeakObjectPtr<UBodySetup> BodySetupPtr);

	/** Get the complex PhysicalMaterial array for this body */
	TArray<UPhysicalMaterial*> GetComplexPhysicalMaterials() const;

	/** Find the correct PhysicalMaterial for simple geometry on a given body and owner. This is really for internal use during serialization */
	static void GetComplexPhysicalMaterials(const FBodyInstance* BodyInstance, TWeakObjectPtr<UPrimitiveComponent> Owner, TArray<UPhysicalMaterial*>& OutPhysicalMaterials);

	/** Get the complex PhysicalMaterials for this body */
	void GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*> &PhysMaterials) const;

	/** Returns the slope override struct for this instance. If we don't have our own custom setting, it will return the setting from the body setup. */
	const struct FWalkableSlopeOverride& GetWalkableSlopeOverride() const;

	/** Sets a custom slope override struct for this instance. Implicitly sets bOverrideWalkableSlopeOnInstance to true. */
	void SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride);

	/** Returns whether this body wants (and can) use the async scene. */
	DEPRECATED(4.8, "Please call UseAsyncScene and pass in the PhysScene of the owning component (GetWorld()->GetPhysicsScene()).")
	bool UseAsyncScene() const;

	bool UseAsyncScene(const FPhysScene* PhysScene) const;

	bool HasSharedShapes() const{ return bHasSharedShapes; }

	/** Indicates whether this body should use the async scene. Must be called before body is init'd, will assert otherwise. Will have no affect if there is no async scene. */
	void SetUseAsyncScene(bool bNewUseAsyncScene);

	/** Returns true if the body is not static */
	bool IsDynamic() const;

	/** Returns true if the body is non kinematic*/
	bool IsNonKinematic() const;

	/** Returns the body's mass */
	float GetBodyMass() const;
	/** Return bounds of physics representation */
	FBox GetBodyBounds() const;
	/** Return the body's inertia tensor. This is returned in local mass space */
	FVector GetBodyInertiaTensor() const;


	/** Set this body to be fixed (kinematic) or not. */
	void SetInstanceSimulatePhysics(bool bSimulate, bool bMaintainPhysicsBlending=false);
	/** Makes sure the current kinematic state matches the simulate flag */
	void UpdateInstanceSimulatePhysics();
	/** Returns true if this body is simulating, false if it is fixed (kinematic) */
	bool IsInstanceSimulatingPhysics();
	/** Should Simulate Physics **/
	bool ShouldInstanceSimulatingPhysics();
	/** Returns whether this body is awake */
	bool IsInstanceAwake() const;
	/** Wake this body */
	void WakeInstance();
	/** Force this body to sleep */
	void PutInstanceToSleep();
	/** Add custom forces and torques on the body. The callback will be called more than once, if substepping enabled, for every substep.  */
	void AddCustomPhysics(FCalculateCustomPhysics& CalculateCustomPhysics);
	/** Add a force to this body */
	void AddForce(const FVector& Force, bool bAllowSubstepping = true, bool bAccelChange = false);
	/** Add a force at a particular world position to this body */
	void AddForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping = true);
	/** Add a torque to this body */
	void AddTorque(const FVector& Torque, bool bAllowSubstepping = true, bool bAccelChange = false);
	/** Add a rotational impulse to this body */
	void AddAngularImpulse(const FVector& Impulse, bool bVelChange);
	/** Add an impulse to this body */
	void AddImpulse(const FVector& Impulse, bool bVelChange);
	/** Add an impulse to this body and a particular world position */
	void AddImpulseAtPosition(const FVector& Impulse, const FVector& Position);
	/** Set the linear velocity of this body */
	void SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent);
	/** Set the angular velocity of this body */
	void SetAngularVelocity(const FVector& NewAngVel, bool bAddToCurrent);
	/** Set the maximum angular velocity of this body */
	void SetMaxAngularVelocity(float NewMaxAngVel, bool bAddToCurrent);
	/** Set the maximum depenetration velocity the physics simulation will introduce */
	void SetMaxDepenetrationVelocity(float MaxVelocity);
	/** Set whether we should get a notification about physics collisions */
	void SetInstanceNotifyRBCollision(bool bNewNotifyCollision);
	/** Enables/disables whether this body is affected by gravity. */
	void SetEnableGravity(bool bGravityEnabled);

	/** See if this body is valid. */
	bool IsValidBodyInstance() const;

	/** Get current transform in world space from physics body. */
	FTransform GetUnrealWorldTransform() const;

	/** Get current transform in world space from physics body. */
	FTransform GetUnrealWorldTransform_AssumesLocked() const;

	/**
	 *	Move the physics body to a new pose.
	 *	@param	bTeleport	If true, no velocity is inferred on the kinematic body from this movement, but it moves right away.
	 */
	void SetBodyTransform(const FTransform& NewTransform, bool bTeleport);

	/** Get current velocity in world space from physics body. */
	FVector GetUnrealWorldVelocity() const;

	/** Get current velocity in world space from physics body. */
	FVector GetUnrealWorldVelocity_AssumesLocked() const;

	/** Get current angular velocity in world space from physics body. */
	FVector GetUnrealWorldAngularVelocity() const;

	/** Get current angular velocity in world space from physics body. */
	FVector GetUnrealWorldAngularVelocity_AssumesLocked() const;

	/** Get current velocity of a point on this physics body, in world space. Point is specified in world space. */
	FVector GetUnrealWorldVelocityAtPoint(const FVector& Point) const;

	/** Get current velocity of a point on this physics body, in world space. Point is specified in world space. */
	FVector GetUnrealWorldVelocityAtPoint_AssumesLocked(const FVector& Point) const;

	/** Set physical material override for this body */
	void SetPhysMaterialOverride(class UPhysicalMaterial* NewPhysMaterial);

	/** Set a new contact report force threhold.  Threshold < 0 disables this feature. */
	void SetContactReportForceThreshold(float Threshold);

	/** Set the collision response of this body to a particular channel */
	void SetResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);

	/** Get the collision response of this body to a particular channel */
	ECollisionResponse GetResponseToChannel(ECollisionChannel Channel) const;

	/** Set the response of this body to all channels */
	void SetResponseToAllChannels(ECollisionResponse NewResponse);

	/** Replace the channels on this body matching the old response with the new response */
	void ReplaceResponseToChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse);

	/** Set the response of this body to the supplied settings */
	void SetResponseToChannels(const FCollisionResponseContainer& NewReponses);

	/** Get Collision ResponseToChannels container for this component **/
	const FCollisionResponseContainer& GetResponseToChannels() const;

	/** Set the movement channel of this body to the one supplied */
	void SetObjectType(ECollisionChannel Channel);

	/** Get the movement channel of this body **/
	ECollisionChannel GetObjectType() const;

	/** Controls what kind of collision is enabled for this body and allows optional disable physics rebuild */
	void SetCollisionEnabled(ECollisionEnabled::Type NewType, bool bUpdatePhysicsFilterData = true);

	/** Get the current type of collision enabled */
	ECollisionEnabled::Type GetCollisionEnabled() const;

	/**  
	 * Set Collision Profile Name
	 * This function is called by constructors when they set ProfileName
	 * This will change current CollisionProfileName to be this, and overwrite Collision Setting
	 * 
	 * @param InCollisionProfileName : New Profile Name
	 */
	void SetCollisionProfileName(FName InCollisionProfileName);

	/** Get the current collision profile assigned to this body */
	FName GetCollisionProfileName() const;

	/** return true if it uses Collision Profile System. False otherwise*/
	bool DoesUseCollisionProfile() const;

	/** Modify the mass scale of this body */
	void SetMassScale(float InMassScale=1.f);

	/** Update instance's mass properties (mass, inertia and center-of-mass offset) based on MassScale, InstanceMassScale and COMNudge. */
	void UpdateMassProperties();

	/** Update instance's linear and angular damping */
	void UpdateDampingProperties();

	/** Update the instance's material properties (friction, restitution) */
	void UpdatePhysicalMaterials();

#if WITH_PHYSX
	DEPRECATED(4.8, "Please call ApplyMaterialToShape_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	static void ApplyMaterialToShape(physx::PxShape* PShape, physx::PxMaterial* PSimpleMat, TArray<UPhysicalMaterial*>& ComplexPhysMats)
	{
		ApplyMaterialToShape_AssumesLocked(PShape, PSimpleMat, ComplexPhysMats, false);
	}

	/** 
	 *  Apply a material directly to the passed in shape. Note this function is very advanced and requires knowledge of shape sharing as well as threading. Note: assumes the appropriate locks have been obtained
	 *  @param  PShape					The shape we are applying the material to
	 *  @param  PSimpleMat				The material to use if a simple shape is provided (or complex materials are empty)
	 *  @param  ComplexPhysMats			The array of materials to apply if a complex shape is provided
	 *	@param	bSharedShape			If this is true it means you've already detached the shape from all actors that use it (attached shared shapes are not writable).
	 */
	static void ApplyMaterialToShape_AssumesLocked(physx::PxShape* PShape, physx::PxMaterial* PSimpleMat, const TArray<UPhysicalMaterial*>& ComplexPhysMats, const bool bSharedShape);

	DEPRECATED(4.8, "Please call ApplyMaterialToInstanceShapes_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	void ApplyMaterialToInstanceShapes(physx::PxMaterial* PSimpleMat, TArray<UPhysicalMaterial*>& ComplexPhysMats)
	{
		ApplyMaterialToInstanceShapes_AssumesLocked(PSimpleMat, ComplexPhysMats);
	}

	/** Note: This function is not thread safe. Make sure you obtain the appropriate PhysX scene lock before calling it*/
	void ApplyMaterialToInstanceShapes_AssumesLocked(physx::PxMaterial* PSimpleMat, TArray<UPhysicalMaterial*>& ComplexPhysMats);
#endif

	/** Update the instances collision filtering data */
	void UpdatePhysicsFilterData(bool bForceSimpleAsComplex = false);

	friend FArchive& operator<<(FArchive& Ar,FBodyInstance& BodyInst);

	/** Get the name for this body, for use in debugging */
	FString GetBodyDebugName() const;

	/** 
	 *  Trace a ray against just this bodyinstance
	 *  @param  OutHit					Information about hit against this component, if true is returned
	 *  @param  Start					Start location of the ray
	 *  @param  End						End location of the ray
	 *	@param	bTraceComplex			Should we trace against complex or simple collision of this body
	 *  @param bReturnPhysicalMaterial	Fill in the PhysMaterial field of OutHit
	 *  @return true if a hit is found
	 */
	bool LineTrace(struct FHitResult& OutHit, const FVector& Start, const FVector& End, bool bTraceComplex, bool bReturnPhysicalMaterial = false) const;

	/** 
	 *  Trace a box against just this bodyinstance
	 *  @param  OutHit          Information about hit against this component, if true is returned
	 *  @param  Start           Start location of the box
	 *  @param  End             End location of the box
	 *  @param  CollisionShape	Collision Shape
	 *	@param	bTraceComplex	Should we trace against complex or simple collision of this body
	 *  @return true if a hit is found
	 */
	bool Sweep(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionShape& Shape, bool bTraceComplex) const;

#if WITH_PHYSX
	/**
	 *  Test if the bodyinstance overlaps with the geometry in the Pos/Rot
	 *
	 *	@param	PGeom			Geometry it would like to test
	 *  @param  ShapePose       Transform information in world. Use U2PTransform to convert from FTransform
	 *  @param  OutMTD			The minimum translation direction needed to push the shape out of this BodyInstance. (Optional)
	 *  @return true if PrimComp overlaps this component at the specified location/rotation
	 */
	DEPRECATED(4.8, "Please call OverlapPhysX_AssumesLocked and make sure you obtain the appropriate PhysX scene locks")
	bool OverlapPhysX(const physx::PxGeometry& Geom, const physx::PxTransform&  ShapePose, FMTDResult* OutMTD = nullptr) const
	{
		return OverlapPhysX_AssumesLocked(Geom, ShapePose, OutMTD);
	}

	/**
	 *  Test if the bodyinstance overlaps with the geometry in the Pos/Rot
	 *
	 *	@param	PGeom			Geometry it would like to test
	 *  @param  ShapePose       Transform information in world. Use U2PTransform to convert from FTransform
	 *  @param  OutMTD			The minimum translation direction needed to push the shape out of this BodyInstance. (Optional)
	 *  @return true if PrimComp overlaps this component at the specified location/rotation
	 *
	 *	Note: This function is not thread safe. Make sure to obtain the appropriate PhysX scene locks before calling this function
	 */
	bool OverlapPhysX_AssumesLocked(const physx::PxGeometry& Geom, const physx::PxTransform&  ShapePose, FMTDResult* OutMTD = nullptr) const;
#endif	//WITH_PHYSX

	/**
	 *  Test if the bodyinstance overlaps with the specified shape at the specified position/rotation
	 *
	 *  @param  Position		Position to place the shape at before testing
	 *  @param  Rotation		Rotation to apply to the shape before testing
	 *	@param	CollisionShape	Shape to test against
	 *  @param  OutMTD			The minimum translation direction needed to push the shape out of this BodyInstance. (Optional)
	 *  @return true if the geometry associated with this body instance overlaps the query shape at the specified location/rotation
	 */
	bool OverlapTest(const FVector& Position, const FQuat& Rotation, const struct FCollisionShape& CollisionShape, FMTDResult* OutMTD = nullptr) const;

	/**
	 *  Test if the bodyinstance overlaps with the specified body instances
	 *
	 *  @param  Position		Position to place our shapes at before testing (shapes of this BodyInstance)
	 *  @param  Rotation		Rotation to apply to our shapes before testing (shapes of this BodyInstance)
	 *  @param  Bodies			The bodies we are testing for overlap with. These bodies will be in world space already
	 *  @return true if any of the bodies passed in overlap with this
	 */
	bool OverlapTestForBodies(const FVector& Position, const FQuat& Rotation, const TArray<FBodyInstance*>& Bodies) const;

	/**
	 *  Determines the set of components that this body instance would overlap with at the supplied location/rotation
	 *  @note The overload taking rotation as an FQuat is slightly faster than the version using FRotator (which will be converted to an FQuat)..
	 *  @param  InOutOverlaps   Array of overlaps found between this component in specified pose and the world (new overlaps will be appended, the array is not cleared!)
	 *  @param  World			World to use for overlap test
	 *  @param  pWorldToComponent Used to convert the body instance world space position into local space before teleporting it to (Pos, Rot) [optional, use when the body instance isn't centered on the component instance]
	 *  @param  Pos             Location to place the component's geometry at to test against the world
	 *  @param  Rot             Rotation to place components' geometry at to test against the world
	 *  @param  TestChannel		The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params
	 *  @param  ResponseParams
	 *	@param	ObjectQueryParams	List of object types it's looking for. When this enters, we do object query with component shape
	 *  @return TRUE if OutOverlaps contains any blocking results
	 */
	bool OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FQuat& Rot,    ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const;
	bool OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const;

	/**
	 * Add an impulse to this bodyinstance, radiating out from the specified position.
	 *
	 * @param Origin		Point of origin for the radial impulse blast, in world space
	 * @param Radius		Size of radial impulse. Beyond this distance from Origin, there will be no affect.
	 * @param Strength		Maximum strength of impulse applied to body.
	 * @param Falloff		Allows you to control the strength of the impulse as a function of distance from Origin.
	 * @param bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no affect).
	 */
	void AddRadialImpulseToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange = false);

	/**
	 *	Add a force to this bodyinstance, originating from the supplied world-space location.
	 *
	 *	@param Origin		Origin of force in world space.
	 *	@param Radius		Radius within which to apply the force.
	 *	@param Strength		Strength of force to apply.
	 *  @param Falloff		Allows you to control the strength of the force as a function of distance from Origin.
	 *  @param bAccelChange If true, Strength is taken as a change in acceleration instead of a physical force (i.e. mass will have no affect).
	 *  @param bAllowSubstepping Whether we should sub-step this radial force. You should only turn this off if you're calling it from a sub-step callback, otherwise there will be energy loss
	 */
	void AddRadialForceToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange = false, bool bAllowSubstepping = true);

	/**
	 * Get distance to the body surface if available
	 * It is only valid if BodyShape is convex
	 * If point is inside or shape is not convex, it will return 0.f
	 *
	 * @param Point				Point in world space
	 * @param OutPointOnBody	Point on the surface of body closest to Point
	 */
	float GetDistanceToBody(const FVector& Point, FVector& OutPointOnBody) const;

	/** 
	 * Returns memory used by resources allocated for this body instance ( ex. Physx resources )
	 **/
	SIZE_T GetBodyInstanceResourceSize(EResourceSizeMode::Type Mode) const;

	/**
	 * UObject notification by OwningComponent
	 */
	void FixupData(class UObject* Loader);

	const FCollisionResponse& GetCollisionResponse() const { return CollisionResponses; }

#if WITH_PHYSX
public:
	FPhysxUserData PhysxUserData;

	// Current state of the physx body for tracking deferred addition and removal.
	BodyInstanceSceneState CurrentSceneState;

private:
	/**
	 *  Trace a box against just this bodyinstance
	 *  @param  OutHit          Information about hit against this component, if true is returned
	 *  @param  Start           Start location of the box
	 *  @param  End             End location of the box
	 *  @param  CollisionShape	Collision Shape
	 *  @param	bTraceComplex	Should we trace against complex or simple collision of this body
	 *  @param	PGeom			Geometry that will sweep
	 *  @return true if a hit is found
	 */
	bool InternalSweepPhysX(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FCollisionShape& Shape, bool bTraceComplex, const physx::PxRigidActor* RigidBody, const physx::PxGeometry* Geometry) const;

	/** 
	 * Helper function to update per shape filtering info. This should interface is not very friendly and should only be used from inside FBodyInstance
	 */
	void UpdatePhysicsShapeFilterData(uint32 SkelMeshCompID, bool bUseComplexAsSimple, bool bUseSimpleAsComplex, bool bPhysicsStatic, TEnumAsByte<ECollisionEnabled::Type> * CollisionEnabledOverride, FCollisionResponseContainer * ResponseOverride, bool * bNotifyOverride);
#endif 
	/**
	 * Invalidate Collision Profile Name
	 * This gets called when it invalidates the reason of Profile Name
	 * for example, they would like to re-define CollisionEnabled or ObjectType or ResponseChannels
	 */
	void InvalidateCollisionProfileName();
		
	/**
	 * Return true if the collision profile name is valid
	 */
	static bool IsValidCollisionProfileName(FName InCollisionProfileName);

	friend class UCollisionProfile;
	friend class FBodyInstanceCustomization;
	
	friend struct FInitBodiesHelper<true>;
	friend struct FInitBodiesHelper<false>;
	friend class FDerivedDataPhysXBinarySerializer;

#if WITH_BOX2D

public:
	class b2Body* BodyInstancePtr;

#endif	//WITH_BOX2D

private:
	bool bHasSharedShapes;
};

template<>
struct TStructOpsTypeTraits<FBodyInstance> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithCopy = false
	};
};


#if WITH_EDITOR

// Helper methods for classes with body instances
struct FBodyInstanceEditorHelpers
{
	ENGINE_API static void EnsureConsistentMobilitySimulationSettingsOnPostEditChange(UPrimitiveComponent* Component, FPropertyChangedEvent& PropertyChangedEvent);

private:
	FBodyInstanceEditorHelpers() {}
};
#endif

//////////////////////////////////////////////////////////////////////////
// BodyInstance inlines

inline bool FBodyInstance::OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	// Pass on to FQuat version
	return OverlapMulti(InOutOverlaps, World, pWorldToComponent, Pos, Rot.Quaternion(), TestChannel, Params, ResponseParams, ObjectQueryParams);
}

