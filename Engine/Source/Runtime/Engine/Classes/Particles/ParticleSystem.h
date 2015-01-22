// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "ParticleSystem.generated.h"

/**
 *	ParticleSystemUpdateMode
 *	Enumeration indicating the method by which the system should be updated
 */
UENUM()
enum EParticleSystemUpdateMode
{
	/** RealTime	- update via the delta time passed in				*/
	EPSUM_RealTime UMETA(DisplayName="Real-Time"),
	/** FixedTime	- update via a fixed time step						*/
	EPSUM_FixedTime UMETA(DisplayName="Fixed-Time"),
	EPSUM_MAX,
};

/**
 *	ParticleSystemLODMethod
 */
UENUM()
enum ParticleSystemLODMethod
{
	PARTICLESYSTEMLODMETHOD_Automatic UMETA(DisplayName="Automatic"),
	PARTICLESYSTEMLODMETHOD_DirectSet UMETA(DisplayName="Direct Set"),
	PARTICLESYSTEMLODMETHOD_ActivateAutomatic UMETA(DisplayName="Activate Automatic"),
	PARTICLESYSTEMLODMETHOD_MAX,
};

/** Occlusion method enumeration */
UENUM()
enum EParticleSystemOcclusionBoundsMethod
{
	/** Don't determine occlusion on this particle system */
	EPSOBM_None UMETA(DisplayName="None"),
	/** Use the bounds of the particle system component when determining occlusion */
	EPSOBM_ParticleBounds UMETA(DisplayName="Particle Bounds"),
	/** Use the custom occlusion bounds when determining occlusion */
	EPSOBM_CustomBounds UMETA(DisplayName="Custom Bounds"),
	EPSOBM_MAX,
};

/** Structure containing per-LOD settings that pertain to the entire UParticleSystem. */
USTRUCT()
struct FParticleSystemLOD
{
	GENERATED_USTRUCT_BODY()

	FParticleSystemLOD()
	{
	}

	static FParticleSystemLOD CreateParticleSystemLOD()
	{
		FParticleSystemLOD NewLOD;
		return NewLOD;
	}
};

/**
 *	Temporary array for tracking 'solo' emitter mode.
 *	Entry will be true if emitter was enabled 
 */
USTRUCT()
struct FLODSoloTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(transient)
	TArray<uint8> SoloEnableSetting;

};

USTRUCT()
struct FNamedEmitterMaterial
{
	GENERATED_USTRUCT_BODY()

	FNamedEmitterMaterial()
	: Name(NAME_None)
	, Material(nullptr)
	{
	}

	UPROPERTY(EditAnywhere, Category = NamedMaterial)
		FName Name;

	UPROPERTY(EditAnywhere, Category = NamedMaterial)
	class UMaterialInterface* Material;
};

/**
 * A ParticleSystem is a complete particle effect that contains any number of ParticleEmitters. By allowing multiple emitters
 * in a system, the designer can create elaborate particle effects that are held in a single system. Once created using
 * Cascade, a ParticleSystem can then be inserted into a level or created in script.
 */
UCLASS(hidecategories=Object, MinimalAPI, BlueprintType)
class UParticleSystem : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=ParticleSystem, AssetRegistrySearchable)
	TEnumAsByte<enum EParticleSystemUpdateMode> SystemUpdateMode;

	/** UpdateTime_FPS	- the frame per second to update at in FixedTime mode		*/
	UPROPERTY(EditAnywhere, Category=ParticleSystem)
	float UpdateTime_FPS;

	/** UpdateTime_Delta	- internal												*/
	UPROPERTY()
	float UpdateTime_Delta;

	/** WarmupTime	- the time to warm-up the particle system when first rendered	*/
	UPROPERTY(EditAnywhere, Category=ParticleSystem)
	float WarmupTime;

	/**	WarmupTickRate - the time step for each tick during warm up.
		Increasing this improves performance. Decreasing, improves accuracy.
		Set to 0 to use the default tick time.										*/
	UPROPERTY(EditAnywhere, Category=ParticleSystem)
	float WarmupTickRate;

	/** Emitters	- internal - the array of emitters in the system				*/
	UPROPERTY(instanced)
	TArray<class UParticleEmitter*> Emitters;

	/** The component used to preview the particle system in Cascade				*/
	UPROPERTY(transient)
	class UParticleSystemComponent* PreviewComponent;

