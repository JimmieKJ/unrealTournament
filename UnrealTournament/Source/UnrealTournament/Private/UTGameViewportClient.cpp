// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameViewportClient.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWDialog.h"
#include "Slate/SUWInputBox.h"
#include "Slate/SUWRedirectDialog.h"
#include "Slate/SUTGameLayerManager.h"
#include "Engine/GameInstance.h"
#include "UTGameEngine.h"

UUTGameViewportClient::UUTGameViewportClient(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReconnectAfterDownloadingMapDelay = 0;
	VerifyFilesToDownloadAndReconnectDelay = 0;
}

void UUTGameViewportClient::AddViewportWidgetContent(TSharedRef<class SWidget> ViewportContent, const int32 ZOrder)
{
#if !UE_SERVER
	if ( !LayerManagerPtr.IsValid() )
	{
		TSharedRef<SUTGameLayerManager> LayerManager = SNew(SUTGameLayerManager);
		Super::AddViewportWidgetContent(LayerManager);

		LayerManagerPtr = LayerManager;
	}

	LayerManagerPtr.Pin()->AddLayer(ViewportContent, ZOrder);
#endif
}

void UUTGameViewportClient::RemoveViewportWidgetContent(TSharedRef<class SWidget> ViewportContent)
{
#if !UE_SERVER
	if ( LayerManagerPtr.IsValid() )
	{
		LayerManagerPtr.Pin()->RemoveLayer(ViewportContent);
	}
#endif
}

