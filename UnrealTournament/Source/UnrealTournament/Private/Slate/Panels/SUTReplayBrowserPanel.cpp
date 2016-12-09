// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "SUTReplayBrowserPanel.h"
#include "../Dialogs/SUTInputBoxDialog.h"
#include "Net/UnrealNetwork.h"
#include "BlueprintContextLibrary.h"
#include "PartyContext.h"
#include "Misc/NetworkVersion.h"

#if WITH_SOCIAL
#include "Social.h"
#endif

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
			ColumnText = NSLOCTEXT("SUTServerBrowserPanel", "UnknownColumnText", "n/a");
		}
	}

	return SNew(STextBlock)
		.Font(ItemEditorFont)
		.Text(ColumnText);
}

void SUTReplayBrowserPanel::FriendsListUpdated()
{
	FriendList.Empty();
	FriendStatIDList.Empty();

	FriendList.Add(MakeShareable(new FString(TEXT("My Replays"))));

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	IOnlineIdentityPtr OnlineIdentityInterface;
	if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
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
}

SUTReplayBrowserPanel::~SUTReplayBrowserPanel()
{
	//ISocialModule::Get().GetFriendsAndChatManager(TEXT(""), true)->OnFriendsListUpdated().Remove(FriendsListUpdatedDelegateHandle);
}

void SUTReplayBrowserPanel::ConstructPanel(FVector2D ViewportSize)
{
	Tag = FName(TEXT("ReplayBrowser"));

	bShouldShowAllReplays = false;
	bLiveOnly = false;
	bShowReplaysFromAllUsers = false;
	
	FriendsListUpdated();
	
//	FriendsListUpdatedDelegateHandle = ISocialModule::Get().GetFriendsAndChatManager(TEXT(""), true)->OnFriendsListUpdated().AddSP(this, &SUTReplayBrowserPanel::FriendsListUpdated);

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
					.OnGenerateRow(this, &SUTReplayBrowserPanel::OnGenerateWidgetForList)
					.OnSelectionChanged(this, &SUTReplayBrowserPanel::OnReplayListSelectionChanged)
					.OnMouseButtonDoubleClick(this, &SUTReplayBrowserPanel::OnListMouseButtonDoubleClick)
					.SelectionMode(ESelectionMode::Single)
					.HeaderRow
					(
						SNew(SHeaderRow)
						.Style(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header")

						+ SHeaderRow::Column("Name")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayNameColumn", "Name"))
							.ToolTipText(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayNameColumnToolTip", "Replay Name."))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("Length")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayLengthColumn", "Length"))
							.ToolTipText(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayLengthColumnToolTip", "Replay Length."))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("Date")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayDateColumn", "Date"))
							.ToolTipText(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayDateColumnToolTip", "Replay Date."))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("Live?")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayLiveColumn", "Live?"))
							.ToolTipText(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayLiveColumnToolTip", "Is this replay live?"))
							.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Header.TextStyle")
						]
						+ SHeaderRow::Column("NumViewers")
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayNumViewersColumn", "Viewer Count"))
							.ToolTipText(NSLOCTEXT("SUTReplayBrowserPanel", "ReplayNumViewersColumnToolTip", "How many people are watching."))
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

						.Text(NSLOCTEXT("SUTReplayBrowserPanel", "WatchReplay", "Watch This Replay"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUTReplayBrowserPanel::OnWatchClick)
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
						.OnGenerateWidget(this, &SUTReplayBrowserPanel::GenerateStringListWidget)
						.OnSelectionChanged(this, &SUTReplayBrowserPanel::OnFriendSelected)
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
						.Text(NSLOCTEXT("SUTReplayBrowserPanel", "MetaTagText", "MetaTag:"))
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
							.OnTextCommitted(this, &SUTReplayBrowserPanel::OnMetaTagTextCommited)
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
						.Text(NSLOCTEXT("SUTReplayBrowserPanel", "LiveOnly", "Live Games Only"))
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

						.Text(NSLOCTEXT("SUTReplayBrowserPanel", "RefreshList", "Refresh"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUTReplayBrowserPanel::OnRefreshClick)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(WatchReplayButton, SButton)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.BlankButton")
						.ContentPadding(FMargin(10.0f, 5.0f, 15.0f, 5.0))

						.Text(NSLOCTEXT("SUTReplayBrowserPanel", "OpenFromPlayerID", "Browse By Player ID"))
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.OnClicked(this, &SUTReplayBrowserPanel::OnPlayerIDClick)
					]
				]
			]
		]
	];

	ReplayStreamer = FNetworkReplayStreaming::Get().GetFactory().CreateReplayStreamer();
	WatchReplayButton->SetEnabled(false);
}

