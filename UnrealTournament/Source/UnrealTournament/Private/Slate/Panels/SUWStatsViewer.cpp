// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUWStatsViewer.h"

#if !UE_SERVER

SUWStatsViewer::~SUWStatsViewer()
{
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
	}

	if (!PlayerOnlineStatusChangedDelegate.IsValid())
	{
		PlayerOnlineStatusChangedDelegate = PlayerOwner->RegisterPlayerOnlineStatusChangedDelegate(FPlayerOnlineStatusChanged::FDelegate::CreateSP(this, &SUWStatsViewer::OwnerLoginStatusChanged));
	}

	QueryWindowList.Add(MakeShareable(new FString(TEXT("All Time"))));
	QueryWindowList.Add(MakeShareable(new FString(TEXT("Monthly"))));
	QueryWindowList.Add(MakeShareable(new FString(TEXT("Weekly"))));
	QueryWindowList.Add(MakeShareable(new FString(TEXT("Daily"))));
		
	FriendList.Add(MakeShareable(new FString(TEXT("My Stats")))); 
	
	if (OnlineIdentityInterface.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(PlayerOwner->GetControllerId());
		if (UserId.IsValid())
		{
			FriendStatIDList.Add(UserId->ToString());
		}
		else
		{
			FriendStatIDList.AddZeroed();
		}
	}
	else
	{
		FriendStatIDList.AddZeroed();
	}

	// Real friends
	TArray<FUTFriend> OnlineFriendsList;
	PlayerOwner->GetFriendsList(OnlineFriendsList);
	for (auto Friend : OnlineFriendsList)
	{
		FriendList.Add(MakeShareable(new FString(Friend.DisplayName)));
		FriendStatIDList.Add(Friend.UserId);
	}

	// Recent players
	TArray<FUTFriend> OnlineRecentPlayersList;
	PlayerOwner->GetRecentPlayersList(OnlineRecentPlayersList);
	for (auto RecentPlayer : OnlineRecentPlayersList)
	{
		FriendList.Add(MakeShareable(new FString(RecentPlayer.DisplayName)));
		FriendStatIDList.Add(RecentPlayer.UserId);
	}

	// Players in current game
	AUTGameState* GameState = GetPlayerOwner()->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		for (auto PlayerState : GameState->PlayerArray)
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS && !PS->StatsID.IsEmpty() && !FriendStatIDList.Contains(PS->StatsID))
			{
				FriendList.Add(MakeShareable(new FString(PS->PlayerName)));
				FriendStatIDList.Add(PS->StatsID);
			}
		}
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
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.Backdrop"))
				]
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(FriendListComboBox, SComboBox< TSharedPtr<FString> >)
					.InitiallySelectedItem(0)
					.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
					.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
					.OptionsSource(&FriendList)
					.OnGenerateWidget(this, &SUWStatsViewer::GenerateStringListWidget)
					.OnSelectionChanged(this, &SUWStatsViewer::OnFriendSelected)
					.Content()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(10.0f, 0.0f, 10.0f, 0.0f)
						[
							SAssignNew(SelectedFriend, STextBlock)
							.Text(FText::FromString(TEXT("Friends")))
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(QueryWindowComboBox, SComboBox< TSharedPtr<FString> >)
					.InitiallySelectedItem(0)
					.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
					.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
					.OptionsSource(&QueryWindowList)
					.OnGenerateWidget(this, &SUWStatsViewer::GenerateStringListWidget)
					.OnSelectionChanged(this, &SUWStatsViewer::OnQueryWindowSelected)
					.Content()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(10.0f, 0.0f, 10.0f, 0.0f)
						[
							SAssignNew(SelectedQueryWindow, STextBlock)
							.Text(FText::FromString(TEXT("Query Window")))
							.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
						]
					]
				]
			]
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

TSharedRef<SWidget> SUWStatsViewer::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::White)
			.Text(FText::FromString(*InItem.Get()))
		];
}

