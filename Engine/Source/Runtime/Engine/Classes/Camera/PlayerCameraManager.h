// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "CameraTypes.h"
#include "CameraShake.h"
#include "PlayerCameraManager.generated.h"

/** 
 * Options that define how to blend when changing view targets. 
 * @see FViewTargetTransitionParams, SetViewTarget 
 */
UENUM()
enum EViewTargetBlendFunction
{
	/** Camera does a simple linear interpolation. */
	VTBlend_Linear,
	/** Camera has a slight ease in and ease out, but amount of ease cannot be tweaked. */
	VTBlend_Cubic,
	/** Camera immediately accelerates, but smoothly decelerates into the target.  Ease amount controlled by BlendExp. */
	VTBlend_EaseIn,
	/** Camera smoothly accelerates, but does not decelerate into the target.  Ease amount controlled by BlendExp. */
	VTBlend_EaseOut,
	/** Camera smoothly accelerates and decelerates.  Ease amount controlled by BlendExp. */
	VTBlend_EaseInOut,
	VTBlend_MAX,
};

/** 
 * Cached camera POV info, stored as optimization so we only
 * need to do a full camera update once per tick.
 */
USTRUCT()
struct FCameraCacheEntry
{
	GENERATED_USTRUCT_BODY()
public:

	/** World time this entry was created. */
	UPROPERTY()
	float TimeStamp;

	/** Camera POV to cache. */
	UPROPERTY()
	FMinimalViewInfo POV;

	FCameraCacheEntry()
		: TimeStamp(0.f)
	{}
};

/** A ViewTarget is the primary actor the camera is associated with. */
USTRUCT()
struct ENGINE_API FTViewTarget
{
	GENERATED_USTRUCT_BODY()
public:

	/** Target Actor used to compute POV */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TViewTarget)
	class AActor* Target;

	/** Computed point of view */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TViewTarget)
	struct FMinimalViewInfo POV;

protected:
	/** PlayerState (used to follow same player through pawn transitions, etc., when spectating) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TViewTarget)
	class APlayerState* PlayerState;

	//@TODO: All for GetNextViewablePlayer
	friend class APlayerController;

public:
	void SetNewTarget(AActor* NewTarget);

	class APawn* GetTargetPawn() const;

	bool Equal(const FTViewTarget& OtherTarget) const;

	FTViewTarget()
		: Target(nullptr)
		, PlayerState(nullptr)
	{}

	/** Make sure ViewTarget is valid */
	void CheckViewTarget(APlayerController* OwningController);
};

/** A set of parameters to describe how to transition between view targets. */
USTRUCT()
struct FViewTargetTransitionParams
{
	GENERATED_USTRUCT_BODY()
public:

	/** Total duration of blend to pending view target. 0 means no blending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ViewTargetTransitionParams)
	float BlendTime;

	/** Function to apply to the blend parameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ViewTargetTransitionParams)
	TEnumAsByte<enum EViewTargetBlendFunction> BlendFunction;

	/** Exponent, used by certain blend functions to control the shape of the curve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ViewTargetTransitionParams)
	float BlendExp;

	/** 
	 * If true, lock outgoing viewtarget to last frame's camera POV for the remainder of the blend.
	 * This is useful if you plan to teleport the old viewtarget, but don't want to affect the blend. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=ViewTargetTransitionParams)
	uint32 bLockOutgoing:1;

	FViewTargetTransitionParams()
		: BlendTime(0.f)
		, BlendFunction(VTBlend_Cubic)
		, BlendExp(2.f)
		, bLockOutgoing(false)
	{}

	/** For a given linear blend value (blend percentage), return the final blend alpha with the requested function applied */
	float GetBlendAlpha(const float& TimePct) const
	{
		switch (BlendFunction)
		{
		case VTBlend_Linear: return FMath::Lerp(0.f, 1.f, TimePct); 
		case VTBlend_Cubic:	return FMath::CubicInterp(0.f, 0.f, 1.f, 0.f, TimePct); 
		case VTBlend_EaseInOut: return FMath::InterpEaseInOut(0.f, 1.f, TimePct, BlendExp); 
		case VTBlend_EaseIn: return FMath::Lerp(0.f, 1.f, FMath::Pow(TimePct, BlendExp)); 
		case VTBlend_EaseOut: return FMath::Lerp(0.f, 1.f, FMath::Pow(TimePct, (FMath::IsNearlyZero(BlendExp) ? 1.f : (1.f / BlendExp))));
		default:
			break;
		}

		return 1.f;
	}
};


