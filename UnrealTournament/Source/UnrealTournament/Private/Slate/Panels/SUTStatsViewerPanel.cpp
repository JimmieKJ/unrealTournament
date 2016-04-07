// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTStatsViewerPanel.h"
#include "UTMcpUtils.h"

#if !UE_SERVER

SUTStatsViewerPanel::~SUTStatsViewerPanel()
{
	if (PlayerOwner.IsValid())
	{
		PlayerOwner->RemovePlayerOnlineStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);
		PlayerOnlineStatusChangedDelegate.Reset();

		if (StatsWebBrowser.IsValid())
		{
			StatsWebBrowser->UnbindUObject(TEXT("StatsViewer"), PlayerOwner.Get());
		}
	}
}

void SUTStatsViewerPanel::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("StatsViewer"));

	if (!PlayerOnlineStatusChangedDelegate.IsValid())
	{
		PlayerOnlineStatusChangedDelegate = PlayerOwner->RegisterPlayerOnlineStatusChangedDelegate(FPlayerOnlineStatusChanged::FDelegate::CreateSP(this, &SUTStatsViewerPanel::OwnerLoginStatusChanged));
	}

	QueryWindowList.Add(MakeShareable(new FString(TEXT("All Time"))));
	QueryWindowList.Add(MakeShareable(new FString(TEXT("Monthly"))));
	QueryWindowList.Add(MakeShareable(new FString(TEXT("Weekly"))));
	QueryWindowList.Add(MakeShareable(new FString(TEXT("Daily"))));
	
	SetupFriendsList();

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
					.Image(SUWindowsStyle::Get().GetBrush("UT.Background.Black"))
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
					.OnGenerateWidget(this, &SUTStatsViewerPanel::GenerateStringListWidget)
					.OnSelectionChanged(this, &SUTStatsViewerPanel::OnFriendSelected)
					.Content()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.Padding(10.0f, 0.0f, 10.0f, 0.0f)
						[
							SAssignNew(SelectedFriend, STextBlock)
							.Text(FText::FromString(TEXT("Player to View")))
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
					.OnGenerateWidget(this, &SUTStatsViewerPanel::GenerateStringListWidget)
					.OnSelectionChanged(this, &SUTStatsViewerPanel::OnQueryWindowSelected)
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

	if (StatsWebBrowser.IsValid())
	{
		StatsWebBrowser->BindUObject(TEXT("StatsViewer"), PlayerOwner.Get());
	}
}

void SUTStatsViewerPanel::SetupFriendsList()
{
	FriendList.Empty();
	FriendStatIDList.Empty();

	FriendList.Add(MakeShareable(new FString(TEXT("My Stats"))));

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}

	if (OnlineIdentityInterface.IsValid())
	{
		TSharedPtr<const FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(PlayerOwner->GetControllerId());
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
}

TSharedRef<SWidget> SUTStatsViewerPanel::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::White)
			.Text(FText::FromString(*InItem.Get()))
		];
}

void SUTStatsViewerPanel::SetQueryWindow(const FString& InQueryWindow)
{
	if (InQueryWindow != QueryWindow)
	{
		LastStatsDownloadTime = -1;
		QueryWindow = InQueryWindow;
	}
}

void SUTStatsViewerPanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);

	if (!PlayerOwner->IsLoggedIn())
	{
		PlayerOwner->LoginOnline(TEXT(""), TEXT(""), false);
	}
	else
	{
		DownloadStats();
	}
}

void SUTStatsViewerPanel::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	if (NewStatus == ELoginStatus::LoggedIn)
	{
		StatsID.Empty();
		SetupFriendsList();
		DownloadStats();
	}
}

void SUTStatsViewerPanel::SetStatsID(const FString& InStatsID)
{
	StatsID = InStatsID;
}

