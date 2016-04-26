// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Camera/CameraComponent.h"
#include "CineCameraComponent.generated.h"

/** #note, this struct has a details customization in CameraFilmbackSettingsCustomization.cpp/h */
USTRUCT()
struct FCameraFilmbackSettings
{
	GENERATED_USTRUCT_BODY()

	/** Horizontal size of filmback or digital sensor, in mm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filmback", meta = (ClampMin = "0.001", ForceUnits = mm))
	float SensorWidth;

	/** Vertical size of filmback or digital sensor, in mm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filmback", meta = (ClampMin = "0.001", ForceUnits = mm))
	float SensorHeight;

	/** Read-only. Computed from Sensor dimensions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Filmback")
	float SensorAspectRatio;
};

USTRUCT()
struct FNamedFilmbackPreset
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FCameraFilmbackSettings FilmbackSettings;
};

/** #note, this struct has a details customization in CameraLensSettingsCustomization.cpp/h */
USTRUCT()
struct FCameraLensSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ForceUnits = mm))
	float MinFocalLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ForceUnits = mm))
	float MaxFocalLength;

	// #todo, split into MinFStopAtMinFocalLength, MinFStopAtMaxFocalLength for variable-min-fstop lenses

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MinFStop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MaxFStop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens", meta = (ForceUnits = mm))
	float MinimumFocusDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens")
	float MaxReproductionRatio;

	bool operator==(const FCameraLensSettings& Other) const
	{
		return (MinFocalLength == Other.MinFocalLength)
			&& (MaxFocalLength == Other.MaxFocalLength)
			&& (MinFStop == Other.MinFStop)
			&& (MaxFStop == Other.MaxFStop)
			&& (MinimumFocusDistance == Other.MinimumFocusDistance)
			&& (MaxReproductionRatio == Other.MaxReproductionRatio);
	}
};


USTRUCT()
struct FNamedLensPreset
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FCameraLensSettings LensSettings;
};

UENUM()
enum class ECameraFocusMethod : uint8
{
	None,		/** Disable DoF entirely. */
	Manual,		/** Allows for specifying or animating exact focus distances. */
	Spot UMETA(Hidden), /** Raycasts into the scene, focuses on the hit location. */		// #todo, make this work
	Tracking,	/** Locks focus to specific object. */
};

USTRUCT()
struct FCameraSpotFocusSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spot Focus")
	TArray<FVector2D> NormalizedSpotCoordinates;
	
	FCameraSpotFocusSettings()
	{
		NormalizedSpotCoordinates.Add(FVector2D(0.5f, 0.5f));
	}
};

USTRUCT()
struct FCameraTrackingFocusSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
	AActor* ActorToTrack;

	// #todo, ability to pick a component and socket/bone?

// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
// 	USceneComponent* ComponentToTrack;
// 	
// 	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
// 	FName BoneOrSocketName;

	/** Offset from actor position to look at. Relative to actor if tracking an actor, relative to world otherwise. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
	FVector RelativeOffset;

	UPROPERTY(Transient, EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus")
	uint8 bDrawDebugTrackingFocusPoint : 1;

	FCameraTrackingFocusSettings()
		: ActorToTrack(nullptr)
	{}
};


USTRUCT()
struct FCameraFocusSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Method")
	TEnumAsByte<ECameraFocusMethod> FocusMethod;
	
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Manual Focus Settings")
	float ManualFocusDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spot Focus Settings")
	FCameraSpotFocusSettings SpotFocusSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tracking Focus Settings")
	FCameraTrackingFocusSettings TrackingFocusSettings;

#if WITH_EDITORONLY_DATA
	/** True to draw a translucent plane at the current focus depth, for easy tweaking when not looking through the camera. */
	UPROPERTY(Transient, EditAnywhere, Category = "Focus Settings")
	uint8 bDrawDebugFocusPlane : 1;

	/** For customizing the focus plane color, in case the default doesn't show up well in your scene. */
	UPROPERTY(EditAnywhere, Category = "Focus Settings", meta = (EditCondition = "bDrawDebugFocusPlane"))
	FColor DebugFocusPlaneColor;