#if WITH_EDITORONLY_DATA
	/** The angle to use when rendering the thumbnail image							*/
	UPROPERTY()
	FRotator ThumbnailAngle;

	/** The distance to place the system when rendering the thumbnail image			*/
	UPROPERTY()
	float ThumbnailDistance;

	/** The time to warm-up the system for the thumbnail image						*/
	UPROPERTY(EditAnywhere, Category=Thumbnail)
	float ThumbnailWarmup;

#endif // WITH_EDITORONLY_DATA
	/** Used for curve editor to remember curve-editing setup.						*/
	UPROPERTY(export)
	class UInterpCurveEdSetup* CurveEdSetup;

	/** If true, the system's Z axis will be oriented toward the camera				*/
	UPROPERTY(EditAnywhere, Category=ParticleSystem)
	uint32 bOrientZAxisTowardCamera:1;

	//
	//	LOD
	//
	/**
	 *	How often (in seconds) the system should perform the LOD distance check.
	 */
	UPROPERTY(EditAnywhere, Category=LOD)
	float LODDistanceCheckTime;

	/**
	 *	The method of LOD level determination to utilize for this particle system
	 *	  PARTICLESYSTEMLODMETHOD_Automatic - Automatically set the LOD level, checking every LODDistanceCheckTime seconds.
	 *    PARTICLESYSTEMLODMETHOD_DirectSet - LOD level is directly set by the game code.
	 *    PARTICLESYSTEMLODMETHOD_ActivateAutomatic - LOD level is determined at Activation time, then left alone unless directly set by game code.
	 */
	UPROPERTY(EditAnywhere, Category=LOD)
	TEnumAsByte<enum ParticleSystemLODMethod> LODMethod;

	/**
	 *	The array of distances for each LOD level in the system.
	 *	Used when LODMethod is set to PARTICLESYSTEMLODMETHOD_Automatic.
	 *
	 *	Example: System with 3 LOD levels
	 *		LODDistances(0) = 0.0
	 *		LODDistances(1) = 2500.0
	 *		LODDistances(2) = 5000.0
	 *
	 *		In this case, when the system is [   0.0 ..   2499.9] from the camera, LOD level 0 will be used.
	 *										 [2500.0 ..   4999.9] from the camera, LOD level 1 will be used.
	 *										 [5000.0 .. INFINITY] from the camera, LOD level 2 will be used.
	 *
	 */
	UPROPERTY(EditAnywhere, editfixedsize, Category=LOD)
	TArray<float> LODDistances;

#if WITH_EDITORONLY_DATA
	/** LOD setting for intepolation (set by Cascade) Range [0..100]				*/
	UPROPERTY()
	int32 EditorLODSetting;

#endif // WITH_EDITORONLY_DATA
	/**
	 *	Internal value that tracks the regenerate LOD levels preference.
	 *	If true, when autoregenerating LOD levels in code, the low level will
	 *	be a duplicate of the high.
	 */
	UPROPERTY()
	uint32 bRegenerateLODDuplicate:1;

	UPROPERTY(EditAnywhere, Category=LOD)
	TArray<struct FParticleSystemLOD> LODSettings;

	/** Whether to use the fixed relative bounding box or calculate it every frame. */
	UPROPERTY(EditAnywhere, Category=Bounds)
	uint32 bUseFixedRelativeBoundingBox:1;

	/**	Fixed relative bounding box for particle system.							*/
	UPROPERTY(EditAnywhere, Category=Bounds)
	FBox FixedRelativeBoundingBox;

	/**
	 * Number of seconds of emitter not being rendered that need to pass before it
	 * no longer gets ticked/ becomes inactive.
	 */
	UPROPERTY(EditAnywhere, Category=ParticleSystem)
	float SecondsBeforeInactive;