void UUTGameViewportClient::PeekTravelFailureMessages(UWorld* World, enum ETravelFailure::Type FailureType, const FString& ErrorString)
{
	Super::PeekTravelFailureMessages(World, FailureType, ErrorString);
#if !UE_SERVER
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);

	if (FailureType == ETravelFailure::PackageMissing)
	{
#if !PLATFORM_MAC
		// Missing map
		if (!ErrorString.IsEmpty() && ErrorString != TEXT("DownloadFiles"))
		{
			bool bAlreadyDownloaded = false;
			int32 SpaceIndex;
			FString URL;
			if (ErrorString.FindChar(TEXT(' '), SpaceIndex))
			{
				URL = ErrorString.Left(SpaceIndex);

				FString Checksum = ErrorString.RightChop(SpaceIndex + 1);
				FString BaseFilename = FPaths::GetBaseFilename(URL);

				// If it already exists with the correct checksum, just mount it again
				if (UTEngine && UTEngine->DownloadedContentChecksums.Contains(BaseFilename))
				{
					FString Path = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("DownloadedPaks"), *BaseFilename) + TEXT(".pak");
					if (UTEngine->DownloadedContentChecksums[BaseFilename] == Checksum)
					{
						if (FCoreDelegates::OnMountPak.IsBound())
						{
							FCoreDelegates::OnMountPak.Execute(Path, 0);
							UTEngine->MountedDownloadedContentChecksums.Add(Path, Checksum);
							bAlreadyDownloaded = true;
						}
					}
					else
					{
						// Delete the original file
						FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
					}
				}
			}
			else
			{
				URL = ErrorString;
			}

			if (GEngine && GEngine->PendingNetGameFromWorld(World))
			{
				LastAttemptedURL = GEngine->PendingNetGameFromWorld(World)->URL;
			}

			if (bAlreadyDownloaded)
			{
				ReconnectAfterDownloadingMapDelay = 0.5f;
				return;
			}
			else if (FPaths::GetExtension(URL) == FString(TEXT("pak")))
			{
				FirstPlayer->OpenDialog(SNew(SUWRedirectDialog)
					.OnDialogResult(FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::RedirectResult))
					.DialogTitle(NSLOCTEXT("UTGameViewportClient", "Redirect", "Download"))
					.RedirectToURL(URL)
					.PlayerOwner(FirstPlayer)
					);

				return;
			}
		}

		bool bMountedPreviousDownload = false;

		if (UTEngine &&	!UTEngine->ContentDownloadCloudId.IsEmpty() && UTEngine->FilesToDownload.Num() > 0)
		{
			LastAttemptedURL = UTEngine->LastURLFromWorld(World);

			TArray<FString> FileURLs;

			for (auto It = UTEngine->FilesToDownload.CreateConstIterator(); It; ++It)
			{
				bool bNeedsToDownload = true;

				if (UTEngine->MountedDownloadedContentChecksums.Contains(It.Key()))
				{
					FString Path = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("DownloadedPaks"), *It.Key()) + TEXT(".pak");

					// Unmount the pak
					if (FCoreDelegates::OnUnmountPak.IsBound())
					{
						FCoreDelegates::OnUnmountPak.Execute(Path);
					}

					// Remove the CRC entry
					UTEngine->MountedDownloadedContentChecksums.Remove(It.Key());
					UTEngine->DownloadedContentChecksums.Remove(It.Key());

					// Delete the original file
					FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*Path);
				}
				else if (UTEngine->DownloadedContentChecksums.Contains(It.Key()))
				{
					if (UTEngine->DownloadedContentChecksums[It.Key()] == It.Value())
					{
						FString Path = FPaths::Combine(*FPaths::GameSavedDir(), TEXT("DownloadedPaks"), *It.Key()) + TEXT(".pak");

						if (FCoreDelegates::OnMountPak.IsBound())
						{
							FCoreDelegates::OnMountPak.Execute(Path, 0);
							UTEngine->MountedDownloadedContentChecksums.Add(Path, It.Value());
							bNeedsToDownload = false;
							bMountedPreviousDownload = true;
						}
					}
				}

				if (bNeedsToDownload)
				{
					FString BaseURL = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/cloudstorage/user/");
					FString McpConfigOverride;
					FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride);
					if (McpConfigOverride == TEXT("gamedev"))
					{
						BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/cloudstorage/user/");
					}

					FileURLs.Add(BaseURL + UTEngine->ContentDownloadCloudId + TEXT("/") + It.Key() + TEXT(".pak"));
				}
			}

			if (FileURLs.Num() > 0)
			{
				FirstPlayer->OpenDialog(SNew(SUWRedirectDialog)
					.OnDialogResult(FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::CloudRedirectResult))
					.DialogTitle(NSLOCTEXT("UTGameViewportClient", "Redirect", "Download"))
					.RedirectURLs(FileURLs)
					.PlayerOwner(FirstPlayer)
					);
			}
			else if (bMountedPreviousDownload)
			{
				// Assume we mounted all the files that we needed
				VerifyFilesToDownloadAndReconnectDelay = 0.5f;
			}

			return;
		}
#endif
	}

	FText NetworkErrorMessage;

	switch (FailureType)
	{
		case ETravelFailure::PackageMissing: NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient", "TravelErrors_PackageMissing", "Package Missing"); break;

		default: NetworkErrorMessage = FText::FromString(ErrorString);
	}

	if (!ReconnectDialog.IsValid())
	{
		ReconnectDialog = FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient", "NetworkErrorDialogTitle", "Network Error"), NetworkErrorMessage, UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_RECONNECT, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
	}

#endif
}

