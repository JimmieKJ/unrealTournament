// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "SUWReplayBrowser.h"
#include "../SUWInputBox.h"
#include "Net/UnrealNetwork.h"

#if !UE_SERVER

TSharedRef<SWidget> SReplayBrowserRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	FSlateFontInfo ItemEditorFont = SUWindowsStyle::Get().GetFontStyle("UWindows.Standard.Font.Small"); //::Get().GetFontStyle(TEXT("NormalFont"));

	FText ColumnText;
	if (ReplayData.IsValid())
	{
		if (ColumnName == FName(TEXT("Name")))
		{
			return SNew(STextBlock)
				.Font(ItemEditorFont)
				.Text(FText::FromString(ReplayData->StreamInfo.FriendlyName));
		}
		else if (ColumnName == FName(TEXT("Date")))
		{
			return SNew(STextBlock)
				.Font(ItemEditorFont)
				.Text(FText::FromString(ReplayData->StreamInfo.Timestamp.ToString()));
		}
		else if (ColumnName == FName(TEXT("Length")))
		{
			return SNew(STextBlock)
				.Font(ItemEditorFont)
				.Text(FText::AsTimespan(FTimespan(0, 0, static_cast<int32>(ReplayData->StreamInfo.LengthInMS / 1000.f))));
		}
		else if (ColumnName == FName(TEXT("Live?")))
		{
			return SNew(STextBlock)
				.Font(ItemEditorFont)
				.Text(FText::FromString(ReplayData->StreamInfo.bIsLive ? TEXT("YES") : TEXT("NO")));
		}
		else if (ColumnName == FName(TEXT("NumViewers")))
		{
			return SNew(STextBlock)
				.Font(ItemEditorFont)
				.Text(FText::AsNumber(ReplayData->StreamInfo.NumViewers));
		}
		else
		{
			ColumnText = NSLOCTEXT("SUWServerBrowser", "UnknownColumnText", "n/a");
		}
	}

	return SNew(STextBlock)
		.Font(ItemEditorFont)
		.Text(ColumnText);
}

void SUWReplayBrowser::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("ReplayBrowser"));

	OnlineSubsystem = IOnlineSubsystem::Get();
	if (OnlineSubsystem)
	{
		OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	}

	bShouldShowAllReplays = false;
	bLiveOnly = false;
	bShowReplaysFromAllUsers = false;
	
	FriendList.Add(MakeShareable(new FString(TEXT("My Replays"))));

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

	FriendList.Add(MakeShareable(new FString(TEXT("All Replays"))));
	FriendStatIDList.AddZeroed();

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
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SImage)
					.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.ServerBrowser.Backdrop"))
				]
			]
		]

		+ SOverlay::Slot()
		[
			SNew(SBox) 
			.HeightOverride(500.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(25.0f, 0.0f, 0.0f, 0.0f)
				[
					// The list view being tested
					SAssignNew(ReplayListView, SListView< TSharedPtr<FReplayData> >)
					// List view items are this tall
					.ItemHeight(24)
					// Tell the list view where to get its source data
					.ListItemsSource(&ReplayList)
					// When the list view needs to generate a widget for some data item, use this method
					.OnGenerateRow(this, &SUWReplayBrowser::OnGenerateWidgetForList)
					.OnSelectionChanged(this, &SUWReplayBrowser::OnReplayListSelectionChanged)
					.OnMouseButtonDoubleClick(this, &SUWReplayBrowser::OnListMouseButtonDoubleClick)
					.SelectionMode(ESelectionMode::Single)
					.HeaderRow
					(
						SNew(SHeaderRow)
						.Style(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header")

						+ SHeaderRow::Column("Name")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUWReplayBrowser", "ReplayNameColumn", "Name"))
							.ToolTipText(NSLOCTEXT("SUWReplayBrowser", "ReplayNameColumnToolTip", "Replay Name."))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("Length")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUWReplayBrowser", "ReplayLengthColumn", "Length"))
							.ToolTipText(NSLOCTEXT("SUWReplayBrowser", "ReplayLengthColumnToolTip", "Replay Length."))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("Date")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUWReplayBrowser", "ReplayDateColumn", "Date"))
							.ToolTipText(NSLOCTEXT("SUWReplayBrowser", "ReplayDateColumnToolTip", "Replay Date."))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("Live?")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUWReplayBrowser", "ReplayLiveColumn", "Live?"))
							.ToolTipText(NSLOCTEXT("SUWReplayBrowser", "ReplayLiveColumnToolTip", "Is this replay live?"))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("NumViewers")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUWReplayBrowser", "ReplayNumViewersColumn", "Viewer Count"))
							.ToolTipText(NSLOCTEXT("SUWReplayBrowser", "ReplayNumViewersColumnToolTip", "How many people are watching."))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
					)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(25.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(WatchReplayButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
						.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))

						.Text(NSLOCTEXT("SUWReplayBrowser", "WatchReplay", "Watch This Replay"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUWReplayBrowser::OnWatchClick)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(25.0f, 0.0f, 0.0f, 0.0f)
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
						.OnGenerateWidget(this, &SUWReplayBrowser::GenerateStringListWidget)
						.OnSelectionChanged(this, &SUWReplayBrowser::OnFriendSelected)
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
					/*
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SUWReplayBrowser", "MetaTagText", "MetaTag:"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
					]					
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.HeightOverride(36)
						[
							SAssignNew(MetaTagText, SEditableTextBox)
							.Style(SUWindowsStyle::Get(), "UT.Common.Editbox")
							.OnTextCommitted(this, &SUWReplayBrowser::OnMetaTagTextCommited)
							.ClearKeyboardFocusOnCommit(false)
							.MinDesiredWidth(300.0f)
							.Text(FText::GetEmpty())
						]
					]
					*/
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWReplayBrowser", "LiveOnly", "Live Games Only"))
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(LiveOnlyCheckbox, SCheckBox)
						.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
						.ForegroundColor(FLinearColor::White)
						.IsChecked(ECheckBoxState::Checked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(WatchReplayButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
						.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))

						.Text(NSLOCTEXT("SUWReplayBrowser", "RefreshList", "Refresh"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUWReplayBrowser::OnRefreshClick)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(WatchReplayButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
						.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))

						.Text(NSLOCTEXT("SUWReplayBrowser", "OpenFromPlayerID", "Browse By Player ID"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUWReplayBrowser::OnPlayerIDClick)
					]
				]
			]
		]
	];

	FURL URL(TEXT("entry"));
	URL.AddOption(TEXT("Remote"));
	ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer(URL);
	WatchReplayButton->SetEnabled(false);
}

