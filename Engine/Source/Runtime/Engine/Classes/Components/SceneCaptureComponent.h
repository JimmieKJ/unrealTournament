// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#pragma once
#include "Runtime/Engine/Public/ShowFlags.h"
#include "SceneCaptureComponent.generated.h"

/** View state needed to create a scene capture renderer */
struct FSceneCaptureViewInfo
{
	FVector ViewLocation;
	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;
	FIntRect ViewRect;
	EStereoscopicPass StereoPass;
};

USTRUCT()
struct FEngineShowFlagsSetting
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	FString ShowFlagName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	bool Enabled;
};

	// -> will be exported to EngineDecalClasses.h
UCLASS(hidecategories=(abstract, Collision, Object, Physics, SceneComponent, Mobility), MinimalAPI)
class USceneCaptureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** The components won't rendered by current component.*/
 	UPROPERTY()
 	TArray<TWeakObjectPtr<UPrimitiveComponent> > HiddenComponents;

	/** The actors to hide in the scene capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	TArray<AActor*> HiddenActors;

	/** The only components to be rendered by this scene capture, if present.*/
 	UPROPERTY()
 	TArray<TWeakObjectPtr<UPrimitiveComponent> > ShowOnlyComponents;

	/** The only actors to be rendered by this scene capture, if present.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	TArray<AActor*> ShowOnlyActors;

	/** Whether to update the capture's contents every frame.  If disabled, the component will render once on load and then only when moved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	bool bCaptureEveryFrame;

	/** Whether to update the capture's contents on movement.  Disable if you are going to capture manually from blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	bool bCaptureOnMovement;

	/** if > 0, sets a maximum render distance override.  Can be used to cull distant objects from a reflection if the reflecting plane is in an enclosed area like a hallway or room */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture, meta=(UIMin = "100", UIMax = "10000"))
	float MaxViewDistanceOverride;

	/** ShowFlags for the SceneCapture's ViewFamily, to control rendering settings for this view. Hidden but accessible through details customization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category=SceneComponent)
	TArray<struct FEngineShowFlagsSetting> ShowFlagSettings;

	// TODO: Make this a UStruct to set directly?
	/** Settings stored here read from the strings and int values in the ShowFlagSettings array */
	FEngineShowFlags ShowFlags;

public:
	/** Indicates which stereo pass this component is capturing for, if any */
    EStereoscopicPass CaptureStereoPass;

	/** Adds the component to our list of hidden components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void HideComponent(UPrimitiveComponent* InComponent);

	/** Adds all primitive components in the actor to our list of hidden components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void HideActorComponents(AActor* InActor);

	/** Adds the component to our list of show-only components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void ShowOnlyComponent(UPrimitiveComponent* InComponent);

	/** Adds all primitive components in the actor to our list of show-only components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void ShowOnlyActorComponents(AActor* InActor);

	/** Returns the view state, if any, and allocates one if needed. This function can return NULL, e.g. when bCaptureEveryFrame is false. */
	ENGINE_API FSceneViewStateInterface* GetViewState();
	ENGINE_API FSceneViewStateInterface* GetStereoViewState();

	/** Return a boolean for whether this flag exists in the ShowFlagSettings array, and a pointer to the flag if it does exist  */
	ENGINE_API bool GetSettingForShowFlag(FString FlagName, FEngineShowFlagsSetting** ShowFlagSettingOut);

#if WITH_EDITOR
	/**
	* Called when a property on this object has been modified externally
	*
	* @param PropertyThatChanged the property that was modified
	*/
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Called after done loading to update show flags from saved data */
	virtual void PostLoad() override;

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:
	/** Update the show flags from our show flags settings (ideally, you'd be able to set this more directly, but currently unable to make FEngineShowFlags a UStruct to use it as a UProperty...) */
	void UpdateShowFlags();

	/**
	 * The view state holds persistent scene rendering state and enables occlusion culling in scene captures.
	 * NOTE: This object is used by the rendering thread. The smart pointer's destructor will destroy it and that
	 */
	FSceneViewStateReference ViewState;
	FSceneViewStateReference StereoViewState;
};

