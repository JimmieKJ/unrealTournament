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

	ScalingStartDist = 4000.f;
	ScalingEndDist = 15000.f;
	MaxIconScale = 0.8f;
	MinIconScale = 0.4f;
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
		// draw flag state in HUD
		float FlagStateX = CircleSlate[Team].Position.X;
		float FlagStateY = 8.f + 0.5f * FlagIconTemplate.GetHeight();
		FlagIconTemplate.RenderColor = Team == 0 ? RedColor : BlueColor;

		FName FlagState = GS->GetFlagState(Team);
		if (FlagState == CarriedObjectState::Held)
		{
			TakenIconTemplate.RenderColor = 0.25f * FLinearColor::White;
			RenderObj_TextureAt(TakenIconTemplate, FlagStateX + 0.1f * FlagIconTemplate.GetWidth(), FlagStateY + 0.1f * FlagIconTemplate.GetHeight(), 1.1f * StatusScale * TakenIconTemplate.GetWidth(), 1.1f * StatusScale * TakenIconTemplate.GetHeight());
			AUTPlayerState* Holder = GS->GetFlagHolder(Team);
			if (Holder)
			{
				FlagHolderNames[Team].Text = FText::FromString(Holder->PlayerName);
				RenderObj_Text(FlagHolderNames[Team]);
			}
			float CarriedX = FlagStateX - 0.25f * FlagIconTemplate.GetWidth();
			float CarriedY = FlagStateY - 0.25f * FlagIconTemplate.GetHeight();
			RenderObj_TextureAt(FlagIconTemplate, CarriedX, CarriedY, StatusScale * FlagIconTemplate.GetWidth(), StatusScale * FlagIconTemplate.GetHeight());
		}
		else
		{
			RenderObj_TextureAt(FlagIconTemplate, FlagStateX, FlagStateY, FlagIconTemplate.GetWidth(), FlagIconTemplate.GetHeight());
			if (FlagState == CarriedObjectState::Dropped)
			{
				RenderObj_TextureAt(DroppedIconTemplate, FlagStateX, FlagStateY, DroppedIconTemplate.GetWidth(), DroppedIconTemplate.GetHeight());
			}
		}

		AUTCTFFlagBase* Base = GS->GetFlagBase(Team);
		if (Base && Base->GetCarriedObject())
		{
			float Dist = (Base->GetActorLocation() - ViewPoint).Size();
			float WorldRenderScale = RenderScale * FMath::Clamp(MaxIconScale - (Dist - ScalingStartDist) / ScalingEndDist, MinIconScale, MaxIconScale);

			bScaleByDesignedResolution = false;
			bool bSpectating = UTPlayerOwner->PlayerState && UTPlayerOwner->PlayerState->bOnlySpectator;
			bool bIsEnemyFlag = !GS->OnSameTeam(Base, UTPlayerOwner);
			AUTCharacter* Holder = NULL;

			// Draw flag base in world
			bool bDrawInWorld = false;
			FVector ScreenPosition(0.f);
			AUTCarriedObject* Flag = Base->GetCarriedObject();
			FVector WorldPosition = Base->GetActorLocation() + Base->GetActorRotation().RotateVector(Flag->HomeBaseOffset) + FVector(0.f, 0.f, Flag->Collision->GetUnscaledCapsuleHalfHeight() * 3.f);
			float OldFlagAlpha = FlagIconTemplate.RenderOpacity;
			float CurrentWorldAlpha = InWorldAlpha;
			if ((ViewRotation.Vector() | (Base->GetActorLocation() - ViewPoint)) > 0.f)
			{
				ScreenPosition = GetCanvas()->Project(WorldPosition);
				bDrawInWorld = (ScreenPosition.X < GetCanvas()->ClipX) && (ScreenPosition.X > 0.f) && (ScreenPosition.Y < GetCanvas()->ClipY) && (ScreenPosition.Y > 0.f);
			}

			if (bDrawInWorld)
			{
				float PctFromCenter = (ScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
				CurrentWorldAlpha = InWorldAlpha * FMath::Min(0.15f/WorldRenderScale + 12.f*PctFromCenter, 1.f);

				ScreenPosition.X -= RenderPosition.X;
				ScreenPosition.Y -= RenderPosition.Y;

				FlagIconTemplate.RenderOpacity = CurrentWorldAlpha;
				CircleBorder[Team].RenderOpacity = CurrentWorldAlpha;
				CircleSlate[Team].RenderOpacity = CurrentWorldAlpha;
				RenderObj_TextureAt(CircleSlate[Team], ScreenPosition.X, ScreenPosition.Y, CircleSlate[Team].GetWidth()* WorldRenderScale, CircleSlate[Team].GetHeight()* WorldRenderScale);
				RenderObj_TextureAt(CircleBorder[Team], ScreenPosition.X, ScreenPosition.Y, CircleBorder[Team].GetWidth()* WorldRenderScale, CircleBorder[Team].GetHeight()* WorldRenderScale);
				RenderObj_TextureAt(FlagIconTemplate, ScreenPosition.X, ScreenPosition.Y, FlagIconTemplate.GetWidth()* WorldRenderScale, FlagIconTemplate.GetHeight()* WorldRenderScale);
				if (FlagState != CarriedObjectState::Home)
				{
					RenderObj_TextureAt(FlagGoneIconTemplate, ScreenPosition.X, ScreenPosition.Y, FlagGoneIconTemplate.GetWidth()* WorldRenderScale, FlagGoneIconTemplate.GetHeight()* WorldRenderScale);
				}
			}

			// Draw flag state in world
			bDrawInWorld = false;
			Dist = (Base->GetCarriedObject()->GetActorLocation() - ViewPoint).Size();
			WorldRenderScale = RenderScale * FMath::Clamp(MaxIconScale - (Dist - ScalingStartDist) / ScalingEndDist, MinIconScale, MaxIconScale);
			if ((bSpectating || bIsEnemyFlag) && (Flag->Holder != UTPlayerOwner->PlayerState) && (FlagState != CarriedObjectState::Home) && ((ViewRotation.Vector() | (Base->GetCarriedObject()->GetActorLocation() - ViewPoint)) > 0.f))
			{
				WorldPosition = Flag->GetActorLocation();
				if (FlagState == CarriedObjectState::Held)
				{
					Holder = Cast<AUTCharacter>(Flag->AttachmentReplication.AttachParent);
					if (Holder)
					{
						WorldPosition = Holder->GetMesh()->GetComponentLocation() + FVector(0.f, 0.f, Holder->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.25f);
					}
				}
				else
				{
					WorldPosition += FVector(0.f, 0.f, Flag->Collision->GetUnscaledCapsuleHalfHeight() * 0.75f);
				}
				ScreenPosition = GetCanvas()->Project(WorldPosition);
				bDrawInWorld = (ScreenPosition.X < GetCanvas()->ClipX) && (ScreenPosition.X > 0.f) && (ScreenPosition.Y < GetCanvas()->ClipY) && (ScreenPosition.Y > 0.f);
			}

			if (bDrawInWorld)
			{
				float PctFromCenter = (ScreenPosition - FVector(0.5f*GetCanvas()->ClipX, 0.5f*GetCanvas()->ClipY, 0.f)).Size() / GetCanvas()->ClipX;
				CurrentWorldAlpha = InWorldAlpha * FMath::Min(0.15f/WorldRenderScale + 12.f*PctFromCenter, 1.f);

				ScreenPosition.X -= RenderPosition.X;
				ScreenPosition.Y -= RenderPosition.Y;
				float ViewDist = (ViewPoint - WorldPosition).Size();

				// don't overlap player beacon
				UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
				float X, Y;
				float Scale = Canvas->ClipX / 1920.f;
				Canvas->TextSize(TinyFont, FString("+999   A999"), X, Y, Scale, Scale);
				if (!Holder || (ViewDist < Holder->TeamPlayerIndicatorMaxDistance))
				{
					ScreenPosition.Y -= 3.5f*Y;
				}
				else
				{
					ScreenPosition.Y -= (ViewDist < Holder->SpectatorIndicatorMaxDistance) ? 2.5f*Y : 1.5f*Y;
				}

				FlagIconTemplate.RenderOpacity = CurrentWorldAlpha;
				CircleBorder[Team].RenderOpacity = CurrentWorldAlpha;
				CircleSlate[Team].RenderOpacity = CurrentWorldAlpha;
				RenderObj_TextureAt(CircleSlate[Team], ScreenPosition.X, ScreenPosition.Y, CircleSlate[Team].GetWidth()* WorldRenderScale, CircleSlate[Team].GetHeight()* WorldRenderScale);
				RenderObj_TextureAt(CircleBorder[Team], ScreenPosition.X, ScreenPosition.Y, CircleBorder[Team].GetWidth()* WorldRenderScale, CircleBorder[Team].GetHeight()* WorldRenderScale);
				if (FlagState == CarriedObjectState::Held)
				{
					TakenIconTemplate.RenderOpacity = CurrentWorldAlpha;
					RenderObj_TextureAt(TakenIconTemplate, ScreenPosition.X, ScreenPosition.Y, 1.1f * TakenIconTemplate.GetWidth()* WorldRenderScale, 1.1f * TakenIconTemplate.GetHeight()* WorldRenderScale);
					RenderObj_TextureAt(FlagIconTemplate, ScreenPosition.X - 0.25f * FlagIconTemplate.GetWidth()* WorldRenderScale, ScreenPosition.Y - 0.25f * FlagIconTemplate.GetHeight()* WorldRenderScale, FlagIconTemplate.GetWidth()* WorldRenderScale, FlagIconTemplate.GetHeight()* WorldRenderScale);
				}
				else
				{
					RenderObj_TextureAt(FlagIconTemplate, ScreenPosition.X, ScreenPosition.Y, FlagIconTemplate.GetWidth()* WorldRenderScale, FlagIconTemplate.GetHeight()* WorldRenderScale);

					if (FlagState == CarriedObjectState::Dropped)
					{
						float DroppedAlpha = DroppedIconTemplate.RenderOpacity;
						DroppedIconTemplate.RenderOpacity = CurrentWorldAlpha;
						RenderObj_TextureAt(DroppedIconTemplate, ScreenPosition.X, ScreenPosition.Y, DroppedIconTemplate.GetWidth()* WorldRenderScale, DroppedIconTemplate.GetHeight());
						DroppedIconTemplate.RenderOpacity = DroppedAlpha;
					}
				}
			}
			FlagIconTemplate.RenderOpacity = OldFlagAlpha;
			CircleBorder[Team].RenderOpacity = 1.f;
			CircleSlate[Team].RenderOpacity = 1.f;
			bScaleByDesignedResolution = true;
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