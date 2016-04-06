// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTCarriedObject.h"
#include "UTCTFFlag.h"
#include "UTGauntletGame.h"
#include "UTGauntletGameState.h"
#include "UTGauntletFlag.h"
#include "UTCTFGameMessage.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMode.h"
#include "UTCTFRewardMessage.h"
#include "UnrealNetwork.h"

AUTGauntletFlag::AUTGauntletFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bTeamPickupSendsHome = false;
	bAnyoneCanPickup = true;
	bTeamLocked = false;

	WeightSpeedPctModifier = 0.85f;

}

void AUTGauntletFlag::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGauntletFlag, bTeamLocked);
	DOREPLIFETIME(AUTGauntletFlag, SwapTimer);
	DOREPLIFETIME(AUTGauntletFlag, bPendingTeamSwitch);
}


void AUTGauntletFlag::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void AUTGauntletFlag::OnRep_Team()
{
	// Change the material to represent the team

	if (Team != nullptr)
	{
		uint8 TeamNum = GetTeamNum();
		if ( TeamMaterials.IsValidIndex(TeamNum) )
		{
			UE_LOG(UT,Log,TEXT("Setting Material"));
			Mesh->SetMaterial(1, TeamMaterials[TeamNum]);
		}
	}
	else
	{
		UE_LOG(UT,Log,TEXT("Setting Neutral Material"));
		Mesh->SetMaterial(1, NeutralMaterial);
	}
}

void AUTGauntletFlag::NoLongerHeld(AController* InstigatedBy)
{
	Super::NoLongerHeld(InstigatedBy);

	if (LastHoldingPawn)
	{
		LastHoldingPawn->ResetMaxSpeedPctModifier();
	}

}

void AUTGauntletFlag::SetHolder(AUTCharacter* NewHolder)
{
	Super::SetHolder(NewHolder);

	// Set the team to match the team of the holder.

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	uint8 HolderTeamNum = NewHolder->GetTeamNum();
	if ( GameState && GameState->Teams.IsValidIndex(HolderTeamNum) )
	{
		uint8 FlagTeamNum = GetTeamNum();
		// If this was our flag, force it to switch teams.
		if (FlagTeamNum == 255)
		{
			uint8 NewTeamNum = HolderTeamNum;
			SetTeam(GameState->Teams[NewTeamNum]);
		}
		else
		{
			bTeamLocked = false;
		}
	}
	if (NewHolder) NewHolder->ResetMaxSpeedPctModifier();
}

void AUTGauntletFlag::MoveToHome()
{
	Super::MoveToHome();
	SetTeam(nullptr);
}

void AUTGauntletFlag::OnObjectStateChanged()
{
	AUTCarriedObject::OnObjectStateChanged();
	GetMesh()->ClothBlendWeight = (ObjectState == CarriedObjectState::Held) ? ClothBlendHeld : ClothBlendHome;

	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			if (ObjectState == CarriedObjectState::Dropped)
			{
				AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
				PC->MyHUD->AddPostRenderedActor(this);
			}
			else
			{
				PC->MyHUD->RemovePostRenderedActor(this);
			}
		}
	}
}

