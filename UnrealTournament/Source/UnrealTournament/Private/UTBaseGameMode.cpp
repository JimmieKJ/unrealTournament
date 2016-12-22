// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "Menus/SUTInGameMenu.h"
#include "UTGameEngine.h"
#include "UTGameInstance.h"
#include "DataChannel.h"
#include "UTDemoRecSpectator.h"
#include "UTGameMessage.h"
#if WITH_PROFILE
#include "UtMcpProfileManager.h"
#endif

AUTBaseGameMode::AUTBaseGameMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	ReplaySpectatorPlayerControllerClass = AUTDemoRecSpectator::StaticClass();
}

void AUTBaseGameMode::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	GetWorldTimerManager().SetTimer(TimerHandle_DefaultTimer, this, &AUTBaseGameMode::DefaultTimer, GetWorldSettings()->GetEffectiveTimeDilation() / GetWorldSettings()->DemoPlayTimeDilation, true);
}

void AUTBaseGameMode::CreateServerID()
{
	// Create a server instance id for this server and save it out to the config file
	ServerInstanceGUID = FGuid::NewGuid();
	ServerInstanceID = ServerInstanceGUID.ToString();
	SaveConfig();
}

void AUTBaseGameMode::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_PROFILE
	if (!IsTemplate() && !IsRunningCommandlet())
	{
		McpProfileManager = NewObject<UUtMcpProfileManager>(this);
		GetMcpProfileManager()->Init(NULL, NULL, TEXT("Server"), FUtMcpRequestComplete());
	}
#endif
}

void AUTBaseGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	if (!PlayerPawnObject.IsNull())
	{
		DefaultPawnClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, *PlayerPawnObject.ToStringReference().ToString(), NULL, LOAD_NoWarn));
	}

	// Grab the InstanceID if it's there.
	LobbyInstanceID = UGameplayStatics::GetIntOption(Options, TEXT("InstanceID"), 0);

	// If we are a lobby instance, then we always want to generate a ServerInstanceID
	if (LobbyInstanceID > 0)
	{
		ServerInstanceGUID = FGuid::NewGuid();
	}
	else   // Otherwise, we want to try and load our instance id from the config so we are consistent.
	{
		if (ServerInstanceID.IsEmpty())
		{
			CreateServerID();
		}
		else
		{
			if ( !FGuid::Parse(ServerInstanceID, ServerInstanceGUID) )
			{
				UE_LOG(UT,Log,TEXT("WARNING: Could to import this server's previous ID.  A new one has been created so older links to this server will not work."));
				CreateServerID();
			}
		}
	}

	FString InOpt = UGameplayStatics::ParseOption(Options, TEXT("Private"));
	bPrivateMatch = EvalBoolOptions(InOpt, false);

	Super::InitGame(MapName, Options, ErrorMessage);

	if (UGameplayStatics::HasOption(Options, TEXT("ServerPassword")))
	{
		ServerPassword = UGameplayStatics::ParseOption(Options, TEXT("ServerPassword"));
	}
	if (UGameplayStatics::HasOption(Options, TEXT("SpectatePassword")))
	{
		SpectatePassword = UGameplayStatics::ParseOption(Options, TEXT("SpectatePassword"));
	}

	if (UGameplayStatics::HasOption(Options, TEXT("RconPassword")))
	{
		FString NewRconPassword = UGameplayStatics::ParseOption(Options, TEXT("RconPassword"));
		UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
		if (UTEngine && !NewRconPassword.IsEmpty()) UTEngine->RconPassword = NewRconPassword;
	}


	bRequirePassword = !ServerPassword.IsEmpty() || !SpectatePassword.IsEmpty();
	bTrainingGround = EvalBoolOptions(UGameplayStatics::ParseOption(Options, TEXT("TG")), bTrainingGround);

	if (UGameplayStatics::HasOption(Options, TEXT("ServerName")))
	{
		ServerNameOverride = UGameplayStatics::ParseOption(Options, TEXT("ServerName"));
		if (!ServerNameOverride.IsEmpty())
		{
			ServerNameOverride = ServerNameOverride.Replace(TEXT("\""), TEXT(""));		
		}
	}
	else
	{
		ServerNameOverride = TEXT("");
	}


	if (bTrainingGround)
	{
		UE_LOG(UT,Log,TEXT("=== This is a Training Ground Server.  It will only be visibly to beginners ==="));
	}


	UE_LOG(UT,Log,TEXT("Password: %i %s"), bRequirePassword, ServerPassword.IsEmpty() ? TEXT("NONE") : *ServerPassword)
}