void UUTGameViewportClient::PeekNetworkFailureMessages(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	static FName NAME_PendingNetDriver(TEXT("PendingNetDriver"));

	// ignore connection loss on game net driver if we have a pending one; this happens during standard blocking server travel
	if ( NetDriver->NetDriverName == NAME_GameNetDriver && GEngine->GetWorldContextFromWorld(World)->PendingNetGame != NULL &&
		(FailureType == ENetworkFailure::ConnectionLost || FailureType == ENetworkFailure::FailureReceived) )
	{
		return;
	}

	Super::PeekNetworkFailureMessages(World, NetDriver, FailureType, ErrorString);
#if !UE_SERVER

	// Don't care about net drivers that aren't the game net driver, they are probably just beacon net drivers
	if (NetDriver->NetDriverName != NAME_PendingNetDriver && NetDriver->NetDriverName != NAME_GameNetDriver)
	{
		return;
	}

	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.

	if (NetDriver->NetDriverName == NAME_PendingNetDriver)
	{
		FirstPlayer->CloseConnectingDialog();
	}

	FText NetworkErrorMessage;

	if (FailureType == ENetworkFailure::PendingConnectionFailure)
	{
		if (ErrorString == TEXT("NEEDPASS"))
		{

			UE_LOG(UT,Log,TEXT("%s %s"), *NetDriver->LowLevelGetNetworkNumber(), *GetNameSafe(GetWorld()));
			if (NetDriver != NULL && NetDriver->ServerConnection != NULL)
			{
				LastAttemptedURL = NetDriver->ServerConnection->URL;

				FirstPlayer->OpenDialog(SNew(SUWInputBox)
										.OnDialogResult( FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::ConnectPasswordResult))
										.PlayerOwner(FirstPlayer)
										.DialogTitle(NSLOCTEXT("UTGameViewportClient", "PasswordRequireTitle", "Password is Required"))
										.MessageText(NSLOCTEXT("UTGameViewportClient", "PasswordRequiredText", "This server requires a password:"))
										);
			}
		}
		else if (ErrorString == TEXT("TOOWEAK"))
		{
			FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","PreLoginError","Login Error"), NSLOCTEXT("UTGameViewportClient","WEAKMSG","You are not skilled enough to play on this server!"), UTDIALOG_BUTTON_OK,FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));	
			FirstPlayer->ShowMenu();
			return;
		}
		else if (ErrorString == TEXT("TOOSTRONG"))
		{
			FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","PreLoginError","Login Error"), NSLOCTEXT("UTGameViewportClient","STRONGMSG","Your skill is too high for this server!"), UTDIALOG_BUTTON_OK,FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));	
			FirstPlayer->ShowMenu();
			return;
		}
		else if (ErrorString == TEXT("NOTLOGGEDIN"))
		{
			// NOTE: It's possible that the player logged in during the connect sequence but after Prelogin was called on the client.  If this is the case, just reconnect.
			if (FirstPlayer->IsLoggedIn())
			{
				FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
				return;
			}
			
			// If we already have a reconnect message, then don't handle this
			if (!ReconnectDialog.IsValid())
			{
				ReconnectDialog = FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","LoginRequiredTitle","Login Required"), 
														   NSLOCTEXT("UTGameViewportClient","LoginRequiredMessage","You need to login to your Epic account before you can play on this server."), UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_RECONNECT, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::LoginFailureDialogResult));
			}
		}
		else if (ErrorString == TEXT("BANNED"))
		{
			FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient", "BannedFromServerTitle", "IMPORTANT"), NSLOCTEXT("UTGameViewportClient", "BannedFromServerMsg", "You have been banned from this server!"), UTDIALOG_BUTTON_OK, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
			FirstPlayer->ShowMenu();
			return;
		}

		// TODO: Explain to the engine team why you can't localize server error strings :(
		else if (ErrorString == TEXT("Server full."))
		{
			FirstPlayer->ShowMenu();
			FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","PreLoginError","Unable to Join"), NSLOCTEXT("UTGameViewportClient","SERVERFULL","The game you are trying to join is full!"), UTDIALOG_BUTTON_OK,FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));	
			return;
		}
		return;
	}


	switch (FailureType)
	{
		case ENetworkFailure::FailureReceived:
		case ENetworkFailure::ConnectionLost:
			NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ConnectionLost","Connection to server Lost!");
			break;
		case ENetworkFailure::ConnectionTimeout:
			NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ConnectionTimeout","Connection to server timed out!");
			break;
		case ENetworkFailure::OutdatedClient:
			NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ClientOutdated","Your client is outdated.  Please update your version of UT.  Go to Forums.UnrealTournament.com for more information.");
			break;
		case ENetworkFailure::OutdatedServer:
			NetworkErrorMessage = NSLOCTEXT("UTGameViewportClient","NetworkErrors_ServerOutdated","The server you are connecting to is running a different version of UT.  Make sure you have the latest version of UT.  Go to Forums.UnrealTournament.com for more information.");
			break;
		default:
			NetworkErrorMessage = FText::FromString(ErrorString);
			break;
	}

	if (!ReconnectDialog.IsValid())
	{
		if (FirstPlayer->GetCurrentMenu().IsValid())
		{
			FirstPlayer->HideMenu();
		}
		ReconnectDialog = FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient","NetworkErrorDialogTitle","Network Error"), NetworkErrorMessage, UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_RECONNECT, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
	}
