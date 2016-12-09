// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/Interface.h"
#include "ViewportWorldInteractionInterface.generated.h"

UINTERFACE( MinimalAPI )
class UViewportWorldInteractionInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class VIEWPORTINTERACTION_API IViewportWorldInteractionInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/** Initializes the world interaction */
	virtual void Init( UWorld* InWorld ) = 0;

	/** Shuts down the world interaction */
	virtual void Shutdown() = 0;

	/** Main update loop including updating interaction, actions and hover */
	virtual void Tick( const float DeltaTime ) = 0;
	
	/** Adds interactor to the worldinteraction */	
	virtual void AddInteractor( class UViewportInteractor* Interactor ) = 0;

	/** Removes interactor from the worldinteraction and removes the interactor from its paired interactor if any */
	virtual void RemoveInteractor( class UViewportInteractor* Interactor ) = 0;

	/** Gets the event for hovering update which is broadcasted for each interactor */
	DECLARE_EVENT_FourParams( IViewportWorldInteractionInterface, FOnVIHoverUpdate, class FEditorViewportClient& /* ViewportClient */, class UViewportInteractor* /* Interactor */, FVector& /* OutHoverImpactPoint */,  bool& /* bWasHandled */ );
	virtual FOnVIHoverUpdate& OnViewportInteractionHoverUpdate() = 0;

	/** Gets the event for input actions update which is broadcasted for each interactor */
	DECLARE_EVENT_FiveParams( IViewportWorldInteractionInterface, FOnVIActionHandle, class FEditorViewportClient& /* ViewportClient */, class UViewportInteractor* /* Interactor */, 
	const struct FViewportActionKeyInput& /* Action */, bool& /* bOutIsInputCaptured */, bool& /* bWasHandled */ );
	virtual FOnVIActionHandle& OnViewportInteractionInputAction() = 0;

	/** To handle raw key input from the Inputprocessor */
	DECLARE_EVENT_FourParams( IViewportWorldInteractionInterface, FOnHandleInputKey, const class FEditorViewportClient& /* ViewportClient */, const FKey /* Key */, const EInputEvent /* Event */, bool& /* bWasHandled */ );
	virtual FOnHandleInputKey& OnHandleKeyInput() = 0;

	/** To handle raw axix input from the Inputprocessor */
	DECLARE_EVENT_SixParams( IViewportWorldInteractionInterface, FOnHandleInputAxis, class FEditorViewportClient& /* ViewportClient */, const int32 /* ControllerId */, const FKey /* Key */, const float /* Delta */, const float /* DeltaTime */, bool& /* bWasHandled */ );
	virtual FOnHandleInputAxis& OnHandleAxisInput() = 0;

	/** Gets the event for when an interactor stops dragging */
	DECLARE_EVENT_OneParam( IViewportWorldInteractionInterface, FOnStopDragging, class UViewportInteractor* /** Interactor */ );
	virtual FOnStopDragging& OnStopDragging() = 0;
};