FReply SUWReplayBrowser::OnPlayerIDClick()
{
	PlayerOwner->OpenDialog(SNew(SUWInputBox)
		.OnDialogResult(FDialogResultDelegate::CreateSP(this, &SUWReplayBrowser::PlayerIDResult))
		.PlayerOwner(PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUWReplayBrowser", "PlayerIDTitle", "Browse By Player ID"))
		.MessageText(NSLOCTEXT("SUWReplayBrowser", "PlayerIDText", "Enter the Player ID:"))
		);

	return FReply::Handled();
}

void SUWReplayBrowser::PlayerIDResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUWInputBox> Box = StaticCastSharedPtr<SUWInputBox>(Widget);
		if (Box.IsValid())
		{
			FString InputText = Box->GetInputText();
			BuildReplayList(InputText);
		}
	}
}

void SUWReplayBrowser::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);
	
	//	MetaTagText->SetText(FText::FromString(MetaString));

	FString UserString = TEXT("");

	if (!bShowReplaysFromAllUsers && OnlineIdentityInterface.IsValid() && GetPlayerOwner() != nullptr)
	{
		UserString = GetPlayerOwner()->GetPreferredUniqueNetId()->ToString();
	}
	BuildReplayList(UserString);
}

void SUWReplayBrowser::OnMetaTagTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	MetaString = NewText.ToString();
	//BuildReplayList();
}

