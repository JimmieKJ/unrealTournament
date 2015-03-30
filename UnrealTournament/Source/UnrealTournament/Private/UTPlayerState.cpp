// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameMode.h"
#include "UTPlayerState.h"
#include "Engine/LocalPlayer.h"
#include "Net/UnrealNetwork.h"
#include "UTGameMessage.h"
#include "UTHat.h"
#include "UTCharacterContent.h"

AUTPlayerState::AUTPlayerState(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWaitingPlayer = false;
	bReadyToPlay = false;
	bPendingTeamSwitch = false;
	LastKillTime = 0.0f;
	int32 Kills = 0;
	bOutOfLives = false;
	int32 Deaths = 0;

	// We want to be ticked.
	PrimaryActorTick.bCanEverTick = true;

	StatManager = nullptr;
	bWroteStatsToCloud = false;
	DuelSkillRatingThisMatch = 0;
	TDMSkillRatingThisMatch = 0;
}

void AUTPlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTPlayerState, CarriedObject);
	DOREPLIFETIME(AUTPlayerState, bWaitingPlayer);
	DOREPLIFETIME(AUTPlayerState, bReadyToPlay);
	DOREPLIFETIME(AUTPlayerState, bPendingTeamSwitch);
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
	DOREPLIFETIME(AUTPlayerState, CountryFlag);
	DOREPLIFETIME(AUTPlayerState, AverageRank);
	DOREPLIFETIME(AUTPlayerState, SelectedCharacter);
	DOREPLIFETIME(AUTPlayerState, TauntClass);
	
	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceA, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, RespawnChoiceB, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, bChosePrimaryRespawnChoice, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerState, WeaponSpreeDamage, COND_OwnerOnly);
}

void AUTPlayerState::SetPlayerName(const FString& S)
{
	Super::SetPlayerName(S);
	bHasValidClampedName = false;
}

void AUTPlayerState::UpdatePing(float InPing)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC && !PC->GetPawn() && (ExactPing > 0.f) && (InPing > 0.001f * ExactPing))
	{
		// ping not valid when not doing ServerMoves
		CurPingBucketTimestamp = GetWorld()->GetTimeSeconds();
		return;
	}
	Super::UpdatePing(InPing);
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
		AController* Controller = Cast<AController>(GetOwner());
		APawn* Pawn = nullptr;
		AUTCharacter* UTChar = nullptr;
		if (Controller)
		{
			Pawn = Controller->GetPawn();
			UTChar = Cast<AUTCharacter>(Pawn);
		}

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
		if (Pawn != NULL)
		{
			Spree++;
			if (Spree % 5 == 0)
			{
				ModifyStat(FName(*(TEXT("SpreeKillLevel") + FString::FromInt(FMath::Min(Spree / 5 - 1, 4)))), 1, EStatMod::Delta);

				if (GetWorld()->GetAuthGameMode() != NULL)
				{
					GetWorld()->GetAuthGameMode()->BroadcastLocalized(GetOwner(), GS->SpreeMessageClass, Spree / 5, this);
				}

				if (UTChar != NULL && UTChar->IsWearingAnyCosmetic())
				{
					UTChar->CosmeticSpreeCount = FMath::Min(Spree / 5, 4);
					UTChar->OnRepCosmeticSpreeCount();
				}
			}
		}

		if (UTChar != NULL && UTChar->IsWearingAnyCosmetic())
		{
			UTChar->LastCosmeticFlashTime = GetWorld()->TimeSeconds;
			UTChar->CosmeticFlashCount++;
			UTChar->OnRepCosmeticFlashCount();
		}

		LastKillTime = GetWorld()->TimeSeconds;
		Kills++;

		ModifyStat(FName(TEXT("Kills")), 1, EStatMod::Delta);
		TSubclassOf<UUTDamageType> UTDamage(*DamageType);
		if (UTDamage)
		{
			if (!UTDamage.GetDefaultObject()->StatsName.IsEmpty())
			{
				ModifyStat(FName(*(UTDamage.GetDefaultObject()->StatsName + TEXT("Kills"))), 1, EStatMod::Delta);
			}
			if (UTDamage.GetDefaultObject()->SpreeSoundName != NAME_None)
			{
				int32 SpreeIndex = -1;
				for (int32 i = 0; i < WeaponSprees.Num(); i++)
				{
					if (WeaponSprees[i].SpreeSoundName == UTDamage.GetDefaultObject()->SpreeSoundName)
					{
						SpreeIndex = i;
						break;
					}
				}
				if (SpreeIndex == -1)
				{
					new(WeaponSprees)FWeaponSpree(UTDamage.GetDefaultObject()->SpreeSoundName);
					SpreeIndex = WeaponSprees.Num() - 1;
				}

				WeaponSprees[SpreeIndex].Kills++;
				if (WeaponSprees[SpreeIndex].Kills == UTDamage.GetDefaultObject()->WeaponSpreeCount)
				{
					AnnounceWeaponSpree(SpreeIndex, UTDamage);
				}
				// more likely to kill again with same weapon, so shorten search through array by swapping
				WeaponSprees.Swap(0, SpreeIndex);
			}
		}
	}
	else
	{
		ModifyStat(FName(TEXT("Suicides")), 1, EStatMod::Delta);
	}
}

