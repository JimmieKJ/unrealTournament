// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewportInteractor.h"
#include "MouseCursorInteractor.generated.h"

/**
 * Viewport interactor for a mouse cursor
 */
UCLASS()
class VIEWPORTINTERACTION_API UMouseCursorInteractor : public UViewportInteractor
{
	GENERATED_UCLASS_BODY()

public:

	/** Initialize default values */
	void Init();

	// ViewportInteractor overrides
	virtual void PollInput() override;

private:

	// ...
};