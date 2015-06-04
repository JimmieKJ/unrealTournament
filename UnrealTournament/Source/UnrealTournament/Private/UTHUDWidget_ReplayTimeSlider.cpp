// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_ReplayTimeSlider.h"
#include "Engine/DemoNetDriver.h"

UUTHUDWidget_ReplayTimeSlider::UUTHUDWidget_ReplayTimeSlider(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(0.0f, 0.0f);
	ScreenPosition = FVector2D(0.0f, 0.0f);
	Origin = FVector2D(0.0f, 0.0f);

	// PLK is lazy
	bScaleByDesignedResolution = false;

	bIsInteractive = false;
}

void UUTHUDWidget_ReplayTimeSlider::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
}

bool UUTHUDWidget_ReplayTimeSlider::ShouldDraw_Implementation(bool bShowScores)
{
	if (UTHUDOwner->UTPlayerOwner && UTHUDOwner->UTPlayerOwner->UTPlayerState && UTGameState && UTHUDOwner->UTPlayerOwner->UTPlayerState->bOnlySpectator)
	{		
		return UTHUDOwner->UTPlayerOwner->GetWorld()->DemoNetDriver != nullptr;
	}

	return false;
}

void UUTHUDWidget_ReplayTimeSlider::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	UDemoNetDriver* DemoDriver = UTHUDOwner->UTPlayerOwner->GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		float CurrentTime = DemoDriver->DemoCurrentTime;
		float TotalTime = DemoDriver->DemoTotalTime;
		float TimeDilation = UTHUDOwner->UTPlayerOwner->GetWorldSettings()->DemoPlayTimeDilation;
		bool bPaused = (UTHUDOwner->UTPlayerOwner->GetWorldSettings()->Pauser != nullptr);

		// Not great style, but canvas is only valid during render
		CachedSliderLength = Canvas->SizeX - 80;
		CachedSliderOffset = 40;
		CachedSliderHeight = 50;
		CachedSliderYPos = Canvas->SizeY * .8f;

		FVector2D StartBarPos = FVector2D(40, Canvas->SizeY * .8f);
		FVector2D EndBarPos = FVector2D(Canvas->SizeX - 40, Canvas->SizeY * .8f);
		Canvas->K2_DrawLine(StartBarPos, EndBarPos, 50, FLinearColor::Black);

		EndBarPos = (CurrentTime / TotalTime) * (EndBarPos - StartBarPos) + StartBarPos;
		Canvas->K2_DrawLine(StartBarPos, EndBarPos, 50, FLinearColor::Red);
		
		if (bIsInteractive && 
			MousePosition.X > CachedSliderOffset && MousePosition.X < CachedSliderOffset + CachedSliderLength &&
			MousePosition.Y > CachedSliderYPos - CachedSliderHeight / 2 && MousePosition.Y < CachedSliderYPos + CachedSliderHeight / 2)
		{
			FVector2D StartScrubPos = FVector2D(MousePosition.X, Canvas->SizeY * .8f);
			FVector2D EndScrubPos = FVector2D(MousePosition.X + 5, Canvas->SizeY * .8f);

			Canvas->K2_DrawLine(StartScrubPos, EndScrubPos, 50, FLinearColor::White);
		}

		FFormatNamedArguments Args;
		Args.Add("TotalTime", FText::AsTimespan(FTimespan(0, 0, static_cast<int32>(TotalTime))));
		Args.Add("CurrentTime", FText::AsTimespan(FTimespan(0, 0, static_cast<int32>(CurrentTime))));
		FText TimeText = FText::Format(FText::FromString(TEXT("{CurrentTime} / {TotalTime}")), Args);

		DrawText(TimeText, StartBarPos.X + 25, StartBarPos.Y - 25, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Top);
	}
}

bool UUTHUDWidget_ReplayTimeSlider::SelectionClick(FVector2D InMousePosition)
{
	MousePosition = InMousePosition;

	if (MousePosition.X > CachedSliderOffset && MousePosition.X < CachedSliderOffset + CachedSliderLength &&
		MousePosition.Y > CachedSliderYPos - CachedSliderHeight / 2 && MousePosition.Y < CachedSliderYPos + CachedSliderHeight / 2)
	{
		float TimelinePercentage = (float)(MousePosition.X - CachedSliderOffset) / (float)(CachedSliderLength);

		UDemoNetDriver* DemoDriver = UTHUDOwner->UTPlayerOwner->GetWorld()->DemoNetDriver;
		if (DemoDriver)
		{
			DemoDriver->GotoTimeInSeconds(TimelinePercentage * DemoDriver->DemoTotalTime);
			return true;
		}
	}

	return false;
}