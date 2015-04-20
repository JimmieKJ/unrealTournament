// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "GameFramework/GameMode.h"
#include "UTDeathMessage.h"
#include "UTGameMessage.h"
#include "UTVictoryMessage.h"
#include "UTTimedPowerup.h"
#include "UTCountDownMessage.h"
#include "UTFirstBloodMessage.h"
#include "UTMutator.h"
#include "UTScoreboard.h"
#include "SlateBasics.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "UTBot.h"
#include "UTSquadAI.h"
#include "Slate/Panels/SULobbyMatchSetupPanel.h"
#include "UTCharacterContent.h"
#include "UTGameEngine.h"
#include "UTWorldSettings.h"
#include "UTLevelSummary.h"

UUTResetInterface::UUTResetInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

namespace MatchState
{
	const FName CountdownToBegin = FName(TEXT("CountdownToBegin"));
	const FName MatchEnteringOvertime = FName(TEXT("MatchEnteringOvertime"));
	const FName MatchIsInOvertime = FName(TEXT("MatchIsInOvertime"));
}

AUTGameMode::AUTGameMode(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FObjectFinder<UClass> PlayerPawnObject(TEXT("Class'/Game/RestrictedAssets/Blueprints/DefaultCharacter.DefaultCharacter_C'"));
	
	if (PlayerPawnObject.Object != NULL)
	{
		DefaultPawnClass = (UClass*)PlayerPawnObject.Object;
	}

	// use our custom HUD class
	HUDClass = AUTHUD::StaticClass();

	GameStateClass = AUTGameState::StaticClass();
	PlayerStateClass = AUTPlayerState::StaticClass();

	PlayerControllerClass = AUTPlayerController::StaticClass();
	MinRespawnDelay = 1.5f;
	bUseSeamlessTravel = false;
	CountDown = 4;
	bPauseable = false;
	RespawnWaitTime = 1.5f;
	ForceRespawnTime = 3.5f;
	MaxReadyWaitTime = 60;
	bHasRespawnChoices = false;
	MinPlayersToStart = 2;
	MaxWaitForPlayers = 90.f;
	bOnlyTheStrongSurvive = false;
	EndScoreboardDelay = 2.0f;
	GameDifficulty = 3.0f;
	BotFillCount = 0;
	bWeaponStayActive = true;
	VictoryMessageClass = UUTVictoryMessage::StaticClass();
	DeathMessageClass = UUTDeathMessage::StaticClass();
	GameMessageClass = UUTGameMessage::StaticClass();
	SquadType = AUTSquadAI::StaticClass();
	MaxSquadSize = 3;
	bClearPlayerInventory = false;
	bDelayedStart = true;
	bDamageHurtsHealth = true;
	bAmmoIsLimited = true;
	bAllowOvertime = true;
	bForceRespawn = false;

	DefaultPlayerName = FString("Malcolm");
	MapPrefix = TEXT("DM");
	LobbyInstanceID = 0;
	DemoFilename = TEXT("%m-%td");
	bDedicatedInstance = false;
}

void AUTGameMode::BeginPlayMutatorHack(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	// WARNING: 'this' is actually an AActor! Only do AActor things!
	if (!IsA(ALevelScriptActor::StaticClass()) && !IsA(AUTMutator::StaticClass()) &&
		(RootComponent == NULL || RootComponent->Mobility != EComponentMobility::Static || (!IsA(AStaticMeshActor::StaticClass()) && !IsA(ALight::StaticClass()))) )
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		// a few type checks being AFTER the CheckRelevance() call is intentional; want mutators to be able to modify, but not outright destroy
		if (Game != NULL && Game != this && !Game->CheckRelevance((AActor*)this) && !IsA(APlayerController::StaticClass()))
		{
			Destroy();
		}
	}
}

void AUTGameMode::Demigod()
{
	bDamageHurtsHealth = !bDamageHurtsHealth;
}

// Parse options for this game...
void AUTGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	// HACK: workaround to inject CheckRelevance() into the BeginPlay sequence
	UFunction* Func = AActor::GetClass()->FindFunctionByName(FName(TEXT("ReceiveBeginPlay")));
	Func->FunctionFlags |= FUNC_Native;
	Func->SetNativeFunc((Native)&AUTGameMode::BeginPlayMutatorHack);

	// MATTFIXME
	/*
	// HACK: workaround for cached inventory flags getting blown away by blueprint recompile cycle
	for (TObjectIterator<AUTInventory> It(RF_NoFlags); It; ++It)
	{
		if (It->IsTemplate(RF_ClassDefaultObject))
		{
			It->PostInitProperties();
		}
	}
	*/

	UE_LOG(UT,Log,TEXT("==============="));
	UE_LOG(UT,Log,TEXT("  Init Game Option: %s"), *Options);

	if (IOnlineSubsystem::Get() != NULL)
	{
		IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
		if (EntitlementInterface.IsValid())
		{
			FOnQueryEntitlementsCompleteDelegate Delegate;
			Delegate.BindUObject(this, &AUTGameMode::EntitlementQueryComplete);
			EntitlementInterface->AddOnQueryEntitlementsCompleteDelegate_Handle(Delegate);
		}
	}

	Super::InitGame(MapName, Options, ErrorMessage);

	GameDifficulty = FMath::Max(0, GetIntOption(Options, TEXT("Difficulty"), GameDifficulty));
	
	HostLobbyListenPort = GetIntOption(Options, TEXT("HostPort"), 14000);
	FString InOpt = ParseOption(Options, TEXT("ForceRespawn"));
	bForceRespawn = EvalBoolOptions(InOpt, bForceRespawn);

	InOpt = ParseOption(Options, TEXT("OnlyStrong"));
	bOnlyTheStrongSurvive = EvalBoolOptions(InOpt, bOnlyTheStrongSurvive);

	MaxWaitForPlayers = GetIntOption(Options, TEXT("MaxPlayerWait"), MaxWaitForPlayers);
	MaxReadyWaitTime = GetIntOption(Options, TEXT("MaxReadyWait"), MaxReadyWaitTime);
	InOpt = ParseOption(Options, TEXT("HasRespawnChoices"));
	bHasRespawnChoices = EvalBoolOptions(InOpt, bHasRespawnChoices);

	TimeLimit = FMath::Max(0,GetIntOption( Options, TEXT("TimeLimit"), TimeLimit ));
	TimeLimit *= 60;

	// Set goal score to end match.
	GoalScore = FMath::Max(0,GetIntOption( Options, TEXT("GoalScore"), GoalScore ));

	MinPlayersToStart = FMath::Max(1, GetIntOption( Options, TEXT("MinPlayers"), MinPlayersToStart));

	RespawnWaitTime = FMath::Max(0,GetIntOption( Options, TEXT("RespawnWait"), RespawnWaitTime ));

	InOpt = ParseOption(Options, TEXT("DedI"));
	bDedicatedInstance = EvalBoolOptions(InOpt, bDedicatedInstance);

	// alias for testing convenience
	if (HasOption(Options, TEXT("Bots")))
	{
		BotFillCount = GetIntOption(Options, TEXT("Bots"), BotFillCount) + 1;
	}
	else
	{
		BotFillCount = GetIntOption(Options, TEXT("BotFill"), BotFillCount);
	}

	for (int32 i = 0; i < BuiltInMutators.Num(); i++)
	{
		AddMutatorClass(BuiltInMutators[i]);
	}
	
	for (int32 i = 0; i < ConfigMutators.Num(); i++)
	{
		AddMutator(ConfigMutators[i]);
	}

	InOpt = ParseOption(Options, TEXT("Mutator"));
	if (InOpt.Len() > 0)
	{
		UE_LOG(UT, Log, TEXT("Mutators: %s"), *InOpt);
		while (InOpt.Len() > 0)
		{
			FString LeftOpt;
			int32 Pos = InOpt.Find(TEXT(","));
			if (Pos > 0)
			{
				LeftOpt = InOpt.Left(Pos);
				InOpt = InOpt.Right(InOpt.Len() - Pos - 1);
			}
			else
			{
				LeftOpt = InOpt;
				InOpt.Empty();
			}
			AddMutator(LeftOpt);
		}
	}

	InOpt = ParseOption(Options, TEXT("Demorec"));
	if (InOpt.Len() > 0)
	{
		bRecordDemo = InOpt != TEXT("off") && InOpt != TEXT("false") && InOpt != TEXT("0") && InOpt != TEXT("no") && InOpt != GFalse.ToString() && InOpt != GNo.ToString();
		if (bRecordDemo)
		{
			DemoFilename = InOpt;
		}
	}

	PostInitGame(Options);

	UE_LOG(UT, Log, TEXT("LobbyInstanceID: %i"), LobbyInstanceID);
	UE_LOG(UT, Log, TEXT("=================="));

	// If we are a lobby instance, establish a communication beacon with the lobby.  For right now, this beacon is created on the local host
	// but in time, the lobby's address will have to be passed
	RecreateLobbyBeacon();
}

void AUTGameMode::AddMutator(const FString& MutatorPath)
{
	int32 PeriodIndex = INDEX_NONE;
	if (MutatorPath.Right(2) != FString(TEXT("_C")) && MutatorPath.FindChar(TCHAR('.'), PeriodIndex))
	{
		FName MutatorModuleName = FName(*MutatorPath.Left(PeriodIndex));
		if (!FModuleManager::Get().IsModuleLoaded(MutatorModuleName))
		{
			if (!FModuleManager::Get().LoadModule(MutatorModuleName).IsValid())
			{
				UE_LOG(UT, Warning, TEXT("Failed to load module for mutator %s"), *MutatorModuleName.ToString());
			}
		}
	}
	TSubclassOf<AUTMutator> MutClass = LoadClass<AUTMutator>(NULL, *MutatorPath, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);
	if (MutClass == NULL && !MutatorPath.Contains(TEXT(".")))
	{
		// use asset registry to try to find shorthand name
		static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
		if (MutatorAssets.Num() == 0)
		{
			GetAllBlueprintAssetData(AUTMutator::StaticClass(), MutatorAssets);

			// create fake asset entries for native classes
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->IsChildOf(AUTMutator::StaticClass()) && It->HasAnyClassFlags(CLASS_Native) && !It->HasAnyClassFlags(CLASS_Abstract))
				{
					FAssetData NewData;
					NewData.AssetName = It->GetFName();
					NewData.TagsAndValues.Add(NAME_GeneratedClass, It->GetPathName());
					MutatorAssets.Add(NewData);
				}
			}
		}
			
		for (const FAssetData& Asset : MutatorAssets)
		{
			if (Asset.AssetName == FName(*MutatorPath) || Asset.AssetName == FName(*FString(TEXT("Mutator_") + MutatorPath)) || Asset.AssetName == FName(*FString(TEXT("UTMutator_") + MutatorPath)))
			{
				const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
				if (ClassPath != NULL)
				{
					MutClass = LoadObject<UClass>(NULL, **ClassPath);
					if (MutClass != NULL)
					{
						break;
					}
				}
			}
		}
	}
	if (MutClass == NULL)
	{
		UE_LOG(UT, Warning, TEXT("Failed to find or load mutator '%s'"), *MutatorPath);
	}
	else
	{
		AddMutatorClass(MutClass);
	}
}

