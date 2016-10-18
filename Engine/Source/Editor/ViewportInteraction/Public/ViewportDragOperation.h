// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "ViewportDragOperation.generated.h"


/**
 * Base class for interactable drag calculations
 */
UCLASS()
class VIEWPORTINTERACTION_API UViewportDragOperation : public UObject
{
	GENERATED_BODY()

public:

	/** 
	 * Execute dragging 
	 *
	 * @param UViewportInteractor - The interactor causing the dragging
	 * @param IViewportInteractableInterface - The interactable owning this drag operation
	 */
	virtual void ExecuteDrag( class UViewportInteractor* Interactor, class IViewportInteractableInterface* Interactable ) PURE_VIRTUAL( UViewportDragOperation::ExecuteDrag, );
};

/**
 * Container component for UViewportDragOperation that can be used by objects in the world that are draggable and implement the ViewportInteractableInterface
 */
UCLASS()
class VIEWPORTINTERACTION_API UViewportDragOperationComponent : public UActorComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** Destructor */
	~UViewportDragOperationComponent();

	/** Get the actual dragging operation */
	UViewportDragOperation* GetDragOperation();

	/** Sets the drag operation class that will be used next time starting dragging */
	void SetDragOperationClass( const TSubclassOf<UViewportDragOperation> InDragOperation );

	/** Starts new dragging operation */
	void StartDragOperation();

	/* Destroys the dragoperation */
	void ClearDragOperation();

private:

	/** The actual dragging implementation */
	UPROPERTY()
	UViewportDragOperation* DragOperation;

	/** The next class that will be used as drag operation */
	UPROPERTY()
	TSubclassOf<UViewportDragOperation> DragOperationSubclass;
};
