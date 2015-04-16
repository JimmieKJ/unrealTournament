// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCanvasRenderTarget2D.generated.h"

DECLARE_DELEGATE_ThreeParams(FOnNonUOnjectRenderTargetUpdate, UCanvas*, int32, int32);

UCLASS()
class UNREALTOURNAMENT_API UUTCanvasRenderTarget2D : public UCanvasRenderTarget2D
{
	GENERATED_BODY()
public:
	// temp workaround for engine crash
	virtual UWorld* GetWorld() const override
	{
		return GetOuter()->GetWorld();
	}

	virtual void ReceiveUpdate(UCanvas* Canvas, int32 Width, int32 Height)
	{
		Super::ReceiveUpdate(Canvas, Width, Height);
		OnNonUObjectRenderTargetUpdate.ExecuteIfBound(Canvas, Width, Height);
	}

	/** allows non-UObject based consumers (e.g. Slate widgets) to use a CanvasRenderTarget */
	FOnNonUOnjectRenderTargetUpdate OnNonUObjectRenderTargetUpdate;
};