void AUTGameMode::AddMutatorClass(TSubclassOf<AUTMutator> MutClass)
{
	if (MutClass != NULL && AllowMutator(MutClass))
	{
		AUTMutator* NewMut = GetWorld()->SpawnActor<AUTMutator>(MutClass);
		if (NewMut != NULL)
		{
			NewMut->Init(OptionsString);
			if (BaseMutator == NULL)
			{
				BaseMutator = NewMut;
			}
			else
			{
				BaseMutator->AddMutator(NewMut);
			}
		}
	}
}

bool AUTGameMode::AllowMutator(TSubclassOf<AUTMutator> MutClass)
{
	const AUTMutator* DefaultMut = MutClass.GetDefaultObject();

	for (AUTMutator* Mut = BaseMutator; Mut != NULL; Mut = Mut->NextMutator)
	{
		if (Mut->GetClass() == MutClass)
		{
			// already have this exact mutator
			UE_LOG(UT, Log, TEXT("Rejected mutator %s - already have one"), *MutClass->GetPathName());
			return false;
		}
		for (int32 i = 0; i < Mut->GroupNames.Num(); i++)
		{
			for (int32 j = 0; j < DefaultMut->GroupNames.Num(); j++)
			{
				if (Mut->GroupNames[i] == DefaultMut->GroupNames[j])
				{
					// group conflict
					UE_LOG(UT, Log, TEXT("Rejected mutator %s - already have mutator %s with group %s"), *MutClass->GetPathName(), *Mut->GetPathName(), *Mut->GroupNames[i].ToString());
					return false;
				}
			}
		}
	}
	return true;
}

void AUTGameMode::InitGameState()
{
	Super::InitGameState();

	UTGameState = Cast<AUTGameState>(GameState);
	if (UTGameState != NULL)
	{
		UTGameState->SetGoalScore(GoalScore);
		UTGameState->SetTimeLimit(0);
		UTGameState->RespawnWaitTime = RespawnWaitTime;
		UTGameState->ForceRespawnTime = ForceRespawnTime;
		UTGameState->bTeamGame = bTeamGame;
		UTGameState->bWeaponStay = bWeaponStayActive;

		UTGameState->bIsInstanceServer = IsGameInstanceServer();
	}
	else
	{
		UE_LOG(UT,Error, TEXT("UTGameState is NULL %s"), *GameStateClass->GetFullName());
	}

	if (GameSession != NULL && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		GameSession->RegisterServer();
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameMode::UpdateOnlineServer, 60.0f);	
	}
}

void AUTGameMode::UpdateOnlineServer()
{
	if (GameSession &&  GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		AUTGameSession* GS = Cast<AUTGameSession>(GameSession);
		if (GS)
		{
			GS->UpdateGameState();
		}
	}
}

void AUTGameMode::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	// Because of the behavior changes to PostBeginPlay() this really has to go here as PreInitializeCompoennts is sort of the UE4 PBP even
	// though PBP still exists.  It can't go in InitGame() or InitGameState() because team info needed for team locked GameObjectives are not
	// setup at that point.

	for (TActorIterator<AUTGameObjective> ObjIt(GetWorld()); ObjIt; ++ObjIt)
	{
		ObjIt->InitializeObjective();	
		GameObjectiveInitialized(*ObjIt);
	}

	// init startup bots
	for (int32 i = 0; i < SelectedBots.Num() && NumPlayers + NumBots < BotFillCount; i++)
	{
		AddNamedBot(SelectedBots[i].BotName, SelectedBots[i].Team);
	}
}

void AUTGameMode::GameObjectiveInitialized(AUTGameObjective* Obj)
{
	// Allow subclasses to track game objectives as they are initialized
}

APlayerController* AUTGameMode::Login(UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	FString ModdedPortal = Portal;
	FString ModdedOptions = Options;
	if (BaseMutator != NULL)
	{
		BaseMutator->ModifyLogin(ModdedPortal, ModdedOptions);
	}
	APlayerController* Result = Super::Login(NewPlayer, Portal, Options, UniqueId, ErrorMessage);
	if (Result)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(Result->PlayerState);
		if (PS != NULL)
		{
			FString InOpt = ParseOption(Options, TEXT("Character"));
			if (InOpt.Len() > 0)
			{
				PS->SetCharacter(InOpt);
			}
			InOpt = ParseOption(Options, TEXT("Hat"));
			if (InOpt.Len() > 0)
			{
				PS->ServerReceiveHatClass(InOpt);
			}
			InOpt = ParseOption(Options, TEXT("Eyewear"));
			if (InOpt.Len() > 0)
			{
				PS->ServerReceiveEyewearClass(InOpt);
			}
			InOpt = ParseOption(Options, TEXT("Taunt"));
			if (InOpt.Len() > 0)
			{
				PS->ServerReceiveTauntClass(InOpt);
			}
			InOpt = ParseOption(Options, TEXT("Taunt2"));
			if (InOpt.Len() > 0)
			{
				PS->ServerReceiveTaunt2Class(InOpt);
			}
			int32 HatVar = GetIntOption(Options, TEXT("HatVar"), 0);
			PS->ServerReceiveHatVariant(HatVar);
			int32 EyewearVar = GetIntOption(Options, TEXT("EyewearVar"), 0);
			PS->ServerReceiveEyewearVariant(EyewearVar);

			// warning: blindly calling this here relies on ValidateEntitlements() defaulting to "allow" if we have not yet obtained this user's entitlement information
			PS->ValidateEntitlements();
		}
	}
	return Result;
}

void AUTGameMode::EntitlementQueryComplete(bool bWasSuccessful, const FUniqueNetId& UniqueId, const FString& Namespace, const FString& ErrorMessage)
{
	// validate player's custom options
	// note that it is possible that they have not entered the game yet, since this is started via PreLogin() - in that case we'll validate from Login()
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (PS->UniqueId.IsValid() && *PS->UniqueId.GetUniqueNetId().Get() == UniqueId)
		{
			AUTPlayerState* UTPS = Cast<AUTPlayerState>(PS);
			if (UTPS != NULL)
			{
				UTPS->ValidateEntitlements();
			}
		}
	}
}

AUTBot* AUTGameMode::AddBot(uint8 TeamNum)
{
	AUTBot* NewBot = GetWorld()->SpawnActor<AUTBot>(AUTBot::StaticClass());
	if (NewBot != NULL)
	{
		if (BotConfig == NULL)
		{
			BotConfig = NewObject<UUTBotConfig>(this);
		}
		// pick bot character
		if (BotConfig->BotCharacters.Num() == 0)
		{
			UE_LOG(UT, Warning, TEXT("AddBot(): No BotCharacters defined"));
			static int32 NameCount = 0;
			NewBot->PlayerState->SetPlayerName(FString(TEXT("TestBot")) + ((NameCount > 0) ? FString::Printf(TEXT("_%i"), NameCount) : TEXT("")));
			NameCount++;
		}
		else
		{
			int32 NumEligible = 0;
			FBotCharacter* BestChar = NULL;
			uint8 BestSelectCount = MAX_uint8;
			for (FBotCharacter& Character : BotConfig->BotCharacters)
			{
				if (Character.SelectCount < BestSelectCount)
				{
					NumEligible = 1;
					BestChar = &Character;
					BestSelectCount = Character.SelectCount;
				}
				else if (Character.SelectCount == BestSelectCount)
				{
					NumEligible++;
					if (FMath::FRand() < 1.0f / float(NumEligible))
					{
						BestChar = &Character;
					}
				}
			}
			BestChar->SelectCount++;
			NewBot->Personality = *BestChar;
			NewBot->PlayerState->SetPlayerName(BestChar->PlayerName);
			AUTPlayerState* PS = Cast<AUTPlayerState>(NewBot->PlayerState);
			if (PS != NULL)
			{
				PS->bReadyToPlay = true;
			}
		}

		NewBot->InitializeSkill(GameDifficulty);
		NumBots++;
		ChangeTeam(NewBot, TeamNum);
		GenericPlayerInitialization(NewBot);
	}
	return NewBot;
}
AUTBot* AUTGameMode::AddNamedBot(const FString& BotName, uint8 TeamNum)
{
	if (BotConfig == NULL)
	{
		BotConfig = NewObject<UUTBotConfig>(this);
	}
	FBotCharacter* TheChar = NULL;
	for (FBotCharacter& Character : BotConfig->BotCharacters)
	{
		if (Character.PlayerName == BotName)
		{
			TheChar = &Character;
			break;
		}
	}

	if (TheChar == NULL)
	{
		UE_LOG(UT, Error, TEXT("Character data for bot '%s' not found"), *BotName);
		return NULL;
	}
	else
	{
		AUTBot* NewBot = GetWorld()->SpawnActor<AUTBot>(AUTBot::StaticClass());
		if (NewBot != NULL)
		{
			TheChar->SelectCount++;
			NewBot->Personality = *TheChar;
			NewBot->PlayerState->SetPlayerName(TheChar->PlayerName);

			NewBot->InitializeSkill(GameDifficulty);
			NumBots++;
			ChangeTeam(NewBot, TeamNum);
			GenericPlayerInitialization(NewBot);
		}

		return NewBot;
	}
}

AUTBot* AUTGameMode::ForceAddBot(uint8 TeamNum)
{
	BotFillCount = FMath::Max<int32>(BotFillCount, NumPlayers + NumBots + 1);
	return AddBot(TeamNum);
}
AUTBot* AUTGameMode::ForceAddNamedBot(const FString& BotName, uint8 TeamNum)
{
	return AddNamedBot(BotName, TeamNum);
}

void AUTGameMode::SetBotCount(uint8 NewCount)
{
	BotFillCount = NumPlayers + NewCount;
}

void AUTGameMode::AddBots(uint8 Num)
{
	BotFillCount = FMath::Max(NumPlayers, BotFillCount) + Num;
}

void AUTGameMode::KillBots()
{
	BotFillCount = 0;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AUTBot* B = Cast<AUTBot>(It->Get());
		if (B != NULL)
		{
			B->Destroy();
			It--;
		}
	}
}

bool AUTGameMode::AllowRemovingBot(AUTBot* B)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(B->PlayerState);
	// flag carriers should stay in the game until they lose it
	if (PS != NULL && PS->CarriedObject != NULL)
	{
		return false;
	}
	else
	{
		// score leader should stay in the game unless it's the last bot
		if (NumBots > 1 && PS != NULL)
		{
			bool bHighScore = true;
			for (APlayerState* OtherPS : GameState->PlayerArray)
			{
				if (OtherPS != PS && OtherPS->Score >= PS->Score)
				{
					bHighScore = false;
					break;
				}
			}
			if (bHighScore)
			{
				return false;
			}
		}

		// remove as soon as dead or out of combat
		// TODO: if this isn't getting them out fast enough we can restrict to only human players
		return B->GetPawn() == NULL || B->GetEnemy() == NULL || B->LostContact(5.0f);
	}
}

