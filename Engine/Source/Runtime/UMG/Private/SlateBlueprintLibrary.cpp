// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "Slate/SlateBrushAsset.h"
#include "Runtime/Engine/Classes/Engine/UserInterfaceSettings.h"
#include "SlateBlueprintLibrary.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// USlateBlueprintLibrary

USlateBlueprintLibrary::USlateBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

bool USlateBlueprintLibrary::IsUnderLocation(const FGeometry& Geometry, const FVector2D& AbsoluteCoordinate)
{
	return Geometry.IsUnderLocation(AbsoluteCoordinate);
}

FVector2D USlateBlueprintLibrary::AbsoluteToLocal(const FGeometry& Geometry, FVector2D AbsoluteCoordinate)
{
	return Geometry.AbsoluteToLocal(AbsoluteCoordinate);
}

FVector2D USlateBlueprintLibrary::LocalToAbsolute(const FGeometry& Geometry, FVector2D LocalCoordinate)
{
	return Geometry.LocalToAbsolute(LocalCoordinate);
}

FVector2D USlateBlueprintLibrary::GetLocalSize(const FGeometry& Geometry)
{
	return Geometry.GetLocalSize();
}

void USlateBlueprintLibrary::LocalToViewport(UObject* WorldContextObject, const FGeometry& Geometry, FVector2D LocalCoordinate, FVector2D& PixelPosition, FVector2D& ViewportPosition)
{
	FVector2D AbsoluteCoordinate = Geometry.LocalToAbsolute(LocalCoordinate);
	AbsoluteToViewport(WorldContextObject, AbsoluteCoordinate, PixelPosition, ViewportPosition);
}

void USlateBlueprintLibrary::AbsoluteToViewport(UObject* WorldContextObject, FVector2D AbsoluteDesktopCoordinate, FVector2D& PixelPosition, FVector2D& ViewportPosition)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			if ( FViewport* Viewport = ViewportClient->Viewport )
			{
				FVector2D ViewportSize;
				ViewportClient->GetViewportSize(ViewportSize);

				PixelPosition = Viewport->VirtualDesktopPixelToViewport(FIntPoint((int32)AbsoluteDesktopCoordinate.X, (int32)AbsoluteDesktopCoordinate.Y)) * ViewportSize;

				float CurrentViewportScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));

				// Remove DPI Scaling.
				ViewportPosition = PixelPosition / CurrentViewportScale;

				return;
			}
		}
	}

	PixelPosition = FVector2D(0, 0);
	ViewportPosition = FVector2D(0, 0);
}

#undef LOCTEXT_NAMESPACE