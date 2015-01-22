// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetComponent.generated.h"

struct FVirtualPointerPosition;

UENUM()
enum class EWidgetSpace : uint8
{
	World,
	Screen
};

/**
* Beware! This feature is experimental and may be substantially changed or removed in future releases.
* A 3D instance of a Widget Blueprint that can be interacted with in the world.
*/
UCLASS(ClassGroup=Experimental, hidecategories=(Object,Activation,"Components|Activation",Sockets,Base,Lighting,LOD,Mesh), editinlinenew, meta=(BlueprintSpawnableComponent,  DevelopmentStatus=Experimental),MinimalAPI)
class UWidgetComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

public:
	/* UPrimitiveComponent Interface */
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual UBodySetup* GetBodySetup() override;
	virtual FCollisionShape GetCollisionShape(float Inflation) const override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void DestroyComponent() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	virtual FComponentInstanceDataBase* GetComponentInstanceData() const override;
	virtual FName GetComponentInstanceDataType() const override;
	virtual void ApplyComponentInstanceData(FComponentInstanceDataBase* ComponentInstanceData) override;

	// Begin UObject
	virtual void PostLoad() override;
	// End UObject

#if WITH_EDITORONLY_DATA
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif

	/** Ensures the user widget is initialized */
	void InitWidget();

	/** Ensures the 3d window is created its size and content. */
	void UpdateWidget();

	/** Ensure the render target is initialized and updates it if needed. */
	void UpdateRenderTarget();

	/** 
	* Ensures the body setup is initialized and updates it if needed.
	* @param bDrawSizeChanged Whether the draw size of this component has changed since the last update call.
	*/
	void UpdateBodySetup( bool bDrawSizeChanged = false );

	/** @return The class of the user widget displayed by this component */
	TSubclassOf<UUserWidget> GetWidgetClass() const { return WidgetClass; }

	/** @return The user widget object displayed by this component */
	UFUNCTION(BlueprintCallable, Category=UI)
	UUserWidget* GetUserWidgetObject() const;

	/** @return List of widgets with their geometry and the cursor position transformed into this Widget component's space. */
	TArray<FWidgetAndPointer> GetHitWidgetPath( const FHitResult& HitResult, bool bIgnoreEnabledStatus );

	/** @return The render target to which the user widget is rendered */
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; };

	/** @return The dynamic material instance used to render the user widget */
	UMaterialInstanceDynamic* GetMaterialInstance() const { return MaterialInstance; }

	/** @return The window containing the user widget content */
	TSharedPtr<SWidget> GetSlateWidget() const;

	/** @return The draw size of the quad in the world */
	const FIntPoint& GetDrawSize() const { return DrawSize; }

	/** @return The max distance from which a player can interact with this widget */
	float GetMaxInteractionDistance() const { return MaxInteractionDistance; }

	/** @return True if the component is opaque */
	bool IsOpaque() const { return bIsOpaque; }

	/** @return The pivot point where the UI is rendered about the origin. */
	FVector2D GetPivot() const { return Pivot; }
	
private:
	/** The class of User Widget to create and display an instance of */
	UPROPERTY(EditAnywhere, Category=UI)
	EWidgetSpace Space;

	/** The class of User Widget to create and display an instance of */
	UPROPERTY(EditAnywhere, Category=UI)
	TSubclassOf<UUserWidget> WidgetClass;
	
	/** The size of the displayed quad. */
	UPROPERTY(EditAnywhere, Category="UI")
	FIntPoint DrawSize;

	/** The Alignment/Pivot point that the widget is placed at relative to the position. */
	UPROPERTY(EditAnywhere, Category=UI)
	FVector2D Pivot;
	
	/** The maximum distance from which a player can interact with this widget */
	UPROPERTY(EditAnywhere, Category=UI, meta=(ClampMin="0.0", UIMax="5000.0", ClampMax="100000.0"))
	float MaxInteractionDistance;

	/** The background color of the component */
	UPROPERTY(EditAnywhere, Category=Rendering)
	FLinearColor BackgroundColor;

	/** 
	 * Should the component be rendered opaque? 
	 * This improves aliasing of the UI in the world.
	 */
	UPROPERTY(EditAnywhere, Category=Rendering)
	bool bIsOpaque;

	/** Is the component visible from behind? */
	UPROPERTY(EditAnywhere, Category=Rendering)
	bool bIsTwoSided;

	/** The User Widget object displayed and managed by this component */
	UPROPERTY(transient, duplicatetransient)
	UUserWidget* Widget;

	/** The body setup of the displayed quad */
	UPROPERTY(transient, duplicatetransient)
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

	/** The target to which the user widget is rendered */
	UPROPERTY(transient, duplicatetransient)
	UTextureRenderTarget2D* RenderTarget;

	/** The dynamic instance of the material that the render target is attached to */
	UPROPERTY(transient, duplicatetransient)
	UMaterialInstanceDynamic* MaterialInstance;

	/** The grid used to find actual hit actual widgets once input has been translated to the components local space */
	TSharedPtr<class FHittestGrid> HitTestGrid;

	/** The slate 3D renderer used to render the user slate widget */
	TSharedPtr<class ISlate3DRenderer> Renderer;
	
	/** The slate window that contains the user widget content */
	TSharedPtr<class SVirtualWindow> SlateWidget;

	/** The relative location of the last hit on this component */
	FVector2D LastLocalHitLocation;

	/** The hit tester to use for this component */
	static TSharedPtr<class FWidget3DHitTester> WidgetHitTester;
};

 