void AUTBaseGameMode::InitGameState()
{
	Super::InitGameState();
	AUTGameState* GS = Cast<AUTGameState>(GameState);
	if (GS && !ServerNameOverride.IsEmpty())
	{
		GS->ServerInstanceGUID = ServerInstanceGUID;
		GS->ServerName = ServerNameOverride;
	}
}

FName AUTBaseGameMode::GetNextChatDestination(AUTPlayerState* PlayerState, FName CurrentChatDestination)
{
	if (CurrentChatDestination == ChatDestinations::Local) return ChatDestinations::Team;
	if (CurrentChatDestination == ChatDestinations::Team)
	{
		if (IsGameInstanceServer())
		{
			return ChatDestinations::Lobby;
		}
	}

	return ChatDestinations::Local;
}

void AUTBaseGameMode::GetInstanceData(TArray<TSharedPtr<FServerInstanceData>>& InstanceData)
{
}

int32 AUTBaseGameMode::GetNumPlayers()
{
	return NumPlayers;
}


int32 AUTBaseGameMode::GetNumMatches()
{
	return 1;
}

void AUTBaseGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	if ( !Options.IsEmpty() )
	{
		UE_LOG(UTConnection, Verbose, TEXT("PreLogin: %s"), *Options);
	}

	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// Allow our game session to validate that a player can play
	AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
	if (ErrorMessage.IsEmpty() && UTGameSession)
	{
		bool bJoinAsSpectator = FCString::Stricmp(*UGameplayStatics::ParseOption(Options, TEXT("SpectatorOnly")), TEXT("1")) == 0;
		UTGameSession->ValidatePlayer(Address, UniqueId, ErrorMessage, bJoinAsSpectator);
	}

	if (ErrorMessage.IsEmpty() && UniqueId.IsValid())
	{
		// precache the user's entitlements now so that we'll hopefully have them by the time they get in
		if (IOnlineSubsystem::Get() != NULL)
		{
			IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
			if (EntitlementInterface.IsValid())
			{
				EntitlementInterface->QueryEntitlements(*UniqueId.GetUniqueNetId().Get(), TEXT("ut"));
			}
#if WITH_PROFILE
			UMcpProfileGroup* Group = GetMcpProfileManager()->CreateProfileGroup(UniqueId, UniqueId.GetUniqueNetId(), true, false);
			UUtMcpProfile* Profile = Group->AddProfile<UUtMcpProfile>(EUtMcpProfile::ToProfileId(EUtMcpProfile::Profile, 0), false);
			FDedicatedServerUrlContext QueryContext = FDedicatedServerUrlContext::Default; // IMPORTANT to make a copy!
			Profile->ForceQueryProfile(QueryContext);
#endif
		}
	}
}

APlayerController* AUTBaseGameMode::Login(class UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	if ( !Options.IsEmpty() )
	{
		UE_LOG(UTConnection, Verbose, TEXT("Login: %s"), *Options);
	}

	// local players don't go through PreLogin()
	if (UniqueId.IsValid() && Cast<ULocalPlayer>(NewPlayer) != NULL && IOnlineSubsystem::Get() != NULL)
	{
		IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
		if (EntitlementInterface.IsValid())
		{
			// note that we need to redundantly query even if we already got this user's entitlements because they might have quit, bought some stuff, then come back
			EntitlementInterface->QueryEntitlements(*UniqueId.GetUniqueNetId().Get(), TEXT("ut"));
		}
#if WITH_PROFILE
		UMcpProfileGroup* Group = GetMcpProfileManager()->CreateProfileGroup(UniqueId, UniqueId.GetUniqueNetId(), true, false);
		UUtMcpProfile* Profile = Group->AddProfile<UUtMcpProfile>(EUtMcpProfile::ToProfileId(EUtMcpProfile::Profile, 0), false);
		FDedicatedServerUrlContext QueryContext = FDedicatedServerUrlContext::Default; // IMPORTANT to make a copy!
		Profile->ForceQueryProfile(QueryContext);
#endif
	}

	APlayerController* PC = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);

