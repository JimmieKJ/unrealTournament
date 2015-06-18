// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUWStatsViewer.h"

#if !UE_SERVER

SUWStatsViewer::~SUWStatsViewer()
{
	if (OnlineUserCloudInterface.IsValid())
	{
		OnlineUserCloudInterface->ClearOnReadUserFileCompleteDelegate_Handle(OnReadUserFileCompleteDelegateHandle);
	}
	if (PlayerOwner.IsValid())
	{
		PlayerOwner->RemovePlayerOnlineStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);
		PlayerOnlineStatusChangedDelegate.Reset();
	}
}

void SUWStatsViewer::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("StatsViewer"));

	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
		OnlineUserCloudInterface = OnlineSubsystem->GetUserCloudInterface();
	}

	if (!PlayerOnlineStatusChangedDelegate.IsValid())
	{
		PlayerOnlineStatusChangedDelegate = PlayerOwner->RegisterPlayerOnlineStatusChangedDelegate(FPlayerOnlineStatusChanged::FDelegate::CreateSP(this, &SUWStatsViewer::OwnerLoginStatusChanged));
	}

	if (OnlineUserCloudInterface.IsValid())
	{
		OnReadUserFileCompleteDelegate.BindSP(this, &SUWStatsViewer::OnReadUserFileComplete);
		OnReadUserFileCompleteDelegateHandle = OnlineUserCloudInterface->AddOnReadUserFileCompleteDelegate_Handle(OnReadUserFileCompleteDelegate);
	}
		
	this->ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(WebBrowserBox, SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SAssignNew(StatsWebBrowser, SWebBrowser)
					.InitialURL(TEXT(""))
					.ShowControls(false)
				]
			]
		]
	];

	LastStatsDownloadTime = -1;
}

void SUWStatsViewer::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline(TEXT(""), TEXT(""), false);
	}
	else
	{
		DownloadStats();
	}
}

void SUWStatsViewer::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	if (NewStatus == ELoginStatus::LoggedIn)
	{
		DownloadStats();
	}
}

void SUWStatsViewer::DownloadStats()
{
	double TimeDiff = FApp::GetCurrentTime() - LastStatsDownloadTime;
	if (LastStatsDownloadTime > 0 && TimeDiff < 30.0)
	{
		return;
	}

	if (OnlineIdentityInterface.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(PlayerOwner->GetControllerId());
		if (UserId.IsValid())
		{
			StatsID = UserId->ToString();
			if (!StatsID.IsEmpty() && OnlineUserCloudInterface.IsValid())
			{
				LastStatsDownloadTime = FApp::GetCurrentTime();
				
				ReadBackendStats();

			}
		}
	}
}

void SUWStatsViewer::ReadBackendStatsComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	bool bShowErrorPage = true;

	if (HttpRequest.IsValid() && HttpResponse.IsValid())
	{
		if (bSucceeded && HttpResponse->GetResponseCode() == 200)
		{
			UE_LOG(LogGameStats, VeryVerbose, TEXT("%s"), *HttpResponse->GetContentAsString());

			// Have to hack around chrome access issues, can't open json from local disk, take JSON and turn into javascript variable
			FString JSONString = FString(TEXT("var BackendStats = ")) + HttpResponse->GetContentAsString() + TEXT(";");

			FString SavePath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/backendstats.json"));
			FArchive* FileOut = IFileManager::Get().CreateFileWriter(*SavePath);
			if (FileOut)
			{
				FileOut->Serialize(TCHAR_TO_ANSI(*JSONString), JSONString.Len());
				FileOut->Close();
			}

			// Read the cloud stats now
			// Invalidate the local cache, this seems to be the best way to do that
			OnlineUserCloudInterface->DeleteUserFile(FUniqueNetIdString(*StatsID), GetStatsFilename(), false, true);

			OnlineUserCloudInterface->ReadUserFile(FUniqueNetIdString(*StatsID), GetStatsFilename());
			
			bShowErrorPage = false;
		}
		else
		{
			UE_LOG(UT, Log, TEXT("%s"), *HttpResponse->GetContentAsString());
		}
	}

	if (bShowErrorPage)
	{
		ShowErrorPage();
	}
}