//=============================================================================
/**
 * A PlayerCameraManager is responsible for managing the camera for a particular
 * player. It defines the final view properties used by other systems (e.g. the renderer),
 * meaning you can think of it as your virtual eyeball in the world. It can compute the 
 * final camera properties directly, or it can arbitrate/blend between other objects or 
 * actors that influence the camera (e.g. blending from one CameraActor to another).
 * 
 * The PlayerCameraManagers primary external responsibility is to reliably respond to
 * various Get*() functions, such as GetCameraViewPoint. Most everything else is
 * implementation detail and overrideable by user projects.
 * 
 * By default, a PlayerCameraManager maintains a "view target", which is the primary actor
 * the camera is associated with. It can also apply various "post" effects to the final 
 * view state, such as camera animations, shakes, post-process effects or special 
 * effects such as dirt on the lens.
 *
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Framework/Camera/
 */
UCLASS(notplaceable, transient, BlueprintType, Blueprintable)
class ENGINE_API APlayerCameraManager : public AActor
{
	GENERATED_UCLASS_BODY()

	/** PlayerController that owns this Camera actor */
	UPROPERTY()
	class APlayerController* PCOwner;

private_subobject:
	/** Dummy component we can use to attach things to the camera. */
	DEPRECATED_FORGAME(4.6, "TransformComponent should not be accessed directly, please use GetTransformComponent() function instead. TransformComponent will soon be private and your code will not compile.")
	UPROPERTY(Category = PlayerCameraManager, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class USceneComponent* TransformComponent;

public:
	/** Usable to define different camera behaviors. A few simple styles are implemented by default. */
	UPROPERTY()
	FName CameraStyle;

	/** FOV to use by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float DefaultFOV;

	/** Value to lock FOV at, if bLockedFOV == true. Ignored otherwise. */
	UPROPERTY()
	float LockedFOV;

	/** The default desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float DefaultOrthoWidth;

	/** Value OrthoWidth is locked at, if bLockedOrthoWidth == true. Ignored otherwise. */
	UPROPERTY()
	float LockedOrthoWidth;

	/** True when this camera should use an orthographic perspective instead of FOV */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	bool bIsOrthographic;

	/** Default aspect ratio */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float DefaultAspectRatio;

	/** Color to fade to (when bEnableFading == true). */
	UPROPERTY()
	FColor FadeColor;

	/** Amount of fading to apply (when bEnableFading == true). */
	UPROPERTY()
	float FadeAmount;

	/** Allows control over scaling individual color channels in the final image (when bEnableColorScaling == true). */
	UPROPERTY()
	FVector ColorScale;

	/** Desired color scale which ColorScale will interpolate to (when bEnableColorScaling and bEnableColorScaleInterp == true) */
	UPROPERTY()
	FVector DesiredColorScale;

	/** Color scale value at start of interpolation (when bEnableColorScaling and bEnableColorScaleInterp == true) */
	UPROPERTY()
	FVector OriginalColorScale;

	/** Total time for color scale interpolation to complete (when bEnableColorScaling and bEnableColorScaleInterp == true) */
	UPROPERTY()
	float ColorScaleInterpDuration;

	/** Time at which interpolation started (when bEnableColorScaling and bEnableColorScaleInterp == true) */
	UPROPERTY()
	float ColorScaleInterpStartTime;

	/** Cached camera properties. */
	UPROPERTY()
	struct FCameraCacheEntry CameraCache;

	/** Cached camera properties. */
	UPROPERTY()
	struct FCameraCacheEntry LastFrameCameraCache;

	/** Current ViewTarget */
	UPROPERTY()
	struct FTViewTarget ViewTarget;

	/** Pending view target for blending */
	UPROPERTY()
	struct FTViewTarget PendingViewTarget;

	/** Time remaining in viewtarget blend. */
	UPROPERTY()
	float BlendTimeToGo;

	/** Current view target transition blend parameters. */
	UPROPERTY()
	struct FViewTargetTransitionParams BlendParams;

	/** List of active camera modifiers that have a chance to update the final camera POV */
	UPROPERTY()
	TArray<class UCameraModifier*> ModifierList;

	/** Distance to place free camera from view target (used in certain CameraStyles) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Debug)
	float FreeCamDistance;

	/** Offset to Z free camera position (used in certain CameraStyles) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Debug)
	FVector FreeCamOffset;

	/** Current camera fade alpha value (when bEnableFading == true) */
	UPROPERTY()
	FVector2D FadeAlpha;

	/** Total duration of the camera fade (when bEnableFading == true) */
	UPROPERTY()
	float FadeTime;

	/** Time remaining in camera fade (when bEnableFading == true) */
	UPROPERTY()
	float FadeTimeRemaining;

protected:
	// "Lens" effects (e.g. blood, dirt on camera)
	/** CameraBlood emitter attached to this camera */
	UPROPERTY(transient)
	TArray<class AEmitterCameraLensEffectBase*> CameraLensEffects;

public:
	/////////////////////
	// Camera Modifiers
	/////////////////////
	
	/** Camera modifier for cone-driven screen shakes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, transient, Category = PlayerCameraManager)
	class UCameraModifier_CameraShake* CameraShakeCamMod;

protected:
	/** Class to use when instantiating screenshake modifier object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	TSubclassOf<class UCameraModifier_CameraShake> CameraShakeCamModClass;

	/** By default camera modifiers are not applied to stock debug cameras (e.g. CameraStyle == "FreeCam"). Setting to true will always apply modifiers. */
	UPROPERTY()
	bool bAlwaysApplyModifiers;
	
	////////////////////////
	// CameraAnim support
	////////////////////////
	
	static const int32 MAX_ACTIVE_CAMERA_ANIMS = 8;

	/** Internal pool of camera anim instance objects available for playing camera animations. Defines the max number of camera anims that can play simultaneously. */
	UPROPERTY()
	class UCameraAnimInst* AnimInstPool[8];    /*MAX_ACTIVE_CAMERA_ANIMS @fixme constant */

	/** Internal list of active post process effects. Parallel array to PostProcessBlendCacheWeights. */
	TArray<struct FPostProcessSettings> PostProcessBlendCache;
	/** Internal list of weights for active post process effects. Parallel array to PostProcessBlendCache. */
	TArray<float> PostProcessBlendCacheWeights;

	/** Adds a postprocess effect at the given weight. */
	void AddCachedPPBlend(struct FPostProcessSettings& PPSettings, float BlendWeight);
	/** Removes all postprocess effects. */
	void ClearCachedPPBlends();

public:
	/** Array of camera anim instances that are currently playing and in-use */
	UPROPERTY()
	TArray<class UCameraAnimInst*> ActiveAnims;

	/** Returns active post process info. */
	void GetCachedPostProcessBlends(TArray<struct FPostProcessSettings> const*& OutPPSettings, TArray<float> const*& OutBlendWeigthts) const;
	
protected:
	/** Array of camera anim instances that are not playing and available to be used. */
	UPROPERTY()
	TArray<class UCameraAnimInst*> FreeAnims;

	/** Internal. Receives the output of individual camera animations. */
	UPROPERTY(transient)
	class ACameraActor* AnimCameraActor;

	/** Tuue if FOV is locked to a constant value (@see LockedFOV) */
	UPROPERTY()
	uint32 bLockedFOV:1;

	/** true if OrthoWidth is locked to a constant value (@see LockedOrthoWidth) */
	UPROPERTY()
	uint32 bLockedOrthoWidth:1;

public:
	/** True if we should apply FadeColor/FadeAmount to the screen. */
	UPROPERTY()
	uint32 bEnableFading:1;

	/** True to apply fading of audio alongside the video. */
	UPROPERTY()
	uint32 bFadeAudio:1;

	/** True to turn on scaling of color channels in final image using ColorScale property. */
	UPROPERTY()
	uint32 bEnableColorScaling:1;

	/** True to smoothly interpolate color scale values when they change. */
	UPROPERTY()
	uint32 bEnableColorScaleInterp:1;

	/** True if clients are handling setting their own viewtarget and the server should not replicate it (e.g. during certain Matinee sequences) */
	UPROPERTY()
	uint32 bClientSimulatingViewTarget:1;

	/** True if server will use camera positions replicated from the client instead of calculating them locally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=PlayerCameraManager)
	uint32 bUseClientSideCameraUpdates:1;

	/** For debugging. If true, replicate the client side camera position but don't use it, and draw the positions on the server */
	UPROPERTY()
	uint32 bDebugClientSideCamera:1;

	/** If true, send a camera update to the server on next update. */
	UPROPERTY(transient)
	uint32 bShouldSendClientSideCameraUpdate:1;

	/** 
	 * True if we did a camera cut this frame. Automatically reset to false every frame.
	 * This flag affects various things in the renderer (such as whether to use the occlusion queries from last frame, and motion blur).
	 */
	UPROPERTY(transient)
	uint32 bGameCameraCutThisFrame:1;

	/** True if camera's orientation should be updated by most recent HMD orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PlayerCameraManager)
	uint32 bFollowHmdOrientation:1;

	/** Minimum view pitch, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float ViewPitchMin;

	/** Maximum view pitch, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float ViewPitchMax;

	/** Minimum view yaw, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float ViewYawMin;

	/** Maximum view yaw, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float ViewYawMax;

	/** Minimum view roll, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float ViewRollMin;

	/** Maximum view roll, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PlayerCameraManager)
	float ViewRollMax;
	
	/** 
	 * Blueprint hook to allow blueprints to override existing camera behavior or implement custom cameras.
	 * If this function returns true, we will use the given returned values and skip further calculations to determine
	 * final camera POV. 
	 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic)
	virtual bool BlueprintUpdateCamera(AActor* CameraTarget, FVector& NewCameraLocation, FRotator& NewCameraRotation, float& NewCameraFOV);

	/** Returns the PlayerController that owns this camera. */
	UFUNCTION(BlueprintCallable, Category="Game|Player")
	virtual APlayerController* GetOwningPlayerController() const;

	virtual void AssignViewTarget(AActor* NewTarget, FTViewTarget& VT, struct FViewTargetTransitionParams TransitionParams=FViewTargetTransitionParams());

	friend struct FTViewTarget;
public:
	/** @return the current ViewTarget. */
	AActor* GetViewTarget() const;

	/** @return the ViewTarget if it is an APawn, or nullptr otherwise */
	class APawn* GetViewTargetPawn() const;

	// Begin AActor Interface
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void PostInitializeComponents() override;
	virtual void Destroyed() override;
	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	// End AActor Interface

	/** Static.  Plays an in-world camera shake that affects all nearby players, with radial distance-based attenuation.
	 * @param InWorld - World context.
	 * @param Shake - Camera shake asset to use.
	 * @param Epicenter - Location to place the effect in world space
	 * @param InnerRadius - Cameras inside this radius get the full intensity shake.
	 * @param OuterRadius - Cameras outside this radius are not affected.
	 * @param Falloff - Exponent that describes the shake intensity falloff curve between InnerRadius and OuterRadius. 1.0 is linear.
	 * @param bOrientShakeTowardsEpicenter - Changes the rotation of shake to point towards epicenter instead of forward. Useful for things like directional hits.
	 */
	static void PlayWorldCameraShake(UWorld* InWorld, TSubclassOf<UCameraShake> Shake, FVector Epicenter, float InnerRadius, float OuterRadius, float Falloff, bool bOrientShakeTowardsEpicenter = false);

protected:
	/** 
	 * Internal. Calculates shake scale for a particular camera.
	 * @return Returns the intensity scalar in the range [0..1] for a shake originating at Epicenter. 
	 */
	static float CalcRadialShakeScale(class APlayerCameraManager* Cam, FVector Epicenter, float InnerRadius, float OuterRadius, float Falloff);

public:

	/**
	 * Performs per-tick camera update. Called once per tick after all other actors have been ticked.
	 * Non-local players replicate the POV if bUseClientSideCameraUpdates is true.
	 */
	virtual void UpdateCamera(float DeltaTime);

	/** 
	 * Internal. Creates and initializes a new camera modifier of the specified class. 
	 * @param ModifierClass - The class of camera modifier to create.
	 * @return Returns the newly created camera modifier.
	 */
	virtual class UCameraModifier* CreateCameraModifier(TSubclassOf<class UCameraModifier> ModifierClass);
	
	/**
	 * Applies the current set of camera modifiers to the given camera POV.
	 * @param	DeltaTime	Time in seconds since last update
	 * @param	InOutPOV	Point of View
	 */
	virtual void ApplyCameraModifiers(float DeltaTime, FMinimalViewInfo& InOutPOV);
	
	/**
	 * Initialize this PlayerCameraManager for the given associated PlayerController.
	 * @param PC	PlayerController associated with this Camera.
	 */
	virtual void InitializeFor(class APlayerController* PC);
	
	/** @return Returns the camera's current full FOV angle, in degrees. */
	virtual float GetFOVAngle() const;
	
	/** 
	 * Locks the FOV to the given value.  Unlock with UnlockFOV.
	 * @param NewFOV - New full FOV angle to use, in degrees.
	 */
	virtual void SetFOV(float NewFOV);

	/** Unlocks the FOV. */
	virtual void UnlockFOV();

	/** @return Returns true if this camera is using an orthographic perspective. */
	virtual bool IsOrthographic() const;

	/** @return Returns the current orthographic width for the camera. */
	virtual float GetOrthoWidth() const;

	/** 
	 * Sets and locks the current orthographic width for the camera. Unlock with UnlockOrthoWidth. 
	 * Only used if IsOrthographic returns true.
	 * @param OrthoWidth - New orthographic width.
	 */
	virtual void SetOrthoWidth(float OrthoWidth);

	/** Unlocks OrthoWidth value */
	virtual void UnlockOrthoWidth();

	/**
	 * Master function to retrieve Camera's actual view point.
	 * Consider calling PlayerController::GetPlayerViewPoint() instead.
	 *
	 * @param	OutCamLoc	Returned camera location
	 * @param	OutCamRot	Returned camera rotation
	 */
	void GetCameraViewPoint(FVector& OutCamLoc, FRotator& OutCamRot) const;
	
	/** @return Returns camera's current rotation. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	FRotator GetCameraRotation() const;

	/** @return Returns camera's current location. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	FVector GetCameraLocation() const;
	
	/** 
	 * Sets the new desired color scale, enables color scaling, and enables color scale interpolation. 
	 * @param NewColorScale - new color scale to use
	 * @param InterpTime - duration of the interpolation from old to new, in seconds.
	 */
	virtual void SetDesiredColorScale(FVector NewColorScale, float InterpTime);
	
protected:
	/** Internal function conditionally called from UpdateCamera to do the actual work of updating the camera. */
	virtual void DoUpdateCamera(float DeltaTime);
	
	/** Internal. Applies appropriate audio fading to the audio system. */
	virtual void ApplyAudioFade();
	
	/**
	 * Internal helper to blend two viewtargets.
	 *
	 * @param	A		Source view target
	 * @param	B		destination view target
	 * @param	Alpha	% of blend from A to B.
	 */
	FPOV BlendViewTargets(const FTViewTarget& A, const FTViewTarget& B, float Alpha);
	
public:
	/** Caches given final POV info for efficient access from other game code. */
	void FillCameraCache(const FMinimalViewInfo& NewInfo);
	
protected:
	/**
	 * Calculates an updated POV for the given viewtarget.
	 * @param	OutVT		ViewTarget to update.
	 * @param	DeltaTime	Delta Time since last camera update (in seconds).
	 */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime);