#endif 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Settings")
	uint8 bSmoothFocusChanges : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus Settings", meta = (EditCondition = "bSmoothFocusChanges"))
	float FocusSmoothingInterpSpeed;

	/** Additional focus depth offset, used for manually tweaking if yoour chosen focus method is needs adjustment */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Focus Settings")
	float FocusOffset;

	FCameraFocusSettings() : 
		FocusMethod(ECameraFocusMethod::Manual),
		ManualFocusDistance(100000.f),
#if WITH_EDITORONLY_DATA
		bDrawDebugFocusPlane(0),
		DebugFocusPlaneColor(102, 26, 204, 153),		// purple
#endif
		bSmoothFocusChanges(0),
		FocusSmoothingInterpSpeed(8.f),
		FocusOffset(0.f)
	{}
};

/**
 * A specialized version of a camera component, geared toward cinematic usage.
 */
UCLASS(
	HideCategories = (CameraSettings), 
	HideFunctions = (SetFieldOfView, SetAspectRatio, SetConstraintAspectRatio), 
	Blueprintable, 
	ClassGroup = Camera, 
	meta = (BlueprintSpawnableComponent), 
	Config = Engine
	)
class CINEMATICCAMERA_API UCineCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	/** Default constuctor. */
	UCineCameraComponent();

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	FCameraFilmbackSettings FilmbackSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	FCameraLensSettings LensSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	FCameraFocusSettings FocusSettings;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	float CurrentFocalLength;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Current Camera Settings")
	float CurrentAperture;
	
	/** Read-only. Control this value via FocusSettings. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Current Camera Settings")
	float CurrentFocusDistance;

#if WITH_EDITORONLY_DATA
	/** Read-only. Control this value with CurrentFocalLength (and filmback settings). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Current Camera Settings")
	float CurrentHorizontalFOV;

	UPROPERTY(transient)
	UStaticMesh* DebugFocusPlaneMesh;

	UPROPERTY(transient)
	UMaterial* DebugFocusPlaneMaterial;

	// The camera mesh to show visually where the camera is placed
	UPROPERTY(transient)
	UStaticMeshComponent* DebugFocusPlaneComponent;

	UPROPERTY(transient)
	UMaterialInstanceDynamic* DebugFocusPlaneMID;
#endif
	
	/** Most recent calculated focus distance. Used for interpolation. */
	float LastFocusDistance;

	float GetHorizontalFieldOfView() const;
	float GetVerticalFieldOfView() const;

	static TArray<FNamedFilmbackPreset> const& GetFilmbackPresets();
	static TArray<FNamedLensPreset> const& GetLensPresets();

protected:

	/** Set to true to skip any interpolations on the next update. Resets to false automatically. */
	uint8 bResetInterpolation : 1;

	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void OnRegister() override;

#if WITH_EDITOR
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void ResetProxyMeshTransform() override;
#endif

	UPROPERTY(config)
	TArray<FNamedFilmbackPreset> FilmbackPresets;

	UPROPERTY(config)
	TArray<FNamedLensPreset> LensPresets;

	UPROPERTY(config)
	FString DefaultFilmbackPresetName;

	UPROPERTY(config)
	FString DefaultLensPresetName;
	
	/** Default focal length (will be constrained by default lens) */
	UPROPERTY(config)
	float DefaultLensFocalLength;
	
	/** Default aperture (will be constrained by default lens) */
	UPROPERTY(config)
	float DefaultLensFStop;

	virtual void UpdateCameraLens(float DeltaTime, FMinimalViewInfo& DesiredView);

	virtual void NotifyCameraCut() override;
	
private:
	void RecalcDerivedData();
	float GetDesiredFocusDistance(FMinimalViewInfo& DesiredView) const;
	float GetWorldToMetersScale() const;

	void CreateDebugFocusPlane();
	void DestroyDebugFocusPlane();
};
