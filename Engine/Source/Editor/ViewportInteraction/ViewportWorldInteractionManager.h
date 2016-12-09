// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/GCObject.h"
#include "Editor/ViewportInteraction/ViewportInteractionInputProcessor.h"
#include "IViewportWorldInteractionManager.h"

class FViewportWorldInteractionManager: public FGCObject,  public IViewportWorldInteractionManager
{
public:
	FViewportWorldInteractionManager();
	virtual ~FViewportWorldInteractionManager();

	// FGCObject overrides
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	// IViewportWorldInteractionManager overrides
	virtual void Tick( float DeltaTime ) override;
	virtual FOnWorldInteractionTick& OnPreWorldInteractionTick() override { return OnPreWorldInteractionTickEvent; };
	virtual FOnWorldInteractionTick& OnPostWorldInteractionTick() override { return OnPostWorldInteractionTickEvent; };
	virtual void SetCurrentViewportWorldInteraction( class UViewportWorldInteraction* WorldInteraction ) override;

	//
	// Input
	//

	/** Passes the input key to the current world interaction */
	bool HandleInputKey( const FKey Key, const EInputEvent Event );

	/** Passes the input axis to the current world interaction */
	bool HandleInputAxis( const int32 ControllerId, const FKey Key, const float Delta, const float DeltaTime );


private:

	/** The current world interaction that is ticking */
	class UViewportWorldInteraction* CurrentWorldInteraction;

	//
	// Events
	//

	/** Event to tick before the worldinteraction ticks */
	FOnWorldInteractionTick OnPreWorldInteractionTickEvent;

	/** Event to tick after the worldinteraction has ticked */
	FOnWorldInteractionTick OnPostWorldInteractionTickEvent;

	//
	// Input
	//

	/** Slate Input Processor */
	TSharedPtr<class FViewportInteractionInputProcessor> InputProcessor;
};
