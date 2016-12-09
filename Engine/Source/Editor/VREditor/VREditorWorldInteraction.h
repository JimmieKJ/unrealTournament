// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AssetData.h"
#include "VREditorWorldInteraction.generated.h"

class UActorComponent;
class UViewportInteractor;
class UViewportWorldInteraction;
class UVREditorMode;
struct FViewportActionKeyInput;

/**
 * VR Editor interaction with the 3D world
 */
UCLASS()
class UVREditorWorldInteraction : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Registers to events and sets initial values */
	void Init( UVREditorMode* NewOwner, UViewportWorldInteraction* ViewportWorldInteraction );
	
	/** Removes registered event */
	void Shutdown();

	/** If this component is interactable by an interactor */
	bool IsInteractableComponent( const UActorComponent* Component ) const;

	/** Snaps the selected objects to the ground */
	void SnapSelectedActorsToGround();

protected:
	/** When an interactor stops dragging */
	void StopDragging( UViewportInteractor* Interactor );

	/** Starts dragging a material, allowing the user to drop it on an object in the scene to place it */
	void StartDraggingMaterialOrTexture( UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, const FVector HitLocation, UObject* MaterialOrTextureAsset );

	/** Tries to place whatever material or texture that's being dragged on the object under the hand's laser pointer */
	void PlaceDraggedMaterialOrTexture( UViewportInteractor* Interactor );

	/** Called when FEditorDelegates::OnAssetDragStarted is broadcast */
	void OnAssetDragStartedFromContentBrowser( const TArray<FAssetData>& DraggedAssets, class UActorFactory* FactoryToUse );

protected:

	/** Owning object */
	UPROPERTY()
	UVREditorMode* Owner;

	/** The actual ViewportWorldInteraction */
	UPROPERTY()
	UViewportWorldInteraction* ViewportWorldInteraction;

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

