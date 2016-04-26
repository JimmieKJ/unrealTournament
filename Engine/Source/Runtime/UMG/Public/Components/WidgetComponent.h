// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetComponent.generated.h"

struct FVirtualPointerPosition;

UENUM()
enum class EWidgetSpace : uint8
{
	/** The widget is rendered in the world as mesh, it can be occluded like any other mesh in the world. */
	World,
	/** The widget is rendered in the screen, completely outside of the world, never occluded. */
	Screen
};

UENUM()
enum class EWidgetBlendMode : uint8
{
	Opaque,
	Masked,
	Transparent
};

/**
 * Beware! This feature is experimental and may be substantially changed or removed in future releases.
 * A 3D instance of a Widget Blueprint that can be interacted with in the world.
 */
UCLASS(ClassGroup=Experimental, hidecategories=(Object,Activation,"Components|Activation",Sockets,Base,Lighting,LOD,Mesh), editinlinenew, meta=(BlueprintSpawnableComponent,  DevelopmentStatus=Experimental) )
class UMG_API UWidgetComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** UActorComponent Interface */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	/* UPrimitiveComponent Interface */
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual UBodySetup* GetBodySetup() override;
	virtual FCollisionShape GetCollisionShape(float Inflation) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;

	void ApplyComponentInstanceData(class FWidgetComponentInstanceData* ComponentInstanceData);

	// Begin UObject
	virtual void PostLoad() override;
	// End UObject

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Ensures the user widget is initialized */
	void InitWidget();

	/** Release resources associated with the widget. */
	void ReleaseResources();

	/** Ensures the 3d window is created its size and content. */
	void UpdateWidget();

	/** Ensure the render target is initialized and updates it if needed. */
	void UpdateRenderTarget();

	/** 
	* Ensures the body setup is initialized and updates it if needed.
	* @param bDrawSizeChanged Whether the draw size of this component has changed since the last update call.
	*/
	void UpdateBodySetup( bool bDrawSizeChanged = false );

	/**
	 * Converts a world-space hit result to a hit location on the widget
	 * @param HitResult The hit on this widget in the world
	 * @param (Out) The transformed 2D hit location on the widget
	 */
	void GetLocalHitLocation(FVector WorldHitLocation, FVector2D& OutLocalHitLocation) const;

	/** @return The class of the user widget displayed by this component */
	TSubclassOf<UUserWidget> GetWidgetClass() const { return WidgetClass; }

	/** @return The user widget object displayed by this component */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	UUserWidget* GetUserWidgetObject() const;

	/** @return List of widgets with their geometry and the cursor position transformed into this Widget component's space. */
	TArray<FWidgetAndPointer> GetHitWidgetPath(FVector WorldHitLocation, bool bIgnoreEnabledStatus, float CursorRadius = 0.0f);

	/** @return The render target to which the user widget is rendered */
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	/** @return The dynamic material instance used to render the user widget */
	UMaterialInstanceDynamic* GetMaterialInstance() const { return MaterialInstance; }

	/** @return The window containing the user widget content */
	TSharedPtr<SWidget> GetSlateWidget() const;

	/**  
	 *  Sets the widget to use directly. This function will keep track of the widget till the next time it's called
	 *	with either a newer widget or a nullptr
	 */ 
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetWidget(UUserWidget* Widget);

	/**
	 * Sets the local player that owns this widget component.  Setting the owning player controls
	 * which player's viewport the widget appears on in a split screen scenario.  Additionally it
	 * forwards the owning player to the actual UserWidget that is spawned.
	 */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetOwnerPlayer(ULocalPlayer* LocalPlayer);

	/** Gets the local player that owns this widget component. */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	ULocalPlayer* GetOwnerPlayer() const;

	/** @return The draw size of the quad in the world */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	FVector2D GetDrawSize() const;

	/** Sets the draw size of the quad in the world */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetDrawSize(FVector2D Size);

	/** @return The max distance from which a player can interact with this widget */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	float GetMaxInteractionDistance() const;

	/** Sets the max distance from which a player can interact with this widget */
	UFUNCTION(BlueprintCallable, Category=UserInterface)
	void SetMaxInteractionDistance(float Distance);

	/** Gets the blend mode for the widget. */
	EWidgetBlendMode GetBlendMode() const { return BlendMode; }

	/** @return The pivot point where the UI is rendered about the origin. */
	FVector2D GetPivot() const { return Pivot; }

	void SetPivot( const FVector2D& InPivot ) { Pivot = InPivot; }

	/** Get the fake window we create for widgets displayed in the world. */
	TSharedPtr< SWindow > GetVirtualWindow() const;
	
	/** Whether or not this component uses legacy default rotation */
	bool IsUsingLegacyRotation() const { return bUseLegacyRotation; }

	/** Updates the actual material being used */
	void UpdateMaterialInstance();
	
	/** Sets the widget class used to generate the widget for this component */
	void SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass);

	EWidgetSpace GetWidgetSpace() const { return Space; }

	void SetWidgetSpace( EWidgetSpace NewSpace ) { Space = NewSpace; }

