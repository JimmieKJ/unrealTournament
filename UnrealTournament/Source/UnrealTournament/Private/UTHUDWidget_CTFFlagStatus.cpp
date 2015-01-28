// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTCTFGameState.h"

UUTHUDWidget_CTFFlagStatus::UUTHUDWidget_CTFFlagStatus(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	YouHaveFlagText = NSLOCTEXT("CTFScore","YouHaveFlagText","You have the flag, return to base!");
	EnemyHasFlagText = NSLOCTEXT("CTFScore","EnemyHasFlagText","The enemy has your flag, recover it!");
	BothFlagsText = NSLOCTEXT("CTFScore","BothFlagsText","Hold enemy flag until your flag is returned!");

	static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("Font'/Game/RestrictedAssets/Fonts/fntAmbex36.fntAmbex36'"));
	MessageFont = Font.Object;

	ScreenPosition = FVector2D(0.5f, 0.0f);
	Size = FVector2D(0,0);
	Origin = FVector2D(0.5f,0.5f);
	AnimationAlpha = 0.0f;
}

void UUTHUDWidget_CTFFlagStatus::InitializeWidget(AUTHUD* Hud)
{
	Super::InitializeWidget(Hud);
}

void UUTHUDWidget_CTFFlagStatus::Draw_Implementation(float DeltaTime)
{
	AUTCTFGameState* GS = Cast<AUTCTFGameState>(UTGameState);
	if (GS == NULL) return;

	FLinearColor RedColor = (GS->Teams.Num() > 1 && GS->Teams[0]) ? GS->Teams[0]->TeamColor : FLinearColor::Red;
	FLinearColor BlueColor = (GS->Teams.Num() > 1 && GS->Teams[1]) ? GS->Teams[1]->TeamColor : FLinearColor::Blue;

	for (int32 Team=0;Team<2;Team++)
	{
		// Draw Blue Flag

		RenderObj_Texture(CircleSlate[Team]);
		RenderObj_Texture(CircleBorder[Team]);

		float X = CircleSlate[Team].Position.X;
		float Y = CircleSlate[Team].Position.Y;
		FlagIconTemplate.RenderColor = Team == 0 ? RedColor : BlueColor;
		RenderObj_TextureAt(FlagIconTemplate, X, Y,FlagIconTemplate.GetWidth(), FlagIconTemplate.GetHeight());

		FName FlagState = GS->GetFlagState(Team);

		if (FlagState == CarriedObjectState::Dropped)
		{
			RenderObj_TextureAt(DroppedIconTemplate, X, Y,DroppedIconTemplate.GetWidth(), DroppedIconTemplate.GetHeight());
		}
		else if (FlagState == CarriedObjectState::Held)
		{
			TakenIconTemplate.RenderColor = Team == 0 ? BlueColor : RedColor;
			RenderObj_TextureAt(TakenIconTemplate, X, Y,TakenIconTemplate.GetWidth(), TakenIconTemplate.GetHeight());
			AUTPlayerState* Holder = GS->GetFlagHolder(Team);
			if (Holder)
			{
				FlagHolderNames[Team].Text = FText::FromString(Holder->PlayerName);
				RenderObj_Text(FlagHolderNames[Team]);
			}
		}

		AUTCTFFlagBase* Base = GS->GetFlagBase(Team);
		if (Base && UTPlayerOwner->GetPawn())
		{
			APawn* P = UTPlayerOwner->GetPawn();
			FRotator Dir = (Base->GetActorLocation() - P->GetActorLocation()).Rotation();
			float Yaw = (Dir.Yaw - P->GetViewRotation().Yaw);
			FlagArrowTemplate.RotPivot = FVector2D(0.5,0.5);
			FlagArrowTemplate.Rotation = Yaw;
			RenderObj_TextureAt(FlagArrowTemplate, X, Y, FlagArrowTemplate.GetWidth(), FlagArrowTemplate.GetHeight());
		}
	}

	// Draw the Flag Status Message
	if (GS->IsMatchInProgress() && UTHUDOwner != NULL && UTHUDOwner->PlayerOwner != NULL)
	{
		AUTPlayerState* OwnerPS = UTHUDOwner->UTPlayerOwner->UTPlayerState;
		if (OwnerPS != NULL && OwnerPS->Team != NULL)
		{
			uint8 MyTeamNum = OwnerPS->GetTeamNum();
			uint8 OtherTeamNum = MyTeamNum == 0 ? 1 : 0;
			FText StatusText = FText::GetEmpty();

			FLinearColor DrawColor = FLinearColor::Yellow;
			if (GS->GetFlagState(MyTeamNum) != CarriedObjectState::Home)	// My flag is out there
			{
				DrawColor = FLinearColor::Red;
				// Look to see if I have the enemy flag
				if (OwnerPS->CarriedObject != NULL && Cast<AUTCTFFlag>(OwnerPS->CarriedObject) != NULL)
				{
					StatusText = BothFlagsText;
				}
				else
				{
					StatusText = (GS->GetFlagState(MyTeamNum) == CarriedObjectState::Dropped) ? FText::GetEmpty() : EnemyHasFlagText;
				}
			}
			else if (OwnerPS->CarriedObject != NULL && Cast<AUTCTFFlag>(OwnerPS->CarriedObject) != NULL)
			{
				StatusText = YouHaveFlagText;
			}

			if (!StatusText.IsEmpty())
			{
				AnimationAlpha += (DeltaTime * 3);
		
				float Alpha = FMath::Sin(AnimationAlpha);
				Alpha = FMath::Abs<float>(Alpha);

				FlagStatusText.RenderOpacity = Alpha;
				FlagStatusText.RenderColor = DrawColor;
				FlagStatusText.Text = StatusText;
				FlagStatusText.bDrawShadow = true;
				FlagStatusText.ShadowDirection = FVector2D(1.f, 2.f);
				FlagStatusText.ShadowColor = FLinearColor::Black;
				RenderObj_Text(FlagStatusText);
			}
			else
			{
				AnimationAlpha = 0.0f;
			}
		}
	}	
}