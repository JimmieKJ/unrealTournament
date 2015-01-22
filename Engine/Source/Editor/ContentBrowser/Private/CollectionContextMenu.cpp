// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserPCH.h"
#include "CollectionViewTypes.h"
#include "CollectionContextMenu.h"
#include "ISourceControlModule.h"
#include "ContentBrowserModule.h"
#include "SColorPicker.h"
#include "GenericCommands.h"


#define LOCTEXT_NAMESPACE "ContentBrowser"


FCollectionContextMenu::FCollectionContextMenu(const TWeakPtr<SCollectionView>& InCollectionView)
	: CollectionView(InCollectionView)
	, bProjectUnderSourceControl(false)
{
}

void FCollectionContextMenu::BindCommands(TSharedPtr< FUICommandList > InCommandList)
{
	InCommandList->MapAction( FGenericCommands::Get().Rename, FUIAction(
		FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteRenameCollection ),
		FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteRenameCollection )
		));
}

TSharedPtr<SWidget> FCollectionContextMenu::MakeCollectionListContextMenu(TSharedPtr< FUICommandList > InCommandList)
{
	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender> MenuExtenderDelegates = ContentBrowserModule.GetAllCollectionListContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute());
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, InCommandList, MenuExtender);

	UpdateProjectSourceControl();

	TArray<TSharedPtr<FCollectionItem>> CollectionList = CollectionView.Pin()->CollectionListPtr->GetSelectedItems();

	MenuBuilder.BeginSection("CollectionOptions", LOCTEXT("CollectionListOptionsMenuHeading", "Collection Options"));
	{
		// New... (submenu)
		MenuBuilder.AddSubMenu(
			LOCTEXT("NewCollection", "New..."),
			LOCTEXT("NewCollectionTooltip", "Create a collection."),
			FNewMenuDelegate::CreateRaw( this, &FCollectionContextMenu::MakeNewCollectionSubMenu )
			);

		// Only add rename/delete if at least one collection is selected
		if ( CollectionList.Num() )
		{
			bool bAnyManagedBySCC = false;
		
			for (int32 CollectionIdx = 0; CollectionIdx < CollectionList.Num(); ++CollectionIdx)
			{
				if ( CollectionList[CollectionIdx]->CollectionType != ECollectionShareType::CST_Local )
				{
					bAnyManagedBySCC = true;
					break;
				}
			}

			if ( CollectionList.Num() == 1 )
			{
				// Rename
				MenuBuilder.AddMenuEntry( FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameCollection", "Rename"), LOCTEXT("RenameCollectionTooltip", "Rename this collection."));
			}

			// Delete
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DestroyCollection", "Delete"),
				LOCTEXT("DestroyCollectionTooltip", "Delete this collection."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteDestroyCollection ),
					FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteDestroyCollection, bAnyManagedBySCC )
					)
				);

			if ( CollectionList.Num() == 1 )
			{
				// If any colors have already been set, display color options as a sub menu
				if ( CollectionViewUtils::HasCustomColors() )
				{
					// Set Color (submenu)
					MenuBuilder.AddSubMenu(
						LOCTEXT("SetColor", "Set Color"),
						LOCTEXT("SetCollectionColorTooltip", "Sets the color this collection should appear as."),
						FNewMenuDelegate::CreateRaw( this, &FCollectionContextMenu::MakeSetColorSubMenu )
						);
				}
				else
				{
					// Set Color
					MenuBuilder.AddMenuEntry(
						LOCTEXT("SetColor", "Set Color"),
						LOCTEXT("SetCollectionColorTooltip", "Sets the color this collection should appear as."),
						FSlateIcon(),
						FUIAction( FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecutePickColor ) )
						);
				}
			}
		}
	}
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

void FCollectionContextMenu::MakeNewCollectionSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("CollectionNewCollection", LOCTEXT("NewCollectionMenuHeading", "New Collection"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewCollection_Shared", "Shared Collection"),
			LOCTEXT("NewCollection_SharedTooltip", "Create a collection that can be seen by anyone."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteNewCollection, ECollectionShareType::CST_Shared ),
			FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteNewCollection, ECollectionShareType::CST_Shared )
			)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewCollection_Private", "Private Collection"),
			LOCTEXT("NewCollection_PrivateTooltip", "Create a collection that can only be seen by you."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteNewCollection, ECollectionShareType::CST_Private ),
			FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteNewCollection, ECollectionShareType::CST_Private )
			)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("NewCollection_Local", "Local Collection"),
			LOCTEXT("NewCollection_LocalTooltip", "Create a collection that is not in source control and can only be seen by you."),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteNewCollection, ECollectionShareType::CST_Local ),
			FCanExecuteAction::CreateSP( this, &FCollectionContextMenu::CanExecuteNewCollection, ECollectionShareType::CST_Local )
			)
			);
	}
	MenuBuilder.EndSection();
}