void AUTPlayerState::AnnounceWeaponSpree(int32 SpreeIndex, TSubclassOf<UUTDamageType> UTDamage)
{
	// will be replicated to owning player, causing OnWeaponSpreeDamage()  // FIXMESTEVE back to replicated function not property?
	WeaponSpreeDamage = UTDamage;
	
	// for standalone
	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
	if (MyPC && MyPC->IsLocalPlayerController())
	{
		OnWeaponSpreeDamage();
	}
}

void AUTPlayerState::OnWeaponSpreeDamage()
{
	// received replicated 
	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetOwner());
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (MyPC && GS && WeaponSpreeDamage)
	{
		MyPC->ClientReceiveLocalizedMessage(GS->SpreeMessageClass, 99, this, NULL, this);
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

void AUTPlayerState::ServerReceiveHatVariant_Implementation(int32 NewVariant)
{
	HatVariant = NewVariant;

	AController* Controller = Cast<AController>(GetOwner());
	if (Controller != NULL)
	{
		AUTCharacter* Pawn = Cast<AUTCharacter>(Controller->GetPawn());
		if (Pawn != NULL)
		{
			Pawn->HatVariant = NewVariant;
			Pawn->OnRepHatVariant();
		}
	}	
}

bool AUTPlayerState::ServerReceiveHatVariant_Validate(int32 NewVariant)
{
	return true;
}

void AUTPlayerState::ServerReceiveEyewearVariant_Implementation(int32 NewVariant)
{
	EyewearVariant = NewVariant;

	AController* Controller = Cast<AController>(GetOwner());
	if (Controller != NULL)
	{
		AUTCharacter* Pawn = Cast<AUTCharacter>(Controller->GetPawn());
		if (Pawn != NULL)
		{
			Pawn->EyewearVariant = NewVariant;
			Pawn->OnRepEyewearVariant();
		}
	}
}

bool AUTPlayerState::ServerReceiveEyewearVariant_Validate(int32 NewVariant)
{
	return true;
}

void AUTPlayerState::ServerReceiveHatClass_Implementation(const FString& NewHatClass)
{
	HatClass = LoadClass<AUTHat>(NULL, *NewHatClass, NULL, LOAD_NoWarn, NULL);
	
	if (!HatClass->IsChildOf(AUTHatLeader::StaticClass()))
	{
		AController* Controller = Cast<AController>(GetOwner());
		if (Controller != NULL)
		{
			AUTCharacter* Pawn = Cast<AUTCharacter>(Controller->GetPawn());
			if (Pawn != NULL)
			{
				Pawn->HatClass = HatClass;
				Pawn->OnRepHat();
			}
		}
	}
}

bool AUTPlayerState::ServerReceiveHatClass_Validate(const FString& NewHatClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveEyewearClass_Implementation(const FString& NewEyewearClass)
{
	EyewearClass = LoadClass<AUTEyewear>(NULL, *NewEyewearClass, NULL, LOAD_NoWarn, NULL);

	AController* Controller = Cast<AController>(GetOwner());
	if (Controller != NULL)
	{
		AUTCharacter* Pawn = Cast<AUTCharacter>(Controller->GetPawn());
		if (Pawn != NULL)
		{
			Pawn->EyewearClass = EyewearClass;
			Pawn->OnRepEyewear();
		}
	}
}

bool AUTPlayerState::ServerReceiveEyewearClass_Validate(const FString& NewEyewearClass)
{
	return true;
}

void AUTPlayerState::ServerReceiveTauntClass_Implementation(const FString& NewTauntClass)
{
	TauntClass = LoadClass<AUTTaunt>(NULL, *NewTauntClass, NULL, LOAD_NoWarn, NULL);
}

bool AUTPlayerState::ServerReceiveTauntClass_Validate(const FString& NewEyewearClass)
{
	return true;
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

void AUTPlayerState::HandleTeamChanged(AController* Controller)
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
				HandleTeamChanged(Controller);
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
		PS->HatClass = HatClass;
		PS->EyewearClass = EyewearClass;
		PS->HatVariant = HatVariant;
		PS->EyewearVariant = EyewearVariant;
		PS->SelectedCharacter = SelectedCharacter;
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

	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(Reason);
}

void AUTPlayerState::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	UUTLocalPlayer* LP = nullptr;
	if (PC != nullptr)
	{
		LP = Cast<UUTLocalPlayer>(PC->Player);
	}

	if (LP)
	{
		// Send over the country flag....
		UUTProfileSettings* Settings = LP->GetProfileSettings();
		if (Settings)
		{
			CountryFlag = Settings->CountryFlag;
			ServerRecieveCountryFlag(CountryFlag);
		}
	}

	if (Role == ROLE_Authority && StatManager == nullptr)
	{
		//Make me a statmanager
		StatManager = ConstructObject<UStatManager>(UStatManager::StaticClass(), this);
		StatManager->InitializeManager(this);
	}

	bool bFoundStatsId = false;
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
		OnlineUserCloudInterface = OnlineSubsystem->GetUserCloudInterface();

		if (Role == ROLE_Authority && OnlineUserCloudInterface.IsValid())
		{
			OnReadUserFileCompleteDelegate = FOnReadUserFileCompleteDelegate::CreateUObject(this, &AUTPlayerState::OnReadUserFileComplete);
			OnlineUserCloudInterface->AddOnReadUserFileCompleteDelegate_Handle(OnReadUserFileCompleteDelegate);

			OnWriteUserFileCompleteDelegate = FOnWriteUserFileCompleteDelegate::CreateUObject(this, &AUTPlayerState::OnWriteUserFileComplete);
			OnlineUserCloudInterface->AddOnWriteUserFileCompleteDelegate_Handle(OnWriteUserFileCompleteDelegate);
		}

		if (OnlineIdentityInterface.IsValid() && LP != nullptr && OnlineIdentityInterface->GetLoginStatus(LP->GetControllerId()))
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(LP->GetControllerId());
			if (UserId.IsValid())
			{
				ServerReceiveStatsID(UserId->ToString());
				bFoundStatsId = true;
			}
		}		
	}

}