#if WITH_PROFILE
	if (PC != NULL)
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PC->PlayerState);
		if (PS != NULL && PS->UniqueId.IsValid())
		{
			UMcpProfileGroup* Group = GetMcpProfileManager()->CreateProfileGroup(UniqueId, UniqueId.GetUniqueNetId(), true, false);
			UUtMcpProfile* Profile = Cast<UUtMcpProfile>(Group->GetProfile(EUtMcpProfile::ToProfileId(EUtMcpProfile::Profile, 0)));
			AUTPlayerState::FMcpProfileSetter::Set(PS, Profile);
		}
	}
#endif

	return PC;
}

void AUTBaseGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	FString CloudID = GetCloudID();

	APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(GetWorld());
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(NewPlayer);
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (NewPlayer != LocalPC && PC && UTEngine)
	{
		PC->ClientRequireContentItemListBegin(CloudID);
		for (auto It = UTEngine->LocalContentChecksums.CreateConstIterator(); It; ++It)
		{
			PC->ClientRequireContentItem(It.Key(), It.Value());
		}
		PC->ClientRequireContentItemListComplete();
	}

	if ( NewPlayer && NewPlayer->PlayerState && !NewPlayer->PlayerState->PlayerName.IsEmpty() )
	{
		UE_LOG(UTConnection, Verbose, TEXT("PostLogin: %s (%s) Login Completed."), *NewPlayer->PlayerState->PlayerName, *NewPlayer->PlayerState->UniqueId.ToString() );
	}

}

void AUTBaseGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);

	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(C);
	if (PC)
	{
		PC->ClientGenericInitialization();
	}
}

void AUTBaseGameMode::ChangeName(AController* Other, const FString& S, bool bNameChange)
{
	// Cap player name at 15 characters...
	FString ClampedName = (S.Len() > 16) ? S.Left(16) : S;

	// Unicode 160 is an empty space, not sure what other characters are broken in our font
	int32 FindCharIndex;
	bool bForceAccountName = (ClampedName.FindChar(160, FindCharIndex) || ClampedName.FindChar(38, FindCharIndex)) || (FCString::Stricmp(TEXT("Player"), *ClampedName) == 0);

	AUTPlayerState* PS = Cast<AUTPlayerState>(Other->PlayerState);
	if (!PS || ((FCString::Stricmp(*PS->PlayerName, *ClampedName) == 0) && !bForceAccountName))
	{
		return;
	}

	// Look to see if someone else is using the the new name
	bool bNameMatchesAccount = false;
	AUTGameState* UTGameState = Cast<AUTGameState>(GameState);
	FText EpicAccountName = FText::GetEmpty();
	if (UTGameState)
	{
		TSharedRef<const FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(*PS->StatsID));
		EpicAccountName = UTGameState->GetEpicAccountNameForAccount(UserId);
		if (bForceAccountName)
		{
			PS->RequestedName = EpicAccountName.IsEmpty() ? ClampedName : TEXT("");
			ClampedName = EpicAccountName.IsEmpty() ? FString::Printf(TEXT("%s%i"), *DefaultPlayerName.ToString(), PS->PlayerId) : EpicAccountName.ToString();
		}
		bNameMatchesAccount = (EpicAccountName.ToString() == ClampedName);
	}
	else if (bForceAccountName)
	{
		ClampedName = TEXT("JCenaHLR");
	}
	for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
	{
		AController* Controller = Iterator->Get();
		if (Controller->PlayerState && (Controller->PlayerState != PS) && (FCString::Stricmp(*Controller->PlayerState->PlayerName, *ClampedName) == 0))
		{
			if (bNameMatchesAccount)
			{
				Controller->PlayerState->SetPlayerName("NOT" + Controller->PlayerState->PlayerName.Left(12));
			}
			else if (Cast<APlayerController>(Other) != NULL)
			{
				if (EpicAccountName.IsEmpty())
				{
					PS->RequestedName = ClampedName;
				}
				ClampedName = EpicAccountName.IsEmpty() ? FString::Printf(TEXT("%s%i"), *DefaultPlayerName.ToString(), PS->PlayerId) : EpicAccountName.ToString();
				break;
			}
		}
	}
	PS->SetPlayerName(ClampedName);
}

bool AUTBaseGameMode::FindRedirect(const FString& PackageName, FPackageRedirectReference& Redirect)
{
	FString BasePackageName = FPaths::GetBaseFilename(PackageName);
	for (int32 i = 0; i < RedirectReferences.Num(); i++)
	{
		FString BaseRedirectPackageName = FPaths::GetBaseFilename(RedirectReferences[i].PackageName);
		if (BasePackageName.Equals(BaseRedirectPackageName,ESearchCase::IgnoreCase))
		{
			Redirect = RedirectReferences[i];
			// if we know the checksum "for reals" then don't use the user specified one
			// TODO: probably the user one should just go away
			UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
			if (UTEngine != NULL)
			{
				const FString* FoundChecksum = UTEngine->MountedDownloadedContentChecksums.Find(BaseRedirectPackageName);
				if (FoundChecksum != NULL)
				{
					Redirect.PackageChecksum = *FoundChecksum;
				}
			}
			return true;
		}
	}
	return false;
}

