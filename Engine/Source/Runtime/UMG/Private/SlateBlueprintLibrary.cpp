// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/SlateBlueprintLibrary.h"
#include "EngineGlobals.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

#include "Engine/UserInterfaceSettings.h"

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

bool USlateBlueprintLibrary::EqualEqual_SlateBrush(const FSlateBrush& A, const FSlateBrush& B)
{
	return A == B;
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

void USlateBlueprintLibrary::ScreenToWidgetLocal(UObject* WorldContextObject, const FGeometry& Geometry, FVector2D ScreenPosition, FVector2D& LocalCoordinate)
{
	FVector2D AbsoluteCoordinate;
	ScreenToWidgetAbsolute(WorldContextObject, ScreenPosition, AbsoluteCoordinate);

	LocalCoordinate = Geometry.AbsoluteToLocal(AbsoluteCoordinate);
}

void USlateBlueprintLibrary::ScreenToWidgetAbsolute(UObject* WorldContextObject, FVector2D ScreenPosition, FVector2D& AbsoluteCoordinate)
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

				const FVector2D NormalizedViewportCoordinates = ScreenPosition / ViewportSize;

				const FIntPoint VirtualDesktopPoint = Viewport->ViewportToVirtualDesktopPixel(NormalizedViewportCoordinates);
				AbsoluteCoordinate = FVector2D(VirtualDesktopPoint);
				return;
			}
		}
	}

	AbsoluteCoordinate = FVector2D(0, 0);
}

#undef LOCTEXT_NAMESPACE