void AUTGameMode::CheckBotCount()
{
	if (NumPlayers + NumBots > BotFillCount)
	{
		for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
		{
			AUTBot* B = Cast<AUTBot>(It->Get());
			if (B != NULL && AllowRemovingBot(B))
			{
				B->Destroy();
				break;
			}
		}
	}
	else while (NumPlayers + NumBots < BotFillCount)
	{
		AddBot();
	}
}

void AUTGameMode::RecreateLobbyBeacon()
{
	if (LobbyInstanceID > 0)
	{
		if (LobbyBeacon)
		{
			// Destroy the existing beacon first
			LobbyBeacon->DestroyBeacon();
			LobbyBeacon = nullptr;
		}

		LobbyBeacon = GetWorld()->SpawnActor<AUTServerBeaconLobbyClient>(AUTServerBeaconLobbyClient::StaticClass());
		if (LobbyBeacon)
		{
			FString BeaconNetDriverName = FString::Printf(TEXT("LobbyBeaconDriver%i"), LobbyInstanceID);
			FURL LobbyURL(nullptr, TEXT("127.0.0.1"), TRAVEL_Absolute);
			LobbyURL.Port = HostLobbyListenPort;

			LobbyBeacon->SetBeaconNetDriverName(BeaconNetDriverName);
			LobbyBeacon->InitLobbyBeacon(LobbyURL, LobbyInstanceID, ServerInstanceGUID);
			UE_LOG(UT, Verbose, TEXT("..... Connecting back to lobby on port %i!"), HostLobbyListenPort);
		}
	}
}

/**
 *	DefaultTimer is called once per second and is useful for consistent timed events that don't require to be 
 *  done every frame.
 **/
void AUTGameMode::DefaultTimer()
{
	// preview world is for blueprint editing, don't try to play
	if (GetWorld()->WorldType == EWorldType::Preview)
	{
		return;
	}

	if (LobbyBeacon && LobbyBeacon->GetNetConnection()->State == EConnectionState::USOCK_Closed)
	{
		// if the server is empty and would be asking the hub to kill it, just kill ourselves rather than waiting for reconnection
		// this relies on there being good monitoring and cleanup code in the hub, but it's better than some kind of network port failure leaving an instance spamming connection attempts forever
		// also handles the hub itself failing
		if (!bDedicatedInstance && NumPlayers <= 0)
		{
			FPlatformMisc::RequestExit(false);
			return;
		}

		// Lost connection with the beacon. Recreate it.
		UE_LOG(UT, Verbose, TEXT("Beacon %s lost connection. Attempting to recreate."), *GetNameSafe(this));
		RecreateLobbyBeacon();
	}

	// Let the game see if it's time to end the match
	CheckGameTime();

	if (bForceRespawn)
	{
		for (auto It = GetWorld()->GetControllerIterator(); It; ++It)
		{
			AController* Controller = *It;
			if (Controller->IsInState(NAME_Inactive))
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(Controller->PlayerState);
				if (PS != NULL && PS->ForceRespawnTime <= 0.0f)
				{
					RestartPlayer(Controller);
				}
			}
		}
	}

	CheckBotCount();

	int32 NumPlayers = GetNumPlayers();

	if (IsGameInstanceServer() && LobbyBeacon)
	{
		if (GetWorld()->GetTimeSeconds() - LastLobbyUpdateTime >= 10.0f) // MAKE ME CONIFG!!!!
		{
			UpdateLobbyMatchStats(TEXT(""));
		}

		if (!bDedicatedInstance)
		{
			// Look to see if we should time out this instance servers
			if (!HasMatchStarted())
			{
				if (GetWorld()->GetRealTimeSeconds() > LobbyInitialTimeoutTime)
				{
					// Catch all...
					SendEveryoneBackToLobby();
					LobbyBeacon->Empty();			
				}
			}
			else 
			{
				if (NumPlayers <= 0)
				{
					// Catch all...
					SendEveryoneBackToLobby();
					LobbyBeacon->Empty();
				}
			}
		}
	}
	else
	{
		// Look to see if we should restart the game due to server inactivity
		if (GetNumPlayers() <= 0 && NumSpectators <= 0 && HasMatchStarted())
		{
			EmptyServerTime++;
			if (EmptyServerTime >= AutoRestartTime)
			{
				TravelToNextMap();
			}
		}
		else
		{
			EmptyServerTime = 0;
		}
	}
}

void AUTGameMode::ForceLobbyUpdate()
{
	LastLobbyUpdateTime = -10.0;
}


void AUTGameMode::Reset()
{
	Super::Reset();

	bGameEnded = false;

	//now respawn all the players
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator)
		{
			RestartPlayer(Controller);
		}
	}

	UTGameState->SetTimeLimit(0);
}

void AUTGameMode::RestartGame()
{
	if (HasMatchStarted())
	{
		Super::RestartGame();
	}
}

bool AUTGameMode::IsEnemy(AController * First, AController* Second)
{
	// In DM - Everyone is an enemy
	return First != Second;
}

void AUTGameMode::Killed(AController* Killer, AController* KilledPlayer, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	// Ignore all killing when entering overtime as we kill off players and don't want it affecting their score.
	if (GetMatchState() != MatchState::MatchEnteringOvertime)
	{
		AUTPlayerState* const KillerPlayerState = Killer ? Cast<AUTPlayerState>(Killer->PlayerState) : NULL;
		AUTPlayerState* const KilledPlayerState = KilledPlayer ? Cast<AUTPlayerState>(KilledPlayer->PlayerState) : NULL;

		//UE_LOG(UT, Log, TEXT("Player Killed: %s killed %s"), (KillerPlayerState != NULL ? *KillerPlayerState->PlayerName : TEXT("NULL")), (KilledPlayerState != NULL ? *KilledPlayerState->PlayerName : TEXT("NULL")));

		bool const bEnemyKill = IsEnemy(Killer, KilledPlayer);

		if (KilledPlayerState != NULL)
		{
			KilledPlayerState->LastKillerPlayerState = KillerPlayerState;

			KilledPlayerState->IncrementDeaths(DamageType, KillerPlayerState);
			TSubclassOf<UUTDamageType> UTDamage(*DamageType);
			if (UTDamage != NULL)
			{
				UTDamage.GetDefaultObject()->ScoreKill(KillerPlayerState, KilledPlayerState, KilledPawn);
			}

			ScoreKill(Killer, KilledPlayer, KilledPawn, DamageType);
			BroadcastDeathMessage(Killer, KilledPlayer, DamageType);
			
			if (bHasRespawnChoices)
			{
				KilledPlayerState->RespawnChoiceA = nullptr;
				KilledPlayerState->RespawnChoiceB = nullptr;
				KilledPlayerState->RespawnChoiceA = Cast<APlayerStart>(ChoosePlayerStart(KilledPlayer));
				KilledPlayerState->RespawnChoiceB = Cast<APlayerStart>(ChoosePlayerStart(KilledPlayer));
				KilledPlayerState->bChosePrimaryRespawnChoice = true;
			}
		}

		DiscardInventory(KilledPawn, Killer);

		if (UTGameState->IsMatchInOvertime() && UTGameState->bOnlyTheStrongSurvive)
		{
			KilledPlayer->ChangeState(NAME_Spectating);
		}
	}
	NotifyKilled(Killer, KilledPlayer, KilledPawn, DamageType);
}

void AUTGameMode::NotifyKilled(AController* Killer, AController* Killed, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	// update AI data
	if (Killer != NULL && Killer != Killed)
	{
		AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
		if (NavData != NULL)
		{
			{
				UUTPathNode* Node = NavData->FindNearestNode(KilledPawn->GetNavAgentLocation(), KilledPawn->GetSimpleCollisionCylinderExtent());
				if (Node != NULL)
				{
					Node->NearbyDeaths++;
				}
			}
			if (Killer->GetPawn() != NULL)
			{
				// it'd be better to get the node from which the shot was fired, but it's probably not worth it
				UUTPathNode* Node = NavData->FindNearestNode(Killer->GetPawn()->GetNavAgentLocation(), Killer->GetPawn()->GetSimpleCollisionCylinderExtent());
				if (Node != NULL)
				{
					Node->NearbyKills++;
				}
			}
		}
	}

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (It->IsValid())
		{
			AUTBot* B = Cast<AUTBot>(It->Get());
			if (B != NULL)
			{
				B->UTNotifyKilled(Killer, Killed, KilledPawn, DamageType.GetDefaultObject());
			}
		}
	}
}

void AUTGameMode::ScorePickup(AUTPickup* Pickup, AUTPlayerState* PickedUpBy, AUTPlayerState* LastPickedUpBy)
{
}

void AUTGameMode::ScoreDamage(int32 DamageAmount, AController* Victim, AController* Attacker)
{
	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreDamage(DamageAmount, Victim, Attacker);
	}
}

void AUTGameMode::ScoreKill(AController* Killer, AController* Other, APawn* KilledPawn, TSubclassOf<UDamageType> DamageType)
{
	if( (Killer == Other) || (Killer == NULL) )
	{
		// If it's a suicide, subtract a kill from the player...

		if (Other != NULL && Other->PlayerState != NULL && Cast<AUTPlayerState>(Other->PlayerState) != NULL)
		{
			Cast<AUTPlayerState>(Other->PlayerState)->AdjustScore(-1);
			Cast<AUTPlayerState>(Other->PlayerState)->IncrementKills(DamageType, false);
		}
	}
	else 
	{
		AUTPlayerState * KillerPlayerState = Cast<AUTPlayerState>(Killer->PlayerState);
		if ( KillerPlayerState != NULL )
		{
			KillerPlayerState->AdjustScore(+1);
			KillerPlayerState->IncrementKills(DamageType, true);
			FindAndMarkHighScorer();
			CheckScore(KillerPlayerState);
		}

		if (!bFirstBloodOccurred)
		{
			BroadcastLocalized(this, UUTFirstBloodMessage::StaticClass(), 0, KillerPlayerState, NULL, NULL);
			bFirstBloodOccurred = true;
		}
	}

	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreKill(Killer, Other, DamageType);
	}
}

bool AUTGameMode::OverridePickupQuery_Implementation(APawn* Other, TSubclassOf<AUTInventory> ItemClass, AActor* Pickup, bool& bAllowPickup)
{
	return (BaseMutator != NULL && BaseMutator->OverridePickupQuery(Other, ItemClass, Pickup, bAllowPickup));
}

void AUTGameMode::DiscardInventory(APawn* Other, AController* Killer)
{
	AUTCharacter* UTC = Cast<AUTCharacter>(Other);
	if (UTC != NULL)
	{
		// toss weapon
		if (UTC->GetWeapon() != NULL)
		{
			UTC->TossInventory(UTC->GetWeapon());
		}
		// toss all powerups
		for (TInventoryIterator<> It(UTC); It; ++It)
		{
			if (It->bAlwaysDropOnDeath)
			{
				UTC->TossInventory(*It, FVector(FMath::FRandRange(0.0f, 200.0f), FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(0.0f, 200.0f)));
			}
		}
		// delete the rest
		UTC->DiscardAllInventory();
	}
}

