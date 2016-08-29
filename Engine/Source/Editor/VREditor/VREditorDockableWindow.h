// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorFloatingUI.h"
#include "ViewportInteractableInterface.h"
#include "VREditorDockableWindow.generated.h"

/**
 * An interactive floating UI panel that can be dragged around
 */
UCLASS()
class AVREditorDockableWindow : public AVREditorFloatingUI, public IViewportInteractableInterface
{
	GENERATED_BODY()
	
public:

	/** Default constructor */
	AVREditorDockableWindow();

	/** Destructor */
	~AVREditorDockableWindow();
	
	/** Updates the meshes for the UI */
	virtual void TickManually( float DeltaTime ) override;

	/** Updates the last dragged relative position */
	void UpdateRelativeRoomTransform();
	
	/** Gets the close button component */
	UStaticMeshComponent* GetCloseButtonMeshComponent() const;

	/** Gets the selection bar component */
	UStaticMeshComponent* GetSelectionBarMeshComponent() const;

	/** Gets the distance between the interactor and the window when sta	rting drag */
	float GetDockSelectDistance() const;

	// IViewportInteractableInterface
	virtual void OnPressed( UViewportInteractor* Interactor, const FHitResult& InHitResult, bool& bOutResultedInDrag ) override;
	virtual void OnHover( UViewportInteractor* Interactor ) override;
	
	/** Enter hover with laser changes the color of SelectionMesh and CloseButtonMesh */
	virtual void OnHoverEnter( UViewportInteractor* Interactor, const FHitResult& InHitResult ) override;
	
	/** Leaving hover with laser changes the color of SelectionMesh and CloseButtonMesh */
	virtual void OnHoverLeave( UViewportInteractor* Interactor, const UActorComponent* NewComponent ) override;
	virtual void OnDragRelease( UViewportInteractor* Interactor ) override;
	virtual class UViewportDragOperationComponent* GetDragOperationComponent() override;

protected:

	// AVREditorFloatingUI overrides
	virtual void SetupWidgetComponent() override;

private:

	/** Set the color on the dynamic materials of the selection bar */
	void SetSelectionBarColor( const FLinearColor& LinearColor );

	/** Set the color on the dynamic materials of the close button */
	void SetCloseButtonColor( const FLinearColor& LinearColor );

	/** The dockable window mesh */
	UPROPERTY()
	class UStaticMeshComponent* WindowMeshComponent;

	/** Mesh underneath the window for easy selecting and dragging */
	UPROPERTY()
	class UStaticMeshComponent* SelectionBarMeshComponent;

	/** Mesh that represents the close button for this UI */
	UPROPERTY()
	class UStaticMeshComponent* CloseButtonMeshComponent;

	/** Selection bar dynamic material  (opaque) */
	UPROPERTY()
	class UMaterialInstanceDynamic* SelectionBarMID;

	/** Select bar dynamic material (translucent) */
	UPROPERTY()
	class UMaterialInstanceDynamic* SelectionBarTranslucentMID;

	/** Close button dynamic material  (opaque) */
	UPROPERTY()
	class UMaterialInstanceDynamic* CloseButtonMID;

	/** Close button dynamic material (translucent) */
	UPROPERTY()
	class UMaterialInstanceDynamic* CloseButtonTranslucentMID;

	UPROPERTY()
	class UViewportDragOperationComponent* DragOperationComponent;

	/** True if at least one hand's laser is aiming toward the UI */
	bool bIsLaserAimingTowardUI;

	/** Scalar that ramps up toward 1.0 after the user aims toward the UI */
	float AimingAtMeFadeAlpha;

	/** True if we're hovering over the selection bar */
	bool bIsHoveringOverSelectionBar;

	/** Scalar that will advance toward 1.0 over time as we hover over the selection bar */
	float SelectionBarHoverAlpha;

	/** True if we're hovering over the close button */
	bool bIsHoveringOverCloseButton;

	/** Scalar that will advance toward 1.0 over time as we hover over the close button */
	float CloseButtonHoverAlpha;

	/** Distance from interactor laser to the handle when starting dragging */
	float DockSelectDistance;
};

/**
 * Calculation for dragging a dockable window
 */
UCLASS()
class UDockableWindowDragOperation : public UViewportDragOperation
{
	GENERATED_BODY()

public:

	// IViewportDragOperation
	virtual void ExecuteDrag( class UViewportInteractor* Interactor, IViewportInteractableInterface* Interactable ) override;
};