// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMode.h"
#include "UTPlayerState.h"
#include "Engine/LocalPlayer.h"
#include "Net/UnrealNetwork.h"
#include "UTGameMessage.h"

AUTPlayerState::AUTPlayerState(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWaitingPlayer = false;
	bReadyToPlay = false;
	LastKillTime = 0.0f;
	int32 Kills = 0;
	bOutOfLives = false;
	int32 Deaths = 0;

	// We want to be ticked.
	PrimaryActorTick.bCanEverTick = true;

	StatManager = nullptr;
	bWroteStatsToCloud = false;
	SkillRatingThisMatch = 0;
}

void AUTPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTPlayerState, CarriedObject);
	DOREPLIFETIME(AUTPlayerState, bWaitingPlayer);
	DOREPLIFETIME(AUTPlayerState, bReadyToPlay);
	DOREPLIFETIME(AUTPlayerState, bOutOfLives);
	DOREPLIFETIME(AUTPlayerState, Kills);
	DOREPLIFETIME(AUTPlayerState, Deaths);
	DOREPLIFETIME(AUTPlayerState, Team);
	DOREPLIFETIME(AUTPlayerState, FlagCaptures);
	DOREPLIFETIME(AUTPlayerState, FlagReturns);
	DOREPLIFETIME(AUTPlayerState, Assists);
	DOREPLIFETIME(AUTPlayerState, LastKillerPlayerState);
	DOREPLIFETIME(AUTPlayerState, bHasHighScore);
	DOREPLIFETIME(AUTPlayerState, ChatDestination);
	
	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceA, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceB, COND_OwnerOnly);
}

void AUTPlayerState::NotifyTeamChanged_Implementation()
{
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter* P = Cast<AUTCharacter>(*It);
		if (P != NULL && P->PlayerState == this)
		{
			P->NotifyTeamChanged();
		}
	}
}

void AUTPlayerState::SetWaitingPlayer(bool B)
{
	bIsSpectator = B;
	bWaitingPlayer = B;
	ForceNetUpdate();
}

void AUTPlayerState::IncrementKills(TSubclassOf<UDamageType> DamageType, bool bEnemyKill)
{
	if (bEnemyKill)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL && GetWorld()->TimeSeconds - LastKillTime < GS->MultiKillDelay)
		{
			ModifyStat(FName(*(TEXT("MultiKillLevel") + FString::FromInt(FMath::Min(MultiKillLevel, 3)))), 1, EStatMod::Delta);

			MultiKillLevel++;
			if (Cast<APlayerController>(GetOwner()) != NULL)
			{
				((APlayerController*)GetOwner())->ClientReceiveLocalizedMessage(GS->MultiKillMessageClass, MultiKillLevel - 1, this);
			}
		}
		else
		{
			MultiKillLevel = 0;
		}
		if (Cast<AController>(GetOwner()) != NULL && ((AController*)GetOwner())->GetPawn() != NULL)
		{
			Spree++;
			if (Spree % 5 == 0)
			{
				ModifyStat(FName(*(TEXT("SpreeKillLevel") + FString::FromInt(FMath::Min(Spree / 5 - 1, 4)))), 1, EStatMod::Delta);

				if (GetWorld()->GetAuthGameMode() != NULL)
				{
					GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / 5, this);
				}
			}
		}
		LastKillTime = GetWorld()->TimeSeconds;
		Kills++;

		ModifyStat(FName(TEXT("Kills")), 1, EStatMod::Delta);
		TSubclassOf<UUTDamageType> UTDamage(*DamageType);
		if (UTDamage && !UTDamage.GetDefaultObject()->StatsName.IsEmpty())
		{
			ModifyStat(FName(*(UTDamage.GetDefaultObject()->StatsName + TEXT("Kills"))), 1, EStatMod::Delta);
		}
	}
	else
	{
		ModifyStat(FName(TEXT("Suicides")), 1, EStatMod::Delta);
	}
}

void AUTPlayerState::IncrementDeaths(TSubclassOf<UDamageType> DamageType, AUTPlayerState* KillerPlayerState)
{
	Deaths += 1;

	ModifyStat(FName(TEXT("Deaths")), 1, EStatMod::Delta);
	TSubclassOf<UUTDamageType> UTDamage(*DamageType);
	if (UTDamage && !UTDamage.GetDefaultObject()->StatsName.IsEmpty())
	{
		ModifyStat(FName(*(UTDamage.GetDefaultObject()->StatsName + TEXT("Deaths"))), 1, EStatMod::Delta);
	}

	// spree has ended
	if (Spree >= 5 && GetWorld()->GetAuthGameMode() != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / -5, this, KillerPlayerState);
		}
	}
	Spree = 0;

	SetNetUpdateTime(FMath::Min(NetUpdateTime, GetWorld()->TimeSeconds + 0.3f * FMath::FRand()));

	if (Role == ROLE_Authority)
	{
		// Trigger it locally
		OnDeathsReceived();
	}
}

