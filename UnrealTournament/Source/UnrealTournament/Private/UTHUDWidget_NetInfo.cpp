// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUDWidget_NetInfo.h"

UUTHUDWidget_NetInfo::UUTHUDWidget_NetInfo(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DesignedResolution = 1080;
	Position = FVector2D(0, 0);
	Size = FVector2D(300.0f, 540.0f);
	ScreenPosition = FVector2D(0.0f, 0.25f);
	Origin = FVector2D(0.0f, 0.0f);

	CellHeight = 40;
	DataColumnX = 0.6f;

	ValueHighlight[0] = FLinearColor::White;
	ValueHighlight[1] = FLinearColor::Yellow;
	ValueHighlight[2] = FLinearColor::Red;

	static ConstructorHelpers::FObjectFinder<UTexture2D> Tex(TEXT("Texture2D'/Game/RestrictedAssets/UI/Textures/UTScoreboard01.UTScoreboard01'"));
	TextureAtlas = Tex.Object;
}

bool UUTHUDWidget_NetInfo::ShouldDraw_Implementation(bool bShowScores)
{
	return (!bShowScores && UTHUDOwner && UTHUDOwner->PlayerOwner && Cast<AUTPlayerState>(UTHUDOwner->PlayerOwner->PlayerState)); // && UTHUDOwner && UTHUDOwner->PlayerOwner && UTHUDOwner->PlayerOwner->bShowNetInfo)
}

void UUTHUDWidget_NetInfo::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	float XOffset = 16.f;
	float DrawOffset = 0.f;
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTHUDOwner->PlayerOwner->PlayerState);
	if (!UTPS)
	{
		return;
	}
	DrawTexture(TextureAtlas, 0.5f*XOffset, DrawOffset, Size.X, Size.Y, 149, 138, 32, 32, 0.5f, FLinearColor::Black);

	DrawOffset += 0.5f * CellHeight;

	// @TODO FIXMESTEVE - exec which adds this widget to HUD and enables it with flag
	// ping
	DrawText(NSLOCTEXT("NetInfo", "Ping title", "Ping"), XOffset, DrawOffset, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
	FFormatNamedArguments Args;
	Args.Add("PingMS", FText::AsNumber(int32(1000.f*UTPS->ExactPing)));
	DrawText(FText::Format(NSLOCTEXT("NetInfo", "Ping", "{PingMS} ms"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->MediumFont, 1.0f, 1.0f, GetValueColor(UTPS->ExactPing, 0.07f, 0.16f), ETextHorzPos::Left, ETextVertPos::Center);

	// ping variance - magnitude and frequency of variance

	// netspeed
	
	// bytes in and out

	// cool graph options

	// packet loss
}

FLinearColor UUTHUDWidget_NetInfo::GetValueColor(float Value, float ThresholdBest, float ThresholdWorst) const
{
	if (Value < ThresholdBest)
	{
		return ValueHighlight[0];
	}
	if (Value > ThresholdWorst)
	{
		return ValueHighlight[2];
	}
	return ValueHighlight[1];
}

