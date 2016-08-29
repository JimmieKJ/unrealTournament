// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPCH.h"
#include "SMediaPlaylistEditorDetails.h"


#define LOCTEXT_NAMESPACE "SMediaPlaylistEditorDetails"


/* SMediaPlaylistEditorDetails interface
 *****************************************************************************/

void SMediaPlaylistEditorDetails::Construct(const FArguments& InArgs, UMediaPlaylist& InMediaPlaylist, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlaylist = &InMediaPlaylist;

	// initialize details view
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = true;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
	}

	TSharedPtr<IDetailsView> DetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(MediaPlaylist);

	ChildSlot
	[
		DetailsView.ToSharedRef()
	];
}


#undef LOCTEXT_NAMESPACE