void AUTPlayerState::SetCharacter(const FString& CharacterPath)
{
	if (Role == ROLE_Authority)
	{
		// TODO: check entitlement
		SelectedCharacter = (CharacterPath.Len() > 0) ? LoadClass<AUTCharacterContent>(NULL, *CharacterPath, NULL, LOAD_None, NULL) : NULL;
// redirect from blueprint, for easier testing in the editor via C/P
#if WITH_EDITORONLY_DATA
		if (SelectedCharacter == NULL)
		{
			UBlueprint* BP = LoadObject<UBlueprint>(NULL, *CharacterPath, NULL, LOAD_None, NULL);
			if (BP != NULL)
			{
				SelectedCharacter = *BP->GeneratedClass;
			}
		}
#endif
		NotifyTeamChanged();
	}
}

bool AUTPlayerState::ServerSetCharacter_Validate(const FString& CharacterPath)
{
	return true;
}
void AUTPlayerState::ServerSetCharacter_Implementation(const FString& CharacterPath)
{
	// TODO: maybe ignore if already have valid character to avoid trolls?
	SetCharacter(CharacterPath);
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
	if (!StatsID.IsEmpty() && OnlineUserCloudInterface.IsValid() && !bWroteStatsToCloud && !bOnlySpectator && StatManager && !GetWorld()->IsPlayInEditor())
	{
		OnlineUserCloudInterface->ReadUserFile(FUniqueNetIdString(*StatsID), GetStatsFilename());
	}
}