	/** Update any attached camera lens effects **/
	virtual void UpdateCameraLensEffects( const FTViewTarget& OutVT );

public:
	/** 
	 * Sets a new ViewTarget.
	 * @param NewViewTarget - New viewtarget actor.
	 * @param TransitionParams - Optional parameters to define the interpolation from the old viewtarget to the new. Transition will be instant by default.
	 */
	void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());
	
	/** 
	 * Called to give PlayerCameraManager a chance to adjust view rotation updates before they are applied. 
	 * e.g. The base implementation enforces view rotation limits using LimitViewPitch, et al. 
	 * @param DeltaTime - Frame time in seconds.
	 * @param OutViewRotation - In/out. The view rotation to modify.
	 * @param OutDeltaRot - In/out. How much the rotation changed this frame.
	 */
	virtual void ProcessViewRotation(float DeltaTime, FRotator& OutViewRotation, FRotator& OutDeltaRot);

	//
	// Camera Lens Effects
	//
	
	/** @return Returns first instance of a lens effect of the given class. */
	virtual class AEmitterCameraLensEffectBase* FindCameraLensEffect(TSubclassOf<class AEmitterCameraLensEffectBase> LensEffectEmitterClass);
	
	/** 
	 * Creates a camera lens effect of the given class on this camera. 
	 * @param LensEffectEmitterClass - Class of lens effect emitter to create.
	 * @return Returns the new emitter actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	virtual AEmitterCameraLensEffectBase* AddCameraLensEffect(TSubclassOf<class AEmitterCameraLensEffectBase> LensEffectEmitterClass);
	
	/** 
	 * Removes the given lens effect from the camera. 
	 * @param Emitter - the emitter actor to remove from the camera
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	virtual void RemoveCameraLensEffect(class AEmitterCameraLensEffectBase* Emitter);
	
	/** Removes all camera lens effects. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	virtual void ClearCameraLensEffects();

	//
	// Camera Shakes.
	//

	/** 
	 * Plays a camera shake on this camera.
	 * @param Shake - The class of camera shake to play.
	 * @param Scale - Scalar defining how "intense" to play the anim. 1.0 is normal or as authored.
	 * @param PlaySpace - Which coordinate system to play the shake in.
	 * @param UserPlaySpaceRot - Coordinate system to play shake when PlaySpace == CAPS_UserDefined.
	 */
	virtual void PlayCameraShake(TSubclassOf<class UCameraShake> Shake, float Scale, enum ECameraAnimPlaySpace::Type PlaySpace=ECameraAnimPlaySpace::CameraLocal, FRotator UserPlaySpaceRot = FRotator::ZeroRotator);
	
	/** Stops playing CameraShake of the given class. */
	virtual void StopCameraShake(TSubclassOf<class UCameraShake> Shake);

	/** Stops playing all camera shakes. */
	virtual void ClearAllCameraShakes();

	//
	//  CameraAnim support.
	//
	
	/**
	 * Play the indicated CameraAnim on this camera.
	 * 
	 * @param Anim The animation that should play on this instance.
	 * @param Rate				How fast to play the animation. 1.0 is normal.
	 * @param Scale				How "intense" to play the animation. 1.0 is normal.
	 * @param BlendInTime		Time to linearly ramp in.
	 * @param BlendOutTime		Time to linearly ramp out.
	 * @param bLoop				True to loop the animation if it hits the end.
	 * @param bRandomStartTime	Whether or not to choose a random time to start playing. Useful with bLoop=true and a duration to randomize things like shakes.
	 * @param Duration			Optional total playtime for this animation, including blends. 0 means to use animations natural duration, or infinite if looping.
	 * @param PlaySpace			Which space to play the animation in.
	 * @param UserPlaySpaceRot  Custom play space, used when PlaySpace is UserDefined.
	 * @return The CameraAnim instance, which can be stored to manipulate/stop the anim after the fact.
	 */
	UFUNCTION(BlueprintCallable, Category="Camera Animation")
	virtual class UCameraAnimInst* PlayCameraAnim(class UCameraAnim* Anim, float Rate=1.f, float Scale=1.f, float BlendInTime=0.f, float BlendOutTime=0.f, bool bLoop=false, bool bRandomStartTime=false, float Duration=0.f, ECameraAnimPlaySpace::Type PlaySpace=ECameraAnimPlaySpace::CameraLocal, FRotator UserPlaySpaceRot=FRotator::ZeroRotator);
	
	/**
	 * Stop playing all instances of the indicated CameraAnim.
	 * @param bImmediate	True to stop it right now and ignore blend out, false to let it blend out as indicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Animation")
	virtual void StopAllInstancesOfCameraAnim(class UCameraAnim* Anim, bool bImmediate = false);
	
	/** 
	 * Stops the given CameraAnimInst from playing.  The given pointer should be considered invalid after this. 
	 * @param bImmediate	True to stop it right now and ignore blend out, false to let it blend out as indicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Animation")
	virtual void StopCameraAnimInst(class UCameraAnimInst* AnimInst, bool bImmediate = false);
	
	/**
	 * Stop playing all CameraAnims on this CameraManager.
	 * @param bImmediate	True to stop it right now and ignore blend out, false to let it blend out as indicated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Camera Animation")
	virtual void StopAllCameraAnims(bool bImmediate = false);

	/** @return Returns first existing instance of the specified camera anim, or NULL if none exists. */
	class UCameraAnimInst* FindInstanceOfCameraAnim(class UCameraAnim const* Anim) const;