void AUTPlayerState::AdjustScore(int32 ScoreAdjustment)
{
	Score += ScoreAdjustment;
	ForceNetUpdate();
}

void AUTPlayerState::OnDeathsReceived()
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState != NULL)
	{
		RespawnTime = UTGameState->RespawnWaitTime;
		ForceRespawnTime = UTGameState->ForceRespawnTime;
	}
}

void AUTPlayerState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// If we are waiting to respawn then count down
	if (RespawnTime > 0.0f)
	{
		RespawnTime -= DeltaTime;
	}

	if (ForceRespawnTime > 0.0f)
	{
		ForceRespawnTime -= DeltaTime;
	}
}

/** Store an id for stats tracking.  Right now we are using the machine ID for this PC until we have 
    have a proper ID available.  */
void AUTPlayerState::ServerReceiveStatsID_Implementation(const FString& NewStatsID)
{
	StatsID = NewStatsID;

	ReadStatsFromCloud();	
}

bool AUTPlayerState::ServerReceiveStatsID_Validate(const FString& NewStatsID)
{
	return true;
}

void AUTPlayerState::ServerRequestChangeTeam_Implementation(uint8 NewTeamIndex)
{
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (Game != NULL && Game->bTeamGame)
	{
		AController* Controller =  Cast<AController>( GetOwner() );
		if (Controller != NULL)
		{
			if (NewTeamIndex == 255 && Team != NULL)
			{
				NewTeamIndex = (Team->TeamIndex + 1) % FMath::Max<uint8>(1, GetWorld()->GetGameState<AUTGameState>()->Teams.Num());
			}
			if (Game->ChangeTeam(Controller, NewTeamIndex, true))
			{
				AUTCharacter* Pawn = Cast<AUTCharacter>(Controller->GetPawn());
				if (Pawn != NULL)
				{
					Pawn->PlayerChangedTeam();
				}
				if (Team)
				{
					int32 Switch = (Team->TeamIndex == 0) ? 9 : 10;
					AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
					if (PC)
					{
						PC->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(), Switch, this, NULL, NULL);
					}
				}
			}
		}
	}
}
bool AUTPlayerState::ServerRequestChangeTeam_Validate(uint8 FireModeNum)
{
	return true;
}

void AUTPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL)
	{
		PS->Team = Team;
		PS->bWroteStatsToCloud = bWroteStatsToCloud;
		PS->StatsID = StatsID;
		PS->Kills = Kills;
		PS->Deaths = Deaths;
		PS->Assists = Assists;
		PS->StatManager = StatManager;
		if (PS->StatManager)
		{
			PS->StatManager->InitializeManager(PS);
		}
	}
}
void AUTPlayerState::OverrideWith(APlayerState* PlayerState)
{
	Super::OverrideWith(PlayerState);
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL)
	{
		// note that we don't need to call Add/RemoveFromTeam() here as that happened when Team was assigned to the passed in PlayerState
		Team = PS->Team;
	}
}

void AUTPlayerState::OnCarriedObjectChanged()
{
	SetCarriedObject(CarriedObject);
}

void AUTPlayerState::SetCarriedObject(AUTCarriedObject* NewCarriedObject)
{
	if (Role == ROLE_Authority)
	{
		CarriedObject = NewCarriedObject;
		ForceNetUpdate();
	}
}

void AUTPlayerState::ClearCarriedObject(AUTCarriedObject* OldCarriedObject)
{
	if (Role == ROLE_Authority)
	{
		if (CarriedObject == OldCarriedObject)
		{
			CarriedObject = NULL;
			ForceNetUpdate();
		}
	}
}

uint8 AUTPlayerState::GetTeamNum() const
{
	return (Team != NULL) ? Team->GetTeamNum() : 255;
}

void AUTPlayerState::EndPlay(const EEndPlayReason::Type Reason)
{
	if (!bIsInactive && Team != NULL && GetOwner() != NULL)
	{
		Team->RemoveFromTeam(Cast<AController>(GetOwner()));
	}

	Super::EndPlay(Reason);
}