void AUTGauntletFlag::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	if (PC->GetPawn())
	{
		float Scale = Canvas->ClipY / 1080.0f;
		FVector2D Size = FVector2D(44,41) * Scale;

		AUTHUD* HUD = Cast<AUTHUD>(PC->MyHUD);
		FVector FlagLoc = GetActorLocation() + FVector(0.0f,0.0f,128.0f);
		FVector ScreenPosition = Canvas->Project(FlagLoc);

		FVector LookVec;
		FRotator LookDir;
		PC->GetPawn()->GetActorEyesViewPoint(LookVec,LookDir);

		if (HUD && FVector::DotProduct(LookDir.Vector().GetSafeNormal(), (FlagLoc - LookVec)) > 0.0f && 
			ScreenPosition.X > 0 && ScreenPosition.X < Canvas->ClipX && 
			ScreenPosition.Y > 0 && ScreenPosition.Y < Canvas->ClipY)
		{
			Canvas->SetDrawColor(255,255,255,255);
			Canvas->DrawTile(HUD->HUDAtlas, ScreenPosition.X - (Size.X * 0.5f), ScreenPosition.Y - Size.Y, Size.X, Size.Y,843,87,44,41);
		}

/*
		if (bPendingTeamSwitch)
		{
			float XL, YL;

			UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->MediumFont;
			FString Text = FString::Printf(TEXT("%i"), SwapTimer);
			Canvas->TextSize(TinyFont, *Text, XL, YL, Scale, Scale);

			FVector ScreenPosition = Canvas->Project(GetActorLocation() + FVector(0, 0, 15.f));
			PC->GetPawn()->GetActorEyesViewPoint(LookVec,LookDir);
			if (HUD && FVector::DotProduct(LookDir.Vector().GetSafeNormal(), (FlagLoc - LookVec)) > 0.0f && 
				ScreenPosition.X > 0 && ScreenPosition.X < Canvas->ClipX && 
				ScreenPosition.Y > 0 && ScreenPosition.Y < Canvas->ClipY)
			{
				FCanvasTextItem TextItem(FVector2D( FMath::TruncToFloat(ScreenPosition.X - (XL * 0.5f)), FMath::TruncToFloat(ScreenPosition.Y - (YL * 0.5f)) ), FText::FromString(Text), TinyFont, FLinearColor::White);
				TextItem.Scale = FVector2D(Scale, Scale);
				TextItem.BlendMode = SE_BLEND_Translucent;
				TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
				Canvas->DrawItem(TextItem);
			}
		}
*/
	}	
}


void AUTGauntletFlag::ChangeState(FName NewCarriedObjectState)
{
	Super::ChangeState(NewCarriedObjectState);
	if (Role == ROLE_Authority)
	{
		if (NewCarriedObjectState == CarriedObjectState::Dropped)
		{
			bPendingTeamSwitch = true;
		}
	}
}


void AUTGauntletFlag::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Role == ROLE_Authority)
	{
		AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
		if (GauntletGameState)
		{
			float DefaultSwapTime = float(GauntletGameState->FlagSwapTime);

			if (ObjectState == CarriedObjectState::Dropped)
			{
				if (bPendingTeamSwitch)
				{
					SwapTimer -= DeltaSeconds;
					if (SwapTimer <= 0)
					{
						TeamReset();
					}
				}
			}
			else if (ObjectState == CarriedObjectState::Held)
			{
				if (SwapTimer < DefaultSwapTime)
				{
					SwapTimer += DeltaSeconds;
				}
			}
			else if (ObjectState == CarriedObjectState::Home)
			{
				SwapTimer = DefaultSwapTime;
			}

			SwapTimer = FMath::Clamp<float>(SwapTimer, 0, DefaultSwapTime);
		}
	}
}

void AUTGauntletFlag::SetTeam(AUTTeamInfo* NewTeam)
{
	Super::SetTeam(NewTeam);

	UE_LOG(UT,Log,TEXT("SetTeam %s"), NewTeam == nullptr ? TEXT("NULL") : *FString::Printf(TEXT("%i"), NewTeam->GetTeamNum() ));

	// Fake the replication
	if (Role == ROLE_Authority)
	{
		AUTGauntletGame* Game = GetWorld()->GetAuthGameMode<AUTGauntletGame>();
		if (Game)
		{
			OnRep_Team();

			// Notify the game.
			Game->FlagTeamChanged(NewTeam ? NewTeam->GetTeamNum() : 255);
		}
	}
}

void AUTGauntletFlag::TeamReset()
{
	AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	if (GauntletGameState && ObjectState == CarriedObjectState::Dropped)
	{
		bPendingTeamSwitch = false;
		SwapTimer = float(GauntletGameState->FlagSwapTime);

		SetTeam(nullptr);
		PlayCaptureEffect();
	}
}

bool AUTGauntletFlag::CanBePickedUpBy(AUTCharacter* Character)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL && (!GS->IsMatchInProgress() || GS->IsMatchIntermission()))
	{
		return false;
	}
	else if (Character->IsRagdoll())
	{
		// check again later in case they get up
		GetWorldTimerManager().SetTimer(CheckTouchingHandle, this, &AUTCarriedObject::CheckTouching, 0.1f, false);
		return false;
	}
	else if (GetTeamNum() == 255 || GetTeamNum() == Character->GetTeamNum())
	{
		return true;
	}
	else
	{
		return false;
	}
}