void AUTBaseGameMode::GatherRequiredRedirects(TArray<FPackageRedirectReference>& Redirects)
{
/*
	// Fake redirects for sync testing
	for (int i=0; i < RedirectReferences.Num(); i++)
	{
		Redirects.AddUnique(RedirectReferences[i]);
	}
*/

	// map pak
	FPackageRedirectReference Redirect;
	if (FindRedirect(GetModPakFilenameFromPkg(GetOutermost()->GetName()), Redirect))
	{
		Redirects.AddUnique(Redirect);
	}
	// game class pak
	if (FindRedirect(GetModPakFilenameFromPkg(GetClass()->GetOutermost()->GetName()), Redirect))
	{
		Redirects.AddUnique(Redirect);
	}
}

void AUTBaseGameMode::GameWelcomePlayer(UNetConnection* Connection, FString& RedirectURL)
{
	TArray<FPackageRedirectReference> AllRedirects;
	GatherRequiredRedirects(AllRedirects);

	uint8 MessageType = UNMT_Redirect;
	for (const FPackageRedirectReference& Redirect : AllRedirects)
	{
		FString RedirectInfo = FString::Printf(TEXT("%s\n%s\n%s"), *Redirect.PackageName, *Redirect.ToString(), *Redirect.PackageChecksum);
		UE_LOG(UT, Verbose, TEXT("Send redirect: %s"), *RedirectInfo);
		FNetControlMessage<NMT_GameSpecific>::Send(Connection, MessageType, RedirectInfo);
	}

	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine != NULL) // in PIE this will happen
	{
		FString PackageName = Connection->ClientWorldPackageName.ToString();
		FString PackageBaseFilename = FPaths::GetBaseFilename(PackageName) + TEXT("-WindowsNoEditor");

		for (int32 i = 0; i < RedirectReferences.Num(); i++)
		{
			if (RedirectReferences[i].PackageName == PackageBaseFilename)
			{
				// already handled by GatherRequiredRedirects(), we just need to make sure not to use the CloudID stuff
				return;
			}
		}

		FString PackageChecksum;
		for (auto It = UTEngine->LocalContentChecksums.CreateConstIterator(); It; ++It)
		{
			if (It.Key() == PackageBaseFilename)
			{
				PackageChecksum = It.Value();
			}
		}

		FString CloudID = GetCloudID();
		if (!CloudID.IsEmpty() && !PackageChecksum.IsEmpty())
		{
			FString BaseURL = GetBackendBaseUrl();
			FString CommandURL = TEXT("/api/stats/accountId/");
			RedirectURL = BaseURL + CommandURL + GetCloudID() + TEXT("/") + PackageBaseFilename + TEXT(".pak") + TEXT(" ") + PackageChecksum;
		}
	}
}

FString AUTBaseGameMode::GetCloudID() const
{
	FString CloudID;

	APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(GetWorld());

	// For dedicated server, will need to pass stats id as a commandline parameter
	if (!FParse::Value(FCommandLine::Get(), TEXT("CloudID="), CloudID))
	{
		if (LocalPC)
		{
			UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(LocalPC->Player);
			IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
			if (OnlineSubsystem && LP)
			{
				IOnlineIdentityPtr OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
				if (OnlineIdentityInterface.IsValid())
				{
					TSharedPtr<const FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(LP->GetControllerId());
					if (UserId.IsValid())
					{
						CloudID = UserId->ToString();
					}
				}
			}
		}
	}

	return CloudID;
}

#if !UE_SERVER
/**
 *	Returns the Menu to popup when the user requests a menu
 **/
TSharedRef<SUTMenuBase> AUTBaseGameMode::GetGameMenu(UUTLocalPlayer* PlayerOwner) const
{
	return SNew(SUTInGameMenu).PlayerOwner(PlayerOwner);
}
#endif