void AUTPlayerState::BeginPlay()
{
	Super::BeginPlay();

	bool bFoundStatsId = false;
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
		OnlineUserCloudInterface = OnlineSubsystem->GetUserCloudInterface();

		if (Role == ROLE_Authority && OnlineUserCloudInterface.IsValid())
		{
			OnReadUserFileCompleteDelegate = FOnReadUserFileCompleteDelegate::CreateUObject(this, &AUTPlayerState::OnReadUserFileComplete);
			OnlineUserCloudInterface->AddOnReadUserFileCompleteDelegate(OnReadUserFileCompleteDelegate);

			OnWriteUserFileCompleteDelegate = FOnWriteUserFileCompleteDelegate::CreateUObject(this, &AUTPlayerState::OnWriteUserFileComplete);
			OnlineUserCloudInterface->AddOnWriteUserFileCompleteDelegate(OnWriteUserFileCompleteDelegate);
		}

		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (OnlineIdentityInterface.IsValid() && PC != nullptr)
		{
			ULocalPlayer* LP = Cast<ULocalPlayer>(PC->Player);
			if (LP != nullptr && OnlineIdentityInterface->GetLoginStatus(LP->ControllerId))
			{
				TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(LP->ControllerId);
				if (UserId.IsValid())
				{
					ServerReceiveStatsID(UserId->ToString());
					bFoundStatsId = true;
				}
			}
		}
	}
		
	if (Role == ROLE_Authority && StatManager == nullptr)
	{
		//Make me a statmanager
		StatManager = ConstructObject<UStatManager>(UStatManager::StaticClass(), this);
		StatManager->InitializeManager(this);
	}
}

bool AUTPlayerState::ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType)
{
	if (StatManager != nullptr)
	{
		return StatManager->ModifyStat(StatName, Amount, ModType);
	}

	return false;
}

bool AUTPlayerState::ServerNextChatDestination_Validate() { return true; }
void AUTPlayerState::ServerNextChatDestination_Implementation()
{
	AUTBaseGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTBaseGameMode>();
	if (GameMode)
	{
		ChatDestination = GameMode->GetNextChatDestination(this, ChatDestination);
	}
}

void AUTPlayerState::ReadStatsFromCloud()
{
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode == nullptr || GameMode->bDisableCloudStats)
	{
		return;
	}

	// Don't read stats from cloud if we've already written them, consider memory to be a valid representation of the stats
	if (!StatsID.IsEmpty() && OnlineUserCloudInterface.IsValid() && !bWroteStatsToCloud && !bOnlySpectator)
	{
		OnlineUserCloudInterface->ReadUserFile(FUniqueNetIdString(*StatsID), GetStatsFilename());
	}
}

void AUTPlayerState::OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	// this notification is for us
	if (bWasSuccessful && InUserId.ToString() == StatsID && FileName == GetStatsFilename())
	{
		UE_LOG(LogGameStats, Log, TEXT("OnReadUserFileComplete bWasSuccessful:%d %s %s"), int32(bWasSuccessful), *InUserId.ToString(), *FileName);

		TArray<uint8> FileContents;
		if (OnlineUserCloudInterface->GetFileContents(InUserId, FileName, FileContents))
		{
			if (FileContents.GetData()[FileContents.Num() - 1] != 0)
			{
				UE_LOG(LogGameStats, Warning, TEXT("Failed to get proper stats json"));
				return;
			}

			FString JsonString = ANSI_TO_TCHAR((char*)FileContents.GetData());

			TSharedPtr<FJsonObject> StatsJson;
			TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
			if (FJsonSerializer::Deserialize(JsonReader, StatsJson) && StatsJson.IsValid())
			{
				FString JsonStatsID;
				if (StatsJson->TryGetStringField(TEXT("StatsID"), JsonStatsID) && JsonStatsID == StatsID)
				{
					UE_LOG(LogGameStats, Log, TEXT("Stats ID matched, adding stats from the cloud to current stats"));
				}
				else
				{
					UE_LOG(LogGameStats, Warning, TEXT("Failed to find matching StatsID in valid stats read."));
				}

				StatManager->InsertDataFromJsonObject(StatsJson);

				SkillRatingThisMatch = StatManager->GetStatValueByName(FName((TEXT("SkillRating"))), EStatRecordingPeriod::Persistent);
			}
		}
	}
}