void AUTGameMode::FindAndMarkHighScorer()
{
	int32 BestScore = 0;
	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState *PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		if (PS != nullptr)
		{
			if (BestScore == 0 || PS->Score > BestScore)
			{
				BestScore = PS->Score;
			}
		}
	}

	for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
	{
		AUTPlayerState *PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
		AController *C = Cast<AController>(PS->GetOwner());
		if (PS != nullptr && PS->Score == BestScore)
		{
			PS->bHasHighScore = true;
			if (C != nullptr)
			{
				AUTCharacter *UTChar = Cast<AUTCharacter>(C->GetPawn());
				if (UTChar && !UTChar->bHasHighScore)
				{
					UTChar->bHasHighScore = true;
					UTChar->HasHighScoreChanged();
				}
			}
		}
		else
		{
			// Clear previous high scores
			PS->bHasHighScore = false;
			if (C != nullptr)
			{
				AUTCharacter *UTChar = Cast<AUTCharacter>(C->GetPawn());
				if (UTChar && UTChar->bHasHighScore)
				{
					UTChar->bHasHighScore = false;
					UTChar->HasHighScoreChanged();
				}
			}
		}
	}
}

bool AUTGameMode::CheckScore(AUTPlayerState* Scorer)
{
	if ( Scorer != NULL )
	{
		if ( (GoalScore > 0) && (Scorer->Score >= GoalScore) )
		{
			EndGame(Scorer,FName(TEXT("fraglimit")));
		}
	}
	return true;
}


void AUTGameMode::StartMatch()
{
	if (HasMatchStarted())
	{
		// Already started
		return;
	}
	if (GetWorld()->IsPlayInEditor() || !bDelayedStart)
	{
		SetMatchState(MatchState::InProgress);
	}
	else
	{
		SetMatchState(MatchState::CountdownToBegin);	
	}

	if (FUTAnalytics::IsAvailable())
	{
		if (GetWorld()->GetNetMode() != NM_Standalone)
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("MapName"), GetWorld()->GetMapName()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("GameName"), GetNameSafe(this)));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("GoalScore"), GoalScore));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeLimit"), TimeLimit));
			UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
			if (UTEngine)
			{
				ParamArray.Add(FAnalyticsEventAttribute(TEXT("CustomContent"), UTEngine->LocalContentChecksums.Num()));
			}
			else
			{
				ParamArray.Add(FAnalyticsEventAttribute(TEXT("CustomContent"), 0));
			}
			FUTAnalytics::GetProvider().RecordEvent( TEXT("NewMatch"), ParamArray );
		}
		else
		{
			UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
			if (UTEngine && UTEngine->LocalContentChecksums.Num() > 0)
			{
				TArray<FAnalyticsEventAttribute> ParamArray;
				ParamArray.Add(FAnalyticsEventAttribute(TEXT("CustomContent"), UTEngine->LocalContentChecksums.Num()));
				FUTAnalytics::GetProvider().RecordEvent(TEXT("MatchWithCustomContent"), ParamArray);
			}
		}
	}
}

void AUTGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	// reset things, relevant for any kind of warmup mode and to make sure placed Actors like pickups are in their initial state no matter how much time has passed in pregame
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->ImplementsInterface(UUTResetInterface::StaticClass()))
		{
			IUTResetInterface::Execute_Reset(*It);
		}
	}

	UTGameState->SetTimeLimit(TimeLimit);
	bFirstBloodOccurred = false;
	AnnounceMatchStart();
}

void AUTGameMode::AnnounceMatchStart()
{
	BroadcastLocalized(this, UUTGameMessage::StaticClass(), 0, NULL, NULL, NULL);
}

void AUTGameMode::BeginGame()
{
	UE_LOG(UT,Log,TEXT("BEGIN GAME GameType: %s"), *GetNameSafe(this));
	UE_LOG(UT,Log,TEXT("Difficulty: %f GoalScore: %i TimeLimit (sec): %i"), GameDifficulty, GoalScore, TimeLimit);

	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AActor* TestActor = *It;
		if (TestActor &&
			!TestActor->IsPendingKill() &&
			TestActor->IsA<APlayerState>())
		{
			Cast<APlayerState>(TestActor)->StartTime = 0;
		}
	}
	GameState->ElapsedTime = 0;

	//Let the game session override the StartMatch function, in case it wants to wait for arbitration
	if (GameSession->HandleStartMatchRequest())
	{
		return;
	}
	SetMatchState(MatchState::InProgress);
}

void AUTGameMode::EndMatch()
{
	Super::EndMatch();
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameMode::PlayEndOfMatchMessage, 1.0f);

	for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator )
	{
		// If a pawn is marked pending kill, *Iterator will be NULL
		APawn* Pawn = *Iterator;
		if (Pawn)
		{
			Pawn->TurnOff();
		}
	}

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->EndMatch();
		}
	}
}

void AUTGameMode::UpdateSkillRating()
{

}

void AUTGameMode::SendEndOfGameStats(FName Reason)
{
	if (FUTAnalytics::IsAvailable())
	{
		if (GetWorld()->GetNetMode() != NM_Standalone)
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("WinnerName"), UTGameState->WinnerPlayerState ? UTGameState->WinnerPlayerState->PlayerName : TEXT("None")));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Reason"), *Reason.ToString()));
			FUTAnalytics::GetProvider().RecordEvent(TEXT("EndFFAMatch"), ParamArray);
		}
	}

	if (!bDisableCloudStats)
	{
		UpdateSkillRating();

		const double CloudStatsStartTime = FPlatformTime::Seconds();
		for (int32 i = 0; i < GetWorld()->GameState->PlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(GetWorld()->GameState->PlayerArray[i]);
			PS->ModifyStat(FName(TEXT("MatchesPlayed")), 1, EStatMod::Delta);
			PS->ModifyStat(FName(TEXT("TimePlayed")), UTGameState->ElapsedTime, EStatMod::Delta);
			PS->ModifyStat(FName(TEXT("PlayerXP")), PS->Score, EStatMod::Delta);
			
			PS->AddMatchToStats(GetClass()->GetPathName(), nullptr, &GetWorld()->GameState->PlayerArray, &InactivePlayerArray);
			if (PS != nullptr)
			{
				PS->WriteStatsToCloud();
			}
		}

		for (int32 i = 0; i < InactivePlayerArray.Num(); i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(InactivePlayerArray[i]);
			if (!PS->HasWrittenStatsToCloud())
			{
				PS->ModifyStat(FName(TEXT("MatchesQuit")), 1, EStatMod::Delta);

				PS->ModifyStat(FName(TEXT("MatchesPlayed")), 1, EStatMod::Delta);
				PS->ModifyStat(FName(TEXT("TimePlayed")), UTGameState->ElapsedTime, EStatMod::Delta);
				PS->ModifyStat(FName(TEXT("PlayerXP")), PS->Score, EStatMod::Delta);

				PS->AddMatchToStats(GetClass()->GetPathName(), nullptr, &GetWorld()->GameState->PlayerArray, &InactivePlayerArray);
				if (PS != nullptr)
				{
					PS->WriteStatsToCloud();
				}
			}
		}

		const double CloudStatsTime = FPlatformTime::Seconds() - CloudStatsStartTime;
		UE_LOG(UT, Verbose, TEXT("Cloud stats write time %.3f"), CloudStatsTime);
	}
}

void AUTGameMode::EndGame(AUTPlayerState* Winner, FName Reason )
{
	// Dont ever end the game in PIE
	if (GetWorld()->WorldType == EWorldType::PIE) return;

	// If we don't have a winner, then go and find one
	if (Winner == NULL)
	{
		for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
		{
			AController* Controller = *Iterator;
			AUTPlayerState* CPS = Cast<AUTPlayerState> (Controller->PlayerState);
			if ( CPS && ((Winner == NULL) || (CPS->Score >= Winner->Score)) )
			{
				Winner = CPS;
			}
		}
	}

	UTGameState->SetWinner(Winner);
	EndTime = GetWorld()->TimeSeconds;

	if (IsGameInstanceServer() && LobbyBeacon)
	{
		FString MatchStats = FString::Printf(TEXT("%i"), GetWorld()->GetGameState()->ElapsedTime);
		LobbyBeacon->EndGame(MatchStats);
	}

	SetEndGameFocus(Winner);

	// Allow replication to happen before reporting scores, stats, etc.
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameMode::HandleMatchHasEnded, 1.5f);
	bGameEnded = true;

	// Setup a timer to pop up the final scoreboard on everyone
	FTimerHandle TempHandle2;
	GetWorldTimerManager().SetTimer(TempHandle2, this, &AUTGameMode::ShowFinalScoreboard, EndScoreboardDelay);

	// Setup a timer to continue to the next map.

	EndTime = GetWorld()->TimeSeconds;
	FTimerHandle TempHandle3;
	GetWorldTimerManager().SetTimer(TempHandle3, this, &AUTGameMode::TravelToNextMap, EndTimeDelay);

	SendEndOfGameStats(Reason);

	EndMatch();
}

void AUTGameMode::InstanceNextMap(const FString& NextMap)
{
	if (NextMap != TEXT(""))
	{
		FString TravelMapName = NextMap;
		if ( FPackageName::IsShortPackageName(NextMap) )
		{
			FPackageName::SearchForPackageOnDisk(NextMap, &TravelMapName); 
		}
		
		GetWorld()->ServerTravel(TravelMapName, false);
	}
	else
	{
		SendEveryoneBackToLobby();
	}
}

/**
 *	NOTE: This is a really simple map list.  It doesn't support multiple maps in the list, etc and is really dumb.  But it
 *  will work for now.
 **/
void AUTGameMode::TravelToNextMap()
{
	FString CurrentMapName = GetWorld()->GetMapName();
	UE_LOG(UT,Log,TEXT("TravelToNextMap: %i %i"),bDedicatedInstance,IsGameInstanceServer());

	if (!bDedicatedInstance && IsGameInstanceServer())
	{
		if (LobbyBeacon)
		{
			FString MatchStats = FString::Printf(TEXT("%i"), GetWorld()->GetGameState()->ElapsedTime);
			LobbyBeacon->Lobby_RequestNextMap(LobbyInstanceID, CurrentMapName);

			// Set a 60 second timeout on sending everyone to the next map.
			FTimerHandle TempHandle4;
			GetWorldTimerManager().SetTimer(TempHandle4, this, &AUTGameMode::SendEveryoneBackToLobby, 60.0);
		}
		else
		{
			SendEveryoneBackToLobby();
		}
		
	}
	else
	{
		if (!RconNextMapName.IsEmpty())
		{
			GetWorld()->ServerTravel(RconNextMapName, false);
			return;
		}

		int32 MapIndex = -1;
		for (int i=0;i<MapRotation.Num();i++)
		{
			if (MapRotation[i].EndsWith(CurrentMapName))
			{
				MapIndex = i;
				break;
			}
		}

		if (MapRotation.Num() > 0)
		{
			MapIndex = (MapIndex + 1) % MapRotation.Num();
			if (MapIndex >=0 && MapIndex < MapRotation.Num())
			{
				GetWorld()->ServerTravel(MapRotation[MapIndex],false);
				return;
			}
		}

		RestartGame();	
	}
}

