// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NewsFeedPrivatePCH.h"
#include "ISettingsModule.h"
#include "SThrobber.h"
#include "SHyperlink.h"


#define LOCTEXT_NAMESPACE "SNewsFeed"


/* SNewsFeed structors
 *****************************************************************************/

SNewsFeed::~SNewsFeed( )
{
	GetMutableDefault<UNewsFeedSettings>()->OnSettingChanged().RemoveAll(this);

	NewsFeedCache->OnLoadCompleted().RemoveAll(this);
	NewsFeedCache->OnLoadFailed().RemoveAll(this);
}


/* SNewsFeed interface
 *****************************************************************************/

void SNewsFeed::Construct( const FArguments& InArgs, const FNewsFeedCacheRef& InNewsFeedCache )
{
	NewsFeedCache = InNewsFeedCache;
	HiddenNewsItemCount = 0;
	ShowingOlderNews = false;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot()
					[
						// news list
						SAssignNew(NewsFeedItemListView, SListView<FNewsFeedItemPtr>)
							.ItemHeight(64.0f)
							.ListItemsSource(&NewsFeedItemList)
							.OnGenerateRow(this, &SNewsFeed::HandleNewsListViewGenerateRow)
							.OnSelectionChanged(this, &SNewsFeed::HandleNewsListViewSelectionChanged)
							.SelectionMode(ESelectionMode::Single)
							.Visibility(this, &SNewsFeed::HandleNewsListViewVisibility)
							.HeaderRow
							(
								SNew(SHeaderRow)
									.Visibility(EVisibility::Collapsed)

								+ SHeaderRow::Column("Icon")
									.FixedWidth(48.0f)

								+ SHeaderRow::Column("Content")
							)
					]

				+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.Padding(FMargin(16.0f, 4.0f))
					[
						// no news available
						SNew(STextBlock)
							.Text(this, &SNewsFeed::HandleNewsUnavailableText)
							.Visibility(this, &SNewsFeed::HandleNewsUnavailableVisibility)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						SNew(SOverlay)

						+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								// settings button
								SNew(SButton)
									.ButtonStyle(FEditorStyle::Get(), "NoBorder")
									.ContentPadding(0.0f)
									.OnClicked(this, &SNewsFeed::HandleRefreshButtonClicked)
									.ToolTipText(LOCTEXT("RefreshButtonToolTip", "Reload the latest news from the server."))
									.Visibility(this, &SNewsFeed::HandleRefreshButtonVisibility)
									[
										SNew(SImage)
											.Image(FEditorStyle::GetBrush("NewsFeed.ReloadButton"))
									]
							]

						+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								// refresh throbber
								SNew(SCircularThrobber)
									.Radius(10.0f)
									.ToolTipText(LOCTEXT("RefreshThrobberToolTip", "Loading latest news..."))
									.Visibility(this, &SNewsFeed::HandleRefreshThrobberVisibility)
							]
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						// mark as read button
						SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ContentPadding(0.0f)
							.IsEnabled(this, &SNewsFeed::HandleMarkAllAsReadIsEnabled)
							.OnClicked(this, &SNewsFeed::HandleMarkAllAsReadClicked)
							.ToolTipText(LOCTEXT("MarkAsReadButtonToolTip", "Mark all news as read."))	
							[
								SNew(SImage)
									.Image(FEditorStyle::GetBrush("NewsFeed.MarkAsRead"))
							]
					]

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						// show older news hyper-link
						SNew(SHyperlink)
							.OnNavigate(this, &SNewsFeed::HandleShowOlderNewsNavigate)
							.Text(this, &SNewsFeed::HandleShowOlderNewsText)
							.ToolTipText(LOCTEXT("ShowOlderNewsToolTip", "Toggle visibility of news items that are below your threshold for recent news."))
							.Visibility(this, &SNewsFeed::HandleShowOlderNewsVisibility)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f)
					[
						// settings button
						SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ContentPadding(0.0f)
							.OnClicked(this, &SNewsFeed::HandleSettingsButtonClicked)
							.ToolTipText(LOCTEXT("SettingsButtonToolTip", "News feed settings."))	
							[
								SNew(SImage)
									.Image(FEditorStyle::GetBrush("NewsFeed.SettingsButton"))
							]
					]
			]
	];

	NewsFeedCache->OnLoadCompleted().AddRaw(this, &SNewsFeed::HandleNewsFeedLoaderLoadCompleted);
	NewsFeedCache->OnLoadFailed().AddRaw(this, &SNewsFeed::HandleNewsFeedLoaderLoadFailed);
	NewsFeedCache->LoadFeed();

	GetMutableDefault<UNewsFeedSettings>()->OnSettingChanged().AddSP(this, &SNewsFeed::HandleNewsFeedSettingsChanged);
}


