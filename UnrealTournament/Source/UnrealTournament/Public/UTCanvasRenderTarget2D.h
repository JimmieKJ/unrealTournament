// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTCanvasRenderTarget2D.generated.h"

DECLARE_DELEGATE_ThreeParams(FOnNonUOnjectRenderTargetUpdate, UCanvas*, int32, int32);

UCLASS()
class UNREALTOURNAMENT_API UUTCanvasRenderTarget2D : public UCanvasRenderTarget2D
{
	GENERATED_BODY()
public:
	UUTCanvasRenderTarget2D(const FObjectInitializer& OI)
		: Super(OI)
	{
		OnCanvasRenderTargetUpdate.AddDynamic(this, &UUTCanvasRenderTarget2D::UpdateDispatcher);
	}

	UFUNCTION()
	void UpdateDispatcher(UCanvas* Canvas, int32 Width, int32 Height)
	{
		OnNonUObjectRenderTargetUpdate.ExecuteIfBound(Canvas, Width, Height);
	}

	/** allows non-UObject based consumers (e.g. Slate widgets) to use a CanvasRenderTarget */
	FOnNonUOnjectRenderTargetUpdate OnNonUObjectRenderTargetUpdate;
};