void SUWReplayBrowser::BuildReplayList(const FString& UserId)
{
	LastUserId = UserId;

	if (GetPlayerOwner() != nullptr && !GetPlayerOwner()->IsLoggedIn())
	{
		GetPlayerOwner()->LoginOnline( TEXT( "" ), TEXT( "" ) );
		return;
	}

	if (ReplayStreamer.IsValid())
	{
		FNetworkReplayVersion Version = FNetworkVersion::GetReplayVersion();
		if (bShouldShowAllReplays)
		{
			Version.NetworkVersion = 0;
			Version.Changelist = 0;
		}

		// Fix up the UI
		LiveOnlyCheckbox->SetIsChecked(bLiveOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

		ReplayStreamer->EnumerateStreams(Version, UserId, MetaString, FOnEnumerateStreamsComplete::CreateSP(this, &SUWReplayBrowser::OnEnumerateStreamsComplete));
	}
}

FReply SUWReplayBrowser::OnWatchClick()
{
	TArray<TSharedPtr<FReplayData>> SelectedReplays = ReplayListView->GetSelectedItems();
	if (SelectedReplays.Num() > 0)
	{
		if (PlayerOwner.IsValid() && PlayerOwner->GetWorld())
		{
			UE_LOG(UT, Verbose, TEXT("Watching stream %s %s"), *SelectedReplays[0]->StreamInfo.FriendlyName, *SelectedReplays[0]->StreamInfo.Name);
			GEngine->Exec(PlayerOwner->GetWorld(), *FString::Printf(TEXT("DEMOPLAY %s"), *SelectedReplays[0]->StreamInfo.Name));

			if (!PlayerOwner->IsMenuGame())
			{
				PlayerOwner->HideMenu();
			}
		}
	}

	return FReply::Handled();
}

FReply SUWReplayBrowser::OnRefreshClick()
{
	//MetaString = MetaTagText->GetText().ToString();

	bLiveOnly = (LiveOnlyCheckbox->GetCheckedState() == ECheckBoxState::Checked);
	BuildReplayList(LastUserId);

	return FReply::Handled();
}

void SUWReplayBrowser::OnEnumerateStreamsComplete(const TArray<FNetworkReplayStreamInfo>& Streams)
{
	ReplayList.Empty();

	for (const auto& StreamInfo : Streams)
	{
		if (StreamInfo.bIsLive && StreamInfo.LengthInMS < 15000)
		{
			UE_LOG(UT, Verbose, TEXT("Live stream found %s, but too short"), *StreamInfo.FriendlyName);
		}
		else
		{
			float SizeInKilobytes = StreamInfo.SizeInBytes / 1024.0f;

			TSharedPtr<FReplayData> NewDemoEntry = MakeShareable(new FReplayData());

			NewDemoEntry->StreamInfo = StreamInfo;
			NewDemoEntry->Date = StreamInfo.Timestamp.ToString(TEXT("%m/%d/%Y %h:%M %A"));	// UTC time
			NewDemoEntry->Size = SizeInKilobytes >= 1024.0f ? FString::Printf(TEXT("%2.2f MB"), SizeInKilobytes / 1024.0f) : FString::Printf(TEXT("%i KB"), (int)SizeInKilobytes);

			UE_LOG(UT, Verbose, TEXT("Stream found %s, %s, Live: %s"), *StreamInfo.FriendlyName, *StreamInfo.Name, StreamInfo.bIsLive ? TEXT("YES") : TEXT("NO"));

			if (bLiveOnly)
			{
				if (!StreamInfo.bIsLive)
				{
					continue;
				}
			}

			ReplayList.Add(NewDemoEntry);
		}
	}

	// Sort demo names by date
	struct FCompareDateTime
	{
		FORCEINLINE bool operator()(const TSharedPtr<FReplayData> & A, const TSharedPtr<FReplayData> & B) const
		{
			if (A->StreamInfo.bIsLive != B->StreamInfo.bIsLive)
			{
				return A->StreamInfo.bIsLive;
			}

			return A->StreamInfo.Timestamp.GetTicks() > B->StreamInfo.Timestamp.GetTicks();
		}
	};

	Sort(ReplayList.GetData(), ReplayList.Num(), FCompareDateTime());

	ReplayListView->RequestListRefresh();
}

TSharedRef<ITableRow> SUWReplayBrowser::OnGenerateWidgetForList(TSharedPtr<FReplayData> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SReplayBrowserRow, OwnerTable).ReplayData(InItem).Style(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Row");
}

void SUWReplayBrowser::OnListMouseButtonDoubleClick(TSharedPtr<FReplayData> SelectedReplay)
{
	if (PlayerOwner.IsValid() && PlayerOwner->GetWorld())
	{
		GEngine->Exec(PlayerOwner->GetWorld(), *FString::Printf(TEXT("DEMOPLAY %s"), *SelectedReplay->StreamInfo.Name));

		if (!PlayerOwner->IsMenuGame())
		{
			PlayerOwner->HideMenu();
		}
	}
}

void SUWReplayBrowser::OnReplayListSelectionChanged(TSharedPtr<FReplayData> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (SelectedItem.IsValid())
	{
		WatchReplayButton->SetEnabled(true);
	}
	else
	{
		WatchReplayButton->SetEnabled(false);
	}
}

TSharedRef<SWidget> SUWReplayBrowser::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
			.Padding(5)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::White)
				.Text(FText::FromString(*InItem.Get()))
			];
}

void SUWReplayBrowser::OnFriendSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Index = INDEX_NONE;
	if (FriendList.Find(NewSelection, Index))
	{
		SelectedFriend->SetText(FText::FromString(*NewSelection));
		
		FString UserString;
		if (Index == 0 && OnlineIdentityInterface.IsValid() && GetPlayerOwner() != nullptr)
		{
			UserString = GetPlayerOwner()->GetPreferredUniqueNetId()->ToString();
			bShowReplaysFromAllUsers = false;
		}
		else if (Index == 1)
		{
			bShowReplaysFromAllUsers = true;
			UserString.Empty();
		}
		else
		{
			bShowReplaysFromAllUsers = false;
			UserString = FriendStatIDList[Index];
		}

		bLiveOnly = (LiveOnlyCheckbox->GetCheckedState() == ECheckBoxState::Checked);

		BuildReplayList(UserString);
	}
}

#endif