void SUWStatsViewer::SetQueryWindow(const FString& InQueryWindow)
{
	if (InQueryWindow != QueryWindow)
	{
		LastStatsDownloadTime = -1;
		QueryWindow = InQueryWindow;
	}
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

void SUWStatsViewer::SetStatsID(const FString& InStatsID)
{
	StatsID = InStatsID;
}

void SUWStatsViewer::DownloadStats()
{
	double TimeDiff = FApp::GetCurrentTime() - LastStatsDownloadTime;
	if (LastQueryWindowDownload == QueryWindow && 
		LastStatsIDDownload == StatsID && 
		LastStatsDownloadTime > 0 && TimeDiff < 30.0)
	{
		return;
	}

	// If stats ID is empty, grab our own stats
	if (StatsID.IsEmpty() && OnlineIdentityInterface.IsValid())
	{
		TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(PlayerOwner->GetControllerId());
		if (UserId.IsValid())
		{
			StatsID = UserId->ToString();
		}
	}

	if (!StatsID.IsEmpty())
	{
		LastStatsDownloadTime = FApp::GetCurrentTime();
		LastStatsIDDownload = StatsID;
		LastQueryWindowDownload = QueryWindow;
				
		FHttpRequestCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUWStatsViewer::ReadBackendStatsComplete);
		ReadBackendStats(Delegate, StatsID, QueryWindow);
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
			FString JSONString = FString(TEXT("var BackendStats = ")) + HttpResponse->GetContentAsString() + TEXT("; var QueryWindow=\"") + QueryWindow + TEXT("\";");

			FString SavePath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/backendstats.json"));
			FArchive* FileOut = IFileManager::Get().CreateFileWriter(*SavePath);
			if (FileOut)
			{
				FileOut->Serialize(TCHAR_TO_ANSI(*JSONString), JSONString.Len());
				FileOut->Close();
			}

			ReadCloudStats();
			
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

void SUWStatsViewer::ReadCloudStats()
{
	FHttpRequestPtr StatsReadRequest = FHttpModule::Get().CreateRequest();
	if (StatsReadRequest.IsValid())
	{
		FString BaseURL = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/cloudstorage/user/");
	}
	FString BaseURL = TEXT("https://ut-public-service-prod10.ol.epicgames.com/ut/api/cloudstorage/user/");

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/cloudstorage/user/");
#endif
	FString McpConfigOverride;
	FParse::Value(FCommandLine::Get(), TEXT("MCPCONFIG="), McpConfigOverride);
	if (McpConfigOverride == TEXT("localhost"))
	{
		BaseURL = TEXT("http://localhost:8080/ut/api/cloudstorage/user/");
	}
	else if (McpConfigOverride == TEXT("gamedev"))
	{
		BaseURL = TEXT("https://ut-public-service-gamedev.ol.epicgames.net/ut/api/cloudstorage/user/");
	}

	FString FinalStatsURL = BaseURL + StatsID + TEXT("/stats.json");

	StatsReadRequest->SetURL(FinalStatsURL);
	StatsReadRequest->OnProcessRequestComplete().BindSP(this, &SUWStatsViewer::ReadCloudStatsComplete);
	StatsReadRequest->SetVerb(TEXT("GET"));

	if (OnlineIdentityInterface.IsValid())
	{
		FString AuthToken = OnlineIdentityInterface->GetAuthToken(0);
		StatsReadRequest->SetHeader(TEXT("Authorization"), FString(TEXT("bearer ")) + AuthToken);
	}
	StatsReadRequest->ProcessRequest();
}

void SUWStatsViewer::ReadCloudStatsComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	bool bShowingStats = false;

	// Get the html out of the Content dir
	FString HTMLPath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/stats.html"));
	FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*(FPaths::GameSavedDir() + TEXT("Stats/")), *(FPaths::GameContentDir() + TEXT("RestrictedAssets/UI/Stats/")), true);

	if (bSucceeded)
	{
		const TArray<uint8> FileContents = HttpResponse->GetContent();
		// Have to hack around chrome access issues, can't open json from local disk, take JSON and turn into javascript variable
		FString JSONString = FString(TEXT("var Stats = ")) + ANSI_TO_TCHAR((char*)FileContents.GetData()) + TEXT(";");

		FString SavePath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/stats.json"));
		FArchive* FileOut = IFileManager::Get().CreateFileWriter(*SavePath);
		if (FileOut)
		{
			FileOut->Serialize(TCHAR_TO_ANSI(*JSONString), JSONString.Len());
			FileOut->Close();

			StatsWebBrowser->LoadURL(TEXT("file://") + HTMLPath);

			bShowingStats = true;
		}
	}
	else
	{
		// Couldn't read cloud stats, try to show just the backend stats

		// Have to hack around chrome access issues, can't open json from local disk, take JSON and turn into javascript variable
		FString JSONString = FString(TEXT("var Stats = {\"PlayerName\":\"") + SelectedFriend->GetText().ToString() + TEXT("\"};"));

		FString SavePath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/stats.json"));
		FArchive* FileOut = IFileManager::Get().CreateFileWriter(*SavePath);
		if (FileOut)
		{
			FileOut->Serialize(TCHAR_TO_ANSI(*JSONString), JSONString.Len());
			FileOut->Close();

			StatsWebBrowser->LoadURL(TEXT("file://") + HTMLPath);

			bShowingStats = true;
		}
	}

	if (!bShowingStats)
	{
		ShowErrorPage();
	}
}

void SUWStatsViewer::ShowErrorPage()
{
	FString HTMLPath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/nostats.html"));
	FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*(FPaths::GameSavedDir() + TEXT("Stats/")), *(FPaths::GameContentDir() + TEXT("RestrictedAssets/UI/Stats/")), true);

	StatsWebBrowser->LoadURL(TEXT("file://") + HTMLPath);
}

FString SUWStatsViewer::GetStatsFilename()
{
	return TEXT("stats.json");
}

void SUWStatsViewer::OnFriendSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Index = INDEX_NONE;
	if (FriendList.Find(NewSelection, Index))
	{
		SelectedFriend->SetText(FText::FromString(*NewSelection));
		StatsID = FriendStatIDList[Index];
		DownloadStats();
	}
}

void SUWStatsViewer::OnQueryWindowSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedQueryWindow->SetText(FText::FromString(*NewSelection));

	if (*NewSelection == TEXT("Monthly"))
	{
		QueryWindow = TEXT("monthly");
	}
	else if (*NewSelection == TEXT("Weekly"))
	{
		QueryWindow = TEXT("weekly");
	}
	else if (*NewSelection == TEXT("Daily"))
	{
		QueryWindow = TEXT("daily");
	}
	else
	{
		QueryWindow = TEXT("alltime");
	}
		
	DownloadStats();
}

#endif