void AUTPlayerState::OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	// this notification is for us
	if (bWasSuccessful && InUserId.ToString() == StatsID && FileName == GetStatsFilename() && StatManager)
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

				DuelSkillRatingThisMatch = StatManager->GetStatValueByName(FName((TEXT("SkillRating"))), EStatRecordingPeriod::Persistent);
				TDMSkillRatingThisMatch = StatManager->GetStatValueByName(FName((TEXT("TDMSkillRating"))), EStatRecordingPeriod::Persistent);
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
	return UUTLocalPlayer::GetStatsFilename(); //TEXT("stats.json");
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
int32 AUTPlayerState::GetSkillRating(FName SkillStatName)
{
	int32 SkillRating = 0;
	
	if (SkillStatName == FName(TEXT("SkillRating")))
	{
		SkillRating = DuelSkillRatingThisMatch;
	}
	else if (SkillStatName == FName(TEXT("TDMSkillRating")))
	{
		SkillRating = TDMSkillRatingThisMatch;
	}
	else
	{
		UE_LOG(LogGameStats, Warning, TEXT("GetSkillRating could not find skill rating for %s"), *SkillStatName.ToString());
	}

	// SkillRating was unset previously, return starting value
	if (SkillRating == 0)
	{
		SkillRating = 1500;
	}

	return SkillRating;
}

void AUTPlayerState::UpdateTeamSkillRating(FName SkillStatName, bool bWonMatch)
{
	// Not writing stats for this player
	if (StatManager == nullptr || StatsID.IsEmpty())
	{
		return;
	}

	int32 SkillRating = GetSkillRating(SkillStatName);
	
	int32 OpponentCount = 0;
	float ExpectedWinPercentage = 0.0f;
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	for (int32 OuterPlayerIdx = 0; OuterPlayerIdx < UTGameState->PlayerArray.Num(); OuterPlayerIdx++)
	{
		AUTPlayerState* Opponent = Cast<AUTPlayerState>(UTGameState->PlayerArray[OuterPlayerIdx]);
		if (Opponent->Team != Team && !Opponent->bOnlySpectator)
		{
			OpponentCount++;
			int32 OpponentSkillRating = Opponent->GetSkillRating(SkillStatName);
			ExpectedWinPercentage += 1.0f / (1.0f + pow(10.0f, (float(OpponentSkillRating - SkillRating) / 400.0f)));
		}
	}

	UE_LOG(LogGameStats, Log, TEXT("UpdateTeamSkillRating %s RA:%d E:%f"), *PlayerName, SkillRating, ExpectedWinPercentage);

	// KFactor selection can be chosen many different ways, feel free to change it
	float KFactor = 32.0f / float(OpponentCount);
	if (SkillRating > 2400)
	{
		KFactor = 16.0f / float(OpponentCount);
	}
	else if (SkillRating >= 2100)
	{
		KFactor = 24.0f / float(OpponentCount);
	}

	int32 NewSkillRating = 0;
	if (bWonMatch)
	{
		NewSkillRating = SkillRating + KFactor*(float(OpponentCount) - ExpectedWinPercentage);
	}
	else
	{
		NewSkillRating = SkillRating + KFactor*(0.0f - ExpectedWinPercentage);
	}

	UE_LOG(LogGameStats, Log, TEXT("UpdateTeamSkillRating %s New Skill Rating %d"), *PlayerName, NewSkillRating);
	ModifyStat(SkillStatName, NewSkillRating, EStatMod::Set);
	ModifyStat(FName(*(SkillStatName.ToString() + TEXT("Samples"))), 1, EStatMod::Delta);
}