void SUWStatsViewer::ReadBackendStats()
{
	FHttpRequestPtr StatsReadRequest = FHttpModule::Get().CreateRequest();
	if (StatsReadRequest.IsValid())
	{
		FString BaseURL = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/stats/accountId/") + StatsID + TEXT("/bulk/window/alltime");
		FString McpConfigOverride;
		FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/stats/accountId/") + StatsID + TEXT("/bulk/window/alltime");
#endif

		if (McpConfigOverride == TEXT("localhost"))
		{
			BaseURL = TEXT("http://localhost:8080/ut/api/stats/accountId/") + StatsID + TEXT("/bulk/window/alltime");
		}
		else if (McpConfigOverride == TEXT("gamedev"))
		{
			BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/stats/accountId/") + StatsID + TEXT("/bulk/window/alltime");
		}

		StatsReadRequest->SetURL(BaseURL);
		StatsReadRequest->OnProcessRequestComplete().BindRaw(this, &SUWStatsViewer::ReadBackendStatsComplete);
		StatsReadRequest->SetVerb(TEXT("GET"));

		UE_LOG(LogGameStats, Verbose, TEXT("%s"), *BaseURL);

		if (OnlineIdentityInterface.IsValid())
		{
			FString AuthToken = OnlineIdentityInterface->GetAuthToken(0);
			StatsReadRequest->SetHeader(TEXT("Authorization"), FString(TEXT("bearer ")) + AuthToken);
		}
		StatsReadRequest->ProcessRequest();
	}
}

void SUWStatsViewer::OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName)
{
	// this notification is for us
	if (InUserId.ToString() == StatsID && FileName == GetStatsFilename())
	{
		bool bShowingStats = false;

		TArray<uint8> FileContents;
		if (bWasSuccessful && OnlineUserCloudInterface->GetFileContents(InUserId, FileName, FileContents) && FileContents.Num() > 0 && FileContents.GetData()[FileContents.Num() - 1] == 0)
		{
			// Get the html out of the Content dir
			FString HTMLPath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/stats.html"));
			FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*(FPaths::GameSavedDir() + TEXT("Stats/")), *(FPaths::GameContentDir() + TEXT("RestrictedAssets/UI/Stats/")), true);

			// Have to hack around chrome access issues, can't open json from local disk, take JSON and turn into javascript variable
			FString JSONString = FString(TEXT("var Stats = ")) + ANSI_TO_TCHAR((char*)FileContents.GetData()) + TEXT(";");

			FString SavePath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/stats.json"));
			FArchive* FileOut = IFileManager::Get().CreateFileWriter(*SavePath);
			if (FileOut)
			{
				FileOut->Serialize(TCHAR_TO_ANSI(*JSONString), JSONString.Len());
				FileOut->Close();


				// Have to hack around SWebBrowser not allowing url changes
				WebBrowserBox->RemoveSlot(StatsWebBrowser.ToSharedRef());
				WebBrowserBox->AddSlot()
					[
						SAssignNew(StatsWebBrowser, SWebBrowser)
						.InitialURL(TEXT("file://") + HTMLPath)
						.ShowControls(false)
					];

				bShowingStats = true;
			}
		}

		if (!bShowingStats)
		{
			ShowErrorPage();
		}
	}
}

void SUWStatsViewer::ShowErrorPage()
{
	FString HTMLPath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/nostats.html"));
	FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*(FPaths::GameSavedDir() + TEXT("Stats/")), *(FPaths::GameContentDir() + TEXT("RestrictedAssets/UI/Stats/")), true);

	// Have to hack around SWebBrowser not allowing url changes
	WebBrowserBox->RemoveSlot(StatsWebBrowser.ToSharedRef());
	WebBrowserBox->AddSlot()
		[
			SAssignNew(StatsWebBrowser, SWebBrowser)
			.InitialURL(TEXT("file://") + HTMLPath)
			.ShowControls(false)
		];
}

FString SUWStatsViewer::GetStatsFilename()
{
	return TEXT("stats.json");
}

#endif