#if WITH_EDITORONLY_DATA
	//
	//	Cascade 'floor' mesh information.
	//
	UPROPERTY()
	FString FloorMesh;

	UPROPERTY()
	FVector FloorPosition;

	UPROPERTY()
	FRotator FloorRotation;

	UPROPERTY()
	float FloorScale;

	UPROPERTY()
	FVector FloorScale3D;

	/** The background color to display in Cascade */
	UPROPERTY()
	FColor BackgroundColor;

#endif // WITH_EDITORONLY_DATA
	/** EDITOR ONLY: Indicates that Cascade would like to have the PeakActiveParticles count reset */
	UPROPERTY()
	uint32 bShouldResetPeakCounts:1;

	/** Set during load time to indicate that physics is used... */
	UPROPERTY(transient)
	uint32 bHasPhysics:1;

	/** Inidicates the old 'real-time' thumbnail rendering should be used	*/
	UPROPERTY(EditAnywhere, Category=Thumbnail)
	uint32 bUseRealtimeThumbnail:1;

	/** Internal: Indicates the PSys thumbnail image is out of date			*/
	UPROPERTY()
	uint32 ThumbnailImageOutOfDate:1;

private:
	/** if true, this psys can tick in any thread **/
	uint32 bIsElligibleForAsyncTick:1;
	/** if true, bIsElligibleForAsyncTick is set up **/
	uint32 bIsElligibleForAsyncTickComputed:1;
public:

#if WITH_EDITORONLY_DATA
	/** Internal: The PSys thumbnail image									*/
	UPROPERTY()
	class UTexture2D* ThumbnailImage;

#endif // WITH_EDITORONLY_DATA
	/** How long this Particle system should delay when ActivateSystem is called on it. */
	UPROPERTY(EditAnywhere, Category=Delay, AssetRegistrySearchable)
	float Delay;

	/** The low end of the emitter delay if using a range. */
	UPROPERTY(EditAnywhere, Category=Delay)
	float DelayLow;

	/**
	 *	If true, select the emitter delay from the range 
	 *		[DelayLow..Delay]
	 */
	UPROPERTY(EditAnywhere, Category=Delay)
	uint32 bUseDelayRange:1;

	/** Local space position that UVs generated with the ParticleMacroUV material node will be centered on. */
	UPROPERTY(EditAnywhere, Category=MacroUV)
	FVector MacroUVPosition;

	/** World space radius that UVs generated with the ParticleMacroUV material node will tile based on. */
	UPROPERTY(EditAnywhere, Category=MacroUV)
	float MacroUVRadius;

	/** 
	 *	Which occlusion bounds method to use for this particle system.
	 *	EPSOBM_None - Don't determine occlusion for this system.
	 *	EPSOBM_ParticleBounds - Use the bounds of the component when determining occlusion.
	 */
	UPROPERTY(EditAnywhere, Category=Occlusion)
	TEnumAsByte<enum EParticleSystemOcclusionBoundsMethod> OcclusionBoundsMethod;

	/** The occlusion bounds to use if OcclusionBoundsMethod is set to EPSOBM_CustomBounds */
	UPROPERTY(EditAnywhere, Category=Occlusion)
	FBox CustomOcclusionBounds;

	UPROPERTY(transient)
	TArray<struct FLODSoloTrack> SoloTracking;

	/** 
	*	Array of named material slots for use by emitters of this system. 
	*	Emitters can use these instead of their own materials by providing the name to the NamedMaterialOverrides property of their required module.
	*	These materials can be overriden using CreateNamedDynamicMaterialInstance() on a ParticleSystemComponent.
	*/
	UPROPERTY(EditAnywhere, Category = Materials)
	TArray<FNamedEmitterMaterial> NamedMaterialSlots;

	// Begin UObject interface.
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	virtual void PreSave() override;
	virtual void PostLoad() override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	// End UObject interface.


	// @todo document
	void UpdateColorModuleClampAlpha(class UParticleModuleColorBase* ColorModule);

	/**
	 *	CalculateMaxActiveParticleCounts
	 *	Determine the maximum active particles that could occur with each emitter.
	 *	This is to avoid reallocation during the life of the emitter.
	 *
	 *	@return	true	if the numbers were determined for each emitter
	 *			false	if not be determined
	 */
	virtual bool		CalculateMaxActiveParticleCounts();
	
	/**
	 *	Retrieve the parameters associated with this particle system.
	 *
	 *	@param	ParticleSysParamList	The list of FParticleSysParams used in the system
	 *	@param	ParticleParameterList	The list of ParticleParameter distributions used in the system
	 */
	ENGINE_API void GetParametersUtilized(TArray<TArray<FString> >& ParticleSysParamList,
							   TArray<TArray<FString> >& ParticleParameterList);

	/**
	 *	Setup the soloing information... Obliterates all current soloing.
	 */
	ENGINE_API void SetupSoloing();

	/**
	 *	Toggle the bIsSoloing flag on the given emitter.
	 *
	 *	@param	InEmitter		The emitter to toggle.
	 *
	 *	@return	bool			true if ANY emitters are set to soloing, false if none are.
	 */
	ENGINE_API bool ToggleSoloing(class UParticleEmitter* InEmitter);

	/**
	 *	Turn soloing off completely - on every emitter
	 *
	 *	@return	bool			true if successful, false if not.
	 */
	ENGINE_API bool TurnOffSoloing();

	/**
	 *	Editor helper function for setting the LOD validity flags used in Cascade.
	 */
	ENGINE_API void SetupLODValidity();

