// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsSettings.h: Declares the PhysicsSettings class.
=============================================================================*/

#pragma once

#include "Engine/DeveloperSettings.h"
#include "PhysicsSettings.generated.h"


/**
 * Structure that represents the name of physical surfaces.
 */
USTRUCT()
struct FPhysicalSurfaceName
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TEnumAsByte<enum EPhysicalSurface> Type;

	UPROPERTY()
	FName Name;

	FPhysicalSurfaceName()
		: Type(SurfaceType_Max)
	{}
	FPhysicalSurfaceName(EPhysicalSurface InType, const FName& InName)
		: Type(InType)
		, Name(InName)
	{}
};

UENUM()
namespace EFrictionCombineMode
{
	enum Type
	{
		//Uses the average value of the materials touching: (a+b) /2
		Average = 0,	
		//Uses the minimum value of the materials touching: min(a,b)
		Min = 1,		
		//Uses the product of the values of the materials touching: a*b
		Multiply = 2,	
		//Uses the maximum value of materials touching: max(a,b)
		Max = 3
	};
}

UENUM()
namespace ESettingsDOF
{
	enum Type
	{
		/*Allows for full 3D movement and rotation*/
		Full3D,
		/*Allows 2D movement along the Y-Z plane.*/
		YZPlane,
		/*Allows 2D movement along the X-Z plane.*/
		XZPlane,
		/*Allows 2D movement along the X-Y plane.*/
		XYPlane,
	};
}

UENUM()
namespace ESettingsLockedAxis
{
	enum Type
	{
		/*No axis is locked*/
		None,
		/*Lock movement along the x-axis*/
		X,
		/*Lock movement along the y-axis*/
		Y,
		/*Lock movement along the z-axis*/
		Z,
		/* Used for backwards compatibility. Indicates that we've updated into the new struct*/
		Invalid
	};
}


/**
 * Default physics settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Physics"))
class ENGINE_API UPhysicsSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	/** Default gravity. */
	UPROPERTY(config, EditAnywhere, Category = Constants)
	float DefaultGravityZ;

	/** Default terminal velocity for Physics Volumes. */
	UPROPERTY(config, EditAnywhere, Category = Constants)
	float DefaultTerminalVelocity;
	
	/** Default fluid friction for Physics Volumes. */
	UPROPERTY(config, EditAnywhere, Category = Constants)
	float DefaultFluidFriction;

	/** Threshold for ragdoll bodies above which they will be added to an aggregate before being added to the scene */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "1", UIMin = "1", ClampMax = "127", UIMax = "127"), Category = Constants)
	int32 RagdollAggregateThreshold;

	/** Triangles from triangle meshes (BSP) with an area less than or equal to this value will be removed from physics collision data. Set to less than 0 to disable. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "-1.0", UIMin = "-1.0", ClampMax = "10.0", UIMax = "10.0"), Category = Constants)
	float TriangleMeshTriangleMinAreaThreshold;

	/** Enables the use of an async scene */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category=Simulation)
	bool bEnableAsyncScene;

	/** Enables shape sharing between sync and async scene for static rigid actors */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Simulation, meta = (editcondition="bEnableAsyncScene"))
	bool bEnableShapeSharing;

	/** Enables persistent contact manifolds. This will generate fewer contact points, but with more accuracy. Reduces stability of stacking, but can help energy conservation.*/
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Simulation)
	bool bEnablePCM;

	/** Whether to warn when physics locks are used incorrectly. Turning this off is not recommended and should only be used by very advanced users. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = Simulation)
	bool bWarnMissingLocks;

	/** Can 2D physics be used (Box2D)? */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bEnable2DPhysics;

	UPROPERTY(config)
	TEnumAsByte<ESettingsLockedAxis::Type> LockedAxis_DEPRECATED;

	/** Useful for constraining all objects in the world, for example if you are making a 2D game using 3D environments.*/
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	TEnumAsByte<ESettingsDOF::Type> DefaultDegreesOfFreedom;
	
	/** Minimum relative velocity required for an object to bounce. A typical value for simulation stability is about 0.2 * gravity*/
	UPROPERTY(config, EditAnywhere, Category = Simulation, meta = (ClampMin = "0", UIMin = "0"))
	float BounceThresholdVelocity;

	/** Friction combine mode, controls how friction is computed for multiple materials. */
	UPROPERTY(config, EditAnywhere, Category=Simulation)
	TEnumAsByte<EFrictionCombineMode::Type> FrictionCombineMode;

	/** Restitution combine mode, controls how restitution is computed for multiple materials. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	TEnumAsByte<EFrictionCombineMode::Type> RestitutionCombineMode;

	/** Max velocity which may be used to depenetrate simulated physics objects. 0 means no maximum. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	float MaxDepenetrationVelocity;

	/**
	*  If true, simulate physics for this component on a dedicated server.
	*  This should be set if simulating physics and replicating with a dedicated server.
	*/
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bSimulateSkeletalMeshOnDedicatedServer;


	/** Max Physics Delta Time to be clamped. */
	UPROPERTY(config, EditAnywhere, meta=(ClampMin="0.0013", UIMin = "0.0013", ClampMax="1.0", UIMax="1.0"), Category=Framerate)
	float MaxPhysicsDeltaTime;

	/** Whether to substep the physics simulation. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	bool bSubstepping;

	/** Whether to substep the async physics simulation. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	bool bSubsteppingAsync;

	/** Max delta time for an individual substep simulation. */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "0.0013", UIMin = "0.0013", ClampMax = "1.0", UIMax = "1.0", editcondition = "bSubStepping"), Category=Framerate)
	float MaxSubstepDeltaTime;

	/** Max number of substeps for physics simulation. */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "1", UIMin = "1", ClampMax = "16", UIMax = "16", editcondition = "bSubstepping"), Category=Framerate)
	int32 MaxSubsteps;

	/** Physics delta time smoothing factor for sync scene. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float SyncSceneSmoothingFactor;

	/** Physics delta time smoothing factor for async scene. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float AsyncSceneSmoothingFactor;

	/** Physics delta time initial average. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0013", UIMin = "1.0", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float InitialAverageFrameRate;

	// PhysicalMaterial Surface Types
	UPROPERTY(config)
	TArray<FPhysicalSurfaceName> PhysicalSurfaces;

public:

	static UPhysicsSettings * Get() { return CastChecked<UPhysicsSettings>(UPhysicsSettings::StaticClass()->GetDefaultObject()); }

	virtual void PostInitProperties() override;

#if WITH_EDITOR

	virtual bool CanEditChange( const UProperty* Property ) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Load Material Type data from INI file **/
	/** this changes displayname meta data. That means we won't need it outside of editor*/
	void LoadSurfaceType();

#endif // WITH_EDITOR
};