void AUTGameMode::ShowFinalScoreboard()
{
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC != NULL)
		{
			PC->ClientToggleScoreboard(true);
		}
	}
}

void AUTGameMode::SetEndGameFocus(AUTPlayerState* Winner)
{
	if (Winner == NULL) return; // It's possible to call this with Winner == NULL if timelimit is hit with noone on the server

	EndGameFocus = Cast<AController>(Winner->GetOwner())->GetPawn();
	if ( (EndGameFocus == NULL) && (Cast<AController>(Winner->GetOwner()) != NULL) )
	{
		// If the controller of the winner does not have a pawn, give him one.
		RestartPlayer(Cast<AController>(Winner->GetOwner()));
		EndGameFocus = Cast<AController>(Winner->GetOwner())->GetPawn();
	}

	if ( EndGameFocus != NULL )
	{
		EndGameFocus->bAlwaysRelevant = true;
	}

	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		Controller->GameHasEnded(EndGameFocus, (Controller->PlayerState != NULL) && (Controller->PlayerState == Winner) );
	}
}


void AUTGameMode::BroadcastDeathMessage(AController* Killer, AController* Other, TSubclassOf<UDamageType> DamageType)
{
	if (DeathMessageClass != NULL)
	{
		if ( (Killer == Other) || (Killer == NULL) )
		{
			BroadcastLocalized(this, DeathMessageClass, 1, NULL, Other->PlayerState, DamageType);
		}
		else
		{
			BroadcastLocalized(this, DeathMessageClass, 0, Killer->PlayerState, Other->PlayerState, DamageType);
		}
	}
}

void AUTGameMode::PlayEndOfMatchMessage()
{
	if (!UTGameState)
	{
		return;
	}
	bool bIsFlawlessVictory = (UTGameState->WinnerPlayerState->Deaths == 0);
/*	if (bIsFlawlessVictory)
	{
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AController* Controller = *Iterator;
			if (Controller->PlayerState != NULL && !Controller->PlayerState->bOnlySpectator && (Controller->PlayerState->Score > 0.f) & (Controller->PlayerState != UTGameState->WinnerPlayerState))
			{
				bIsFlawlessVictory = false;
				break;
			}
		}
	}*/
	uint32 FlawlessOffset = bIsFlawlessVictory ? 2 : 0;
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
		if (PC && (PC->PlayerState != NULL) && !PC->PlayerState->bOnlySpectator)
		{
			PC->ClientReceiveLocalizedMessage(VictoryMessageClass, FlawlessOffset + ((UTGameState->WinnerPlayerState == PC->PlayerState) ? 1 : 0), UTGameState->WinnerPlayerState, PC->PlayerState, NULL);
		}
	}
}

// workaround for call chain from engine, SetPlayerDefaults() could be called while pawn is alive to reset its values but we don't want it to spawn inventory unless it's called from RestartPlayer()
static bool bSetPlayerDefaultsSpawnInventory = false;

void AUTGameMode::RestartPlayer(AController* aPlayer)
{
	if ((aPlayer == NULL) || (aPlayer->PlayerState == NULL) || aPlayer->PlayerState->PlayerName.IsEmpty())
	{
		UE_LOG(UT, Warning, TEXT("RestartPlayer with a bad player, bad playerstate, or empty player name"));
		return;
	}

	if (!IsMatchInProgress() || aPlayer->PlayerState->bOnlySpectator)
	{
		return;
	}

	bSetPlayerDefaultsSpawnInventory = true;
	Super::RestartPlayer(aPlayer);
	bSetPlayerDefaultsSpawnInventory = false;

	if (Cast<AUTBot>(aPlayer) != NULL)
	{
		((AUTBot*)aPlayer)->LastRespawnTime = GetWorld()->TimeSeconds;
	}

	// clear spawn choices
	Cast<AUTPlayerState>(aPlayer->PlayerState)->RespawnChoiceA = nullptr;
	Cast<AUTPlayerState>(aPlayer->PlayerState)->RespawnChoiceB = nullptr;

	// clear multikill in progress
	Cast<AUTPlayerState>(aPlayer->PlayerState)->LastKillTime = -100.f;
}

void AUTGameMode::GiveDefaultInventory(APawn* PlayerPawn)
{
	AUTCharacter* UTCharacter = Cast<AUTCharacter>(PlayerPawn);
	if (UTCharacter != NULL)
	{
		if (bClearPlayerInventory)
		{
			UTCharacter->DefaultCharacterInventory.Empty();
		}
		UTCharacter->AddDefaultInventory(DefaultInventory);
	}
}

/* 
  Make sure pawn properties are back to default
  Also add default inventory
*/
void AUTGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);

	if (BaseMutator != NULL)
	{
		BaseMutator->ModifyPlayer(PlayerPawn);
	}

	if (bSetPlayerDefaultsSpawnInventory)
	{
		GiveDefaultInventory(PlayerPawn);
	}
}

void AUTGameMode::ChangeName(AController* Other, const FString& S, bool bNameChange)
{
	// Cap player name's at 15 characters...
	FString SMod = S;
	if (SMod.Len()>15)
	{
		SMod = SMod.Left(15);
	}

    if ( !Other->PlayerState|| FCString::Stricmp(*Other->PlayerState->PlayerName, *SMod) == 0 )
    {
		return;
	}

	// Look to see if someone else is using the the new name
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AController* Controller = *Iterator;
		if (Controller->PlayerState && FCString::Stricmp(*Controller->PlayerState->PlayerName, *SMod) == 0)
		{
			if ( Cast<APlayerController>(Other) != NULL )
			{
					Cast<APlayerController>(Other)->ClientReceiveLocalizedMessage( GameMessageClass, 5 );
					if ( FCString::Stricmp(*Other->PlayerState->PlayerName, *DefaultPlayerName) == 0 )
					{
						Other->PlayerState->SetPlayerName(FString::Printf(TEXT("%s%i"), *DefaultPlayerName, Other->PlayerState->PlayerId));
					}
				return;
			}
		}
	}

    Other->PlayerState->SetPlayerName(SMod);
}

bool AUTGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	if ( Player && Cast<APlayerStartPIE>(Player->StartSpot.Get()) )
	{
		return true;
	}

	return ( GetWorld()->GetNetMode() == NM_Standalone && Player != NULL && Player->StartSpot.IsValid() &&
		(GetMatchState() == MatchState::WaitingToStart || (Player->PlayerState != NULL && Cast<AUTPlayerState>(Player->PlayerState)->bWaitingPlayer))
		 && (RatePlayerStart(Cast<APlayerStart>(Player->StartSpot.Get()), Player) >= 0.f) );
}


AActor* AUTGameMode::FindPlayerStart(AController* Player, const FString& IncomingName)
{
	AActor* const Best = Super::FindPlayerStart(Player, IncomingName);
	if (Best)
	{
		LastStartSpot = Best;
	}

	return Best;
}

FString AUTGameMode::InitNewPlayer(APlayerController* NewPlayerController, const TSharedPtr<FUniqueNetId>& UniqueId, const FString& Options, const FString& Portal)
{
	FString ErrorMessage = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	AUTPlayerState* NewPlayerState = Cast<AUTPlayerState>(NewPlayerController->PlayerState);
	if (bHasRespawnChoices && NewPlayerState && !NewPlayerState->bIsSpectator)
	{
		NewPlayerState->RespawnChoiceA = nullptr;
		NewPlayerState->RespawnChoiceB = nullptr;
		NewPlayerState->RespawnChoiceA = Cast<APlayerStart>(ChoosePlayerStart(NewPlayerController));
		NewPlayerState->RespawnChoiceB = Cast<APlayerStart>(ChoosePlayerStart(NewPlayerController));
		NewPlayerState->bChosePrimaryRespawnChoice = true;
	}

	return ErrorMessage;
}

AActor* AUTGameMode::ChoosePlayerStart(AController* Player)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(Player->PlayerState);
	if (bHasRespawnChoices && UTPS->RespawnChoiceA != nullptr && UTPS->RespawnChoiceB != nullptr)
	{
		if (UTPS->bChosePrimaryRespawnChoice)
		{
			return UTPS->RespawnChoiceA;
		}
		else
		{
			return UTPS->RespawnChoiceB;
		}
	}

	if (PlayerStarts.Num() == 0)
	{
		return Super::ChoosePlayerStart(Player);
	}
	if (GetWorld()->WorldType == EWorldType::PIE)
	{
		for (int32 i = 0; i < PlayerStarts.Num(); i++)
		{
			APlayerStart* P = PlayerStarts[i];

			if (P->IsA(APlayerStartPIE::StaticClass()))
			{
				// Always prefer the first "Play from Here" PlayerStart, if we find one while in PIE mode
				return P;
			}
		}
	}
	
	// Always randomize the list order a bit to prevent groups of bad starts from permanently making the next decent start overused
	for (int i = 0; i < 2; i++)
	{
		int32 RandIndexOne = FMath::RandHelper(PlayerStarts.Num());
		int32 RandIndexTwo = FMath::RandHelper(PlayerStarts.Num());
		APlayerStart* SavedStart = PlayerStarts[RandIndexOne]; 
		PlayerStarts[RandIndexOne] = PlayerStarts[RandIndexTwo];
		PlayerStarts[RandIndexTwo] = SavedStart;
	}

	// Start by choosing a random start
	int32 RandStart = FMath::RandHelper(PlayerStarts.Num());

	float BestRating = 0.f;
	APlayerStart* BestStart = NULL;
	for ( int32 i=RandStart; i<PlayerStarts.Num(); i++ )
	{
		APlayerStart* P = PlayerStarts[i];

		float NewRating = RatePlayerStart(P,Player);

		if (NewRating >= 30.0f)
		{
			// this PlayerStart is good enough
			return P;
		}
		if ( NewRating > BestRating )
		{
			BestRating = NewRating;
			BestStart = P;
		}
	}
	for ( int32 i=0; i<RandStart; i++ )
	{
		APlayerStart* P = PlayerStarts[i];

		float NewRating = RatePlayerStart(P,Player);

		if (NewRating >= 30.0f)
		{
			// this PlayerStart is good enough
			return P;
		}
		if ( NewRating > BestRating )
		{
			BestRating = NewRating;
			BestStart = P;
		}
	}
	return (BestStart != NULL) ? BestStart : Super::ChoosePlayerStart(Player);
}