void FCollectionContextMenu::MakeSetColorSubMenu(FMenuBuilder& MenuBuilder)
{
	// New Color
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewColor", "New Color"),
		LOCTEXT("NewCollectionColorTooltip", "Changes the color this collection should appear as."),
		FSlateIcon(),
		FUIAction( FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecutePickColor ) )
		);

	// Clear Color (only required if any of the selection has one)
	if ( SelectedHasCustomColors() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearColor", "Clear Color"),
			LOCTEXT("ClearCollectionColorTooltip", "Resets the color this collection appears as."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &FCollectionContextMenu::ExecuteResetColor ) )
			);
	}

	// Add all the custom colors the user has chosen so far
	TArray< FLinearColor > CustomColors;
	if ( CollectionViewUtils::HasCustomColors( &CustomColors ) )
	{	
		MenuBuilder.BeginSection("PathContextCustomColors", LOCTEXT("CustomColorsExistingColors", "Existing Colors") );
		{
			for ( int32 ColorIndex = 0; ColorIndex < CustomColors.Num(); ColorIndex++ )
			{
				const FLinearColor& Color = CustomColors[ ColorIndex ];
				MenuBuilder.AddWidget(
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						[
							SNew(SButton)
							.ButtonStyle( FEditorStyle::Get(), "Menu.Button" )
							.OnClicked( this, &FCollectionContextMenu::OnColorClicked, Color )
							[
								SNew(SColorBlock)
								.Color( Color )
								.Size( FVector2D(77,16) )
							]
						],
					LOCTEXT("CustomColor", ""),
					/*bNoIndent=*/true
				);
			}
		}
		MenuBuilder.EndSection();
	}
}

void FCollectionContextMenu::UpdateProjectSourceControl()
{
	// Force update of source control so that we're always showing the valid options
	bProjectUnderSourceControl = false;
	if(ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable() && FPaths::IsProjectFilePathSet())
	{
		FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(FPaths::GetProjectFilePath(), EStateCacheUsage::ForceUpdate);
		bProjectUnderSourceControl = (SourceControlState->IsSourceControlled() && !SourceControlState->IsIgnored() && !SourceControlState->IsUnknown());
	}
}

bool FCollectionContextMenu::CanRenameSelectedCollections() const
{
	TArray<TSharedPtr<FCollectionItem>> CollectionList = CollectionView.Pin()->CollectionListPtr->GetSelectedItems();
	
	if(CollectionList.Num() == 1)
	{
		return !(CollectionList[0]->CollectionType != ECollectionShareType::CST_Local) || (bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable());
	}
	
	return false;
}

void FCollectionContextMenu::ExecuteNewCollection(ECollectionShareType::Type CollectionType)
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	CollectionView.Pin()->CreateCollectionItem(CollectionType);
}

void FCollectionContextMenu::ExecuteRenameCollection()
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	TArray<TSharedPtr<FCollectionItem>> CollectionList = CollectionView.Pin()->CollectionListPtr->GetSelectedItems();

	if ( !ensure(CollectionList.Num() == 1) )
	{
		return;
	}

	CollectionView.Pin()->RenameCollectionItem(CollectionList[0]);
}

void FCollectionContextMenu::ExecuteDestroyCollection()
{
	if ( !ensure(CollectionView.IsValid()) )
	{
		return;
	}

	TArray<TSharedPtr<FCollectionItem>> CollectionList = CollectionView.Pin()->CollectionListPtr->GetSelectedItems();

	FText Prompt;
	if ( CollectionList.Num() == 1 )
	{
		Prompt = FText::Format(LOCTEXT("CollectionDestroyConfirm_Single", "Delete {0}?"), FText::FromString(CollectionList[0]->CollectionName));
	}
	else
	{
		Prompt = FText::Format(LOCTEXT("CollectionDestroyConfirm_Multiple", "Delete {0} Collections?"), FText::AsNumber(CollectionList.Num()));
	}

	FOnClicked OnYesClicked = FOnClicked::CreateSP( this, &FCollectionContextMenu::ExecuteDestroyCollectionConfirmed, CollectionList );
	ContentBrowserUtils::DisplayConfirmationPopup(
		Prompt,
		LOCTEXT("CollectionDestroyConfirm_Yes", "Delete"),
		LOCTEXT("CollectionDestroyConfirm_No", "Cancel"),
		CollectionView.Pin().ToSharedRef(),
		OnYesClicked);
}