/* SNewsFeed implementation
 *****************************************************************************/

void SNewsFeed::ReloadNewsFeedItems( )
{
	NewsFeedItemList.Empty();
	HiddenNewsItemCount = 0;

	const UNewsFeedSettings* NewsFeedSettings = GetDefault<UNewsFeedSettings>();
	const TArray<FNewsFeedItemPtr>& NewsFeedItems = NewsFeedCache->GetItems();

	for (auto NewsFeedItem : NewsFeedItems)
	{
		if (!GetDefault<UNewsFeedSettings>()->ShowOnlyUnreadItems || !NewsFeedItem->Read)
		{
			if (ShowingOlderNews || (NewsFeedItemList.Num() < NewsFeedSettings->MaxItemsToShow))
			{
				NewsFeedItemList.Add(NewsFeedItem);
			}
			else
			{
				++HiddenNewsItemCount;
			}
		}
	}

	NewsFeedItemListView->RequestListRefresh();
}


/* SNewsFeed callbacks
 *****************************************************************************/

EVisibility SNewsFeed::HandleLoadFailedBoxVisibility( ) const
{
	return NewsFeedCache->IsFinished() ? EVisibility::Collapsed : EVisibility::Visible;
}


FReply SNewsFeed::HandleMarkAllAsReadClicked( )
{
	UNewsFeedSettings* NewsFeedSettings = GetMutableDefault<UNewsFeedSettings>();

	for (auto NewsFeedItem : NewsFeedItemList)
	{
		NewsFeedSettings->ReadItems.AddUnique(NewsFeedItem->ItemId);
		NewsFeedItem->Read = true;
	}

	NewsFeedSettings->SaveConfig();

	if (NewsFeedSettings->ShowOnlyUnreadItems)
	{
		ReloadNewsFeedItems();
	}

	return FReply::Handled();
}


bool SNewsFeed::HandleMarkAllAsReadIsEnabled( ) const
{
	for (auto NewsFeedItem : NewsFeedItemList)
	{
		if (!NewsFeedItem->Read)
		{
			return true;
		}
	}

	return false;
}


void SNewsFeed::HandleNewsFeedLoaderLoadCompleted( )
{
	ReloadNewsFeedItems();
}


void SNewsFeed::HandleNewsFeedLoaderLoadFailed( )
{

}


void SNewsFeed::HandleNewsFeedSettingsChanged( FName PropertyName )
{
	ShowingOlderNews = false;
	ReloadNewsFeedItems();
}


TSharedRef<ITableRow> SNewsFeed::HandleNewsListViewGenerateRow( FNewsFeedItemPtr NewsFeedItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SNewsFeedListRow, OwnerTable, NewsFeedCache.ToSharedRef())
		.NewsFeedItem(NewsFeedItem);
}