#endif
}

void UUTGameViewportClient::FinalizeViews(FSceneViewFamily* ViewFamily, const TMap<ULocalPlayer*, FSceneView*>& PlayerViewMap)
{
	Super::FinalizeViews(ViewFamily, PlayerViewMap);
	
	// set special show flags when using the casting guide
	if (GameInstance != NULL)
	{
		const TArray<ULocalPlayer*> GamePlayers = GameInstance->GetLocalPlayers();
		if (GamePlayers.Num() > 0 && GamePlayers[0] != NULL)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(GamePlayers[0]->PlayerController);
			if (PC != NULL && PC->bCastingGuide)
			{
				ViewFamily->EngineShowFlags.PostProcessing = 0;
				ViewFamily->EngineShowFlags.Atmosphere = 0;
				ViewFamily->EngineShowFlags.DynamicShadows = 0;
				ViewFamily->EngineShowFlags.LightFunctions = 0;
				ViewFamily->EngineShowFlags.ReflectionEnvironment = 0;
				ViewFamily->EngineShowFlags.Specular = 0;
				for (TMap<ULocalPlayer*, FSceneView*>::TConstIterator It(PlayerViewMap); It; ++It)
				{
					It.Value()->SpecularOverrideParameter = FVector4(0.5f, 0.5f, 0.5f, 0.0f);
				}
			}
		}
	}
}

void UUTGameViewportClient::PostRender(UCanvas* Canvas)
{
#if WITH_EDITORONLY_DATA
	if (!GIsEditor)
#endif
	{
		// work around bug where we can end up with no PlayerController during initial connection (or when losing connection) by rendering an overlay to explain to the user what's going on
		TArray<ULocalPlayer*> GamePlayers;
		if (GameInstance != NULL)
		{
			GamePlayers = GameInstance->GetLocalPlayers();
			// remove players that aren't currently set up for proper rendering (no PlayerController or currently using temp proxy while waiting for server)
			for (int32 i = GamePlayers.Num() - 1; i >= 0; i--)
			{
				if (GamePlayers[i]->PlayerController == NULL || GamePlayers[i]->PlayerController->Player == NULL)
				{
					GamePlayers.RemoveAt(i);
				}
			}
		}
		if (GamePlayers.Num() > 0)
		{
			Super::PostRender(Canvas);
		}
		else
		{
			UFont* Font = GetDefault<AUTHUD>()->MediumFont;
			FText Message = NSLOCTEXT("UTHUD", "WaitingForServer", "Waiting for server to respond...");
			float XL, YL;
			Canvas->SetLinearDrawColor(FLinearColor::White);
			Canvas->TextSize(Font, Message.ToString(), XL, YL);
			Canvas->DrawText(Font, Message, (Canvas->ClipX - XL) * 0.5f, (Canvas->ClipY - YL) * 0.5f);
		}
	}
}

void UUTGameViewportClient::LoginFailureDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));
	if (ButtonID == UTDIALOG_BUTTON_OK)
	{
		if (FirstPlayer->IsLoggedIn())
		{
			// We are already Logged in.. Reconnect;
			FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
		}
		else
		{
			FirstPlayer->LoginOnline(TEXT(""),TEXT(""),false, false);
		}
	}
	else
	{
		FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
	}
}

void UUTGameViewportClient::RankDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	ReconnectDialog.Reset();
}


void UUTGameViewportClient::NetworkFailureDialogResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_RECONNECT)
	{
		UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));
		FirstPlayer->PlayerController->ConsoleCommand(TEXT("Reconnect"));
	}
	ReconnectDialog.Reset();
}