void AUTPlayerState::UpdateIndividualSkillRating(FName SkillStatName)
{
	// Not writing stats for this player
	if (StatManager == nullptr || StatsID.IsEmpty())
	{
		return;
	}

	int32 SkillRating = GetSkillRating(SkillStatName);

	int32 OpponentCount = 0;
	float ExpectedWinPercentage = 0.0f;
	float ActualWinPercentage = 0.0f;
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	for (int32 OuterPlayerIdx = 0; OuterPlayerIdx < UTGameState->PlayerArray.Num(); OuterPlayerIdx++)
	{
		AUTPlayerState* Opponent = Cast<AUTPlayerState>(UTGameState->PlayerArray[OuterPlayerIdx]);
		if (Opponent != this && !Opponent->bOnlySpectator)
		{
			OpponentCount++;
			int32 OpponentSkillRating = Opponent->GetSkillRating(SkillStatName);
			ExpectedWinPercentage += 1.0f / (1.0f + pow(10.0f, (float(OpponentSkillRating - SkillRating) / 400.0f)));

			if (Score > Opponent->Score)
			{
				ActualWinPercentage += 1.0f;
			}
			else if (Score == Opponent->Score)
			{
				ActualWinPercentage += 0.5f;
			}
		}
	}

	UE_LOG(LogGameStats, Log, TEXT("UpdateTeamSkillRating %s RA:%d E:%f"), *PlayerName, SkillRating, ExpectedWinPercentage);

	// KFactor selection can be chosen many different ways, feel free to change it
	float KFactor = 32.0f / float(OpponentCount);
	if (SkillRating > 2400)
	{
		KFactor = 16.0f / float(OpponentCount);
	}
	else if (SkillRating >= 2100)
	{
		KFactor = 24.0f / float(OpponentCount);
	}

	int32 NewSkillRating = SkillRating + KFactor*(ActualWinPercentage - ExpectedWinPercentage);

	UE_LOG(LogGameStats, Log, TEXT("UpdateIndividualSkillRating %s New Skill Rating %d"), *PlayerName, NewSkillRating);
	ModifyStat(SkillStatName, NewSkillRating, EStatMod::Set);
	ModifyStat(FName(*(SkillStatName.ToString() + TEXT("Samples"))), 1, EStatMod::Delta);
}

bool AUTPlayerState::ServerRecieveCountryFlag_Validate(uint32 NewCountryFlag) { return true; }

void AUTPlayerState::ServerRecieveCountryFlag_Implementation(uint32 NewCountryFlag)
{
	CountryFlag = NewCountryFlag;
}