float AUTGameMode::RatePlayerStart(APlayerStart* P, AController* Player)
{
	float Score = 30.0f;

	AActor* LastSpot = (Player != NULL && Player->StartSpot.IsValid()) ? Player->StartSpot.Get() : NULL;
	AUTPlayerState *UTPS = Cast<AUTPlayerState>(Player->PlayerState);
	if (P == LastStartSpot || (LastSpot != NULL && P == LastSpot))
	{
		// avoid re-using starts
		Score -= 15.0f;
	}
	FVector StartLoc = P->GetActorLocation() + AUTCharacter::StaticClass()->GetDefaultObject<AUTCharacter>()->BaseEyeHeight;
	if (UTPS->RespawnChoiceA)
	{
		if (P == UTPS->RespawnChoiceA)
		{
			// make sure to have two choices
			return -5.f;
		}
		// try to get far apart choices
		float Dist = (UTPS->RespawnChoiceA->GetActorLocation() - StartLoc).Size();
		if (Dist < 5000.0f)
		{
			Score -= 5.f;
		}
	}

	if (Player != NULL)
	{
		bool bTwoPlayerGame = (NumPlayers + NumBots == 2);
		for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
		{
			AController* OtherController = *Iterator;
			ACharacter* OtherCharacter = Cast<ACharacter>( OtherController->GetPawn());

			if ( OtherCharacter && OtherCharacter->PlayerState )
			{
				if (FMath::Abs(StartLoc.Z - OtherCharacter->GetActorLocation().Z) < P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + OtherCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
					&& (StartLoc - OtherCharacter->GetActorLocation()).Size2D() < P->GetCapsuleComponent()->GetScaledCapsuleRadius() + OtherCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius())
				{
					// overlapping - would telefrag
					return -10.f;
				}

				float NextDist = (OtherCharacter->GetActorLocation() - StartLoc).Size();
				static FName NAME_RatePlayerStart = FName(TEXT("RatePlayerStart"));
				bool bIsLastKiller = (OtherCharacter->PlayerState == Cast<AUTPlayerState>(Player->PlayerState)->LastKillerPlayerState);

				if (((NextDist < 8000.0f) || bTwoPlayerGame) && !UTGameState->OnSameTeam(Player, OtherController))
				{
					if (!GetWorld()->LineTraceTest(StartLoc, OtherCharacter->GetActorLocation() + FVector(0.f, 0.f, OtherCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false)))
					{
						// Avoid the last person that killed me
						if (bIsLastKiller)
						{
							Score -= 7.f;
						}

						Score -= (5.f - 0.0003f * NextDist);
					}
					else if (NextDist < 4000.0f)
					{
						// Avoid the last person that killed me
						Score -= bIsLastKiller ? 5.f : 0.0005f * (5000.f - NextDist);

						if (!GetWorld()->LineTraceTest(StartLoc, OtherCharacter->GetActorLocation(), ECC_Visibility, FCollisionQueryParams(NAME_RatePlayerStart, false, this)))
						{
							Score -= 2.f;
						}
					}
				}
			}
			else if (bHasRespawnChoices && OtherController->PlayerState && !OtherController->GetPawn() && !OtherController->PlayerState->bOnlySpectator)
			{
				// make sure no one else has this start as a pending choice
				AUTPlayerState* UTPS = Cast<AUTPlayerState>(OtherController->PlayerState);
				if (UTPS)
				{
					if (P == UTPS->RespawnChoiceA || P == UTPS->RespawnChoiceB)
					{
						return -5.f;
					}
					if (bTwoPlayerGame)
					{
						// avoid choosing starts near a pending start
						if (UTPS->RespawnChoiceA)
						{
							float Dist = (UTPS->RespawnChoiceA->GetActorLocation() - StartLoc).Size();
							Score -= 7.f * FMath::Max(0.f, (5000.f - Dist) / 5000.f);
						}
						if (UTPS->RespawnChoiceB)
						{
							float Dist = (UTPS->RespawnChoiceB->GetActorLocation() - StartLoc).Size();
							Score -= 7.f * FMath::Max(0.f, (5000.f - Dist) / 5000.f);
						}
					}
				}
			}

		}
	}
	return FMath::Max(Score, 0.2f);
}

/**
 *	We are going to duplicate GameMode's StartNewPlayer because we need to replicate the scoreboard class along with the hud class.  
 *  We are doing this here like this because we are trying to not change engine.  Ultimately the code to create the hud should be
 *  moved to it's own easy to override function instead of being hard-coded in StartNewPlayer
 **/
void AUTGameMode::StartNewPlayer(APlayerController* NewPlayer)
{
	AUTPlayerController* UTNewPlayer = Cast<AUTPlayerController>(NewPlayer);
	if (UTNewPlayer != NULL)
	{
		// tell client what hud class to use
		TSubclassOf<UUTScoreboard> ScoreboardClass = LoadClass<UUTScoreboard>(NULL, *ScoreboardClassName.AssetLongPathname, NULL, LOAD_None, NULL);
		UTNewPlayer->ClientSetHUDAndScoreboard(HUDClass, ScoreboardClass);

		// start match, or let player enter, immediately
		if (UTGameState->HasMatchStarted())
		{
			RestartPlayer(NewPlayer);
		}

		if (NewPlayer->GetPawn() != NULL)
		{
			NewPlayer->GetPawn()->ClientSetRotation(NewPlayer->GetPawn()->GetActorRotation());
		}
	}
	else
	{
		Super::StartNewPlayer(NewPlayer);
	}
}

void AUTGameMode::StartPlay()
{
	Super::StartPlay();
	StartPlayTime = GetWorld()->GetTimeSeconds();
}

bool AUTGameMode::ReadyToStartMatch()
{
	if (GetWorld()->IsPlayInEditor() || !bDelayedStart)
	{
		// starting on first frame has side effects in PIE because of differences in ordering; components haven't been initialized/registered yet...
		if (GetWorld()->TimeSeconds == 0.0f)
		{
			GetWorldTimerManager().SetTimerForNextTick(this, &AUTGameMode::StartMatch);
			return false;
		}
		else
		{
			// PIE is always ready to start.
			return true;
		}
	}

	// By default start when we have > 0 players
	if (GetMatchState() == MatchState::WaitingToStart)
	{
		UTGameState->PlayersNeeded = (GetNetMode() == NM_Standalone) ? 0 : FMath::Max(0, MinPlayersToStart - NumPlayers - NumBots);
		if ((UTGameState->PlayersNeeded == 0) && (NumPlayers + NumSpectators > 0))
		{
			if ((MaxReadyWaitTime <= 0) || (UTGameState->RemainingTime > 0) || (GetNetMode() == NM_Standalone))
			{
				for (int i=0;i<UTGameState->PlayerArray.Num();i++)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
					if (PS != NULL && !PS->bOnlySpectator && !PS->bReadyToPlay)
					{
						return false;
					}
				}
			}

			return true;
		}
		else
		{
			if ((MaxWaitForPlayers > 0) && (GetWorld()->GetTimeSeconds() - StartPlayTime > MaxWaitForPlayers))
			{
				BotFillCount = FMath::Max(BotFillCount, MinPlayersToStart);
			}
			if (MaxReadyWaitTime > 0)
			{
				// reset max wait for players to ready up
				UTGameState->SetTimeLimit(MaxReadyWaitTime);
			}
		}
	}
	return false;
}

/**
 *	Overwriting all of these functions to work the way I think it should.  Really, the match state should
 *  only be in 1 place otherwise it's prone to mismatch errors.  I'm chosen the GameState because it's
 *  replicated and will be available on clients.
 **/
bool AUTGameMode::HasMatchStarted() const
{
	return UTGameState->HasMatchStarted();
}

bool AUTGameMode::IsMatchInProgress() const
{
	return UTGameState->IsMatchInProgress();
}

bool AUTGameMode::HasMatchEnded() const
{
	return UTGameState->HasMatchEnded();
}

/**	I needed to rework the ordering of SetMatchState until it can be corrected in the engine. **/
void AUTGameMode::SetMatchState(FName NewState)
{
	if (MatchState == NewState)
	{
		return;
	}

	MatchState = NewState;
	if (GameState)
	{
		GameState->SetMatchState(NewState);
	}

	CallMatchStateChangeNotify();

	if (BaseMutator != NULL)
	{
		BaseMutator->NotifyMatchStateChange(MatchState);
	}
}

void AUTGameMode::CallMatchStateChangeNotify()
{
	// Call change callbacks

	if (MatchState == MatchState::WaitingToStart)
	{
		HandleMatchIsWaitingToStart();
	}
	else if (MatchState == MatchState::CountdownToBegin)
	{
		HandleCountdownToBegin();
	}
	else if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();
	}
	else if (MatchState == MatchState::LeavingMap)
	{
		HandleLeavingMap();
	}
	else if (MatchState == MatchState::Aborted)
	{
		HandleMatchAborted();
	}
	else if (MatchState == MatchState::MatchEnteringOvertime)
	{
		HandleEnteringOvertime();
	}
	else if (MatchState == MatchState::MatchIsInOvertime)
	{
		HandleMatchInOvertime();
	}
}

void AUTGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	// save AI data only after completed matches
	AUTRecastNavMesh* NavData = GetUTNavData(GetWorld());
	if (NavData != NULL)
	{
		NavData->SaveMapLearningData();
	}
}

void AUTGameMode::HandleEnteringOvertime()
{
	if (bOnlyTheStrongSurvive)
	{
		// We are entering overtime, kill off anyone not at the top of the leader board....

		AUTPlayerState* BestPlayer = NULL;
		AUTPlayerState* KillPlayer = NULL;
		float BestScore = 0.0;

		for (int PlayerIdx = 0; PlayerIdx < UTGameState->PlayerArray.Num(); PlayerIdx++)
		{
			if (UTGameState->PlayerArray[PlayerIdx] != NULL)
			{
				if (BestPlayer == NULL || UTGameState->PlayerArray[PlayerIdx]->Score > BestScore)
				{
					if (BestPlayer != NULL)
					{
						KillPlayer = BestPlayer;
					}
					BestPlayer = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
					BestScore = BestPlayer->Score;
				}
				else if (UTGameState->PlayerArray[PlayerIdx]->Score < BestScore)
				{
					KillPlayer = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
				}
			}

			if (KillPlayer != NULL)
			{
				// No longer the best.. kill him.. KILL HIM NOW!!!!!
				AController* COwner = Cast<AController>(KillPlayer->GetOwner());
				if (COwner != NULL )
				{
					if (COwner->GetPawn() != NULL)
					{
						AUTCharacter* UTChar = Cast<AUTCharacter>(COwner->GetPawn());
						if (UTChar != NULL)
						{
							//UE_LOG(UT, Log, TEXT("    -- Calling Died"));
							// Kill off the pawn...
							UTChar->Died(NULL, FDamageEvent(UUTDamageType::StaticClass()));
							// Send this character a message/taunt about not making the cut....
						}
					}

					// Tell the player they didn't make the cut
					AUTPlayerController* PC = Cast<AUTPlayerController>(COwner);
					if (PC)
					{
						PC->ClientReceiveLocalizedMessage(UUTGameMessage::StaticClass(), 8);					
						PC->ChangeState(NAME_Spectating);
					}
				}


				KillPlayer = NULL;
			}
		}
		
		// force respawn any players that are applicable for overtime but are currently dead
		if (BestPlayer != NULL)
		{
			for (APlayerState* TestPlayer : UTGameState->PlayerArray)
			{
				if (TestPlayer->Score == BestPlayer->Score && !TestPlayer->bOnlySpectator)
				{
					AController* C = Cast<AController>(TestPlayer->GetOwner());
					if (C != NULL && C->GetPawn() == NULL)
					{
						RestartPlayer(C);
					}
				}
			}
		}

		UTGameState->bOnlyTheStrongSurvive = true;
	}

	SetMatchState(MatchState::MatchIsInOvertime);
}

