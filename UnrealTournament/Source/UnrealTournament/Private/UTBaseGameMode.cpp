// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "UTGameEngine.h"

AUTBaseGameMode::AUTBaseGameMode(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void AUTBaseGameMode::InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage )
{
	// Grab the InstanceID if it's there.
	LobbyInstanceID = GetIntOption( Options, TEXT("InstanceID"), 0);

	// Create a server instance id for this server
	ServerInstanceGUID = FGuid::NewGuid();

	Super::InitGame(MapName, Options, ErrorMessage);

	ServerPassword = TEXT("");
	ServerPassword = ParseOption(Options, TEXT("ServerPassword"));
	bRequirePassword = !ServerPassword.IsEmpty();
	bTrainingGround = EvalBoolOptions(Options, bTrainingGround);

	
	UE_LOG(UT,Log,TEXT("Password: %i %s"), bRequirePassword, ServerPassword.IsEmpty() ? TEXT("NONE") : *ServerPassword)
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

int32 AUTBaseGameMode::GetInstanceData(TArray<FString>& HostNames, TArray<FString>& Descriptions)
{
	return 0;
}

int32 AUTBaseGameMode::GetNumPlayers()
{
	return NumPlayers;
}


int32 AUTBaseGameMode::GetNumMatches()
{
	return 1;
}

void AUTBaseGameMode::PreLogin(const FString& Options, const FString& Address, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	// Allow our game session to validate that a player can play
	AUTGameSession* UTGameSession = Cast<AUTGameSession>(GameSession);
	if (ErrorMessage.IsEmpty() && UTGameSession)
	{
		UTGameSession->ValidatePlayer(Address, UniqueId, ErrorMessage);
	}

	if (ErrorMessage.IsEmpty() && UniqueId.IsValid())
	{
		// precache the user's entitlements now so that we'll hopefully have them by the time they get in
		if (IOnlineSubsystem::Get() != NULL)
		{
			IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
			if (EntitlementInterface.IsValid())
			{
				EntitlementInterface->QueryEntitlements(*UniqueId.Get(), TEXT("ut"));
			}
		}
	}
}

APlayerController* AUTBaseGameMode::Login(class UPlayer* NewPlayer, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage)
{
	// local players don't go through PreLogin()
	if (UniqueId.IsValid() && Cast<ULocalPlayer>(NewPlayer) != NULL && IOnlineSubsystem::Get() != NULL)
	{
		IOnlineEntitlementsPtr EntitlementInterface = IOnlineSubsystem::Get()->GetEntitlementsInterface();
		if (EntitlementInterface.IsValid())
		{
			// note that we need to redundantly query even if we already got this user's entitlements because they might have quit, bought some stuff, then come back
			EntitlementInterface->QueryEntitlements(*UniqueId.Get(), TEXT("ut"));
		}
	}

	return Super::Login(NewPlayer, Portal, Options, UniqueId, ErrorMessage);
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
}

void AUTBaseGameMode::GenericPlayerInitialization(AController* C)
{
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(C);
	if (PC)
	{
		PC->ClientGenericInitialization();
	}
}

FString AUTBaseGameMode::GetRedirectURL(const FString& MapName) const
{
	for (int32 i = 0; i < RedirectReferences.Num(); i++)
	{
		if (RedirectReferences[i].MapName == MapName)
		{
			return RedirectReferences[i].MapURL + TEXT(" ") + RedirectReferences[i].MapChecksum;
		}
	}

	FString CloudID = GetCloudID();
	FString RedirectURL;

	if (!CloudID.IsEmpty())
	{
		FString BaseURL = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/cloudstorage/user/");
		FString McpConfigOverride;
		FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride);
		if (McpConfigOverride == TEXT("gamedev"))
		{
			BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/cloudstorage/user/");
		}

		FString MapBaseFilename = FPaths::GetBaseFilename(MapName);
		RedirectURL = BaseURL + GetCloudID() + TEXT("/") + MapBaseFilename + TEXT("-WindowsNoEditor.pak");
	}

	return RedirectURL;
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
					TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(LP->GetControllerId());
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
