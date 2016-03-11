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

const float DEFAULT_SWAP_TIME = 10.0f;

AUTGauntletFlag::AUTGauntletFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bTeamPickupSendsHome = false;
	bAnyoneCanPickup = true;
	TimeUntilTeamSwitch = 5;
	bTeamLocked = false;

	WeightSpeedPctModifier = 0.85f;

}

void AUTGauntletFlag::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTGauntletFlag, TimeUntilTeamSwitch);
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
		AUTPlayerState* LastHoldingPS = Cast<AUTPlayerState>(LastHoldingPawn->PlayerState);
		if (LastHoldingPS)
		{
			LastHoldingPS->bSpecialTeamPlayer = false;
		}

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
	if (Holder) Holder->bSpecialTeamPlayer = true;
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
	if (bPendingTeamSwitch)
	{
		float XL, YL;

		float Scale = Canvas->ClipX / 1920;

		UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->MediumFont;
		FString Text = FString::Printf(TEXT("%i"), SwapTimer);
		Canvas->TextSize(TinyFont, *Text, XL, YL, Scale, Scale);

		FVector ScreenPosition = Canvas->Project(GetActorLocation() + FVector(0, 0, 15.f));
		float XPos = ScreenPosition.X - (XL * 0.5);
		if (XPos < Canvas->ClipX || XPos + XL < 0.0f)
		{
			FCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos), FMath::TruncToFloat(Canvas->OrgY + ScreenPosition.Y - YL)), FText::FromString(Text), TinyFont, FLinearColor::White);
			TextItem.Scale = FVector2D(Scale, Scale);
			TextItem.BlendMode = SE_BLEND_Translucent;
			TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
			Canvas->DrawItem(TextItem);
		}
	}

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
		int32 DefaultSwapTime = GauntletGameState ? GauntletGameState->FlagSwapTime : DEFAULT_SWAP_TIME;

		if (ObjectState == CarriedObjectState::Dropped)
		{
			if (bPendingTeamSwitch)
			{
				ActualSwapTimer -= DeltaSeconds;
				if (ActualSwapTimer < 0)
				{
					TeamSwap();
				}
			}
		}
		else if (ObjectState == CarriedObjectState::Held)
		{
			if (ActualSwapTimer < DEFAULT_SWAP_TIME)
			{
				ActualSwapTimer = FMath::Clamp<float>(ActualSwapTimer + DeltaSeconds, 0.0f, DefaultSwapTime);
			}
		}
		else if (ObjectState == CarriedObjectState::Home)
		{
			ActualSwapTimer = DEFAULT_SWAP_TIME;
		}

		SwapTimer = FMath::Clamp<int32>(int32(ActualSwapTimer + 1), 0, int32(DefaultSwapTime));
	}
}

void AUTGauntletFlag::SetTeam(AUTTeamInfo* NewTeam)
{
	Super::SetTeam(NewTeam);

	// Fake the replication
	if (Role == ROLE_Authority)
	{
		AUTGauntletGame* Game = GetWorld()->GetAuthGameMode<AUTGauntletGame>();
		if (Game && NewTeam)
		{
			OnRep_Team();

			// Notify the game.
			Game->FlagTeamChanged(NewTeam->GetTeamNum());
		}
	}
}

void AUTGauntletFlag::TeamSwap()
{
	AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	int32 DefaultSwapTime = GauntletGameState ? GauntletGameState->FlagSwapTime : DEFAULT_SWAP_TIME;

	bPendingTeamSwitch = false;
	ActualSwapTimer = DefaultSwapTime;

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	AUTGauntletGame* Game = GetWorld()->GetAuthGameMode<AUTGauntletGame>();

	uint8 NewTeamNum = 1 - GetTeamNum();
	if (Game && GameState && GameState->Teams.IsValidIndex(NewTeamNum))
	{
		// Before we can swap teams, look to see if the new team has any players left alive or with lives remaining

		if (Game->CanFlagTeamSwap(NewTeamNum))
		{
			PlayCaptureEffect();
			SetTeam(GameState->Teams[NewTeamNum]);
		}
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