void AUTGameMode::HandleMatchInOvertime()
{
	// Send the overtime message....
	BroadcastLocalized( this, UUTGameMessage::StaticClass(), 1, NULL, NULL, NULL);
}

void AUTGameMode::HandleCountdownToBegin()
{
	if (bRecordDemo)
	{
		FString MapName = GetOutermost()->GetName();
		GetWorld()->Exec(GetWorld(), *FString::Printf(TEXT("Demorec %s"), *DemoFilename.Replace(TEXT("%m"), *MapName.RightChop(MapName.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1))));
	}
	CountDown = 5;
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameMode::CheckCountDown, 1.0, false);
}

void AUTGameMode::CheckCountDown()
{
	if (CountDown >0)
	{
		// Broadcast the localized message saying when the game begins.
		BroadcastLocalized( this, UUTCountDownMessage::StaticClass(), CountDown, NULL, NULL, NULL);
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTGameMode::CheckCountDown, 1.0, false);
		CountDown--;
	}
	else
	{
		BeginGame();
	}
}

void AUTGameMode::CheckGameTime()
{
	if (IsMatchInProgress() && !HasMatchEnded() && TimeLimit > 0 && UTGameState->RemainingTime <= 0)
	{
		// Game should be over.. look to see if we need to go in to overtime....	

		uint32 bTied = 0;
		AUTPlayerState* Winner = IsThereAWinner(bTied);

		if (!bAllowOvertime || !bTied)
		{
			EndGame(Winner, FName(TEXT("TimeLimit")));			
		}
		else if (bAllowOvertime && !UTGameState->IsMatchInOvertime())
		{
			// Stop the clock in Overtime. 
			UTGameState->bStopGameClock = true;
			SetMatchState(MatchState::MatchEnteringOvertime);
		}
	}
}

void AUTGameMode::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();

	if (MaxReadyWaitTime > 0)
	{
		UTGameState->SetTimeLimit(MaxReadyWaitTime);
	}
}

/**
 *	Look though the player states and see if we have a winner.  If there is a tie, we return
 *  NULL so that we can enter overtime.
 **/
AUTPlayerState* AUTGameMode::IsThereAWinner(uint32& bTied)
{
	AUTPlayerState* BestPlayer = NULL;
	float BestScore = 0.0;

	for (int PlayerIdx=0; PlayerIdx < UTGameState->PlayerArray.Num();PlayerIdx++)
	{
		if (UTGameState->PlayerArray[PlayerIdx] != NULL)
		{
			if (BestPlayer == NULL || UTGameState->PlayerArray[PlayerIdx]->Score > BestScore)
			{
				BestPlayer = Cast<AUTPlayerState>(UTGameState->PlayerArray[PlayerIdx]);
				BestScore = BestPlayer->Score;
				bTied = 0;
			}
			else if (UTGameState->PlayerArray[PlayerIdx]->Score == BestScore)
			{
				bTied = 1;
			}
		}
	}

	return BestPlayer;
}

void AUTGameMode::OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState)
{
	Super::OverridePlayerState(PC, OldPlayerState);

	// if we're in this function GameMode swapped PlayerState objects so we need to update the precasted copy
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	if (UTPC != NULL)
	{
		UTPC->UTPlayerState = Cast<AUTPlayerState>(UTPC->PlayerState);
	}
}

void AUTGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	if (BaseMutator != NULL)
	{
		BaseMutator->PostPlayerInit(C);
	}

	UpdatePlayersPresence();

}

void AUTGameMode::PostLogin( APlayerController* NewPlayer )
{
	Super::PostLogin(NewPlayer);

	NewPlayer->ClientSetLocation(NewPlayer->GetFocalLocation(), NewPlayer->GetControlRotation());
	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}

	CheckBotCount();
}

void AUTGameMode::Logout(AController* Exiting)
{
	if (BaseMutator != NULL)
	{
		BaseMutator->NotifyLogout(Exiting);
	}

	// Let's Analytics know how long this player has been online....

	AUTPlayerState* PS = Cast<AUTPlayerState>(Exiting->PlayerState);
		
	if (PS != NULL && FUTAnalytics::IsAvailable())
	{
		float TotalTimeOnline = GameState->ElapsedTime - PS->StartTime;
		TArray<FAnalyticsEventAttribute> ParamArray;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ID"), PS->StatsID));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PlayerName"), PS->PlayerName));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeOnline"), TotalTimeOnline));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Kills"), PS->Kills));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Deaths"), PS->Deaths));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Score"), PS->Score));
		FUTAnalytics::GetProvider().RecordEvent( TEXT("PlayerLogoutStat"), ParamArray );
		PS->RespawnChoiceA = NULL;
		PS->RespawnChoiceB = NULL;
	}

	if (Cast<AUTBot>(Exiting) != NULL)
	{
		NumBots--;
	}

	Super::Logout(Exiting);

	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UpdateGameState();
		}
	}

	UpdatePlayersPresence();
}

bool AUTGameMode::PlayerCanRestart( APlayerController* Player )
{
	// Can't restart in overtime
	if (bOnlyTheStrongSurvive && UTGameState->IsMatchInOvertime())
	{
		return false;
	}
	else
	{
		return Super::PlayerCanRestart(Player);
	}
}

bool AUTGameMode::ModifyDamage_Implementation(int32& Damage, FVector& Momentum, APawn* Injured, AController* InstigatedBy, const FHitResult& HitInfo, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	AUTCharacter* InjuredChar = Cast<AUTCharacter>(Injured);
	if (InjuredChar != NULL && InjuredChar->bSpawnProtectionEligible && InstigatedBy != NULL && InstigatedBy != Injured->Controller && GetWorld()->TimeSeconds - Injured->CreationTime < UTGameState->SpawnProtectionTime)
	{
		Damage = 0;
	}

	if (BaseMutator != NULL)
	{
		BaseMutator->ModifyDamage(Damage, Momentum, Injured, InstigatedBy, HitInfo, DamageCauser, DamageType);
	}

	return true;
}

bool AUTGameMode::CheckRelevance_Implementation(AActor* Other)
{
	if (BaseMutator == NULL)
	{
		return true;
	}
	else
	{
		bool bPreventModify = false;
		bool bForceKeep = BaseMutator->AlwaysKeep(Other, bPreventModify);
		if (bForceKeep && bPreventModify)
		{
			return true;
		}
		else
		{
			return (BaseMutator->CheckRelevance(Other) || bForceKeep);
		}
	}
}

void AUTGameMode::SetWorldGravity(float NewGravity)
{
	AWorldSettings* Settings = GetWorld()->GetWorldSettings();
	Settings->bWorldGravitySet = true;
	Settings->WorldGravityZ = NewGravity;
}

bool AUTGameMode::ChangeTeam(AController* Player, uint8 NewTeam, bool bBroadcast)
{
	// By default, we don't do anything.
	return true;
}

TSubclassOf<AGameSession> AUTGameMode::GetGameSessionClass() const
{
	return AUTGameSession::StaticClass();
}

void AUTGameMode::ScoreObject(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason)
{
	if (BaseMutator != NULL)
	{
		BaseMutator->ScoreObject(GameObject, HolderPawn, Holder, Reason);
	}
}

void AUTGameMode::GetSeamlessTravelActorList(bool bToEntry, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToEntry, ActorList);

	for (AUTMutator* Mut = BaseMutator; Mut != NULL; Mut = Mut->NextMutator)
	{
		Mut->GetSeamlessTravelActorList(bToEntry, ActorList);
	}
}