protected:
	void RemoveWidgetFromScreen();

	/** Draws the current widget to the render target if possible. */
	void DrawWidgetToRenderTarget(float DeltaTime);

protected:
	/** The coordinate space in which to render the widget */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	EWidgetSpace Space;

	/** The class of User Widget to create and display an instance of */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	TSubclassOf<UUserWidget> WidgetClass;
	
	/** The size of the displayed quad. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	FIntPoint DrawSize;

	/** The Alignment/Pivot point that the widget is placed at relative to the position. */
	UPROPERTY(EditAnywhere, Category=UserInterface)
	FVector2D Pivot;
	
	/** The maximum distance from which a player can interact with this widget */
	UPROPERTY(EditAnywhere, Category=UserInterface, meta=(ClampMin="0.0", UIMax="5000.0", ClampMax="100000.0"))
	float MaxInteractionDistance;

	/**
	 * The owner player for a widget component, if this widget is drawn on the screen, this controls
	 * what player's screen it appears on for split screen, if not set, users player 0.
	 */
	UPROPERTY()
	ULocalPlayer* OwnerPlayer;

	/** The background color of the component */
	UPROPERTY(EditAnywhere, Category=Rendering)
	FLinearColor BackgroundColor;

	/** The blend mode for the widget. */
	UPROPERTY(EditAnywhere, Category=Rendering)
	EWidgetBlendMode BlendMode;

	UPROPERTY()
	bool bIsOpaque_DEPRECATED;

	/** Is the component visible from behind? */
	UPROPERTY(EditAnywhere, Category=Rendering)
	bool bIsTwoSided;
	
	/**
	 * When enabled, distorts the UI along a parabola shape giving the UI the appearance 
	 * that it's on a curved surface in front of the users face.  This only works for UI 
	 * rendered to a render target.
	 */
	UPROPERTY(EditAnywhere, Category=Rendering)
	float ParabolaDistortion;

	/** Should the component tick the widget when it's off screen? */
	UPROPERTY(EditAnywhere, Category=Animation)
	bool TickWhenOffscreen;

	/** The User Widget object displayed and managed by this component */
	UPROPERTY(Transient, DuplicateTransient)
	UUserWidget* Widget;

	/** The body setup of the displayed quad */
	UPROPERTY(Transient, DuplicateTransient)
	class UBodySetup* BodySetup;

	/** The material instance for translucent widget components */
	UPROPERTY()
	UMaterialInterface* TranslucentMaterial;

	/** The material instance for translucent, one-sided widget components */
	UPROPERTY()
	UMaterialInterface* TranslucentMaterial_OneSided;

	/** The material instance for opaque widget components */
	UPROPERTY()
	UMaterialInterface* OpaqueMaterial;

	/** The material instance for opaque, one-sided widget components */
	UPROPERTY()
	UMaterialInterface* OpaqueMaterial_OneSided;

	/** The material instance for masked widget components. */
	UPROPERTY()
	UMaterialInterface* MaskedMaterial;

	/** The material instance for masked, one-sided widget components. */
	UPROPERTY()
	UMaterialInterface* MaskedMaterial_OneSided;

	/** The target to which the user widget is rendered */
	UPROPERTY(Transient, DuplicateTransient)
	UTextureRenderTarget2D* RenderTarget;

	/** The dynamic instance of the material that the render target is attached to */
	UPROPERTY(Transient, DuplicateTransient)
	UMaterialInstanceDynamic* MaterialInstance;

	UPROPERTY()
	bool bUseLegacyRotation;

	UPROPERTY(Transient, DuplicateTransient)
	bool bAddedToScreen;
protected:

	/** The grid used to find actual hit actual widgets once input has been translated to the components local space */
	TSharedPtr<class FHittestGrid> HitTestGrid;
	
	/** The slate window that contains the user widget content */
	TSharedPtr<class SVirtualWindow> SlateWidget;

	/** The relative location of the last hit on this component */
	FVector2D LastLocalHitLocation;

	/** The hit tester to use for this component */
	static TSharedPtr<class FWidget3DHitTester> WidgetHitTester;

	/** Helper class for drawing widgets to a render target. */
	TSharedPtr<class FWidgetRenderer> WidgetRenderer;
};
