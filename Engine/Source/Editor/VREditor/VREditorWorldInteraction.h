// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportWorldInteraction.h"
#include "VREditorWorldInteraction.generated.h"

/**
 * VR Editor interaction with the 3D world
 */
UCLASS()
class UVREditorWorldInteraction : public UViewportWorldInteraction
{
	GENERATED_UCLASS_BODY()

public:

	// UViewportWorldInteraction overrides
	virtual void Shutdown() override;
	virtual bool IsInteractableComponent( const UActorComponent* Component ) const override;
	virtual void StopDragging( class UViewportInteractor* Interactor ) override;

	/** Sets the owner of this system */
	void SetOwner( class FVREditorMode* NewOwner )
	{
		Owner = NewOwner;
	}

	/** Gets the owner of this system */
	class FVREditorMode& GetOwner()
	{
		return *Owner;
	}

	/** Gets the owner of this system (const) */
	const class FVREditorMode& GetOwner() const
	{
		return *Owner;
	}

		/** Snaps the selected objects to the ground */
	void SnapSelectedActorsToGround();

protected:

	/** Starts dragging a material, allowing the user to drop it on an object in the scene to place it */
	void StartDraggingMaterialOrTexture( UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, const FVector HitLocation, UObject* MaterialOrTextureAsset );

	/** Tries to place whatever material or texture that's being dragged on the object under the hand's laser pointer */
	void PlaceDraggedMaterialOrTexture( UViewportInteractor* Interactor );

	/** Called when FEditorDelegates::OnAssetDragStarted is broadcast */
	void OnAssetDragStartedFromContentBrowser( const TArray<FAssetData>& DraggedAssets, class UActorFactory* FactoryToUse );

protected:

	/** Owning object */
	FVREditorMode* Owner;

	/** Sound for dropping materials and textures */
	UPROPERTY()
	class USoundCue* DropMaterialOrMaterialSound;

	//
	// Dragging object from UI
	//

	/** The UI used to drag an asset into the level */
	UPROPERTY()
	class UWidgetComponent* FloatingUIAssetDraggedFrom;

	/** The material or texture asset we're dragging to place on an object */
	UPROPERTY()
	UObject* PlacingMaterialOrTextureAsset;
};