#if WITH_EDITOR
	/**
	 *	Remove all duplicate modules.
	 *
	 *	@param	bInMarkForCooker	If true, mark removed objects to not cook out.
	 *	@param	OutRemovedModules	Optional map to fill in w/ removed modules...
	 *
	 *	@return	bool				true if successful, false if not
	 */
	ENGINE_API bool RemoveAllDuplicateModules(bool bInMarkForCooker, TMap<UObject*,bool>* OutRemovedModules);

	/**
	 *	Update all emitter module lists
	 */
	ENGINE_API void UpdateAllModuleLists();
#endif
	/** Return the currently set LOD method											*/
	virtual enum ParticleSystemLODMethod GetCurrentLODMethod();

	/**
	 *	Return the number of LOD levels for this particle system
	 *
	 *	@return	The number of LOD levels in the particle system
	 */
	virtual int32 GetLODLevelCount();
	
	/**
	 *	Return the distance for the given LOD level
	 *
	 *	@param	LODLevelIndex	The LOD level that the distance is being retrieved for
	 *
	 *	@return	-1.0f			If the index is invalid
	 *			Distance		The distance set for the LOD level
	 */
	virtual float GetLODDistance(int32 LODLevelIndex);
	
	/**
	 *	Set the LOD method
	 *
	 *	@param	InMethod		The desired method
	 */
	virtual void SetCurrentLODMethod(ParticleSystemLODMethod InMethod);

	/**
	 *	Set the distance for the given LOD index
	 *
	 *	@param	LODLevelIndex	The LOD level to set the distance ofr
	 *	@param	InDistance		The distance to set
	 *
	 *	@return	true			If successful
	 *			false			Invalid LODLevelIndex
	 */
	virtual bool SetLODDistance(int32 LODLevelIndex, float InDistance);

	/**
	 * Builds all emitters in the particle system.
	 */
	ENGINE_API void BuildEmitters();

	/** return true if this psys can tick in any thread */
	FORCEINLINE bool CanTickInAnyThread()
	{
		if (!bIsElligibleForAsyncTickComputed)
		{
			ComputeCanTickInAnyThread();
		}
		return bIsElligibleForAsyncTick;
	}
	/** Decide if this psys can tick in any thread, and set bIsElligibleForAsyncTick */
	ENGINE_API void ComputeCanTickInAnyThread();

	/** Returns true if this system contains any GPU emitters. */
	bool HasGPUEmitter() const;

	/** 
	Returns true if this system contains an emitter of the pasesd type. 
	@ param TypeData - The emitter type to check for. Must be a child class of UParticleModuleTypeDataBase
	*/
	UFUNCTION(BlueprintCallable, Category = "Particle System")
	bool ContainsEmitterType(UClass* TypeData);

};