void SUTStatsViewerPanel::DownloadStats()
{
	double TimeDiff = FApp::GetCurrentTime() - LastStatsDownloadTime;
	if (LastQueryWindowDownload == QueryWindow && 
		LastStatsIDDownload == StatsID && 
		LastStatsDownloadTime > 0 && TimeDiff < 30.0)
	{
		return;
	}

	TSharedPtr<const FUniqueNetId> LocalPlayerUserId;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}

	if (OnlineIdentityInterface.IsValid())
	{
		LocalPlayerUserId = OnlineIdentityInterface->GetUniquePlayerId(PlayerOwner->GetControllerId());
	}

	// If stats ID is empty, grab our own stats
	if (StatsID.IsEmpty())
	{
		if (LocalPlayerUserId.IsValid())
		{
			StatsID = LocalPlayerUserId->ToString();
		}
	}

	if (!StatsID.IsEmpty())
	{
		LastStatsDownloadTime = FApp::GetCurrentTime();
		LastStatsIDDownload = StatsID;
		LastQueryWindowDownload = QueryWindow;
				
		TArray<FString> MatchRatingTypes;
		MatchRatingTypes.Add(TEXT("SkillRating"));
		MatchRatingTypes.Add(TEXT("TDMSkillRating"));
		MatchRatingTypes.Add(TEXT("DMSkillRating"));
		MatchRatingTypes.Add(TEXT("CTFSkillRating"));
		MatchRatingTypes.Add(TEXT("ShowdownSkillRating"));
		UUTMcpUtils* McpUtils = UUTMcpUtils::Get(PlayerOwner->GetWorld(), LocalPlayerUserId);
		TWeakPtr<SUTStatsViewerPanel> StatsPanelPtr(SharedThis(this));
		FHttpRequestCompleteDelegate Delegate;
		Delegate.BindSP(this, &SUTStatsViewerPanel::ReadBackendStatsComplete);
		FString StatsIDCapture = StatsID;
		FString QueryWindowCapture = QueryWindow;
		McpUtils->GetBulkAccountMmr(MatchRatingTypes, [Delegate, StatsIDCapture, QueryWindowCapture](const FOnlineError& Result, const FBulkAccountMmr& Response)
		{
			if (Result.bSucceeded)
			{
				// Have to hack around chrome access issues, can't open json from local disk, take JSON and turn into javascript variable
				FString JSONString = FString(TEXT("var MmrStats = ["));
				for (int i = 0; i < Response.RatingTypes.Num(); i++)
				{
					if (i != 0)
					{
						JSONString += TEXT(",");
					}

					JSONString += TEXT("{\"ratingtype\":\"") + Response.RatingTypes[i] + TEXT("\", ");
					JSONString += TEXT("\"rating\":") + FString::FromInt(Response.Ratings[i]) + TEXT(", ");
					JSONString += TEXT("\"numgamesplayed\":") + FString::FromInt(Response.NumGamesPlayed[i]) + TEXT("}");
				}
				JSONString += TEXT("];");

				FString SavePath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/mmrstats.json"));
				FArchive* FileOut = IFileManager::Get().CreateFileWriter(*SavePath);
				if (FileOut)
				{
					FileOut->Serialize(TCHAR_TO_ANSI(*JSONString), JSONString.Len());
					FileOut->Close();
				}
			}

			ReadBackendStats(Delegate, StatsIDCapture, QueryWindowCapture);
		});
	}
}

void SUTStatsViewerPanel::ReadBackendStatsComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
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

void SUTStatsViewerPanel::ReadCloudStats()
{

	FHttpRequestPtr StatsReadRequest = FHttpModule::Get().CreateRequest();
	
	FString BaseURL = GetBackendBaseUrl();
	FString CommandURL = TEXT("/api/cloudstorage/user/");

	FString FinalStatsURL = BaseURL + CommandURL + StatsID + TEXT("/stats.json");

	StatsReadRequest->SetURL(FinalStatsURL);
	StatsReadRequest->OnProcessRequestComplete().BindSP(this, &SUTStatsViewerPanel::ReadCloudStatsComplete);
	StatsReadRequest->SetVerb(TEXT("GET"));

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}

	if (OnlineIdentityInterface.IsValid())
	{
		FString AuthToken = OnlineIdentityInterface->GetAuthToken(0);
		StatsReadRequest->SetHeader(TEXT("Authorization"), FString(TEXT("bearer ")) + AuthToken);
	}
	StatsReadRequest->ProcessRequest();
}

void SUTStatsViewerPanel::ReadCloudStatsComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
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

void SUTStatsViewerPanel::ShowErrorPage()
{
	FString HTMLPath = FPaths::ConvertRelativePathToFull(FPaths::GameSavedDir() + TEXT("Stats/nostats.html"));
	FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*(FPaths::GameSavedDir() + TEXT("Stats/")), *(FPaths::GameContentDir() + TEXT("RestrictedAssets/UI/Stats/")), true);

	StatsWebBrowser->LoadURL(TEXT("file://") + HTMLPath);
}

FString SUTStatsViewerPanel::GetStatsFilename()
{
	return TEXT("stats.json");
}

void SUTStatsViewerPanel::OnFriendSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Index = INDEX_NONE;
	if (FriendList.Find(NewSelection, Index))
	{
		//SelectedFriend->SetText(FText::FromString(*NewSelection));
		StatsID = FriendStatIDList[Index];
		DownloadStats();
	}
}

void SUTStatsViewerPanel::OnQueryWindowSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
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

void SUTStatsViewerPanel::ChangeStatsID(const FString& NewStatsID)
{
	if (!NewStatsID.IsEmpty())
	{
		QueryWindow = TEXT("alltime");
		StatsID = NewStatsID;
		DownloadStats();
	}
}

#endif