FReply SUTReplayBrowserPanel::OnPlayerIDClick()
{
	PlayerOwner->OpenDialog(SNew(SUTInputBoxDialog)
		.OnDialogResult(FDialogResultDelegate::CreateSP(this, &SUTReplayBrowserPanel::PlayerIDResult))
		.PlayerOwner(PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUTReplayBrowserPanel", "PlayerIDTitle", "Browse By Player ID"))
		.MessageText(NSLOCTEXT("SUTReplayBrowserPanel", "PlayerIDText", "Enter the Player ID:"))
		);

	return FReply::Handled();
}

void SUTReplayBrowserPanel::PlayerIDResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID != UTDIALOG_BUTTON_CANCEL)
	{
		TSharedPtr<SUTInputBoxDialog> Box = StaticCastSharedPtr<SUTInputBoxDialog>(Widget);
		if (Box.IsValid())
		{
			FString InputText = Box->GetInputText();
			BuildReplayList(InputText);
		}
	}
}

void SUTReplayBrowserPanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);
	
	//	MetaTagText->SetText(FText::FromString(MetaString));

	FString UserString = TEXT("");

	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	IOnlineIdentityPtr OnlineIdentityInterface;
	if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
	if (!bShowReplaysFromAllUsers && OnlineIdentityInterface.IsValid() && GetPlayerOwner() != nullptr)
	{
		UserString = GetPlayerOwner()->GetPreferredUniqueNetId()->ToString();
	}
	BuildReplayList(UserString);
}

void SUTReplayBrowserPanel::OnMetaTagTextCommited(const FText& NewText, ETextCommit::Type CommitType)
{
	MetaString = NewText.ToString();
	//BuildReplayList();
}

void SUTReplayBrowserPanel::BuildReplayList(const FString& UserId)
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

		ReplayStreamer->EnumerateStreams(Version, UserId, MetaString, FOnEnumerateStreamsComplete::CreateSP(this, &SUTReplayBrowserPanel::OnEnumerateStreamsComplete));
	}
}

bool SUTReplayBrowserPanel::CanWatchReplays()
{
	UPartyContext* PartyContext = Cast<UPartyContext>(UBlueprintContextLibrary::GetContext(PlayerOwner->GetWorld(), UPartyContext::StaticClass()));
	if (PartyContext)
	{
		const int32 PartySize = PartyContext->GetPartySize();
		if (PartySize > 1)
		{
			PlayerOwner->ShowToast(NSLOCTEXT("SUTReplayBrowserPanel", "NoReplaysInParty", "You may not do this while in a party."));
			return false;
		}
	}

	return true;
}

FReply SUTReplayBrowserPanel::OnWatchClick()
{
	if (!CanWatchReplays())
	{
		return FReply::Handled();
	}

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

FReply SUTReplayBrowserPanel::OnRefreshClick()
{
	//MetaString = MetaTagText->GetText().ToString();

	bLiveOnly = (LiveOnlyCheckbox->GetCheckedState() == ECheckBoxState::Checked);
	BuildReplayList(LastUserId);

	return FReply::Handled();
}

void SUTReplayBrowserPanel::OnEnumerateStreamsComplete(const TArray<FNetworkReplayStreamInfo>& Streams)
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

TSharedRef<ITableRow> SUTReplayBrowserPanel::OnGenerateWidgetForList(TSharedPtr<FReplayData> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SReplayBrowserRow, OwnerTable).ReplayData(InItem).Style(SUWindowsStyle::Get(), "UWindows.Standard.ServerBrowser.Row");
}

void SUTReplayBrowserPanel::OnListMouseButtonDoubleClick(TSharedPtr<FReplayData> SelectedReplay)
{
	if (!CanWatchReplays())
	{
		return;
	}

	if (PlayerOwner.IsValid() && PlayerOwner->GetWorld())
	{
		GEngine->Exec(PlayerOwner->GetWorld(), *FString::Printf(TEXT("DEMOPLAY %s"), *SelectedReplay->StreamInfo.Name));

		if (!PlayerOwner->IsMenuGame())
		{
			PlayerOwner->HideMenu();
		}
	}
}

void SUTReplayBrowserPanel::OnReplayListSelectionChanged(TSharedPtr<FReplayData> SelectedItem, ESelectInfo::Type SelectInfo)
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

TSharedRef<SWidget> SUTReplayBrowserPanel::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
			.Padding(5)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::White)
				.Text(FText::FromString(*InItem.Get()))
			];
}

void SUTReplayBrowserPanel::OnFriendSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 Index = INDEX_NONE;
	if (FriendList.Find(NewSelection, Index))
	{
		SelectedFriend->SetText(FText::FromString(*NewSelection));
		
		FString UserString;
		IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
		IOnlineIdentityPtr OnlineIdentityInterface;
		if (OnlineSubsystem) OnlineIdentityInterface = OnlineSubsystem->GetIdentityInterface();
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