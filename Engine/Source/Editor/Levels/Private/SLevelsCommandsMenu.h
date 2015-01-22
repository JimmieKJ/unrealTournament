// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LevelsModule.h"
#include "Editor/LevelEditor/Public/LevelEditorActions.h"
#include "Editor/ContentBrowser/Public/ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "LevelsCommands"

/**
 * The widget that represents a row in the LevelsView's list view control.  Generates widgets for each column on demand.
 */
class SLevelsCommandsMenu : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SLevelsCommandsMenu )
		:_CloseWindowAfterMenuSelection( true ) {}

		SLATE_ARGUMENT( bool, CloseWindowAfterMenuSelection )
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InImplementation	The UI logic not specific to slate
	 */
	void Construct( const FArguments& InArgs, const TSharedRef< FLevelCollectionViewModel > InViewModel )
	{
		ViewModel = InViewModel;
		const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();

		// Get all menu extenders for this context menu from the Levels module
		FLevelsModule& LevelsModule = FModuleManager::GetModuleChecked<FLevelsModule>( TEXT("Levels") );
		TArray<FLevelsModule::FLevelsMenuExtender> MenuExtenderDelegates = LevelsModule.GetAllLevelsMenuExtenders();

		TArray<TSharedPtr<FExtender>> Extenders;
		for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
		{
			if (MenuExtenderDelegates[i].IsBound())
			{
				Extenders.Add(MenuExtenderDelegates[i].Execute(ViewModel->GetCommandList()));
			}
		}
		TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

		FMenuBuilder MenuBuilder( InArgs._CloseWindowAfterMenuSelection, ViewModel->GetCommandList(), MenuExtender );
		{ 
			const TArray< TSharedPtr< FLevelViewModel > >& InvalidSelectedLevels = ViewModel->GetInvalidSelectedLevels();
			const TArray< TSharedPtr< FLevelViewModel > >& SelectedLevels = ViewModel->GetSelectedLevels();
			
			bool bOnlyInvalidSelectedLevel	= InvalidSelectedLevels.Num() == 1 && SelectedLevels.Num() == 0;
			bool bOnlySelectedLevel			= InvalidSelectedLevels.Num() == 0 && SelectedLevels.Num() == 1;
			
			if ( bOnlyInvalidSelectedLevel )
			{
				// We only show the level missing level commands if only 1 invalid level and no valid levels
				MenuBuilder.BeginSection("MissingLevel", LOCTEXT("ViewHeaderRemove", "Missing Level") );
				{
					MenuBuilder.AddMenuEntry( Commands.FixUpInvalidReference );
					MenuBuilder.AddMenuEntry( Commands.RemoveInvalidReference );
				}
				MenuBuilder.EndSection();
			}
			else if ( bOnlySelectedLevel )
			{
				const TSharedPtr< FLevelViewModel > Level = SelectedLevels[ 0 ];

				MenuBuilder.BeginSection("Level", FText::FromString( Level->GetName(/*bForceDisplayPath=*/true, /*bDisplayTags=*/false) ) );
				{
					MenuBuilder.AddMenuEntry( Commands.MakeLevelCurrent );
					MenuBuilder.AddMenuEntry( Commands.MoveActorsToSelected );
				}
				MenuBuilder.EndSection();
			}
				
			MenuBuilder.BeginSection("Levels", LOCTEXT("LevelsHeader", "Levels") );
			{
				MenuBuilder.AddMenuEntry( Commands.EditProperties );
				MenuBuilder.AddMenuEntry( Commands.SaveSelectedLevels );
				MenuBuilder.AddMenuEntry( Commands.MigrateSelectedLevels );

			if(ViewModel.IsValid())
			{
				MenuBuilder.AddSubMenu( 
					LOCTEXT("SourceControl", "Source Control"),
					LOCTEXT("SourceControl_ToolTip", "Source Control Options"),
					FNewMenuDelegate::CreateRaw(this, &SLevelsCommandsMenu::FillSourceControlMenu ) );
			}


				MenuBuilder.AddSubMenu( 
					LOCTEXT("ViewHeader", "View"),
					LOCTEXT("ViewSubMenu_ToolTip", "Level Browser View Settings"),
					FNewMenuDelegate::CreateRaw(this, &SLevelsCommandsMenu::FillViewMenu ) );
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("LevelsAddChangeStreaming");
			{
				MenuBuilder.AddSubMenu( 
					LOCTEXT("LevelsCreateHeader", "Add Level"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateRaw(this, &SLevelsCommandsMenu::FillAddLevelMenu ) );
				MenuBuilder.AddMenuEntry( Commands.RemoveSelectedLevels );

				if( InViewModel->GetSelectedLevels().Num() )
				{
					MenuBuilder.AddSubMenu( 
						LOCTEXT("LevelsChangeStreamingMethod", "Change Streaming Method"),
						LOCTEXT("LevelsChangeStreamingMethod_Tooltip", "Changes the streaming method for the selected levels"),
						FNewMenuDelegate::CreateRaw(this, &SLevelsCommandsMenu::FillSetStreamingMethodMenu ) );
				}

				MenuBuilder.AddSubMenu( 
					LOCTEXT("LevelsStreamingVolumesMethod", "Streaming Volumes"),
					LOCTEXT("LevelsStreamingVolumesMethod_Tooltip", "Streaming Volume commands"),
					FNewMenuDelegate::CreateRaw( this, &SLevelsCommandsMenu::FillStreamingVolumesMenu ) );
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("LevelsVisibility", LOCTEXT("VisibilityHeader", "Visibility") );
			{
				MenuBuilder.AddMenuEntry( Commands.ShowSelectedLevels );
				MenuBuilder.AddMenuEntry( Commands.HideSelectedLevels );
				MenuBuilder.AddMenuEntry( Commands.ShowOnlySelectedLevels );
				MenuBuilder.AddMenuEntry( Commands.ShowAllLevels );
				MenuBuilder.AddMenuEntry( Commands.HideAllLevels );
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("LevelsLock");
			{
				MenuBuilder.AddSubMenu( 
					LOCTEXT("LockHeader", "Lock"),
					LOCTEXT("LockSubMenu_ToolTip", "Selected Level(s) lock commands"),
					FNewMenuDelegate::CreateRaw(this, &SLevelsCommandsMenu::FillLockMenu ) );
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("LevelsSelection", LOCTEXT("SelectionHeader", "Selection") );
			{
				MenuBuilder.AddMenuEntry( Commands.SelectAllLevels );
				MenuBuilder.AddMenuEntry( Commands.DeselectAllLevels );
				MenuBuilder.AddMenuEntry( Commands.InvertLevelSelection );
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("ActorsSelection", LOCTEXT("ActorsHeader", "Actors") );
			{
				MenuBuilder.AddMenuEntry( Commands.SelectLevelActors );
				MenuBuilder.AddMenuEntry( Commands.DeselectLevelActors );
			}
			MenuBuilder.EndSection();
		}

		ChildSlot
		[
			MenuBuilder.MakeWidget()
		];
	}


private:

	/** Populates the 'Add' menu */
	void FillAddLevelMenu( class FMenuBuilder& MenuBuilder )
	{
		const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();

		MenuBuilder.BeginSection("LevelsAddLevel");
		{
			MenuBuilder.AddMenuEntry( Commands.CreateEmptyLevel );
			MenuBuilder.AddMenuEntry( Commands.AddExistingLevel );
			MenuBuilder.AddMenuEntry( Commands.AddSelectedActorsToNewLevel );
			MenuBuilder.AddMenuEntry( Commands.MergeSelectedLevels );
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("LevelsStreamingMethod", LOCTEXT("LevelsStreamingMethod", "Streaming Method"));
		{
			MenuBuilder.AddMenuEntry( Commands.SetAddStreamingMethod_Blueprint, NAME_None, LOCTEXT("SetAddStreamingMethodBlueprintOverride", "Blueprint") );
			MenuBuilder.AddMenuEntry( Commands.SetAddStreamingMethod_AlwaysLoaded, NAME_None, LOCTEXT("SetAddStreamingMethodAlwaysLoadedOverride", "Always Loaded") );
		}
		MenuBuilder.EndSection();
	}

	/** Populates the 'Lock' menu */
	void FillLockMenu( class FMenuBuilder& MenuBuilder )
	{
		const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();

		MenuBuilder.AddMenuEntry( Commands.LockSelectedLevels );
		MenuBuilder.AddMenuEntry( Commands.UnockSelectedLevels );
		MenuBuilder.AddMenuEntry( Commands.LockAllLevels );
		MenuBuilder.AddMenuEntry( Commands.UnockAllLevels );

		if(GEditor->bLockReadOnlyLevels)
		{
			MenuBuilder.AddMenuEntry( Commands.UnlockReadOnlyLevels );
		}
		else
		{
			MenuBuilder.AddMenuEntry( Commands.LockReadOnlyLevels );
		}
	}

	/** Populates the 'View' menu */
	void FillViewMenu( class FMenuBuilder& MenuBuilder )
	{
		const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();

		MenuBuilder.AddMenuEntry( Commands.DisplayActorCount );
		MenuBuilder.AddMenuEntry( Commands.DisplayLightmassSize );
		MenuBuilder.AddMenuEntry( Commands.DisplayFileSize );
		MenuBuilder.AddMenuEntry( Commands.DisplayPaths );
		MenuBuilder.AddMenuEntry( Commands.DisplayEditorOffset );
	}

	/** Populates the 'Set Streaming Method' menu */
	void FillSetStreamingMethodMenu( class FMenuBuilder& MenuBuilder )
	{
		const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();

		MenuBuilder.AddMenuEntry( Commands.SetStreamingMethod_Blueprint, NAME_None, LOCTEXT("SetStreamingMethodBlueprintOverride", "Blueprint") );
		MenuBuilder.AddMenuEntry( Commands.SetStreamingMethod_AlwaysLoaded, NAME_None, LOCTEXT("SetStreamingMethodAlwaysLoadedOverride", "Always Loaded") );
	}

	/** Populates the 'Source Control' menu */
	void FillSourceControlMenu( class FMenuBuilder& MenuBuilder )
	{
		const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();
		if(ViewModel->CanExecuteSCC())
		{
			if( ViewModel->CanExecuteSCCCheckOut())
			{
				MenuBuilder.AddMenuEntry( Commands.SCCCheckOut );
			}

			if( ViewModel->CanExecuteSCCOpenForAdd())
			{
				MenuBuilder.AddMenuEntry( Commands.SCCOpenForAdd );
			}

			if( ViewModel->CanExecuteSCCCheckIn())
			{
				MenuBuilder.AddMenuEntry( Commands.SCCCheckIn );
			}

			MenuBuilder.AddMenuEntry( Commands.SCCRefresh );
			MenuBuilder.AddMenuEntry( Commands.SCCHistory );
			MenuBuilder.AddMenuEntry( Commands.SCCDiffAgainstDepot );
		}
		else
		{
			MenuBuilder.AddMenuEntry( Commands.SCCConnect );
		}
	}

	/** Populates the 'Streaming Volumes' menu */
	void FillStreamingVolumesMenu( class FMenuBuilder& MenuBuilder )
	{
		const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();

		MenuBuilder.AddMenuEntry( Commands.AddStreamingLevelVolumes );
		MenuBuilder.AddMenuEntry( Commands.SetStreamingLevelVolumes );
		MenuBuilder.AddMenuEntry( Commands.SelectStreamingVolumes );
		MenuBuilder.AddMenuEntry( Commands.ClearStreamingVolumes );
	}

private:

	/** The UI logic of the LevelsView that is not Slate specific */
	TSharedPtr< FLevelCollectionViewModel > ViewModel;
};


#undef LOCTEXT_NAMESPACE
