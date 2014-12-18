// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCanvasRenderTarget2D.generated.h"

UCLASS()
class UUTCanvasRenderTarget2D : public UCanvasRenderTarget2D
{
	GENERATED_BODY()

	// temp workaround for engine crash
	virtual UWorld* GetWorld() const override
	{
		return GetOuter()->GetWorld();
	}
};