void UUTGameViewportClient::ConnectPasswordResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
#if !UE_SERVER
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUWInputBox> Box = StaticCastSharedPtr<SUWInputBox>(Widget);
		if (Box.IsValid())
		{
			FString InputText = Box->GetInputText();

			UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.
			if (!InputText.IsEmpty() && FirstPlayer != NULL)
			{
				FString ReconnectCommand = FString::Printf(TEXT("open %s:%i?password=%s"), *LastAttemptedURL.Host, LastAttemptedURL.Port, *InputText);
				FirstPlayer->PlayerController->ConsoleCommand(ReconnectCommand);
			}
		}
	}
#endif
}

void UUTGameViewportClient::ReconnectAfterDownloadingMap()
{
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.
	if (FirstPlayer != nullptr)
	{
		FString ReconnectCommand = FString::Printf(TEXT("open %s:%i"), *LastAttemptedURL.Host, LastAttemptedURL.Port);
		FirstPlayer->PlayerController->ConsoleCommand(ReconnectCommand);
	}
}

void UUTGameViewportClient::RedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
#if !UE_SERVER
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		ReconnectAfterDownloadingMap();
	}
#endif
}

void UUTGameViewportClient::VerifyFilesToDownloadAndReconnect()
{
#if !UE_SERVER
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.
	if (FirstPlayer != nullptr)
	{
		if (UTEngine)
		{
			for (auto It = UTEngine->FilesToDownload.CreateConstIterator(); It; ++It)
			{
				if (!UTEngine->MountedDownloadedContentChecksums.Contains(It.Key()))
				{
					// File failed to download at all.
					FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient", "DownloadFail", "Download Failed"), NSLOCTEXT("UTGameViewportClient", "DownloadFailMsg", "The download of cloud content failed."), UTDIALOG_BUTTON_OK, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
					return;
				}

				if (UTEngine->MountedDownloadedContentChecksums[It.Key()] != It.Value())
				{
					// File was the wrong checksum.
					FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient", "WrongChecksum", "Checksum failed"), NSLOCTEXT("UTGameViewportClient", "WrongChecksumMsg", "The files downloaded from the cloud do not match the files the server is using."), UTDIALOG_BUTTON_OK, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
					return;
				}
			}
		}

		FString ReconnectCommand = FString::Printf(TEXT("open %s:%i"), *LastAttemptedURL.Host, LastAttemptedURL.Port);
		FirstPlayer->PlayerController->ConsoleCommand(ReconnectCommand);
	}
#endif
}

void UUTGameViewportClient::CloudRedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
#if !UE_SERVER

	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		VerifyFilesToDownloadAndReconnect();
	}
	else
	{
		UUTLocalPlayer* FirstPlayer = Cast<UUTLocalPlayer>(GEngine->GetLocalPlayerFromControllerId(this, 0));	// Grab the first local player.
		if (FirstPlayer != nullptr)
		{
			FirstPlayer->ShowMessage(NSLOCTEXT("UTGameViewportClient", "UnableToGetCustomContent", "Server running custom content"), NSLOCTEXT("UTGameViewportClient", "UnableToGetCustomContentMsg", "The server is running custom content and your client was unable to download it."), UTDIALOG_BUTTON_OK, FDialogResultDelegate::CreateUObject(this, &UUTGameViewportClient::NetworkFailureDialogResult));
		}
	}
#endif
}

void UUTGameViewportClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ReconnectAfterDownloadingMapDelay > 0)
	{
		ReconnectAfterDownloadingMapDelay -= DeltaSeconds;
		if (ReconnectAfterDownloadingMapDelay <= 0)
		{
			ReconnectAfterDownloadingMap();
		}
	}

	if (VerifyFilesToDownloadAndReconnectDelay > 0)
	{
		VerifyFilesToDownloadAndReconnectDelay -= DeltaSeconds;
		if (VerifyFilesToDownloadAndReconnectDelay <= 0)
		{
			VerifyFilesToDownloadAndReconnect();
		}
	}
}
