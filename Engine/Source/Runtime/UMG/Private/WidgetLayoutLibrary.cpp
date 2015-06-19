// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "Slate/SlateBrushAsset.h"
#include "WidgetLayoutLibrary.h"
#include "Runtime/Engine/Classes/Engine/UserInterfaceSettings.h"
#include "Runtime/Engine/Classes/Engine/RendererSettings.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UWidgetLayoutLibrary

UWidgetLayoutLibrary::UWidgetLayoutLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

bool UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(APlayerController* PlayerController, FVector WorldLocation, FVector2D& ScreenPosition)
{
	if ( PlayerController )
	{
		FVector2D PixelLocation;
		const bool bProjected = PlayerController->ProjectWorldLocationToScreen(WorldLocation, PixelLocation);

		if ( bProjected )
		{
			// If the user has configured a resolution quality we need to multiply
			// the pixels by the resolution quality to arrive at the true position in
			// the viewport, as the rendered image will be stretched to fill whatever
			// size the viewport is at.
			Scalability::FQualityLevels ScalabilityQuality = Scalability::GetQualityLevels();
			const float QualityScale = ( ScalabilityQuality.ResolutionQuality / 100.0f );

			// Remove the resolution quality scale.
			ScreenPosition = PixelLocation / QualityScale;

			// Get the application / DPI scale
			const float Scale = UWidgetLayoutLibrary::GetViewportScale(PlayerController);

			// Apply inverse DPI scale so that the widget ends up in the expected position
			ScreenPosition = ScreenPosition / Scale;

			return true;
		}
	}

	return false;
}

float UWidgetLayoutLibrary::GetViewportScale(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			FVector2D ViewportSize;
			ViewportClient->GetViewportSize(ViewportSize);
			return GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
		}
	}

	return 1;
}

bool UWidgetLayoutLibrary::GetMousePositionScaledByDPI(APlayerController* Player, float& LocationX, float& LocationY)
{
	if ( Player && Player->GetMousePosition(LocationX, LocationY) )
	{
		float Scale = UWidgetLayoutLibrary::GetViewportScale(Player);
		LocationX = LocationX / Scale;
		LocationY = LocationY / Scale;

		return true;
	}

	return false;
}

FVector2D UWidgetLayoutLibrary::GetViewportSize(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			FVector2D ViewportSize;
			ViewportClient->GetViewportSize(ViewportSize);
			return ViewportSize;
		}
	}

	return FVector2D(1, 1);
}

UCanvasPanelSlot* UWidgetLayoutLibrary::SlotAsCanvasSlot(UWidget* ChildWidget)
{
	return Cast<UCanvasPanelSlot>(ChildWidget->Slot);
}

UGridSlot* UWidgetLayoutLibrary::SlotAsGridSlot(UWidget* ChildWidget)
{
	return Cast<UGridSlot>(ChildWidget->Slot);
}

UHorizontalBoxSlot* UWidgetLayoutLibrary::SlotAsHorizontalBoxSlot(UWidget* ChildWidget)
{
	return Cast<UHorizontalBoxSlot>(ChildWidget->Slot);
}

UOverlaySlot* UWidgetLayoutLibrary::SlotAsOverlaySlot(UWidget* ChildWidget)
{
	return Cast<UOverlaySlot>(ChildWidget->Slot);
}

UUniformGridSlot* UWidgetLayoutLibrary::SlotAsUniformGridSlot(UWidget* ChildWidget)
{
	return Cast<UUniformGridSlot>(ChildWidget->Slot);
}

UVerticalBoxSlot* UWidgetLayoutLibrary::SlotAsVerticalBoxSlot(UWidget* ChildWidget)
{
	return Cast<UVerticalBoxSlot>(ChildWidget->Slot);
}

void UWidgetLayoutLibrary::RemoveAllWidgets(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if ( World && World->IsGameWorld() )
	{
		if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
		{
			ViewportClient->RemoveAllViewportWidgets();
		}
	}
}

#undef LOCTEXT_NAMESPACE