void SNewsFeed::HandleNewsListViewSelectionChanged( FNewsFeedItemPtr Selection, ESelectInfo::Type SelectInfo )
{
	if (Selection.IsValid() && (SelectInfo == ESelectInfo::OnMouseClick))
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ItemID"), Selection->ItemId.ToString()));

		// execute item action
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

		if (DesktopPlatform != nullptr)
		{
			bool Succeeded = false;

			if (Selection->Url.StartsWith(TEXT("http://")) || Selection->Url.StartsWith(TEXT("https://")))
			{
				FPlatformProcess::LaunchURL(*Selection->Url, NULL, NULL);
				Succeeded = true;
			}
			else if (Selection->Url.StartsWith(TEXT("ue4://market/")))
			{
				Succeeded = DesktopPlatform->OpenLauncher(false, FString::Printf(TEXT("ue/marketplace/%s"), *Selection->Url.RightChop(13)), FString());
			}
//			else if (Selection->Url.StartsWith(TEXT("ue4://")))
//			{
//				Succeeded = DesktopPlatform->OpenLauncher(false, FString::Printf(TEXT("-OpenUrl=%s"), *Selection->Url));
//			}

			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("OpenUrlSucceeded"), Succeeded ? TEXT("TRUE") : TEXT("FALSE")));

			if (FEngineAnalytics::IsAvailable())
			{
				FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.OpenNewsFeedItem"), EventAttributes);
			}
		}

		// mark item as read
		UNewsFeedSettings* NewsFeedSettings = GetMutableDefault<UNewsFeedSettings>();
		NewsFeedSettings->ReadItems.AddUnique(Selection->ItemId);
		NewsFeedSettings->SaveConfig();
		Selection->Read = true;

		// refresh list view
		NewsFeedItemListView->SetSelection(nullptr);

		if (NewsFeedSettings->ShowOnlyUnreadItems)
		{
			ReloadNewsFeedItems();
		}
	}
}


FText SNewsFeed::HandleNewsUnavailableText( ) const
{
	if (GetDefault<UNewsFeedSettings>()->ShowOnlyUnreadItems)
	{
		return LOCTEXT("NoUnreadNewsAvailable", "There are currently no unread news");
	}

	return LOCTEXT("NoNewsAvailable", "There are currently no news");
}


EVisibility SNewsFeed::HandleNewsListViewVisibility( ) const
{
	return (NewsFeedItemList.Num() == 0) ? EVisibility::Collapsed : EVisibility::Visible;
}


EVisibility SNewsFeed::HandleNewsUnavailableVisibility( ) const
{
	return (NewsFeedItemList.Num() > 0) ? EVisibility::Collapsed : EVisibility::Visible;
}


FReply SNewsFeed::HandleRefreshButtonClicked( ) const
{
	NewsFeedCache->LoadFeed();

	return FReply::Handled();
}


EVisibility SNewsFeed::HandleRefreshButtonVisibility( ) const
{
	return NewsFeedCache->IsLoading() ? EVisibility::Hidden : EVisibility::Visible;
}


EVisibility SNewsFeed::HandleRefreshThrobberVisibility( ) const
{
	return NewsFeedCache->IsLoading() ? EVisibility::Visible : EVisibility::Hidden;
}


FReply SNewsFeed::HandleSettingsButtonClicked( ) const
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Editor", "LevelEditor", "NewsFeed");

	return FReply::Handled();
}


void SNewsFeed::HandleShowOlderNewsNavigate( )
{
	ShowingOlderNews = !ShowingOlderNews;
	ReloadNewsFeedItems();
}


FText SNewsFeed::HandleShowOlderNewsText( ) const
{
	if (ShowingOlderNews)
	{
		return LOCTEXT("HideOlderNewsHyperlink", "Hide older news");
	}

	return FText::Format(LOCTEXT("ShowOlderNewsHyperlink", "Show older news ({0})"), FText::AsNumber(HiddenNewsItemCount));
}


EVisibility SNewsFeed::HandleShowOlderNewsVisibility( ) const
{
	return (ShowingOlderNews || (HiddenNewsItemCount > 0)) ? EVisibility::Visible : EVisibility::Hidden;
}


#undef LOCTEXT_NAMESPACE
