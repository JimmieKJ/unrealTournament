// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VREditorBaseActor.h"
#include "ViewportInteractableInterface.h"
#include "VREditorButton.generated.h"

//Forward declarations
class UViewportInteractor;


/**
* A button for VR Editor
*/
UCLASS()
class AVREditorButton : public AVREditorBaseActor, public IViewportInteractableInterface
{
	GENERATED_BODY()

public:

	/** Default constructor which sets up safe defaults */
	AVREditorButton();

	// Begin IViewportInteractableInterface
	virtual void OnPressed( UViewportInteractor* Interactor, const FHitResult& InHitResult, bool& bOutResultedInDrag  ) override;
	virtual void OnHover( UViewportInteractor* Interactor ) override;
	virtual void OnHoverEnter( UViewportInteractor* Interactor, const FHitResult& InHitResult ) override;
	virtual void OnHoverLeave( UViewportInteractor* Interactor, const UActorComponent* NewComponent ) override;
	virtual void OnDragRelease( UViewportInteractor* Interactor ) override;
	virtual class UViewportDragOperationComponent* GetDragOperationComponent() override { return nullptr; };
	// End IViewportInteractableInterface

	/** Gets the on pressed event */
	DECLARE_EVENT_TwoParams( AVREditorButton, FOnPressedEvent, AVREditorButton*, UViewportInteractor* );
	FOnPressedEvent& GetOnPressed() { return OnPressedEvent; }

	/** Gets the on hover event */
	DECLARE_EVENT_TwoParams( AVREditorButton, FOnHoverEvent, AVREditorButton*, UViewportInteractor* );
	FOnHoverEvent& GetOnHover() { return OnHoverEvent; }

	/** Gets the on hover enter event */
	DECLARE_EVENT_TwoParams( AVREditorButton, FOnHoverEnterEvent, AVREditorButton*, UViewportInteractor* );
	FOnHoverEnterEvent& GetOnHoverEnter() { return OnHoverEnterEvent; }

	/** Gets the on hover leave event */
	DECLARE_EVENT_TwoParams( AVREditorButton, FOnHoverLeaveEvent, AVREditorButton*, UViewportInteractor* );
	FOnHoverLeaveEvent	& GetOnHoverLeave() { return OnHoverLeaveEvent; }

	/** Gets the on release event */
	DECLARE_EVENT_TwoParams( AVREditorButton, FOnReleaseEvent, AVREditorButton*, UViewportInteractor* );
	FOnReleaseEvent& GetOnRelease() { return OnReleaseEvent; }

private:

	/** Mesh for the button to interact with */
	UPROPERTY()
	UStaticMeshComponent* ButtonMeshComponent;

	/** Broadcasted when pressed on this interactable by an interactor */
	FOnPressedEvent OnPressedEvent;

	/** Broadcasted when interactor is hovering over this interactable */
	FOnHoverEvent OnHoverEvent;

	/** Broadcasted when interactor started hovering over this interactable */
	FOnHoverEnterEvent OnHoverEnterEvent;

	/** Broadcasted when interactor stopped hovering over this interactable */
	FOnHoverLeaveEvent OnHoverLeaveEvent;

	/** Broadcasted when interactor released this interactable */
	FOnReleaseEvent OnReleaseEvent;
};