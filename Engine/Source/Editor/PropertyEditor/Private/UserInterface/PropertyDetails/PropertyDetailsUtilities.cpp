// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorPrivatePCH.h"
#include "PropertyDetailsUtilities.h"

FPropertyDetailsUtilities::FPropertyDetailsUtilities(IDetailsViewPrivate& InDetailsView)
	: DetailsView( InDetailsView )
{
}

class FNotifyHook* FPropertyDetailsUtilities::GetNotifyHook() const
{
	return DetailsView.GetNotifyHook();
}

bool FPropertyDetailsUtilities::AreFavoritesEnabled() const
{
	// not implemented
	return false;
}

void FPropertyDetailsUtilities::ToggleFavorite( const TSharedRef< FPropertyEditor >& PropertyEditor ) const
{
	// not implemented
}

void FPropertyDetailsUtilities::CreateColorPickerWindow( const TSharedRef< FPropertyEditor >& PropertyEditor, bool bUseAlpha ) const
{
	DetailsView.CreateColorPickerWindow( PropertyEditor, bUseAlpha );
}

void FPropertyDetailsUtilities::EnqueueDeferredAction( FSimpleDelegate DeferredAction )
{
	DetailsView.EnqueueDeferredAction( DeferredAction );
}

bool FPropertyDetailsUtilities::IsPropertyEditingEnabled() const
{
	return DetailsView.IsPropertyEditingEnabled();
}

void FPropertyDetailsUtilities::ForceRefresh()
{
	DetailsView.ForceRefresh();
}

void FPropertyDetailsUtilities::RequestRefresh()
{
	DetailsView.RefreshTree();
}

TSharedPtr<class FAssetThumbnailPool> FPropertyDetailsUtilities::GetThumbnailPool() const
{
	return DetailsView.GetThumbnailPool();
}

void FPropertyDetailsUtilities::NotifyFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	DetailsView.NotifyFinishedChangingProperties(PropertyChangedEvent);
}

bool FPropertyDetailsUtilities::DontUpdateValueWhileEditing() const
{
	return DetailsView.DontUpdateValueWhileEditing();
}