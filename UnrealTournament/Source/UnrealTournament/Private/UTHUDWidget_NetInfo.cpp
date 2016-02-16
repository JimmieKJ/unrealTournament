// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
	DataColumnX = 0.5f;

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

void UUTHUDWidget_NetInfo::AddPing(float NewPing)
{
	PingHistory[BucketIndex] = NewPing;
	BucketIndex++;
	if (BucketIndex > 299)
	{
		BucketIndex = 0;
	}
}

float UUTHUDWidget_NetInfo::CalcAvgPing()
{
	float TotalPing = 0.f;
	if (BucketIndex < 100)
	{
		// wrap around
		for (int32 i = 200 + BucketIndex; i < 300; i++)
		{
			TotalPing += PingHistory[i];
		}
	}
	for (int32 i = FMath::Max(0, BucketIndex - 100); i < BucketIndex; i++)
	{
		TotalPing += PingHistory[i];
	}
	return 0.01f * TotalPing;
}

float UUTHUDWidget_NetInfo::CalcPingStandardDeviation(float AvgPing)
{
	float Variance = 0.f;
	if (BucketIndex < 100)
	{
		// wrap around
		for (int32 i = 200 + BucketIndex; i < 300; i++)
		{
			Variance += FMath::Square(PingHistory[i] - AvgPing);
		}
	}
	for (int32 i = FMath::Max(0, BucketIndex - 100); i < BucketIndex; i++)
	{
		Variance += FMath::Square(PingHistory[i] - AvgPing);
	}

	Variance = 0.01f * Variance;
	return FMath::Sqrt(Variance);
}

void UUTHUDWidget_NetInfo::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	if (!UTHUDOwner || !UTHUDOwner->PlayerOwner)
	{
		return;
	}
	float XOffset = 16.f;
	float DrawOffset = 0.f;
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(UTHUDOwner->PlayerOwner->PlayerState);
	UNetDriver* NetDriver = GetWorld()->GetNetDriver();

	DrawTexture(TextureAtlas, 0.5f*XOffset, DrawOffset, Size.X, Size.Y, 149, 138, 32, 32, 0.5f, FLinearColor::Black);
	DrawOffset += 0.5f * CellHeight;

	// ping
	if (UTPS)
	{
		DrawText(NSLOCTEXT("NetInfo", "Ping title", "Ping"), XOffset, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		FFormatNamedArguments Args;
		Args.Add("PingMS", FText::AsNumber(int32(UTPS->ExactPing)));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "Ping", "{PingMS} ms"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, GetValueColor(UTPS->ExactPing, 70.f, 160.f), ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;

		float AvgPing = CalcAvgPing();
		FFormatNamedArguments AltArgs;
		AltArgs.Add("PingMS", FText::AsNumber(int32(1000.f*AvgPing)));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "AltPing", "{PingMS} ms ALT"), AltArgs), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, GetValueColor(AvgPing, 70.f, 160.f), ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;

		float PingSD = CalcPingStandardDeviation(AvgPing);
		FFormatNamedArguments SDArgs;
		SDArgs.Add("PingSD", FText::AsNumber(int32(1000.f*PingSD)));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "Ping Standard Deviation", "{PingSD} StdDev"), SDArgs), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, GetValueColor(PingSD, 5.f, 20.f), ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;

		//fixmesteve add max deviation, chart of moving ping
		// FIXMESTEVE - updating slowly, not per frame - why?
		DrawOffset += CellHeight;
	}

	// netspeed
	if (UTHUDOwner->PlayerOwner->Player)
	{
		DrawText(NSLOCTEXT("NetInfo", "NetSpeed title", "Net Speed"), XOffset, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		FFormatNamedArguments Args;
		Args.Add("NetBytes", FText::AsNumber(UTHUDOwner->PlayerOwner->Player->CurrentNetSpeed));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "NetBytes", "{NetBytes} bytes/s"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;
		DrawOffset += CellHeight;
	}

	// bytes in and out
	if (NetDriver)
	{
		DrawText(NSLOCTEXT("NetInfo", "BytesIn title", "Bytes In"), XOffset, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		FFormatNamedArguments Args;
		Args.Add("NetBytesIN", FText::AsNumber(NetDriver->InBytesPerSecond));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "NetBytesIN", "{NetBytesIN} bytes/s"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;

		DrawText(NSLOCTEXT("NetInfo", "BytesOut title", "Bytes Out"), XOffset, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		Args.Add("NetBytes", FText::AsNumber(NetDriver->OutBytesPerSecond));
		DrawText(FText::Format(NSLOCTEXT("NetInfo", "NetBytes", "{NetBytes} bytes/s"), Args), XOffset + DataColumnX*Size.X, DrawOffset, UTHUDOwner->SmallFont, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Left, ETextVertPos::Center);
		DrawOffset += CellHeight;
	}

	// cool graph options

	// packet loss - also measure burstiness

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

