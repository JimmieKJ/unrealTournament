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
	return (!bShowScores && UTHUDOwner && Cast<AUTPlayerController>(UTHUDOwner->PlayerOwner) && Cast<AUTPlayerController>(UTHUDOwner->PlayerOwner)->bShowNetInfo);
}

void UUTHUDWidget_NetInfo::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	float XOffset = 16.f;
	float DrawOffset = 0.f;
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTHUDOwner->PlayerOwner->PlayerState);
	UNetConnection* Connection = Cast<UNetConnection>(UTHUDOwner->PlayerOwner->Player);
	UNetDriver* NetDriver = GetWorld()->GetNetDriver();

	DrawTexture(TextureAtlas, 0.5f*XOffset, DrawOffset, Size.X, Size.Y, 149, 138, 32, 32, 0.5f, FLinearColor::Black);

	DrawOffset += 0.5f * CellHeight;

	// ping
	if (UTPS)
	{
		DrawText(NSLOCTEXT("NetInfo", "Ping title", "Ping"), XOffset, DrawOffset, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		FFormatNamedArguments Args;
		Args.Add("PingMS", FText::AsNumber(int32(UTPS->ExactPing)));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "Ping", "{PingMS} ms"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, GetValueColor(UTPS->ExactPing, 70.f, 160.f), ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;
	}

	// ping variance - magnitude and frequency of variance

	// netspeed
	if (Connection)
	{
		DrawText(NSLOCTEXT("NetInfo", "NetSpeed title", "Net Speed"), XOffset, DrawOffset, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		FFormatNamedArguments Args;
		Args.Add("NetBytes", FText::AsNumber(Connection->CurrentNetSpeed));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "NetBytes", "{NetBytes} bytes/s"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;
	}

	// bytes in and out
	if (NetDriver)
	{
		DrawText(NSLOCTEXT("NetInfo", "BytesIn title", "Bytes In"), XOffset, DrawOffset, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		FFormatNamedArguments Args;
		Args.Add("NetBytesIN", FText::AsNumber(NetDriver->InBytesPerSecond));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "NetBytesIN", "{NetBytesIN} bytes/s"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;

		DrawText(NSLOCTEXT("NetInfo", "BytesOut title", "Bytes Out"), XOffset, DrawOffset, UTHUDOwner->MediumFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		Args.Add("NetBytes", FText::AsNumber(NetDriver->OutBytesPerSecond));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "NetBytes", "{NetBytes} bytes/s"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;
	}

	// cool graph options

	// packet loss

	// ping prediction stats
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