FReply FCollectionContextMenu::ExecuteDestroyCollectionConfirmed(TArray<TSharedPtr<FCollectionItem>> CollectionList)
{
	FCollectionManagerModule& CollectionManagerModule = FModuleManager::Get().LoadModuleChecked<FCollectionManagerModule>(TEXT("CollectionManager"));

	TArray<TSharedPtr<FCollectionItem>> ItemsToRemove;
	for (int32 CollectionIdx = 0; CollectionIdx < CollectionList.Num(); ++CollectionIdx)
	{
		const TSharedPtr<FCollectionItem>& CollectionItem = CollectionList[CollectionIdx];

		if ( CollectionManagerModule.Get().DestroyCollection(FName(*CollectionItem->CollectionName), CollectionItem->CollectionType) )
		{
			// Refresh the list now that the collection has been removed
			ItemsToRemove.Add(CollectionItem);
		}
		else
		{
			// Display a warning
			const FVector2D& CursorPos = FSlateApplication::Get().GetCursorPos();
			FSlateRect MessageAnchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);
			ContentBrowserUtils::DisplayMessage(
				FText::Format( LOCTEXT("CollectionDestroyFailed", "Failed to destroy collection. {0}"), CollectionManagerModule.Get().GetLastError() ),
				MessageAnchor,
				CollectionView.Pin()->CollectionListPtr.ToSharedRef()
				);
		}
	}

	if ( ItemsToRemove.Num() > 0 )
	{
		CollectionView.Pin()->RemoveCollectionItems(ItemsToRemove);
	}

	return FReply::Handled();
}

void FCollectionContextMenu::ExecuteResetColor()
{
	ResetColors();
}

void FCollectionContextMenu::ExecutePickColor()
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Spawn a color picker, so the user can select which color they want
	TArray<FLinearColor*> LinearColorArray;
	FColorPickerArgs PickerArgs;
	PickerArgs.bIsModal = false;
	PickerArgs.ParentWidget = CollectionViewPtr;
	if ( SelectedCollections.Num() > 0 )
	{
		// Make sure an color entry exists for all the collections, otherwise they won't update in realtime with the widget color
		for (int32 PathIdx = SelectedCollections.Num() - 1; PathIdx >= 0; --PathIdx)
		{
			const FCollectionNameType& SelectedCollection = SelectedCollections[PathIdx];
			TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
			if ( !Color.IsValid() )
			{
				Color = MakeShareable( new FLinearColor( CollectionViewUtils::GetDefaultColor() ) );
				CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, Color, true );
			}
			else
			{
				// Default the color to the first valid entry
				PickerArgs.InitialColorOverride = *Color.Get();
			}
			LinearColorArray.Add( Color.Get() );
		}	
		PickerArgs.LinearColorArray = &LinearColorArray;
	}
		
	PickerArgs.OnColorPickerWindowClosed = FOnWindowClosed::CreateSP(this, &FCollectionContextMenu::NewColorComplete);

	OpenColorPicker(PickerArgs);
}

bool FCollectionContextMenu::CanExecuteNewCollection(ECollectionShareType::Type CollectionType) const
{
	return (CollectionType == ECollectionShareType::CST_Local) || (bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable());
}

bool FCollectionContextMenu::CanExecuteRenameCollection() const
{
	return CanRenameSelectedCollections();
}

bool FCollectionContextMenu::CanExecuteDestroyCollection(bool bAnyManagedBySCC) const
{
	return !bAnyManagedBySCC || (bProjectUnderSourceControl && ISourceControlModule::Get().IsEnabled() && ISourceControlModule::Get().GetProvider().IsAvailable());
}

bool FCollectionContextMenu::SelectedHasCustomColors() const
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		// Ignore any that are the default color
		const TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
		if ( Color.IsValid() && !Color->Equals( CollectionViewUtils::GetDefaultColor() ) )
		{
			return true;
		}
	}
	return false;
}

void FCollectionContextMenu::NewColorComplete(const TSharedRef<SWindow>& Window)
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Save the colors back in the config (ptr should have already updated by the widget)
	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		const TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
		check( Color.IsValid() );
		CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, Color );
	}
}

FReply FCollectionContextMenu::OnColorClicked( const FLinearColor InColor )
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Make sure an color entry exists for all the collections, otherwise it can't save correctly
	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		TSharedPtr<FLinearColor> Color = CollectionViewUtils::LoadColor( SelectedCollection.Name.ToString(), SelectedCollection.Type );
		if ( !Color.IsValid() )
		{
			Color = MakeShareable( new FLinearColor() );
		}
		*Color.Get() = InColor;
		CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, Color );
	}

	// Dismiss the menu here, as we can't make the 'clear' option appear if a folder has just had a color set for the first time
	FSlateApplication::Get().DismissAllMenus();

	return FReply::Handled();
}

void FCollectionContextMenu::ResetColors()
{
	TSharedPtr<SCollectionView> CollectionViewPtr = CollectionView.Pin();
	TArray<FCollectionNameType> SelectedCollections = CollectionViewPtr->GetSelectedCollections();

	// Clear the custom colors for all the selected collections
	for(const FCollectionNameType& SelectedCollection : SelectedCollections)
	{
		CollectionViewUtils::SaveColor( SelectedCollection.Name.ToString(), SelectedCollection.Type, nullptr );
	}
}

#undef LOCTEXT_NAMESPACE
