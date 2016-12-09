// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IViewportWorldInteractionManager
{
public:
	DECLARE_EVENT_OneParam( IViewportWorldInteractionManager, FOnWorldInteractionTick, const float /* DeltaTime */ );

	/** Gets the event for ticking before the viewport world interaction */
	virtual FOnWorldInteractionTick& OnPreWorldInteractionTick() = 0;

	/** Gets the event for ticking after the viewport world interaction */
	virtual FOnWorldInteractionTick& OnPostWorldInteractionTick() = 0;

	/** 
	 * Sets the current ViewportWorldInteraction
	 *
	 * @param UViewportWorldInteraction the next ViewportWorldInteraction used
	 */
	virtual void SetCurrentViewportWorldInteraction( class UViewportWorldInteraction* /*WorldInteraction*/ ) = 0;

	/**
	 * Updates the current ViewportWorldInteraction
	 */
	virtual void Tick( float DeltaTime ) = 0;
};
