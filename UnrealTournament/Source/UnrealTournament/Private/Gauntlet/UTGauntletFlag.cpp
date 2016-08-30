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
	static ConstructorHelpers::FObjectFinder<UClass> GFClass(TEXT("Blueprint'/Game/RestrictedAssets/Proto/UT3_Pickups/Flag/GhostFlag.GhostFlag_C'"));
	GhostFlagClass = GFClass.Object;
	bSingleGhostFlag = false;
	bTeamPickupSendsHome = false;
	bAnyoneCanPickup = true;
	WeightSpeedPctModifier = 1.0f;
}

void AUTGauntletFlag::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

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
		UE_LOG(UT,Verbose,TEXT("[UTGauntletFlag::OnRep_Team]  Team = %i (%s)"), TeamNum, *Team->GetName());

		if ( TeamMaterials.IsValidIndex(TeamNum) )
		{
			UE_LOG(UT,Log,TEXT("Setting Material"));
			Mesh->SetMaterial(1, TeamMaterials[TeamNum]);
		}
	}
	else
	{
		UE_LOG(UT,Verbose,TEXT("[UTGauntletFlag::OnRep_Team]  Team = 255 (nullptr)"));

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
	FString DebugMsg = TEXT("[UTGauntletFlag::SetHolder] ");

	// Set the team to match the team of the holder.
	if (NewHolder)
	{
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		uint8 HolderTeamNum = NewHolder->GetTeamNum();
		if ( GameState && GameState->Teams.IsValidIndex(HolderTeamNum) )
		{
			uint8 FlagTeamNum = GetTeamNum();
			DebugMsg += FString::Printf(TEXT("Holder = %s Team = %i FlagTeamNum = %i"), (NewHolder->PlayerState ? *NewHolder->PlayerState->PlayerName : TEXT("<nullptr>")), HolderTeamNum, FlagTeamNum );

			// If this was our flag, force it to switch teams.
			if (FlagTeamNum == 255)
			{
				uint8 NewTeamNum = HolderTeamNum;
				SetTeam(GameState->Teams[NewTeamNum]);
	
				DebugMsg += FString::Printf(TEXT(" TeamInfo: %s"), GameState->Teams[NewTeamNum] ? *GameState->Teams[NewTeamNum]->GetName() : TEXT("<nullptr>") );
			}
		}
		else
		{
			DebugMsg += FString::Printf(TEXT("No Game State or Invalid Team (%i vs %i)"), HolderTeamNum, GameState ? GameState->Teams.Num() : -1);
		}

		bPendingTeamSwitch = false;
	}
	else
	{
		DebugMsg += TEXT("No Holder");
	}

	// NOTE: The UTCarriedObject::SetHolder() clears the ghosts and we don't want this to happen since we control this
	// in SetTeam.  So we flag it to ignore the call.  Hacky, I know but hey, #PreAlpha!

	bIgnoreClearGhostCalls = true;
	Super::SetHolder(NewHolder);
	bIgnoreClearGhostCalls = false;

	if (NewHolder) 
	{
		NewHolder->ResetMaxSpeedPctModifier();
	}

	UE_LOG(UT,Verbose, TEXT("%s"),*DebugMsg);

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
				AUTGauntletGameState* SCTFGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
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
/*
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
*/
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
		AUTGauntletGameState* SCTFGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
		if (SCTFGameState)
		{
			float DefaultSwapTime = float(SCTFGameState->FlagSwapTime);

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

	UE_LOG(UT,Verbose,TEXT("[AUTGauntletFlag::SetTeam] %s"), NewTeam == nullptr ? TEXT("NULL") : *FString::Printf(TEXT("%i"), NewTeam->GetTeamNum() ));

	// Fake the replication
	if (Role == ROLE_Authority)
	{
		uint8 NewTeamNum = NewTeam == nullptr ? 255 : NewTeam->GetTeamNum();

		AUTGauntletGameState* GauntletGameState = GetWorld()->GetGameState<AUTGauntletGameState>();

		if (GauntletGameState != nullptr)
		{
			// If we are back to neutral, clear the flags and put 2 destination flags
			if (NewTeamNum == 255)
			{
				ClearGhostFlags();
				for (int32 i=0; i < 2; i++)
				{
					AUTCTFFlagBase* DestinationBase = GauntletGameState->GetFlagBase(i);
					if (DestinationBase)
					{
						PutGhostFlagAt(FFlagTrailPos(DestinationBase->GetActorLocation() + FVector(0.0f, 0.0f, 64.0f)), false, true, i);
					}
				}
			}
			else
			{
				ClearGhostFlags();
				bFriendlyCanPickup = true;
				bAnyoneCanPickup = false;
				AUTCTFFlagBase* DestinationBase = GauntletGameState->GetFlagBase(1 - NewTeamNum);
				if (DestinationBase)
				{
					PutGhostFlagAt(FFlagTrailPos(DestinationBase->GetActorLocation() + FVector(0.0f, 0.0f, 64.0f)), false, true, NewTeamNum);
				}
			}
		}

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
	AUTGauntletGameState* SCTFGameState = GetWorld()->GetGameState<AUTGauntletGameState>();
	if (SCTFGameState && ObjectState == CarriedObjectState::Dropped)
	{

		bFriendlyCanPickup = false;
		bAnyoneCanPickup = true;

		bPendingTeamSwitch = false;
		SwapTimer = float(SCTFGameState->FlagSwapTime);

		SetTeam(nullptr);
		PlayCaptureEffect();
	}
}


FText AUTGauntletFlag::GetHUDStatusMessage(AUTHUD* HUD)
{
	if (GetTeamNum() == 255) // Anyone can pick this up
	{
		return NSLOCTEXT("UTGauntletFlag","AvailableForPickup","Grab the Flag");
	}
	if (bPendingTeamSwitch)
	{
		return NSLOCTEXT("UTGauntletFlag","Neutral","Becoming Neutral in");
	}
	return FText::GetEmpty();
}

void AUTGauntletFlag::ClearGhostFlags()
{
	if (!bIgnoreClearGhostCalls) Super::ClearGhostFlags();
}