#if !UE_SERVER
void AUTGameMode::CreateConfigWidgets(TSharedPtr<class SVerticalBox> MenuSpace, bool bCreateReadOnly, TArray< TSharedPtr<TAttributePropertyBase> >& ConfigProps)
{
/*
	TSharedPtr< TAttributeProperty<int32> > TimeLimitAttr = MakeShareable(new TAttributeProperty<int32>(this, &TimeLimit, TEXT("TimeLimit")));
	ConfigProps.Add(TimeLimitAttr);
	TSharedPtr< TAttributeProperty<int32> > GoalScoreAttr = MakeShareable(new TAttributeProperty<int32>(this, &GoalScore, TEXT("GoalScore")));
	ConfigProps.Add(GoalScoreAttr);
	TSharedPtr< TAttributePropertyBool > ForceRespawnAttr = MakeShareable(new TAttributePropertyBool(this, &bForceRespawn, TEXT("ForceRespawn")));
	ConfigProps.Add(ForceRespawnAttr);
	TSharedPtr< TAttributeProperty<int32> > CombatantsAttr = MakeShareable(new TAttributeProperty<int32>(this, &BotFillCount, TEXT("BotFill")));
	ConfigProps.Add(CombatantsAttr);
	TSharedPtr< TAttributeProperty<float> > BotSkillAttr = MakeShareable(new TAttributeProperty<float>(this, &GameDifficulty, TEXT("Difficulty")));
	ConfigProps.Add(BotSkillAttr);

	// FIXME: temp 'ReadOnly' handling by creating new widgets; ideally there would just be a 'disabled' or 'read only' state in Slate...
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f,0.0f,0.0f,5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "NumCombatants", "Number of Combatants"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(20.0f,0.0f,0.0f,0.0f)
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
					.Text(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
				) :
				StaticCastSharedRef<SWidget>(
					SNew(SNumericEntryBox<int32>)
					.Value(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
					.OnValueChanged(CombatantsAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
					.AllowSpin(true)
					.Delta(1)
					.MinValue(1)
					.MaxValue(32)
					.MinSliderValue(1)
					.MaxSliderValue(32)
					.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")

				)
			]
		]
	];
	// TODO: BotSkill should be a list box with the usual items; this is a simple placeholder
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f,0.0f,0.0f,5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "BotSkill", "Bot Skill"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UT.Common.ButtonText.White")
					.Text(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::GetAsText)
				) :
				StaticCastSharedRef<SWidget>(
					SNew(SNumericEntryBox<float>)
					.LabelPadding(FMargin(10.0f, 0.0f))
					.Value(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::GetOptional)
					.OnValueChanged(BotSkillAttr.ToSharedRef(), &TAttributeProperty<float>::Set)
					.AllowSpin(true)
					.Delta(1)
					.MinValue(0)
					.MaxValue(7)
					.MinSliderValue(0)
					.MaxSliderValue(7)
					.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
				)
			]
		]
	];
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f,0.0f,0.0f,5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "GoalScore", "Goal Score"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
					SNew(STextBlock)
					.TextStyle(SUWindowsStyle::Get(),"UT.Common.ButtonText.White")
					.Text(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
				) :
				StaticCastSharedRef<SWidget>(
					SNew(SNumericEntryBox<int32>)
					.Value(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
					.OnValueChanged(GoalScoreAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
					.AllowSpin(true)
					.Delta(1)
					.MinValue(0)
					.MaxValue(999)
					.MinSliderValue(0)
					.MaxSliderValue(99)
					.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
				)
			]
		]
	];
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f,0.0f,0.0f,5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "TimeLimit", "Time Limit"))
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f,0.0f,0.0f,0.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.White")
				.Text(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetAsText)
				) :
				StaticCastSharedRef<SWidget>(
				SNew(SNumericEntryBox<int32>)
				.Value(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::GetOptional)
				.OnValueChanged(TimeLimitAttr.ToSharedRef(), &TAttributeProperty<int32>::Set)
				.AllowSpin(true)
				.Delta(1)
				.MinValue(0)
				.MaxValue(999)
				.MinSliderValue(0)
				.MaxSliderValue(60)
				.EditableTextBoxStyle(SUWindowsStyle::Get(), "UT.Common.NumEditbox.White")
				)
			]
		]
	];
	MenuSpace->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.Padding(0.0f,0.0f,0.0f,5.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(350)
			[
				SNew(STextBlock)
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
				.Text(NSLOCTEXT("UTGameMode", "ForceRespawn", "Force Respawn").ToString())
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(20.0f, 0.0f, 0.0f, 10.0f)
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(300)
			[
				bCreateReadOnly ?
				StaticCastSharedRef<SWidget>(
					SNew(SCheckBox)
					.IsChecked(ForceRespawnAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.ForegroundColor(FLinearColor::White)
					.Type(ESlateCheckBoxType::CheckBox)
				) :
				StaticCastSharedRef<SWidget>(
					SNew(SCheckBox)
					.IsChecked(ForceRespawnAttr.ToSharedRef(), &TAttributePropertyBool::GetAsCheckBox)
					.OnCheckStateChanged(ForceRespawnAttr.ToSharedRef(), &TAttributePropertyBool::SetFromCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.ForegroundColor(FLinearColor::White)
					.Type(ESlateCheckBoxType::CheckBox)
				)
			]
		]
	];
*/
}



#endif

void AUTGameMode::ProcessServerTravel(const FString& URL, bool bAbsolute)
{
	if (GameSession != NULL)
	{
		AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
		if (UTGameSession != NULL)
		{
			UTGameSession->UnRegisterServer(false);
		}
	}

	Super::ProcessServerTravel(URL, bAbsolute);
}

FText AUTGameMode::BuildServerRules(AUTGameState* GameState)
{
	return FText::Format(NSLOCTEXT("UTGameMode", "GameRules", "{0} - GoalScore: {1}  Time Limit: {2}"), DisplayName, FText::AsNumber(GameState->GoalScore), (GameState->TimeLimit > 0) ? FText::Format(NSLOCTEXT("UTGameMode", "TimeMinutes", "{0} min"), FText::AsNumber(uint32(GameState->TimeLimit / 60))) : NSLOCTEXT("General", "None", "None"));
}

void AUTGameMode::BuildServerResponseRules(FString& OutRules)
{
	// TODO: need to rework this so it can be displayed in the clien't local language
	OutRules += FString::Printf(TEXT("Goal Score\t%i\t"), GoalScore);
	OutRules += FString::Printf(TEXT("Time Limit\t%i\t"), int32(TimeLimit/60.0));
	OutRules += FString::Printf(TEXT("Allow Overtime\t%s\t"), bAllowOvertime ? TEXT("True") : TEXT("False"));
	OutRules += FString::Printf(TEXT("Forced Respawn\t%s\t"), bForceRespawn ?  TEXT("True") : TEXT("False"));
	OutRules += FString::Printf(TEXT("Only The Strong\t%s\t"), bOnlyTheStrongSurvive ? TEXT("True") : TEXT("False"));

	AUTMutator* Mut = BaseMutator;
	while (Mut)
	{
		OutRules += FString::Printf(TEXT("Mutator\t%s\t"), *Mut->DisplayName.ToString());
		Mut = Mut->NextMutator;
	}
}

void AUTGameMode::BlueprintBroadcastLocalized( AActor* Sender, TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	BroadcastLocalized(Sender, Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

void AUTGameMode::BlueprintSendLocalized( AActor* Sender, AUTPlayerController* Receiver, TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	Receiver->ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

void AUTGameMode::PrecacheAnnouncements(UUTAnnouncer* Announcer) const
{
	// slow but fairly reliable base implementation that looks up all local messages
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UUTLocalMessage::StaticClass()))
		{
			It->GetDefaultObject<UUTLocalMessage>()->PrecacheAnnouncements(Announcer);
		}
	}
}

void AUTGameMode::AssignDefaultSquadFor(AController* C)
{
	if (C != NULL)
	{
		if (SquadType == NULL)
		{
			UE_LOG(UT, Warning, TEXT("Game mode %s missing SquadType"), *GetName());
			SquadType = AUTSquadAI::StaticClass();
		}
		AUTPlayerState* PS = Cast<AUTPlayerState>(C->PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			PS->Team->AssignDefaultSquadFor(C);
		}
		else
		{
			// default is to just spawn a squad for each individual
			AUTBot* B = Cast<AUTBot>(C);
			if (B != NULL)
			{
				B->SetSquad(GetWorld()->SpawnActor<AUTSquadAI>(SquadType));
			}
		}
	}
}

FString AUTGameMode::GetDefaultLobbyOptions() const
{
	return DefaultLobbyOptions;
}

void AUTGameMode::NotifyLobbyGameIsReady()
{
	if (ensure(LobbyBeacon))
	{
		LobbyBeacon->Lobby_NotifyInstanceIsReady(LobbyInstanceID, ServerInstanceGUID);
	}
}

void AUTGameMode::UpdateLobbyMatchStats(FString Update)
{
	// Update the players

	UpdateLobbyPlayerList();
	UpdateLobbyBadge(TEXT(""));

	if (ensure(LobbyBeacon) && UTGameState)
	{
		// Add the time remaining command
		if (Update != TEXT("")) Update += TEXT("?");
		Update += FString::Printf(TEXT("GameTime=%i"), TimeLimit > 0 ? UTGameState->RemainingTime : UTGameState->ElapsedTime);
		LobbyBeacon->UpdateMatch(Update);
	}

	LastLobbyUpdateTime = GetWorld()->GetTimeSeconds();
}

void AUTGameMode::UpdateLobbyPlayerList()
{
	if (ensure(LobbyBeacon))
	{
		for (int i=0;i<UTGameState->PlayerArray.Num();i++)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
			if (PS && !PS->bIsSpectator)
			{
				int32 Score = int(PS->Score);
				LobbyBeacon->UpdatePlayer(PS->UniqueId, PS->PlayerName, Score);
			}
		}
	}
}

void AUTGameMode::UpdateLobbyBadge(FString BadgeText)
{
	if (BadgeText != "") BadgeText += TEXT("\n");

	AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	FString MapName = GetWorld()->GetMapName();
	if (WS)
	{
		const UUTLevelSummary* Summary = WS->GetLevelSummary();
		if ( Summary && Summary->Title != TEXT("") )
		{
			MapName = Summary->Title;
		}
	}

	BadgeText += FString::Printf(TEXT("<UWindows.Standard.MatchBadge.Small>%s</>\n<UWindows.Standard.MatchBadge.Small>%i Players</>"), *MapName, NumPlayers);

	if (BadgeText != TEXT("") && ensure(LobbyBeacon))
	{
		LobbyBeacon->Lobby_UpdateBadge(LobbyInstanceID, BadgeText);
	}

}


void AUTGameMode::SendEveryoneBackToLobby()
{
	// Game Instance Servers just tell everyone to just return to the lobby.
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AUTPlayerController* Controller = Cast<AUTPlayerController>(*Iterator);
		if (Controller)
		{
			Controller->ClientReturnToLobby();
		}
	}
}

#if !UE_SERVER
FString AUTGameMode::GetHUBPregameFormatString()
{
	return FString::Printf(TEXT("<UWindows.Standard.MatchBadge.Header>%s</>\n\n<UWindows.Standard.MatchBadge.Small>Host</>\n<UWindows.Standard.MatchBadge>{Player0Name}</>\n\n<UWindows.Standard.MatchBadge.Small>({NumPlayers} Players)</>"), *DisplayName.ToString());
}
#endif

void AUTGameMode::UpdatePlayersPresence()
{
	bool bAllowJoin = (NumPlayers < GameSession->MaxPlayers);
	UE_LOG(UT,Verbose,TEXT("AllowJoin: %i %i %i"), bAllowJoin, NumPlayers, GameSession->MaxPlayers);
	FString PresenceString = FText::Format(NSLOCTEXT("UTGameMode","PlayingPresenceFormatStr","Playing {0} on {1}"), DisplayName, FText::FromString(*GetWorld()->GetMapName())).ToString();
	for( FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator )
	{
		AUTPlayerController* Controller = Cast<AUTPlayerController>(*Iterator);
		if (Controller)
		{
			Controller->ClientSetPresence(PresenceString, bAllowJoin, bAllowJoin, bAllowJoin, false, IsGameInstanceServer());
		}
	}
}

#if !UE_SERVER
// TODO: use Attribs to make this live instead of fixed.
TSharedRef<SWidget> AUTGameMode::NewPlayerInfoLine(FString LeftStr, FString RightStr)
{
	TSharedPtr<SOverlay> Line;
	SAssignNew(Line, SOverlay)
	+SOverlay::Slot()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		[
			SNew(STextBlock)
			.Text(FText::FromString(LeftStr))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
			.ColorAndOpacity(FLinearColor::Gray)
		]
	]
	+ SOverlay::Slot()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text(FText::FromString(RightStr))
			.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
		]
	];

	return Line.ToSharedRef();
}

void AUTGameMode::BuildPlayerInfo(TSharedPtr<SVerticalBox> Panel, AUTPlayerState* PlayerState)
{
	Panel->AddSlot().Padding(30.0,5.0,30.0,0.0)
	[
		NewPlayerInfoLine(FString("Kills"), FString::Printf(TEXT("%i"), PlayerState->Kills))
	];
	Panel->AddSlot().Padding(30.0, 5.0, 30.0, 0.0)
	[
		NewPlayerInfoLine(FString("Deaths"), FString::Printf(TEXT("%i"), PlayerState->Deaths))
	];

}
#endif
