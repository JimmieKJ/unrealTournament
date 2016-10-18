// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

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
	virtual void Init( const TSharedPtr<class FEditorViewportClient>& InEditorViewportClient ) = 0;

	/** Shuts down the world interaction */
	virtual void Shutdown() = 0;

	/** Main update loop including updating interaction, actions and hover */
	virtual void Tick( class FEditorViewportClient* ViewportClient, const float DeltaTime ) = 0;
	
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
};