void AUTBaseGameMode::SendRconMessage(const FString& DestinationId, const FString &Message)
{	
	AUTGameState* UTGameState = Cast<AUTGameState>(GameState);
	if (UTGameState)
	{
		for (int32 i = 0; i < UTGameState->PlayerArray.Num(); i++)
		{
			if ( DestinationId == TEXT("") || UTGameState->PlayerArray[i]->UniqueId.ToString() == DestinationId || UTGameState->PlayerArray[i]->PlayerName.Equals(DestinationId,ESearchCase::IgnoreCase) )
			{			
				AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(UTGameState->PlayerArray[i]);
				if (UTPlayerState)
				{
					UTPlayerState->ClientReceiveRconMessage(Message);
				}
			}
		}
	}
}

void AUTBaseGameMode::RconKick(const FString& NameOrUIDStr, bool bBan, const FString& Reason)
{
	AGameState* GS = GetWorld()->GetGameState<AGameState>();
	AGameSession* GSession = GameSession;
	if (GS && GSession)
	{
		for (int32 i=0; i < GS->PlayerArray.Num(); i++)
		{
			if ( (GS->PlayerArray[i]->PlayerName.ToLower() == NameOrUIDStr.ToLower()) ||
				 (GS->PlayerArray[i]->UniqueId.ToString() == NameOrUIDStr))
			{
				APlayerController* PC = Cast<APlayerController>(GS->PlayerArray[i]->GetOwner());
				if (PC)
				{
					if (bBan)
					{
						GSession->BanPlayer(PC,FText::FromString(Reason));
					}
					else
					{
						GSession->KickPlayer(PC, FText::FromString(Reason));
					}
				}
			}
		}
	}
}

void AUTBaseGameMode::RconAuth(AUTBasePlayerController* Admin, const FString& Password)
{
	if (Admin)
	{
		if (Admin->UTPlayerState && !Admin->UTPlayerState->bIsRconAdmin)
		{
			UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
			if ( (UTEngine && !UTEngine->RconPassword.IsEmpty() && UTEngine->RconPassword.Equals(Password, ESearchCase::CaseSensitive)) ||
				 (!GetDefault<UUTGameEngine>()->RconPassword.IsEmpty() && GetDefault<UUTGameEngine>()->RconPassword.Equals(Password, ESearchCase::CaseSensitive)) )
			{
				Admin->ClientSay(Admin->UTPlayerState, TEXT("Rcon authenticated!"), ChatDestinations::System);
				Admin->UTPlayerState->bIsRconAdmin = true;
				return;
			}
		}

		Admin->ClientSay(Admin->UTPlayerState, TEXT("Rcon password incorrect or unset"), ChatDestinations::System);
	}

}

void AUTBaseGameMode::RconNormal(AUTBasePlayerController* Admin)
{
	if (Admin && Admin->UTPlayerState->bIsRconAdmin)
	{
		Admin->ClientSay(Admin->UTPlayerState, TEXT("Rcon status removed!"), ChatDestinations::System);
		Admin->UTPlayerState->bIsRconAdmin = false;
	}
}

bool AUTBaseGameMode::IsValidElo(AUTPlayerState* PS, bool bRankedSession) const
{
	return (PS && (GetNumMatchesFor(PS, bRankedSession) >= 10));
}

uint8 AUTBaseGameMode::GetNumMatchesFor(AUTPlayerState* PS, bool bRankedSession) const
{
	if (!PS)
	{
		return 0;
	}
	uint8 MaxMatches = FMath::Max(PS->DMMatchesPlayed, PS->DuelMatchesPlayed);
	MaxMatches = FMath::Max(MaxMatches, PS->CTFMatchesPlayed);
	MaxMatches = FMath::Max(MaxMatches, PS->TDMMatchesPlayed);
	MaxMatches = FMath::Max(MaxMatches, PS->ShowdownMatchesPlayed);
	MaxMatches = FMath::Max(MaxMatches, PS->FlagRunMatchesPlayed);
	return MaxMatches;
}