void AUTPlayerState::ValidateEntitlements()
{
	if (IOnlineSubsystem::Get() != NULL && UniqueId.IsValid())
	{
		IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
		if (EntitlementInterface.IsValid())
		{
			// we assume that any successful entitlement query is going to return at least one - the entitlement to play the game
			TArray< TSharedRef<FOnlineEntitlement> > AllEntitlements;
			EntitlementInterface->GetAllEntitlements(*UniqueId.GetUniqueNetId().Get(), TEXT("ut"), AllEntitlements);
			if (AllEntitlements.Num() > 0)
			{
				FUniqueEntitlementId Entitlement = GetRequiredEntitlementFromObj(HatClass);
				if (!Entitlement.IsEmpty() && !EntitlementInterface->GetItemEntitlement(*UniqueId.GetUniqueNetId().Get(), Entitlement).IsValid())
				{
					ServerReceiveHatClass(FString());
				}
				Entitlement = GetRequiredEntitlementFromObj(EyewearClass);
				if (!Entitlement.IsEmpty() && !EntitlementInterface->GetItemEntitlement(*UniqueId.GetUniqueNetId().Get(), Entitlement).IsValid())
				{
					ServerReceiveEyewearClass(FString());
				}
				Entitlement = GetRequiredEntitlementFromObj(TauntClass);
				if (!Entitlement.IsEmpty() && !EntitlementInterface->GetItemEntitlement(*UniqueId.GetUniqueNetId().Get(), Entitlement).IsValid())
				{
					ServerReceiveTauntClass(FString());
				}
				Entitlement = GetRequiredEntitlementFromObj(SelectedCharacter);
				if (!Entitlement.IsEmpty() && !EntitlementInterface->GetItemEntitlement(*UniqueId.GetUniqueNetId().Get(), Entitlement).IsValid())
				{
					ServerSetCharacter(FString());
				}
			}
		}
	}
}

/**
 *	Find the current first local player and asks if I'm their friend.  NOTE: the UTLocalPlayer will, when he friends list changes
 *  update these as well.
 **/
void AUTPlayerState::OnRep_UniqueId()
{
	Super::OnRep_UniqueId();
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(GetWorld(),0));
	if (LP)
	{
		bIsFriend = LP->IsAFriend(UniqueId);
	}
}

#if !UE_SERVER

const FSlateBrush* AUTPlayerState::GetELOBadgeImage() const
{
	int32 Badge = 0;
	int32 Level = 0;

	UUTLocalPlayer::GetBadgeFromELO(AverageRank, Badge, Level);
	FString BadgeStr = FString::Printf(TEXT("UT.Badge.%i"), Badge);
	return SUWindowsStyle::Get().GetBrush(*BadgeStr);
}

const FSlateBrush* AUTPlayerState::GetELOBadgeNumberImage() const
{
	int32 Badge = 0;
	int32 Level = 0;

	UUTLocalPlayer::GetBadgeFromELO(AverageRank, Badge, Level);
	FString BadgeNumberStr = FString::Printf(TEXT("UT.Badge.Numbers.%i"), FMath::Clamp<int32>(Level + 1, 1, 9));
	return SUWindowsStyle::Get().GetBrush(*BadgeNumberStr);
}

void AUTPlayerState::BuildPlayerInfo(TSharedPtr<SVerticalBox> Panel)
{

	Panel->AddSlot()
	.Padding(10.0f, 5.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(150)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("Generic", "PlayerNamePrompt", "Name :"))
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					.ColorAndOpacity(FLinearColor::Gray)
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0,0.0,0.0,0.0)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(PlayerName))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
			]
		]
		+SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.WidthOverride(32)
				.HeightOverride(32)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Image(GetELOBadgeImage())
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					[
						SNew(SImage)
						.Image(GetELOBadgeNumberImage())
					]
				]
			]
		]
	];

	Panel->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(150)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "ScorePrompt", "Score :"))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.0, 0.0, 0.0, 0.0)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::AsNumber(Score))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
		]
	];

	Panel->AddSlot()
	.Padding(10.0f, 0.0f, 10.0f, 5.0f)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(150)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("Generic", "RankPrompt", "Rank :"))
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.ColorAndOpacity(FLinearColor::Gray)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.0, 0.0, 0.0, 0.0)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::AsNumber(AverageRank))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
		]
	];


}
#endif

