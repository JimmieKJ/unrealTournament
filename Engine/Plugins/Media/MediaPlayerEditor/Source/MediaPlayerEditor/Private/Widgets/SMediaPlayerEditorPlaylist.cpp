// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "SMediaPlayerEditorPlaylist.h"
#include "SMediaSourceTableRow.h"


#define LOCTEXT_NAMESPACE "SMediaPlayerEditorMedia"


/* SMediaPlayerEditorPlaylist structors
 *****************************************************************************/

SMediaPlayerEditorPlaylist::SMediaPlayerEditorPlaylist()
	: MediaPlayer(nullptr)
{ }


SMediaPlayerEditorPlaylist::~SMediaPlayerEditorPlaylist()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->OnMediaEvent().RemoveAll(this);
	}

	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}


/* SMediaPlayerEditorPlaylist interface
 *****************************************************************************/

void SMediaPlayerEditorPlaylist::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = &InMediaPlayer;
	Style = InStyle;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(0.0f)
					[
						// message list
						SAssignNew(MediaSourceListView, SListView<TSharedPtr<FMediaSourceTableEntry>>)
							.ItemHeight(24.0f)
							.ListItemsSource(&MediaSourceList)
							.SelectionMode(ESelectionMode::Single)
							.OnGenerateRow(this, &SMediaPlayerEditorPlaylist::HandleMediaSourceListGenerateRow)
							.OnMouseButtonDoubleClick(this, &SMediaPlayerEditorPlaylist::HandleMediaSourceListDoubleClick)
							.HeaderRow
							(
								SNew(SHeaderRow)

								+ SHeaderRow::Column("Icon")
									.DefaultLabel(LOCTEXT("PlaylistIconColumnHeader", " "))
									.FixedWidth(12.0f)

								+ SHeaderRow::Column("Index")
									.DefaultLabel(LOCTEXT("PlaylistIndexColumnHeader", "#"))
									.FillWidth(0.1f)

								+ SHeaderRow::Column("Source")
									.DefaultLabel(LOCTEXT("PlaylistSourceColumnHeader", "Media Source"))
									.FillWidth(0.5f)

								+ SHeaderRow::Column("Type")
									.DefaultLabel(LOCTEXT("PlaylistTypeColumnHeader", "Type"))
									.FillWidth(0.4f)
							)
					]
			]
	];

	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SMediaPlayerEditorPlaylist::HandleCoreObjectPropertyChanged);
	MediaPlayer->OnMediaEvent().AddSP(this, &SMediaPlayerEditorPlaylist::HandleMediaPlayerMediaEvent);

	ReloadMediaSourceList();
}


/* SMediaPlayerEditorPlaylist implementation
 *****************************************************************************/

void SMediaPlayerEditorPlaylist::ReloadMediaSourceList()
{
	MediaSourceList.Reset();

	UMediaPlaylist* Playlist = MediaPlayer->GetPlaylist();

	if (Playlist != nullptr)
	{
		for (int32 EntryIndex = 0; EntryIndex < Playlist->Num(); ++EntryIndex)
		{
			MediaSourceList.Add(MakeShareable(new FMediaSourceTableEntry(EntryIndex, Playlist->Get(EntryIndex))));
		}
	}

	MediaSourceListView->RequestListRefresh();
}


/* SMediaPlayerEditorPlaylist callbacks
 *****************************************************************************/

void SMediaPlayerEditorPlaylist::HandleCoreObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& ChangedEvent)
{
	if ((Object != nullptr) && (Object == MediaPlayer->GetPlaylist()))
	{
		ReloadMediaSourceList();
	}
}


void SMediaPlayerEditorPlaylist::HandleMediaPlayerMediaEvent(EMediaEvent Event)
{
	if ((Event == EMediaEvent::MediaClosed) || (Event == EMediaEvent::MediaOpened))
	{
		ReloadMediaSourceList();
	}
}


void SMediaPlayerEditorPlaylist::HandleMediaSourceListDoubleClick(TSharedPtr<FMediaSourceTableEntry> InItem)
{
	if (InItem.IsValid())
	{
		MediaPlayer->OpenPlaylistIndex(MediaPlayer->GetPlaylist(), InItem->Index);
	}
}


TSharedRef<ITableRow> SMediaPlayerEditorPlaylist::HandleMediaSourceListGenerateRow(TSharedPtr<FMediaSourceTableEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SMediaSourceTableRow, OwnerTable)
		.Entry(Entry)
		.Opened_Lambda([=]() { return MediaPlayer->GetPlaylistIndex() == Entry->Index; })
		.Style(Style);
}


#undef LOCTEXT_NAMESPACE