int32 AUTBaseGameMode::GetEloFor(AUTPlayerState* PS, bool bRankedSession) const
{
	if (!PS)
	{
		return NEW_USER_ELO;
	}

	int32 MaxElo = 0;
	if (IsValidElo(PS, bRankedSession))
	{
		//only consider valid Elos
		if (PS->DuelMatchesPlayed >= 10)
		{
			MaxElo = FMath::Max(MaxElo, PS->DuelRank);
		}
		if (PS->ShowdownMatchesPlayed >= 10)
		{
			MaxElo = FMath::Max(MaxElo, PS->ShowdownRank);
		}
		if (PS->TDMMatchesPlayed >= 10)
		{
			MaxElo = FMath::Max(MaxElo, PS->TDMRank);
		}
		if (PS->DMMatchesPlayed >= 10)
		{
			MaxElo = FMath::Max(MaxElo, PS->DMRank);
		}
		if (PS->CTFMatchesPlayed >= 10)
		{
			MaxElo = FMath::Max(MaxElo, PS->CTFRank);
		}
		if (PS->FlagRunMatchesPlayed >= 10)
		{
			MaxElo = FMath::Max(MaxElo, PS->FlagRunRank);
		}
	}
	else
	{
		// return highest Elo
		MaxElo = FMath::Max(PS->DuelRank, PS->TDMRank);
		MaxElo = FMath::Max(MaxElo, PS->ShowdownRank);
		MaxElo = FMath::Max(MaxElo, PS->DMRank);
		MaxElo = FMath::Max(MaxElo, PS->CTFRank);
	}
	return MaxElo;
}

void AUTBaseGameMode::SetEloFor(AUTPlayerState* PS, bool bRankedSession, int32 NewEloValue, bool bIncrementMatchCount)
{
}


void AUTBaseGameMode::ReceivedRankForPlayer(AUTPlayerState* UTPlayerState)
{
	// By default we do nothing here.  Hubs do things with this functions, look at AUTLobbyGameMode::ReveivedRankForPlayer()
}

bool AUTBaseGameMode::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	if( FParse::Command( &Cmd, TEXT("dumpgame") ) )
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
		MakeJsonReport(JsonObject);

		FString Result;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
		Ar.Logf(TEXT("%s"), *Result);
		return true;
	}
	else if (FParse::Command( &Cmd, TEXT("dumpsession") ))
	{
		GLog->AddOutputDevice(&Ar);
		UE_SET_LOG_VERBOSITY(LogOnline, VeryVerbose);
		const IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
		if (OnlineSub)
		{
			const IOnlineSessionPtr SessionInt = OnlineSub->GetSessionInterface();
			if (SessionInt.IsValid())
			{
				SessionInt->DumpSessionState();
			}
		}
		UE_SET_LOG_VERBOSITY(LogOnline, Warning);
		GLog->RemoveOutputDevice(&Ar);
		return true;
	}
	else if (FParse::Command( &Cmd, TEXT("dumptime") ))
	{
		Ar.Logf(TEXT("TimeSeconds: %f   RealTimeSeconds: %f    DeltaSeconds: %f"), GetWorld()->GetTimeSeconds(), GetWorld()->GetRealTimeSeconds(), GetWorld()->GetDeltaSeconds());
		return true;
	}
	return Super::ProcessConsoleExec(Cmd, Ar, Executor);

}


void AUTBaseGameMode::MakeJsonReport(TSharedPtr<FJsonObject> JsonObject)
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState)
	{
		UTGameState->MakeJsonReport(JsonObject);
	}
}

void AUTBaseGameMode::CheckMapStatus(FString MapPackageName, bool& bIsEpicMap, bool& bIsMeshedMap, bool& bHasRights)
{
	bIsEpicMap = false;
	bIsMeshedMap = false;
	bHasRights = false;
	
	for (int32 i=0; i < EpicMapList.Num(); i++)
	{
		if (EpicMapList[i].MapPackageName.Equals(MapPackageName,ESearchCase::IgnoreCase) )
		{
			bIsEpicMap = EpicMapList[i].bIsEpicMap;
			bIsMeshedMap = EpicMapList[i].bIsMeshedMap;
			break;
		}
	}

	int32 Pos = INDEX_NONE;
	MapPackageName.FindLastChar('/', Pos);
	FString AssetName = (Pos == INDEX_NONE) ? MapPackageName : MapPackageName.Right(MapPackageName.Len() - Pos -1);
	FString EntitlementId = GetRequiredEntitlementFromPackageName(FName(*AssetName)); //GetRequiredEntitlementFromAsset(MapAsset);
	if ( !EntitlementId.IsEmpty() )
	{
		// This is a store map.  Look to see if the user has entitlements
		bHasRights = LocallyHasEntitlement(EntitlementId);
		if (!bIsEpicMap) bIsMeshedMap = true;
	}
	else
	{
		bHasRights = true;
		if (!bIsEpicMap) 
		{
			bIsMeshedMap = false;
		}
	}
}

bool AUTBaseGameMode::SupportsInstantReplay() const
{
	return false;
}