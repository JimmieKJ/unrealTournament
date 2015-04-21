// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTHUDWidget_CTFFlagStatus.h"
#include "UTCTFGameState.h"

UUTHUDWidget_CTFFlagStatus::UUTHUDWidget_CTFFlagStatus(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	YouHaveFlagText = NSLOCTEXT("CTFScore","YouHaveFlagText","You have the flag, return to base!");
	EnemyHasFlagText = NSLOCTEXT("CTFScore","EnemyHasFlagText","The enemy has your flag, recover it!");
	BothFlagsText = NSLOCTEXT("CTFScore","BothFlagsText","You have the enemy flag - hold it until your flag is returned!");

	ScreenPosition = FVector2D(0.5f, 0.0f);
	Size = FVector2D(0.f, 0.f);
	Origin = FVector2D(0.5f, 0.5f);
	AnimationAlpha = 0.0f;
	StatusScale = 1.f;
	InWorldAlpha = 0.5f;
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
	if (bStatusDir)
	{
		StatusScale += DeltaTime;
		bStatusDir = (StatusScale < 1.f);
	}
	else
	{
		StatusScale -= DeltaTime;
		bStatusDir = (StatusScale < 0.7f);
	}
	FVector ViewPoint;
	FRotator ViewRotation;
	UTPlayerOwner->GetPlayerViewPoint(ViewPoint, ViewRotation);

	for (int32 Team=0;Team<2;Team++)
	{
		AUTCTFFlagBase* Base = GS->GetFlagBase(Team);
		bool bDrawInWorld = false;
		bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
		bool bIsEnemyFlag = !GS->OnSameTeam(Base, UTPlayerOwner);
		FVector ScreenPosition(0.f);
		if (Base && Base->GetCarriedObject() && ((ViewRotation.Vector() | (Base->GetCarriedObject()->GetActorLocation() - ViewPoint)) > 0.f))
		{
			AUTCarriedObject* Flag = Base->GetCarriedObject();
			ScreenPosition = GetCanvas()->Project(Flag->GetActorLocation() + FVector(0.f, 0.f, Flag->Collision->GetUnscaledCapsuleHalfHeight() * 2.f));
			bDrawInWorld = (ScreenPosition.X < GetCanvas()->ClipX) && (ScreenPosition.X > 0.f) && (ScreenPosition.Y < GetCanvas()->ClipY) && (ScreenPosition.Y > 0.f);
			ScreenPosition.X -= RenderPosition.X;
			ScreenPosition.Y -= RenderPosition.Y;
		}

		RenderObj_Texture(CircleSlate[Team]);
		RenderObj_Texture(CircleBorder[Team]);

		float X = CircleSlate[Team].Position.X;
		float Y = CircleSlate[Team].Position.Y;
		FlagIconTemplate.RenderColor = Team == 0 ? RedColor : BlueColor;

		FName FlagState = GS->GetFlagState(Team);
		if (FlagState == CarriedObjectState::Held)
		{
			TakenIconTemplate.RenderColor = Team == 0 ? BlueColor : RedColor;
			TakenIconTemplate.RenderColor.R *= FMath::Square(StatusScale) - 0.25f;
			TakenIconTemplate.RenderColor.B *= FMath::Square(StatusScale) - 0.25f;
			RenderObj_TextureAt(TakenIconTemplate, X + 0.1f * FlagIconTemplate.GetWidth(), Y + 0.1f * FlagIconTemplate.GetHeight(), 1.1f * StatusScale * TakenIconTemplate.GetWidth(), 1.1f * StatusScale * TakenIconTemplate.GetHeight());
			if (bDrawInWorld && (bSpectating || bIsEnemyFlag))
			{
				TakenIconTemplate.RenderOpacity = InWorldAlpha;
				RenderObj_TextureAt(TakenIconTemplate, ScreenPosition.X, ScreenPosition.Y, 1.1f * TakenIconTemplate.GetWidth(), 1.1f * TakenIconTemplate.GetHeight());
			}
			AUTPlayerState* Holder = GS->GetFlagHolder(Team);
			if (Holder)
			{
				FlagHolderNames[Team].Text = FText::FromString(Holder->PlayerName);
				RenderObj_Text(FlagHolderNames[Team]);
			}
			float CarriedX = X - 0.25f * FlagIconTemplate.GetWidth();
			float CarriedY = Y - 0.25f * FlagIconTemplate.GetHeight();
			RenderObj_TextureAt(FlagIconTemplate, CarriedX, CarriedY, StatusScale * FlagIconTemplate.GetWidth(), StatusScale * FlagIconTemplate.GetHeight());
			if (bDrawInWorld && (bSpectating || bIsEnemyFlag))
			{
				float OldAlpha = FlagIconTemplate.RenderOpacity;
				FlagIconTemplate.RenderOpacity = InWorldAlpha;
				RenderObj_TextureAt(FlagIconTemplate, ScreenPosition.X - 0.25f * FlagIconTemplate.GetWidth(), ScreenPosition.Y - 0.25f * FlagIconTemplate.GetHeight(), FlagIconTemplate.GetWidth(), FlagIconTemplate.GetHeight());
				FlagIconTemplate.RenderOpacity = OldAlpha;
			}
		}
		else
		{
			RenderObj_TextureAt(FlagIconTemplate, X, Y, FlagIconTemplate.GetWidth(), FlagIconTemplate.GetHeight());
			if (bDrawInWorld && (bSpectating || bIsEnemyFlag || (FlagState == CarriedObjectState::Home)))
			{
				float OldAlpha = FlagIconTemplate.RenderOpacity;
				FlagIconTemplate.RenderOpacity = InWorldAlpha;
				RenderObj_TextureAt(FlagIconTemplate, ScreenPosition.X, ScreenPosition.Y, FlagIconTemplate.GetWidth(), FlagIconTemplate.GetHeight());
				FlagIconTemplate.RenderOpacity = OldAlpha;
			}
			if (FlagState == CarriedObjectState::Dropped)
			{
				RenderObj_TextureAt(DroppedIconTemplate, X, Y, DroppedIconTemplate.GetWidth(), DroppedIconTemplate.GetHeight());
				if (bDrawInWorld && (bSpectating || bIsEnemyFlag))
				{
					float OldAlpha = DroppedIconTemplate.RenderOpacity;
					DroppedIconTemplate.RenderOpacity = InWorldAlpha;
					RenderObj_TextureAt(DroppedIconTemplate, ScreenPosition.X, ScreenPosition.Y, DroppedIconTemplate.GetWidth(), DroppedIconTemplate.GetHeight());
					DroppedIconTemplate.RenderOpacity = OldAlpha;
				}
			}
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