void AUTPlayerState::OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	// this notification is for us
	if (InUserId.ToString() == StatsID && FileName == GetStatsFilename())
	{
		UE_LOG(LogGameStats, Log, TEXT("OnWriteUserFileComplete bWasSuccessful:%d %s %s"), int32(bWasSuccessful), *InUserId.ToString(), *FileName);
		bWroteStatsToCloud = true;
	}
}

FString AUTPlayerState::GetStatsFilename()
{
	if (!StatsID.IsEmpty())
	{
		FString ProfileFilename = FString::Printf(TEXT("%s.user.stats"), *StatsID);
		return ProfileFilename;
	}

	return TEXT("local.user.stats");
}

void AUTPlayerState::WriteStatsToCloud()
{
	if (!StatsID.IsEmpty() && OnlineUserCloudInterface.IsValid() && StatManager != nullptr && !bOnlySpectator)
	{
		// We ended with this player name, save it in the stats
		StatManager->PreviousPlayerNames.AddUnique(PlayerName);
		if (StatManager->PreviousPlayerNames.Num() > StatManager->NumPreviousPlayerNamesToKeep)
		{
			StatManager->PreviousPlayerNames.RemoveAt(0, StatManager->PreviousPlayerNames.Num() - StatManager->NumPreviousPlayerNamesToKeep);
		}

		TArray<uint8> FileContents;
		TSharedPtr<FJsonObject> StatsJson = MakeShareable(new FJsonObject);
		StatsJson->SetStringField(TEXT("StatsID"), StatsID);
		StatsJson->SetStringField(TEXT("PlayerName"), PlayerName);
		StatManager->PopulateJsonObject(StatsJson);

		FString OutputJsonString;
		TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > Writer = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&OutputJsonString);
		FJsonSerializer::Serialize(StatsJson.ToSharedRef(), Writer);
		{
			FMemoryWriter MemoryWriter(FileContents);
			MemoryWriter.Serialize(TCHAR_TO_ANSI(*OutputJsonString), OutputJsonString.Len() + 1);
		}

		//UE_LOG(LogGameStats, Log, TEXT("%s"), *OutputJsonString);

		OnlineUserCloudInterface->WriteUserFile(FUniqueNetIdString(*StatsID), GetStatsFilename(), FileContents);
	}
}

void AUTPlayerState::AddMatchToStats(const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates)
{
	if (StatManager != nullptr && !StatsID.IsEmpty())
	{
		StatManager->AddMatchToStats(GameType, Teams, ActivePlayerStates, InactivePlayerStates);
	}
}

// Don't access the skill rating in the statmanager so UpdateSkillRating() can rely on this function
int32 AUTPlayerState::GetSkillRating()
{
	int32 SkillRating = SkillRatingThisMatch;
	
	// SkillRating was unset previously, return starting value
	if (SkillRating == 0)
	{
		SkillRating = 1500;
	}

	return SkillRating;
}

void AUTPlayerState::UpdateSkillRating(FName SkillStatName, AUTPlayerState *Opponent, bool bWonMatch)
{
	if (Opponent == nullptr)
	{
		UE_LOG(LogGameStats, Warning, TEXT("UpdateSkillRating No Opponent supplied"));
		return;
	}

	// Not writing stats for this player
	if (StatManager == nullptr || StatsID.IsEmpty())
	{
		return;
	}

	int32 SkillRating = GetSkillRating();
	int32 OpponentSkillRating = Opponent->GetSkillRating();
	float ExpectedWinPercentage = 1.0f / (1.0f + pow(10.0f, (float(OpponentSkillRating - SkillRating) / 400.0f)));

	UE_LOG(LogGameStats, Log, TEXT("UpdateSkillRating %s RA:%d RB:%d E:%f"), *PlayerName, SkillRating, OpponentSkillRating, ExpectedWinPercentage);

	// KFactor selection can be chosen many different ways, feel free to change it
	float KFactor = 32.0f;
	if (SkillRating > 2400)
	{
		KFactor = 16.0f;
	}
	else if (SkillRating >= 2100)
	{
		KFactor = 24.0f;
	}

	int32 NewSkillRating = 0;
	if (bWonMatch)
	{
		NewSkillRating = SkillRating + KFactor*(1.0f - ExpectedWinPercentage);
	}
	else
	{
		NewSkillRating = SkillRating + KFactor*(0.0f - ExpectedWinPercentage);
	}

	UE_LOG(LogGameStats, Log, TEXT("UpdateSkillRating %s New Skill Rating %d"), *PlayerName, NewSkillRating);
	ModifyStat(SkillStatName, NewSkillRating, EStatMod::Set);
}