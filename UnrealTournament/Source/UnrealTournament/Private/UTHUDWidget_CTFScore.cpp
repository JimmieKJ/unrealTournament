// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CTFScore.h"
#include "UTCTFGameState.h"

UUTHUDWidget_CTFScore::UUTHUDWidget_CTFScore(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void UUTHUDWidget_CTFScore::Draw_Implementation(float DeltaTime)
{
	Super::Draw_Implementation(DeltaTime);

	FLinearColor BGColor = ApplyHUDColor(FLinearColor::White);
	float RedScale  = UTHUDOwner->UTPlayerOwner->GetTeamNum() == 0 ? 1.25f : 1.f;
	float BlueScale = UTHUDOwner->UTPlayerOwner->GetTeamNum() == 1 ? 1.25f : 1.f;

	// Draw the Red Score...
	AUTCTFGameState* CGS = Cast<AUTCTFGameState>(UTGameState);
	if (CGS != NULL && CGS->Teams.Num() >= 2 && CGS->Teams[0] != NULL && CGS->Teams[1] != NULL)
	{
		// Draw the Red Flag State...
		DrawFlagIcon(-46 * RedScale,53 * RedScale, 43,41, 843, 87, 43,41, FLinearColor::Red, RedScale);
		FName FlagState = CGS->GetFlagState(0);

		float RedHolderScaleModifier = 1.f;
		if (FlagState == CarriedObjectState::Held)
		{
			RedPulseScale += DeltaTime * 3.f;
			RedHolderScaleModifier = 0.3f * FMath::Abs(FMath::Sin(RedPulseScale));
		}
		else
		{
			RedPulseScale = 0.f;
		}

		if (FlagState == CarriedObjectState::Dropped)
		{
			DrawFlagIcon(-46 * RedScale,53 * RedScale, 25,34, 894, 1, 25,34, FLinearColor::Yellow, RedScale);
		}
		else if (FlagState == CarriedObjectState::Held)
		{
			DrawFlagIcon(-46 * RedScale,53 * RedScale, 43,35, 843, 50, 43,35, FLinearColor::Blue, RedScale - RedHolderScaleModifier);
			if (CGS->GetFlagHolder(0) != nullptr && !CGS->GetFlagHolder(0)->PlayerName.IsEmpty())
			{
				DrawText(FText::FromString(CGS->GetFlagHolder(0)->PlayerName), -75* RedScale, 56 * RedScale, UTHUDOwner->TinyFont, RedScale,1.0, FLinearColor::White, ETextHorzPos::Right);
			}
		}

		DrawBasePosition(CGS->GetFlagBase(0), -46, 53, RedScale);

		// Draw the Blue Flag
		DrawFlagIcon(46 * BlueScale,53 * BlueScale, 43,41, 843, 87, 43,41, FLinearColor::Blue, BlueScale);
		FlagState = CGS->GetFlagState(1);
		float BlueHolderScaleModifier = 1.f;
		if (FlagState == CarriedObjectState::Held)
		{
			BluePulseScale += DeltaTime * 3.f;
			BlueHolderScaleModifier = 0.3f * FMath::Abs(FMath::Sin(BluePulseScale));
		}
		else
		{
			BluePulseScale = 0.f;
		}

		if (FlagState == CarriedObjectState::Dropped)
		{
			DrawFlagIcon(46 * BlueScale,53 * BlueScale, 25,34, 894, 1, 25,34, FLinearColor::Yellow, BlueScale);
		}
		else if (FlagState == CarriedObjectState::Held)
		{
			DrawFlagIcon(46 * BlueScale,53 * BlueScale, 43,35, 843, 50, 43,35, FLinearColor::Red, RedScale - BlueHolderScaleModifier);
			if (CGS->GetFlagHolder(1) != nullptr && !CGS->GetFlagHolder(1)->PlayerName.IsEmpty())
			{
				DrawText(FText::FromString(CGS->GetFlagHolder(1)->PlayerName), 75* BlueScale, 56 * BlueScale, UTHUDOwner->TinyFont, BlueScale,1.0, FLinearColor::White);
			}
		}
		DrawBasePosition(CGS->GetFlagBase(1), 46, 53, BlueScale);
	}
}

void UUTHUDWidget_CTFScore::DrawBasePosition(AUTCTFFlagBase* Base, float CenterX, float CenterY, float Scale)
{
	if (Base && UTCharacterOwner)
	{
		FRotator Dir = (Base->GetActorLocation() - UTCharacterOwner->GetActorLocation()).Rotation();
		float Yaw = (Dir.Yaw - UTCharacterOwner->GetViewRotation().Yaw);
		DrawTexture(IconTexture, CenterX * Scale, CenterY * Scale, 52* Scale, 52 * Scale, 897,452,43,43, 1.0f, FLinearColor::White,FVector2D(0.5,0.5), Yaw);
	}
}

void UUTHUDWidget_CTFScore::DrawFlagIcon(float CenterX, float CenterY,  float Width, float Height, float U, float V, float UL, float VL, FLinearColor DrawColor, float Scale)
{
	// NOTE: the 0.5x are because I scaled up the source art so it didn't look bad.
	DrawTexture(IconTexture, CenterX - (Width * Scale * 0.5f), CenterY - (Height * Scale * 0.5f), Width * Scale, Height * Scale, U,V,UL,VL,1.f, DrawColor);
}

