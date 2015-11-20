// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneComponent.h"
#include "LeapMotionImageComponent.generated.h"

namespace Leap { class Image; }

/**
*  Add this component to any actor to allow image from Leap Motion device's cameras to be displayed in the background of the scene.
*/
UCLASS(ClassGroup = LeapMotion, BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent) )
class LEAPMOTIONCONTROLLER_API ULeapMotionImageComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:	
	// Sets default values for this component's properties
	ULeapMotionImageComponent();

	/** Whether the image is displayed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bIsVisible;

	/** Whether the image is paused */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	bool bIsPaused;

	/** Distance from the PlayerCameraManager at which the image display surface is placed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	float DisplaySurfaceDistance;

	/** The mesh component with image display surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UStaticMeshComponent* DisplaySurfaceComponent;

	/** Material template used to display the pass-through image */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LeapMotion)
	UMaterialInterface* PassthroughMaterial;

	/** Dynamic instance of the pass-through material */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	UMaterialInstanceDynamic* DynamicPassthroughMaterial;

	/** Left raw image from the Leap device */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	UTexture2D* ImagePassthroughLeft;

	/** Right raw image from the Leap device */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	UTexture2D* ImagePassthroughRight;

	/** Texture encoding lens distortion for the left camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	UTexture2D* DistortionTextureLeft;

	/** Texture encoding lens distortion for the right camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = LeapMotion)
	UTexture2D* DistortionTextureRight;

	// Called when the game starts
	virtual void InitializeComponent() override;
	
	// Called every frame
	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

	void InitImageTexture();

	/** Trigger update of pass-through images */
	UFUNCTION(BlueprintCallable, Category = LeapMotion)
	void UpdateImageTexture();

	UTexture2D* BuildDistortionTextures(const Leap::Image& Image);

	void UpdateDistortionTextures(const Leap::Image& Image, UTexture2D* DistortionTexture);

	/** And event triggered when the display surface is attached to the PlayerCameraManager. You can overwrite the mesh type & its placement here */
	UFUNCTION(BlueprintNativeEvent, Category = LeapMotion)
	void AttachDisplaySurface();

#if WITH_EDITOR
	/** Track changes to property applied in the editor. */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