protected:
	/** Gets specified temporary CameraActor ready to update the specified Anim. */
	void InitTempCameraActor(class ACameraActor* CamActor, class UCameraAnimInst const* AnimInstToInitFor) const;
	void ApplyAnimToCamera(class ACameraActor const* AnimatedCamActor, class UCameraAnimInst const* AnimInst, FMinimalViewInfo& InOutPOV);

	/** @return Returns an available CameraAnimInst, or NULL if no more are available. */
	class UCameraAnimInst* AllocCameraAnimInst();

	/** 
	 * Frees an allocated CameraAnimInst for future use. 
	 * @param Inst - Instance to free. 
	 */
	void ReleaseCameraAnimInst(class UCameraAnimInst* Inst);

protected:
	/** 
	 * Limit the player's view pitch. 
	 * @param ViewRotation - ViewRotation to modify. Both input and output.
	 * @param InViewPitchMin - Minimum view pitch, in degrees.
	 * @param InViewPitchMax - Maximum view pitch, in degrees.
	 */
	virtual void LimitViewPitch(FRotator& ViewRotation, float InViewPitchMin, float InViewPitchMax);

	/** 
	 * Limit the player's view roll. 
	 * @param ViewRotation - ViewRotation to modify. Both input and output.
	 * @param InViewRollMin - Minimum view roll, in degrees.
	 * @param InViewRollMax - Maximum view roll, in degrees.
	 */
	virtual void LimitViewRoll(FRotator& ViewRotation, float InViewRollMin, float InViewRollMax);

	/** 
	 * Limit the player's view yaw. 
	 * @param ViewRotation - ViewRotation to modify. Both input and output.
	 * @param InViewYawMin - Minimum view yaw, in degrees.
	 * @param InViewYawMax - Maximum view yaw, in degrees.
	 */
	virtual void LimitViewYaw(FRotator& ViewRotation, float InViewYawMin, float InViewYawMax);

	/**
	 * Does the actual work to UpdateViewTarget. This is called from UpdateViewTarget under normal 
	 * circumstances (target is not a camera actor and no debug cameras are active)
	 * Provides a way for subclasses to override behavior without copy-pasting the special case code.
	 *
	 * @param	OutVT		ViewTarget to use.
	 * @param	DeltaTime	Delta Time since last camera update (in seconds).
	 */
	virtual void UpdateViewTargetInternal(FTViewTarget& OutVT, float DeltaTime);

private:
	// Buried to prevent use; use GetCameraLocation instead
	FRotator GetActorRotation() const { return Super::GetActorRotation(); }

	// Buried to prevent use; use GetCameraRotation instead
	FVector GetActorLocation() const { return Super::GetActorLocation(); }

public:
	/** @return Returns TransformComponent subobject */
	class USceneComponent* GetTransformComponent() const;
};
