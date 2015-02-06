// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "LevelsPrivatePCH.h"

#include "Matinee/MatineeActor.h"

#include "EditorLevelUtils.h"
#include "LevelUtils.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/MainFrame/Public/MainFrame.h"
#include "DesktopPlatformModule.h"
#include "LevelEditor.h"
#include "AssetToolsModule.h"

#include "ISourceControlModule.h"
#include "ISourceControlRevision.h"
#include "SourceControlWindows.h"
#include "Engine/LevelStreamingKismet.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Selection.h"
#include "Engine/LevelStreamingVolume.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"

#define LOCTEXT_NAMESPACE "LevelsView"

FLevelCollectionViewModel::FLevelCollectionViewModel( const TWeakObjectPtr< UEditorEngine >& InEditor )
	: bIsRefreshing( false )
	, bPendingUpdateActorsCount( false )
	, Filters( MakeShareable( new LevelFilterCollection ) )
	, CommandList( MakeShareable( new FUICommandList ) )
	, Editor( InEditor )
	, CurrentWorld( NULL )
	, AddedLevelStreamingClass( ULevelStreamingKismet::StaticClass() )
	, bSelectionHasChanged( true )
{
	OnResetLevels();
}

FLevelCollectionViewModel::~FLevelCollectionViewModel()
{
	Filters->OnChanged().RemoveAll( this );
	Editor->UnregisterForUndo( this );
	if( CurrentWorld.IsValid() )
	{
		RemoveLevelChangeHandlers( CurrentWorld.Get() );		
	}
	if( GEngine )
	{
		GEngine->OnWorldAdded().RemoveAll( this );
		GEngine->OnWorldDestroyed().RemoveAll( this );
		GEngine->OnLevelActorAdded().RemoveAll( this );
		GEngine->OnLevelActorDeleted().RemoveAll( this );
	}
}

void FLevelCollectionViewModel::Initialize()
{
	BindCommands();	
	
	Filters->OnChanged().AddSP( this, &FLevelCollectionViewModel::OnFilterChanged );
	Editor->RegisterForUndo( this );

	// Add world added/destroyed handlers
	if( GEngine )
	{
		GEngine->OnWorldAdded().AddSP( this, &FLevelCollectionViewModel::WorldAdded );
		GEngine->OnWorldDestroyed().AddSP( this, &FLevelCollectionViewModel::WorldDestroyed );
		GEngine->OnLevelActorAdded().AddSP( this, &FLevelCollectionViewModel::OnLevelActorAdded );
		GEngine->OnLevelActorDeleted().AddSP( this, &FLevelCollectionViewModel::OnLevelActorDeleted );


		// We need a world to browse levels. Get the first one in the list
		if( GEditor )
		{
			SetCurrentWorld( GEditor->GetEditorWorldContext().World() );
		}
	}
	
	USelection::SelectionChangedEvent.AddSP(this, &FLevelCollectionViewModel::OnActorSelectionChanged);
	SelectionChanged.AddSP(this, &FLevelCollectionViewModel::OnActorOrLevelSelectionChanged);
}

void FLevelCollectionViewModel::Tick( float DeltaTime )
{
	if (bPendingUpdateActorsCount)
	{
		UpdateLevelActorsCount();
	}
}

TStatId FLevelCollectionViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FLevelCollectionViewModel, STATGROUP_Tickables);
}

void FLevelCollectionViewModel::BindCommands()
{
	const FLevelsViewCommands& Commands = FLevelsViewCommands::Get();
	FUICommandList& ActionList = *CommandList;

	//selected level
	ActionList.MapAction( Commands.MakeLevelCurrent,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::MakeLevelCurrent_Executed ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::IsSelectedLevelUnlocked ) );
	
	ActionList.MapAction( Commands.MoveActorsToSelected,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::MoveActorsToSelected_Executed  ),
		FCanExecuteAction::CreateSP(this, &FLevelCollectionViewModel::IsValidMoveActorsToLevel));

	//invalid selected levels
	ActionList.MapAction( Commands.FixUpInvalidReference,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::FixupInvalidReference_Executed ) );
	
	ActionList.MapAction( Commands.RemoveInvalidReference,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::RemoveInvalidSelectedLevels_Executed ));

	//levels
	ActionList.MapAction( Commands.EditProperties,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::EditProperties_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::CanEditProperties ) );

	ActionList.MapAction( Commands.SaveSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SaveSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlocked ) );

	ActionList.MapAction( Commands.SCCCheckOut,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnSCCCheckOut  ) );

	ActionList.MapAction( Commands.SCCCheckIn,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnSCCCheckIn  ) );

	ActionList.MapAction( Commands.SCCOpenForAdd,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnSCCOpenForAdd  ) );

	ActionList.MapAction( Commands.SCCHistory,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnSCCHistory  ) );

	ActionList.MapAction( Commands.SCCRefresh,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnSCCRefresh  ) );

	ActionList.MapAction( Commands.SCCDiffAgainstDepot,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnSCCDiffAgainstDepot  ) );

	ActionList.MapAction( Commands.SCCConnect,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnSCCConnect  ) );

	ActionList.MapAction( Commands.MigrateSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::MigrateSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlocked ) );
	
	ActionList.MapAction( Commands.DisplayActorCount,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnToggleDisplayActorCount ),
		FCanExecuteAction(),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::GetDisplayActorCountState ) );

	ActionList.MapAction( Commands.DisplayLightmassSize,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnToggleLightmassSize ),
		FCanExecuteAction(),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::GetDisplayLightmassSizeState ) );

	ActionList.MapAction( Commands.DisplayFileSize,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnToggleFileSize ),
		FCanExecuteAction(),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::GetDisplayFileSizeState ) );

	ActionList.MapAction( Commands.DisplayPaths,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnToggleDisplayPaths ),
		FCanExecuteAction(),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::GetDisplayPathsState ) );

	ActionList.MapAction( Commands.DisplayEditorOffset,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::OnToggleEditorOffset ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FLevelCollectionViewModel::GetDisplayEditorOffsetState ) );

	ActionList.MapAction( Commands.CreateEmptyLevel,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::CreateEmptyLevel_Executed  ) );
	
	ActionList.MapAction( Commands.AddExistingLevel,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AddExistingLevel_Executed ) );

	ActionList.MapAction( Commands.AddSelectedActorsToNewLevel,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AddSelectedActorsToNewLevel_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreActorsSelected ) );
	
	ActionList.MapAction( Commands.RemoveSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::RemoveSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlockedAndNotPersistent ) );
	
	ActionList.MapAction( Commands.MergeSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::MergeSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlocked ) );
	
	// new level streaming method
	ActionList.MapAction( Commands.SetAddStreamingMethod_Blueprint,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SetAddedLevelStreamingClass_Executed, ULevelStreamingKismet::StaticClass()  ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FLevelCollectionViewModel::IsNewStreamingMethodChecked, ULevelStreamingKismet::StaticClass()));

	ActionList.MapAction( Commands.SetAddStreamingMethod_AlwaysLoaded,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SetAddedLevelStreamingClass_Executed, ULevelStreamingAlwaysLoaded::StaticClass()  ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FLevelCollectionViewModel::IsNewStreamingMethodChecked, ULevelStreamingAlwaysLoaded::StaticClass()));

	// change streaming method
	ActionList.MapAction( Commands.SetStreamingMethod_Blueprint,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SetStreamingLevelsClass_Executed, ULevelStreamingKismet::StaticClass()  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlockedAndNotPersistent ),
		FIsActionChecked::CreateSP( this, &FLevelCollectionViewModel::IsStreamingMethodChecked, ULevelStreamingKismet::StaticClass()));

	ActionList.MapAction( Commands.SetStreamingMethod_AlwaysLoaded,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SetStreamingLevelsClass_Executed, ULevelStreamingAlwaysLoaded::StaticClass()  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlockedAndNotPersistent ),
		FIsActionChecked::CreateSP( this, &FLevelCollectionViewModel::IsStreamingMethodChecked, ULevelStreamingAlwaysLoaded::StaticClass()));

	//level selection
	ActionList.MapAction( Commands.SelectAllLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SelectAllLevels_Executed  ) );
	
	ActionList.MapAction( Commands.DeselectAllLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::DeselectAllLevels_Executed  ) );
	
	ActionList.MapAction( Commands.InvertLevelSelection,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::InvertSelection_Executed  ) );
	
	
	//actors
	ActionList.MapAction( Commands.SelectLevelActors,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SelectActors_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlocked ) );
	
	ActionList.MapAction( Commands.DeselectLevelActors,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::DeselectActors_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlocked ) );
	
	
	//streaming volumes
	ActionList.MapAction( Commands.AddStreamingLevelVolumes,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AddStreamingLevelVolumes_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::CanAddOrSelectStreamingVolumes ) );

	ActionList.MapAction( Commands.SetStreamingLevelVolumes,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SetStreamingLevelVolumes_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::CanAddOrSelectStreamingVolumes ) );

	ActionList.MapAction( Commands.SelectStreamingVolumes,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::SelectStreamingVolumes_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlocked ) );
	
	ActionList.MapAction( Commands.ClearStreamingVolumes,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::ClearStreamingVolumes_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsUnlocked ) );
	
	
	//visibility
	ActionList.MapAction( Commands.ShowSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::ShowSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreLevelsSelected ) );
	
	ActionList.MapAction( Commands.HideSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::HideSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreLevelsSelected ) );
	
	ActionList.MapAction( Commands.ShowOnlySelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::ShowOnlySelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreLevelsSelected ) );

	ActionList.MapAction( Commands.ShowAllLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::ShowAllLevels_Executed  ) );
	
	ActionList.MapAction( Commands.HideAllLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::HideAllLevels_Executed  ) );
	
	
	//lock
	ActionList.MapAction( Commands.LockSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::LockSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsNotPersistent ) );
	
	ActionList.MapAction( Commands.UnockSelectedLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::UnockSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionViewModel::AreSelectedLevelsNotPersistent ) );
	
	ActionList.MapAction( Commands.LockAllLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::LockAllLevels_Executed  ) );
	
	ActionList.MapAction( Commands.UnockAllLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::UnockAllLevels_Executed  ) );

	ActionList.MapAction( Commands.LockReadOnlyLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::ToggleReadOnlyLevels_Executed  ) );

	ActionList.MapAction( Commands.UnlockReadOnlyLevels,
		FExecuteAction::CreateSP( this, &FLevelCollectionViewModel::ToggleReadOnlyLevels_Executed  ) );
}

void FLevelCollectionViewModel::ToggleReadOnlyLevels_Executed()
{
	GEngine->bLockReadOnlyLevels = !GEngine->bLockReadOnlyLevels;
}

void FLevelCollectionViewModel::OnToggleDisplayActorCount()
{
	ULevelBrowserSettings* Settings = GetMutableDefault<ULevelBrowserSettings>();
	Settings->bDisplayActorCount = !Settings->bDisplayActorCount;
	Settings->PostEditChange();

	DisplayActorCountChanged.Broadcast(Settings->bDisplayActorCount);
}

bool FLevelCollectionViewModel::GetDisplayActorCountState() const
{
	return (GetDefault<ULevelBrowserSettings>()->bDisplayActorCount);
}

void FLevelCollectionViewModel::OnToggleLightmassSize()
{
	ULevelBrowserSettings* Settings = GetMutableDefault<ULevelBrowserSettings>();
	Settings->bDisplayLightmassSize = !GetDisplayLightmassSizeState();
	Settings->PostEditChange();

	DisplayLightmassSizeChanged.Broadcast(Settings->bDisplayLightmassSize);
}

bool FLevelCollectionViewModel::GetDisplayLightmassSizeState() const
{
	return GetDefault<ULevelBrowserSettings>()->bDisplayLightmassSize;
}

void FLevelCollectionViewModel::OnToggleFileSize()
{
	ULevelBrowserSettings* Settings = GetMutableDefault<ULevelBrowserSettings>();
	Settings->bDisplayFileSize = !GetDisplayFileSizeState();
	Settings->PostEditChange();

	DisplayFileSizeChanged.Broadcast(Settings->bDisplayFileSize);
}

bool FLevelCollectionViewModel::GetDisplayFileSizeState() const
{
	return GetDefault<ULevelBrowserSettings>()->bDisplayFileSize;
}

void FLevelCollectionViewModel::OnToggleEditorOffset()
{
	ULevelBrowserSettings* Settings = GetMutableDefault<ULevelBrowserSettings>();
	Settings->bDisplayEditorOffset = !Settings->bDisplayEditorOffset;
	Settings->PostEditChange();

	DisplayEditorOffsetChanged.Broadcast(Settings->bDisplayEditorOffset);
}

bool FLevelCollectionViewModel::GetDisplayEditorOffsetState() const
{
	return GetDefault<ULevelBrowserSettings>()->bDisplayEditorOffset;
}

void FLevelCollectionViewModel::OnToggleDisplayPaths()
{
	ULevelBrowserSettings* Settings = GetMutableDefault<ULevelBrowserSettings>();
	Settings->bDisplayPaths = !GetDisplayPathsState();
	Settings->PostEditChange();
}

bool FLevelCollectionViewModel::GetDisplayPathsState() const
{
	return (GetDefault<ULevelBrowserSettings>()->bDisplayPaths);
}

bool FLevelCollectionViewModel::CanShiftSelection()
{
	if ( !IsOneLevelSelected() )
	{
		return false;
	}

	for ( int32 i = 0; i < SelectedLevels.Num(); ++i )
	{
		if ( !SelectedLevels[i].IsValid() || SelectedLevels[i]->IsLocked() || 
			 SelectedLevels[i]->IsPersistent() || !SelectedLevels[i]->IsLevel() )
		{
			return false;
		}
	}

	return true;
}

void FLevelCollectionViewModel::ShiftSelection( bool bUp )
{
	if ( !CanShiftSelection() )
	{
		return;
	}

	if ( !IsSelectedLevelUnlocked()  )
	{
		FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("ShiftLevelLocked", "Shift Level: The requested operation could not be completed because the level is locked.") );
		return;
	}

	const ULevel* InLevel = SelectedLevels[0]->GetLevel().Get();
	UWorld *OwningWorld = InLevel->OwningWorld;

	check( OwningWorld );

	int32 PrevFoundLevelIndex = -1;
	int32 FoundLevelIndex = -1;
	int32 PostFoundLevelIndex = -1;
	for( int32 LevelIndex = 0 ; LevelIndex < OwningWorld->StreamingLevels.Num() ; ++LevelIndex )
	{
		ULevelStreaming* StreamingLevel = OwningWorld->StreamingLevels[LevelIndex];
		if( StreamingLevel != NULL )
		{
			ULevel* Level = StreamingLevel->GetLoadedLevel();
			if( Level != NULL && Level->OwningWorld != NULL )
			{
				if ( FoundLevelIndex > -1 )
				{
					// Mark the first valid index after the found level and stop searching.
					PostFoundLevelIndex = LevelIndex;
					break;
				}
				else
				{
					if ( Level == InLevel )
					{
						// We've found the level.
						FoundLevelIndex = LevelIndex;
					}
					else
					{
						// Mark this level as being the index before the found level.
						PrevFoundLevelIndex = LevelIndex;
					}
				}
			}
		}
	}

	// If we found the level . . .
	if ( FoundLevelIndex > -1 )
	{
		// Check if we found a destination index to swap it to.
		const int32 DestIndex = bUp ? PrevFoundLevelIndex : PostFoundLevelIndex;
		const bool bFoundPrePost = DestIndex > -1;
		if ( bFoundPrePost )
		{
			// Swap the level into position.
			const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "ShiftLevelInLevelBrowser", "Shift level in Level Browser") );
			OwningWorld->Modify();
			OwningWorld->StreamingLevels.Swap( FoundLevelIndex, DestIndex );
			OwningWorld->MarkPackageDirty();
		}
	}

	RefreshSortIndexes();
}

void FLevelCollectionViewModel::AddFilter( const TSharedRef< LevelFilter >& InFilter )
{
	Filters->Add( InFilter );
	OnFilterChanged();
}

void FLevelCollectionViewModel::RemoveFilter( const TSharedRef< LevelFilter >& InFilter )
{
	Filters->Remove( InFilter );
	OnFilterChanged();
}

TArray< TSharedPtr< FLevelViewModel > >& FLevelCollectionViewModel::GetLevels()
{ 
	return FilteredLevelViewModels;
}

const TArray< TSharedPtr< FLevelViewModel > >& FLevelCollectionViewModel::GetSelectedLevels() const 
{ 
	return SelectedLevels;
}

const TArray< TSharedPtr< FLevelViewModel > >& FLevelCollectionViewModel::GetInvalidSelectedLevels() const 
{ 
	return InvalidSelectedLevels;
}

void FLevelCollectionViewModel::GetSelectedLevelNames( OUT TArray< FName >& OutSelectedLevelNames ) const
{
	AppendSelectLevelNames( OutSelectedLevelNames );
}

void FLevelCollectionViewModel::SetSelectedLevels( const TArray< TSharedPtr< FLevelViewModel > >& InSelectedLevels ) 
{ 
	InvalidSelectedLevels.Empty();
	
	TArray<class ULevel*> Levels;
	TArray<UPackage*> PackagesToUpdate;
	for( auto LevelIter = InSelectedLevels.CreateConstIterator(); LevelIter; ++LevelIter )
	{
		const auto LevelViewModel = *LevelIter;

		// Only add the level if its valid.
		if( LevelViewModel->IsLevel() )
		{
			ULevel* Level = LevelViewModel->GetLevel().Get();
			if(Level != NULL)
			{
				Levels.Add( Level );
				if(Level->GetOutermost() != NULL)
				{
					PackagesToUpdate.Add(Level->GetOutermost());
				}
			}
		}
		else if ( LevelViewModel->IsLevelStreaming() )
		{
			InvalidSelectedLevels.AddUnique(LevelViewModel);
		}
	}

	ISourceControlModule::Get().QueueStatusUpdate(PackagesToUpdate);

	// Pass this list to our own function
	SetSelectedLevelsInWorld( Levels );
}

void FLevelCollectionViewModel::SetSelectedLevels( const TArray< FName >& LevelNames ) 
{ 
	TArray<class ULevel*> Levels;
	TArray<UPackage*> PackagesToUpdate;
	for( auto LevelIter = FilteredLevelViewModels.CreateConstIterator(); LevelIter; ++LevelIter )
	{
		const auto LevelViewModel = *LevelIter;

		if( LevelNames.Contains( LevelViewModel->GetFName() ) )
		{
			// Only add the level if its valid and not a volume level.
			if( LevelViewModel->IsLevel() )
			{
				ULevel* Level = LevelViewModel->GetLevel().Get();
				if(Level != NULL)
				{
					Levels.Add( Level );
					if(Level->GetOutermost() != NULL)
					{
						PackagesToUpdate.Add(Level->GetOutermost());
					}
				}
			}
		}
	}

	ISourceControlModule::Get().QueueStatusUpdate(PackagesToUpdate);

	// Pass this list to our own function
	SetSelectedLevelsInWorld( Levels );
}

void FLevelCollectionViewModel::SetSelectedLevelsInWorld( const TArray< ULevel* >& Levels ) 
{ 
	// Pass the list we just created to the world to set the selection
	if( CurrentWorld.IsValid() )
	{
		CurrentWorld->SetSelectedLevels( Levels );
	}
}

void FLevelCollectionViewModel::SetSelectedLevel( const FName& LevelName ) 
{ 
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	TArray<class ULevel*> Levels;
	for( auto LevelIter = FilteredLevelViewModels.CreateConstIterator(); LevelIter; ++LevelIter )
	{
		const auto LevelViewModel = *LevelIter;

		if( LevelName == LevelViewModel->GetFName() )
		{
			// Only add the level if its valid and not a volume level.
			if( LevelViewModel->IsLevel() )
			{
				ULevel* Level = LevelViewModel->GetLevel().Get();
				if(Level != NULL)
				{
					Levels.AddUnique( Level );
					if(Level->GetOutermost() != NULL)
					{
						ISourceControlModule::Get().QueueStatusUpdate(Level->GetOutermost());
					}
				}
				break;
			}

		}
	}
	// Pass this list to our own function
	SetSelectedLevelsInWorld( Levels );
}

const TSharedRef< FUICommandList > FLevelCollectionViewModel::GetCommandList() const 
{ 
	return CommandList;
}

void FLevelCollectionViewModel::BroadcastUpdateIfNotInRefresh()
{
	if( bIsRefreshing == false )
	{
		LevelsChanged.Broadcast( ELevelsAction::Reset, NULL, NAME_None );
	}
}

void FLevelCollectionViewModel::OnFilterChanged()
{
	RefreshFilteredLevels();
	BroadcastUpdateIfNotInRefresh();
}

void FLevelCollectionViewModel::Refresh()
{
	OnLevelsChanged( ELevelsAction::Reset, NULL, NAME_None );
}

void FLevelCollectionViewModel::RefreshSelected()
{
	check( !bIsRefreshing );
	bIsRefreshing = true;
	SelectedLevels.Reset() ;	
	if( CurrentWorld.IsValid() )
	{
		for( auto LevelIter = FilteredLevelViewModels.CreateConstIterator(); LevelIter; ++LevelIter )
		{
			const auto LevelViewModel = *LevelIter;
			// Only add the level if its valid.
			if( LevelViewModel->IsLevel() )
			{
				if( CurrentWorld->IsLevelSelected( LevelViewModel->GetLevel().Get() ) == true )
				{
					SelectedLevels.Add( LevelViewModel );
				}
			}
		}
	}
	// Broadcast that we have changed our selection
	SelectionChanged.Broadcast();
	BroadcastUpdateIfNotInRefresh();
 	bIsRefreshing = false;
}

void FLevelCollectionViewModel::OnLevelsChanged( const ELevelsAction::Type Action, const TWeakObjectPtr< ULevel >& ChangedLevel, const FName& ChangedProperty )
{
	check( !bIsRefreshing );
 	bIsRefreshing = true;
	
	switch ( Action )
	{
		case ELevelsAction::Add:
			OnLevelAdded( ChangedLevel );
			break;

		case ELevelsAction::Rename:
			//We purposely ignore re-filtering in this case
			SortFilteredLevels();
			break;

		case ELevelsAction::Modify:
			RefreshFilteredLevels();
			break;

		case ELevelsAction::Delete:
			OnLevelDelete();
			break;

		case ELevelsAction::Reset:
		default:
			OnResetLevels();
			break;
	}

 	LevelsChanged.Broadcast( Action, ChangedLevel, ChangedProperty );
 	bIsRefreshing = false;
}

void FLevelCollectionViewModel::CreateLocalArrays()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	if ( CurrentWorld.IsValid() )
	{		
		TArray< ULevel* > Levels;
		for (int32 iLevel = 0; iLevel < CurrentWorld->GetNumLevels() ; iLevel++)
		{
			ULevel* Level = CurrentWorld->GetLevel( iLevel );
			if(Level != NULL)
			{
				Levels.AddUnique( Level );
				if(Level->GetOutermost() != NULL)
				{
					ISourceControlModule::Get().QueueStatusUpdate(Level->GetOutermost());
				}
			}
		}

		TArray< ULevelStreaming* > LevelsStreaming;
		for (int32 iLevel = 0; iLevel < CurrentWorld->StreamingLevels.Num() ; iLevel++)
		{
			ULevelStreaming* StreamingLevel = CurrentWorld->StreamingLevels[iLevel];	
			if ( StreamingLevel != NULL )
			{
				ULevel* Level = StreamingLevel->GetLoadedLevel();
				if ( !Levels.Contains(Level) )
				{
					LevelsStreaming.AddUnique(StreamingLevel);
				}
			}
		}

		CreateViewModels( Levels, LevelsStreaming );		
	}
}

void FLevelCollectionViewModel::PopulateLevelsList()
{
	DestructivelyPurgeInvalidViewModels();
	CreateLocalArrays();	
	RefreshSortIndexes();
	SortFilteredLevels();
	UpdateLevelActorsCount();
}

void FLevelCollectionViewModel::RefreshSortIndexes()
{
	for( auto LevelIt = AllLevelViewModels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only refresh the level if its valid.
		if ( (*LevelIt)->IsValid() )
		{
			(*LevelIt)->RefreshStreamingLevelIndex();
		}
	}

	OnFilterChanged();
}

void FLevelCollectionViewModel::OnLevelAdded( const TWeakObjectPtr< ULevel >& AddedLevel )
{
	// Force a rebuild of the cached data
	PopulateLevelsList();	
	// Tell anyone that wants to know of changes
	BroadcastUpdateIfNotInRefresh();
}

void FLevelCollectionViewModel::OnLevelDelete()
{
	// Force a rebuild of the cached data
	PopulateLevelsList();	
	// Tell anyone that wants to know of changes
	BroadcastUpdateIfNotInRefresh();
}

void FLevelCollectionViewModel::OnResetLevels()
{
	// Force a rebuild of the cached data
	PopulateLevelsList();	
	// Tell anyone that wants to know of changes
	BroadcastUpdateIfNotInRefresh();
}

void FLevelCollectionViewModel::DestructivelyPurgeInvalidViewModels() 
{
	for( int LevelIndex = AllLevelViewModels.Num() - 1; LevelIndex >= 0; --LevelIndex )
	{
		const auto LevelViewModel = AllLevelViewModels[ LevelIndex ];
		AllLevelViewModels.RemoveAt( LevelIndex );
		FilteredLevelViewModels.Remove( LevelViewModel );
		SelectedLevels.Remove( LevelViewModel );
	}
}

void FLevelCollectionViewModel::CreateViewModels( const TArray< ULevel* >& InLevels, const TArray< ULevelStreaming* >& InLevelStreaming) 
{
	//InLevels
	for( auto LevelIt = InLevels.CreateConstIterator(); LevelIt; ++LevelIt )
	{
		if( *LevelIt )
		{
			const TSharedRef< FLevelViewModel > NewLevelViewModel = FLevelViewModel::Create( *LevelIt, NULL, Editor );
			AllLevelViewModels.Add( NewLevelViewModel );

			if( Filters->PassesAllFilters( NewLevelViewModel ) )
			{
				FilteredLevelViewModels.Add( NewLevelViewModel );
			}
		}
	}
	
	//InStreamedLevels
	for( auto LevelStreamingIt = InLevelStreaming.CreateConstIterator(); LevelStreamingIt; ++LevelStreamingIt )
	{
		ULevelStreaming* LevelStreaming = *LevelStreamingIt;
		if( LevelStreaming != NULL )
		{
			const TSharedRef< FLevelViewModel > NewLevelViewModel = FLevelViewModel::Create(LevelStreaming->GetLoadedLevel(), LevelStreaming, Editor );
			AllLevelViewModels.Add( NewLevelViewModel );

			if( Filters->PassesAllFilters( NewLevelViewModel ) )
			{
				FilteredLevelViewModels.Add( NewLevelViewModel );
			}
		}
	}
}

void FLevelCollectionViewModel::ClearStreamingLevelVolumes()
{
	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only process the level if its valid.
		if ( (*LevelIt)->IsValid() )
		{
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( (*LevelIt)->GetLevel().Get() );
			if ( StreamingLevel )
			{
				StreamingLevel->Modify();

				// Disassociate the level from the volume.
				for ( int32 i = 0 ; i < StreamingLevel->EditorStreamingVolumes.Num() ; ++i )
				{
					ALevelStreamingVolume* LevelStreamingVolume = StreamingLevel->EditorStreamingVolumes[i];
					if ( LevelStreamingVolume )
					{
						LevelStreamingVolume->Modify();
						LevelStreamingVolume->StreamingLevelNames.Remove( StreamingLevel->GetWorldAssetPackageFName() );
					}
				}

				// Disassociate the volumes from the level.
				StreamingLevel->EditorStreamingVolumes.Empty();
			}
		}
	}
}

void FLevelCollectionViewModel::RefreshFilteredLevels()
{
	FilteredLevelViewModels.Empty();

	for( auto LevelIt = AllLevelViewModels.CreateIterator(); LevelIt; ++LevelIt )
	{
		const auto LevelViewModel = *LevelIt;
		// Only add valid levels.
		if( LevelViewModel->IsValid() )
		{
			if( Filters->PassesAllFilters( LevelViewModel ) )
			{
				FilteredLevelViewModels.Add( LevelViewModel );
			}
		}
	}

	SortFilteredLevels();
}

void FLevelCollectionViewModel::SortFilteredLevels()
{
	struct FCompareLevels
	{
		FORCEINLINE bool operator()( const TSharedPtr< FLevelViewModel >& Lhs, const TSharedPtr< FLevelViewModel >& Rhs ) const 
		{ 
			//First, sort the ViewModels into tiers:
			//Persistent Level
			//Streaming Levels

			int LeftTier = 0;
			int RightTier = 0;

			if ( Lhs->IsLevel() )
			{
				LeftTier = ( Lhs->IsPersistent() )? 1 : 2;
			}
			if ( Rhs->IsLevel() )
			{
				RightTier = ( Rhs->IsPersistent() )? 1 : 2;
			}

			if ( LeftTier != RightTier )
			{
				//Sort the tiers
				return (LeftTier < RightTier);
			}
			else
			{
				//Within the Streaming Levels tier, sort by a user-defined order.
				return (Lhs->GetStreamingLevelIndex() < Rhs->GetStreamingLevelIndex() );
			}
			return false;
		}
	};

	FilteredLevelViewModels.Sort( FCompareLevels() );
}

void FLevelCollectionViewModel::AppendSelectLevelNames( TArray< FName >& OutLevelNames ) const
{
	for(auto LevelsIt = SelectedLevels.CreateConstIterator(); LevelsIt; ++LevelsIt)
	{
		const TSharedPtr< FLevelViewModel >& Level = *LevelsIt;
		OutLevelNames.Add( Level->GetFName() );
	}
}

void FLevelCollectionViewModel::GetSelectedLevelPackages( TArray<UPackage*>* OutSelectedPackages, TArray<FString>* OutSelectedPackagesNames )
{
	for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
	{
		if ( (*SelectedLevelIt)->GetLevel().IsValid() )
		{
			ULevel* CurrentLevel = (*SelectedLevelIt)->GetLevel().Get();
			if(OutSelectedPackages)
			{
				(*OutSelectedPackages).Add( CurrentLevel->GetOutermost() );
			}

			if(OutSelectedPackagesNames)
			{
				(*OutSelectedPackagesNames).Add( CurrentLevel->GetOutermost()->GetName() );
			}
		}
	}
}

//delegates
bool FLevelCollectionViewModel::IsOneLevelSelected() const
{
	return ( SelectedLevels.Num() == 1 ); 
}

bool FLevelCollectionViewModel::IsSelectedLevelUnlocked() const
{
	if ( !IsOneLevelSelected() )
	{
		return false;
	}

	return ( SelectedLevels[0].IsValid() && !SelectedLevels[0]->IsLocked() );
}

bool FLevelCollectionViewModel::AreLevelsSelected() const
{
	return ( SelectedLevels.Num() > 0 ); 
}

bool FLevelCollectionViewModel::AreSelectedLevelsUnlocked() const
{
	if ( !AreLevelsSelected() )
	{
		return false;
	}

	for ( int32 i = 0; i < SelectedLevels.Num(); ++i )
	{
		if ( !SelectedLevels[i].IsValid() || SelectedLevels[i]->IsLocked() )
		{
			return false;
		}
	}

	return true;
}

bool FLevelCollectionViewModel::AreSelectedLevelsNotPersistent() const
{
	if ( !AreLevelsSelected() )
	{
		return false;
	}

	for ( int32 i = 0; i < SelectedLevels.Num(); ++i )
	{
		if ( !SelectedLevels[i].IsValid() || SelectedLevels[i]->IsPersistent() )
		{
			return false;
		}
	}

	return true;
}

bool FLevelCollectionViewModel::AreSelectedLevelsUnlockedAndNotPersistent() const
{
	if ( !AreLevelsSelected() )
	{
		return false;
	}

	for ( int32 i = 0; i < SelectedLevels.Num(); ++i )
	{
		if ( !SelectedLevels[i].IsValid() || SelectedLevels[i]->IsLocked() || SelectedLevels[i]->IsPersistent() )
		{
			return false;
		}
	}

	return true;
}

bool FLevelCollectionViewModel::AreAllSelectedLevelsUnlockedAndVisible() const
{
	for (const auto& Level : SelectedLevels)
	{
		if (Level->IsLocked() == true ||
			Level->IsVisible() == false) 
		{
			return false;
		}
	}

	return SelectedLevels.Num() > 0;
}

bool FLevelCollectionViewModel::AreActorsSelected() const
{
	return Editor->GetSelectedActorCount() > 0;
}

bool FLevelCollectionViewModel::CanEditProperties() const
{
	if ( !AreLevelsSelected() )
	{
		return false;
	}

	for ( int32 i = 0; i < SelectedLevels.Num(); ++i )
	{
		if ( !SelectedLevels[i].IsValid() || SelectedLevels[i]->IsLocked() || SelectedLevels[i]->IsPersistent() )
		{
			return false;
		}
	}

	return true;
}

bool FLevelCollectionViewModel::CanAddOrSelectStreamingVolumes() const
{
	bool bStreamingLevelVolumeSelected = false;
	
	for( FSelectionIterator It( GEditor->GetSelectedActorIterator() ); It; ++It )
	{
		const ALevelStreamingVolume* StreamingVolume = Cast<ALevelStreamingVolume>( *It );
		if ( StreamingVolume && StreamingVolume->GetLevel() == GWorld->PersistentLevel )
		{
			bStreamingLevelVolumeSelected = true;
			break;
		}
	}

	return !bStreamingLevelVolumeSelected ? false : AreSelectedLevelsUnlocked();
}

//selected level
void FLevelCollectionViewModel::MakeLevelCurrent_Executed()
{
	check( SelectedLevels.Num() == 1 );
	SelectedLevels[0]->MakeLevelCurrent();
}

void FLevelCollectionViewModel::EditProperties_Executed()
{
	//Remove any selected actor info from the Inspector and the Property views.
	GEditor->SelectNone( true, true );

	TArray<UObject*> SelectedObjects;

	for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
	{
		if( (*SelectedLevelIt)->IsLevelStreaming() )
		{
			ULevelStreaming* LevelStreaming = (*SelectedLevelIt)->GetLevelStreaming().Get();
			SelectedObjects.Add( LevelStreaming );
		}
	}

	if ( SelectedObjects.Num() > 0 )
	{
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( TEXT("PropertyEditor") );

		// If the slate main frame is shown, summon a new property viewer in the Level editor module
		if(MainFrameModule.IsWindowInitialized())
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
			LevelEditorModule.SummonSelectionDetails();
		}
		else // create a floating property window if the slate main frame is not initialized
		{
			PropertyEditorModule.CreateFloatingDetailsView( SelectedObjects, true );
		}	

		GUnrealEd->UpdateFloatingPropertyWindowsFromActorList( SelectedObjects );
	}
}

void FLevelCollectionViewModel::MoveActorsToSelected_Executed()
{
	// We shouldn't be able to get here if we have more than 1 level selected. But just in case lets check.
	if (SelectedLevels.Num() == 1)
	{
		// If matinee is open, and if an actor being moved belongs to it, message the user
		if (GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_InterpEdit))
		{
			const FEdModeInterpEdit* InterpEditMode = (const FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_InterpEdit);
			if (InterpEditMode && InterpEditMode->MatineeActor)
			{
				TArray<AActor*> ControlledActors;
				InterpEditMode->MatineeActor->GetControlledActors(ControlledActors);

				// are any of the selected actors in the matinee
				USelection* SelectedActors = GEditor->GetSelectedActors();
				for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
				{
					AActor* Actor = CastChecked<AActor>(*Iter);
					if (Actor != nullptr && (Actor == InterpEditMode->MatineeActor || ControlledActors.Contains(Actor)))
					{
						const bool ExitInterp = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, NSLOCTEXT("UnrealEd", "MatineeUnableToMove", "You must close Matinee before moving actors.\nDo you wish to do this now and continue?"));
						if (!ExitInterp)
						{
							return;
						}
						GLevelEditorModeTools().DeactivateMode(FBuiltinEditorModes::EM_InterpEdit);
						break;
					}
				}
			}
		}
		
		GEditor->MoveSelectedActorsToLevel( SelectedLevels[0]->GetLevel().Get() );
	}	
}


//levels
void FLevelCollectionViewModel::SaveSelectedLevels_Executed()
{
	// NOTE: We'll build a list of levels to save here.  We don't want to use the SelectedLevels member
	//   since that list will be reset when Serialize is called
	TArray< ULevel* > LevelsToSave;
	for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
	{
		if ( (*SelectedLevelIt)->GetLevel().IsValid() )
		{
			LevelsToSave.Add( (*SelectedLevelIt)->GetLevel().Get() );
		}
	}

	TArray< UPackage* > PackagesNotNeedingCheckout;
	// Prompt the user to check out the levels from source control before saving
	if ( FEditorFileUtils::PromptToCheckoutLevels( false, LevelsToSave, &PackagesNotNeedingCheckout ) )
	{
		for( auto LevelToSaveIt = LevelsToSave.CreateIterator(); LevelToSaveIt; ++LevelToSaveIt )
		{
			FEditorFileUtils::SaveLevel( *LevelToSaveIt );
		}
	}
	else if ( PackagesNotNeedingCheckout.Num() > 0 )
	{
		// The user canceled the checkout dialog but some packages didnt need to be checked out in order to save
		// For each selected level if the package its in didnt need to be saved, save the level!
		for( auto LevelToSaveIt = LevelsToSave.CreateIterator(); LevelToSaveIt; ++LevelToSaveIt )
		{
			ULevel* Level = *LevelToSaveIt;
			if( PackagesNotNeedingCheckout.Contains( Level->GetOutermost() ) )
			{
				FEditorFileUtils::SaveLevel( Level );
			}
			else
			{
				//remove it from the list, so that only successfully saved levels are highlighted when saving is complete
				LevelsToSave.Remove( Level );
			}
		}
	}

	// Select the levels that were saved successfully
	SetSelectedLevelsInWorld( LevelsToSave );
}

void FLevelCollectionViewModel::OnSCCCheckOut()
{
	TArray<UPackage*> PackagesToCheckOut;
	GetSelectedLevelPackages(&PackagesToCheckOut, NULL);

	// Update the source control status of all potentially relevant packages
	ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FUpdateStatus>(), PackagesToCheckOut);

	// Now check them out
	FEditorFileUtils::CheckoutPackages(PackagesToCheckOut);
}

void FLevelCollectionViewModel::OnSCCCheckIn()
{
	TArray<UPackage*> Packages;
	TArray<FString> PackageNames;
	GetSelectedLevelPackages(&Packages, &PackageNames);

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave( Packages, true, true );

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined );
	if ( bShouldProceed )
	{
		FSourceControlWindows::PromptForCheckin(PackageNames);
	}
	else
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure.") );
		}
	}
}

void FLevelCollectionViewModel::OnSCCOpenForAdd()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	TArray<FString> PackageNames;
	GetSelectedLevelPackages(NULL, &PackageNames);

	TArray<FString> PackagesToAdd;
	TArray<UPackage*> PackagesToSave;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		const FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(*PackageIt), EStateCacheUsage::Use);
		if ( SourceControlState.IsValid() && !SourceControlState->IsSourceControlled() )
		{
			PackagesToAdd.Add(*PackageIt);

			// Make sure the file actually exists on disk before adding it
			FString Filename;
			if ( !FPackageName::DoesPackageExist(*PackageIt, NULL, &Filename) )
			{
				UPackage* Package = FindPackage(NULL, **PackageIt);
				if ( Package )
				{
					PackagesToSave.Add(Package);
				}
			}
		}
	}

	if ( PackagesToAdd.Num() > 0 )
	{
		// If any of the packages are new, save them now
		if ( PackagesToSave.Num() > 0 )
		{
			const bool bCheckDirty = false;
			const bool bPromptToSave = false;
			const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);
		}

		SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlHelpers::PackageFilenames(PackagesToAdd));
	}
}

void FLevelCollectionViewModel::OnSCCHistory()
{
	TArray<FString> PackageNames;
	for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
	{
		if ( (*SelectedLevelIt)->GetLevel().IsValid() )
		{
			PackageNames.Add( (*SelectedLevelIt)->GetLevel().Get()->GetOutermost()->GetName() );
		}
	}

	FSourceControlWindows::DisplayRevisionHistory(SourceControlHelpers::PackageFilenames(PackageNames));
}

void FLevelCollectionViewModel::OnSCCRefresh()
{
	if(ISourceControlModule::Get().IsEnabled())
	{
		TArray<UPackage*> Packages;
		for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
		{
			if ( (*SelectedLevelIt)->GetLevel().IsValid() )
			{
				Packages.Add( (*SelectedLevelIt)->GetLevel().Get()->GetOutermost() );
			}
		}

		ISourceControlModule::Get().QueueStatusUpdate(Packages);
	}
}

void FLevelCollectionViewModel::OnSCCDiffAgainstDepot()
{
	// Load the asset registry module
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Iterate over each selected asset
	for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
	{
		UPackage* OriginalPackage = (*SelectedLevelIt)->GetLevel().Get()->GetOutermost();
		FString PackageName = OriginalPackage->GetName();

		// Make sure our history is up to date
		TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
		UpdateStatusOperation->SetUpdateHistory(true);
		SourceControlProvider.Execute(UpdateStatusOperation, OriginalPackage);

		// Get the SCC state
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(OriginalPackage, EStateCacheUsage::Use);

		// If the level is in SCC.
		if( SourceControlState.IsValid() && SourceControlState->IsSourceControlled() )
		{
			// Get the file name of package
			FString RelativeFileName;
			if(FPackageName::DoesPackageExist(PackageName, NULL, &RelativeFileName))
			{
				if(SourceControlState->GetHistorySize() > 0)
				{
					TSharedPtr<ISourceControlRevision, ESPMode::ThreadSafe> Revision = SourceControlState->GetHistoryItem(0);
					check(Revision.IsValid());

					// Get the head revision of this package from source control
					FString AbsoluteFileName = FPaths::ConvertRelativePathToFull(RelativeFileName);
					FString TempFileName;
					if(Revision->Get(TempFileName))
					{
						// Forcibly disable compile on load in case we are loading old blueprints that might try to update/compile
						TGuardValue<bool> DisableCompileOnLoad(GForceDisableBlueprintCompileOnLoad, true);

						// Try and load that package
						FText NotMapReason;
						UPackage* OldPackage = LoadPackage(NULL, *TempFileName, LOAD_None);
						if(OldPackage != NULL && GEditor->PackageIsAMapFile(*TempFileName, NotMapReason))
						{
							/* Set the revision information*/
							UPackage* Package = OriginalPackage;

							FRevisionInfo OldRevision;
							OldRevision.Changelist = Revision->GetCheckInIdentifier();
							OldRevision.Date = Revision->GetDate();
							OldRevision.Revision = Revision->GetRevision();

							FRevisionInfo NewRevision; 
							NewRevision.Revision = TEXT("");

							// Dump assets to temp text files
							FString OldTextFilename = AssetToolsModule.Get().DumpAssetToTempFile(OldPackage);
							FString NewTextFilename = AssetToolsModule.Get().DumpAssetToTempFile(OriginalPackage);
							FString DiffCommand = GetDefault<UEditorLoadingSavingSettings>()->TextDiffToolPath.FilePath;

							AssetToolsModule.Get().CreateDiffProcess(DiffCommand, OldTextFilename, NewTextFilename);

							AssetToolsModule.Get().DiffAssets(OldPackage, OriginalPackage, OldRevision, NewRevision);
						}
					}
				}
			} 
		}
	}
}

void FLevelCollectionViewModel::OnSCCConnect() const
{
	ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless);
}

void FLevelCollectionViewModel::MigrateSelectedLevels_Executed()
{
	// Gather the package names for the levels
	TArray<FName> PackageNames;
	for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
	{
		if ( UObject* Level = (*SelectedLevelIt)->GetLevel().Get() )
		{
			PackageNames.Add( Level->GetOutermost()->GetFName() );
		}
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().MigratePackages( PackageNames );
}

void FLevelCollectionViewModel::CreateEmptyLevel_Executed()
{
	EditorLevelUtils::CreateNewLevel( CurrentWorld.Get(), false, AddedLevelStreamingClass );
}

void FLevelCollectionViewModel::AddExistingLevel_Executed()
{
	AddExistingLevel();
}

void FLevelCollectionViewModel::AddExistingLevel(bool bRemoveInvalidSelectedLevelsAfter)
{
	if ( UEditorEngine::IsUsingWorldAssets() )
	{
		if (!bAssetDialogOpen)
		{
			bAssetDialogOpen = true;
			FEditorFileUtils::FOnLevelsChosen LevelsChosenDelegate = FEditorFileUtils::FOnLevelsChosen::CreateSP(this, &FLevelCollectionViewModel::HandleAddExistingLevelSelected, bRemoveInvalidSelectedLevelsAfter);
			FEditorFileUtils::FOnLevelPickingCancelled DialogCancelledDelegate = FEditorFileUtils::FOnLevelPickingCancelled::CreateSP(this, &FLevelCollectionViewModel::HandleAddExistingLevelCancelled);
			const bool bAllowMultipleSelection = true;
			FEditorFileUtils::OpenLevelPickingDialog(LevelsChosenDelegate, DialogCancelledDelegate, bAllowMultipleSelection);
		}
	}
	else
	{
		TArray<FString> OpenFilenames;
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		bool bOpened = false;
		if ( DesktopPlatform )
		{
			void* ParentWindowWindowHandle = NULL;

			IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
			const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
			if ( MainFrameParentWindow.IsValid() && MainFrameParentWindow->GetNativeWindow().IsValid() )
			{
				ParentWindowWindowHandle = MainFrameParentWindow->GetNativeWindow()->GetOSWindowHandle();
			}

			bOpened = DesktopPlatform->OpenFileDialog(
				ParentWindowWindowHandle,
				NSLOCTEXT("UnrealEd", "Open", "Open").ToString(),
				*FEditorDirectories::Get().GetLastDirectory(ELastDirectory::UNR),
				TEXT(""),
				*FEditorFileUtils::GetFilterString(FI_Load),
				EFileDialogFlags::Multiple,
				OpenFilenames
				);
		}

		if( bOpened )
		{
			// Save the path as default for next time
			FEditorDirectories::Get().SetLastDirectory( ELastDirectory::UNR, FPaths::GetPath( OpenFilenames[ 0 ] ) );

			TArray<FString> Filenames;
			for( int32 FileIndex = 0 ; FileIndex < OpenFilenames.Num() ; ++FileIndex )
			{
				// Strip paths from to get the level package names.
				const FString FilePath( OpenFilenames[FileIndex] );

				// make sure the level is in our package cache, because the async loading code will use this to find it
				if (!FPaths::FileExists(FilePath))
				{
					FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_LevelImportFromExternal", "Importing external sublevels is not allowed. Move the level files into the standard content directory and try again.\nAfter moving the level(s), restart the editor.") );
					return;				
				}

				FText ErrorMessage;
				bool bFilenameIsValid = FEditorFileUtils::IsValidMapFilename(OpenFilenames[FileIndex], ErrorMessage);
				if ( !bFilenameIsValid )
				{
					// Start the loop over, prompting for save again
					const FText DisplayFilename = FText::FromString( IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*OpenFilenames[FileIndex]) );
					FFormatNamedArguments Arguments;
					Arguments.Add(TEXT("Filename"), DisplayFilename);
					Arguments.Add(TEXT("LineTerminators"), FText::FromString(LINE_TERMINATOR LINE_TERMINATOR));
					Arguments.Add(TEXT("ErrorMessage"), ErrorMessage);
					const FText DisplayMessage = FText::Format( NSLOCTEXT("UnrealEd", "Error_InvalidLevelToAdd", "Unable to add streaming level {Filename}{LineTerminators}{ErrorMessage}"), Arguments );
					FMessageDialog::Open( EAppMsgType::Ok, DisplayMessage );
					return;
				}

				Filenames.Add( FilePath );
			}

			TArray<FString> PackageNames;
			for ( const auto& Filename : Filenames )
			{
				const FString& PackageName = FPackageName::FilenameToLongPackageName(Filename);
				PackageNames.Add(PackageName);
			}

			EditorLevelUtils::AddLevelsToWorld(CurrentWorld.Get(), PackageNames, AddedLevelStreamingClass);

			if (bRemoveInvalidSelectedLevelsAfter)
			{
				RemoveInvalidSelectedLevels_Executed();
			}
		}
	}
}

void FLevelCollectionViewModel::HandleAddExistingLevelSelected(const TArray<FAssetData>& SelectedAssets, bool bRemoveInvalidSelectedLevelsAfter)
{
	bAssetDialogOpen = false;
	TArray<FString> PackageNames;
	for (const auto& AssetData : SelectedAssets)
	{
		PackageNames.Add(AssetData.PackageName.ToString());
	}

	EditorLevelUtils::AddLevelsToWorld(CurrentWorld.Get(), PackageNames, AddedLevelStreamingClass);

	if (bRemoveInvalidSelectedLevelsAfter)
	{
		RemoveInvalidSelectedLevels_Executed();
	}
}

void FLevelCollectionViewModel::HandleAddExistingLevelCancelled()
{
	bAssetDialogOpen = false;
}

void FLevelCollectionViewModel::AddSelectedActorsToNewLevel_Executed()
{
	EditorLevelUtils::CreateNewLevel( CurrentWorld.Get(), true, AddedLevelStreamingClass );
}

void FLevelCollectionViewModel::FixupInvalidReference_Executed()
{
	// Browsing is essentially the same as adding an existing level
	const bool bRemoveInvalidSelectedLevelsAfter = true;
	AddExistingLevel(bRemoveInvalidSelectedLevelsAfter);
}

void FLevelCollectionViewModel::RemoveInvalidSelectedLevels_Executed()
{
	for( auto InvalidLevelIt = InvalidSelectedLevels.CreateIterator(); InvalidLevelIt; ++InvalidLevelIt )
	{
		ULevelStreaming* LevelStreaming = (*InvalidLevelIt)->GetLevelStreaming().Get();
		EditorLevelUtils::RemoveInvalidLevelFromWorld( LevelStreaming );
	}
	// Force a rebuild of the cached data
	PopulateLevelsList();
}

void FLevelCollectionViewModel::RemoveSelectedLevels_Executed()
{
	// If we have dirty levels that can be removed from the world
	bool bHaveDirtyLevels = false;
	// Gather levels to remove
	TArray< ULevel* > LevelsToRemove;
	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		ULevel* CurLevel = (*LevelIt)->GetLevel().Get();
		if( CurLevel != NULL )
		{
			if( CurLevel->GetOutermost()->IsDirty() && !FLevelUtils::IsLevelLocked( CurLevel ) )
			{
				// this level is dirty and can be removed from the world
				bHaveDirtyLevels = true;
			}
			LevelsToRemove.Add( CurLevel );
		}		
	}

	// Depending on the state of the level, create a warning message
	FText LevelWarning = LOCTEXT("RemoveLevel_Undo", "Removing levels cannot be undone.  Proceed?");
	if( bHaveDirtyLevels )
	{
		LevelWarning = LOCTEXT("RemoveLevel_Dirty", "Removing levels cannot be undone.  Any changes to these levels will be lost.  Proceed?");
	}

	// Ask the user if they really wish to remove the level(s)
	FSuppressableWarningDialog::FSetupInfo Info( LevelWarning, LOCTEXT("RemoveLevel_Message", "Remove Level"), "RemoveLevelWarning" );
	Info.ConfirmText = LOCTEXT( "RemoveLevel_Yes", "Yes");
	Info.CancelText = LOCTEXT( "RemoveLevel_No", "No");	
	FSuppressableWarningDialog RemoveLevelWarning( Info );
	if ( RemoveLevelWarning.ShowModal() == FSuppressableWarningDialog::Cancel )
	{
		return;
	}

	// If matinee is opened, and if it belongs to the level being removed, close it
	if( GLevelEditorModeTools().IsModeActive( FBuiltinEditorModes::EM_InterpEdit ) )
	{
		const FEdModeInterpEdit* InterpEditMode = (const FEdModeInterpEdit*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_InterpEdit );

		if ( InterpEditMode && InterpEditMode->MatineeActor && LevelsToRemove.Contains( InterpEditMode->MatineeActor->GetLevel() ) )
		{
			GLevelEditorModeTools().ActivateDefaultMode();
		}
	}
	else if( GLevelEditorModeTools().IsModeActive( FBuiltinEditorModes::EM_Landscape ) )
	{
		GLevelEditorModeTools().ActivateDefaultMode();
	}

	// Disassociate selected levels from streaming volumes since the levels will be removed
	ClearStreamingLevelVolumes();

	// Remove each level!
	for( auto LevelIt = LevelsToRemove.CreateIterator(); LevelIt; ++LevelIt )
	{
		ULevel* CurLevel = (*LevelIt);

		if( !FLevelUtils::IsLevelLocked(CurLevel) )
		{
			// If the level isn't locked (which means it can be removed) unselect all actors before removing the level
			// This avoids crashing in areas that rely on getting a selected actors level. The level will be invalid after its removed.
			for( int32 ActorIdx = 0; ActorIdx < CurLevel->Actors.Num(); ++ActorIdx )
			{
				GEditor->SelectActor( CurLevel->Actors[ ActorIdx ], /*bInSelected=*/ false, /*bSelectEvenIfHidden=*/ false );
			}
		}
		
		EditorLevelUtils::RemoveLevelFromWorld( CurLevel );
	}
	
	// Collect garbage to clear out the destroyed level
	CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
}

void FLevelCollectionViewModel::MergeSelectedLevels_Executed()
{
	// Stash off a copy of the original array, so the selection can be restored
	TArray< TSharedPtr< FLevelViewModel > > SelectedLevelsCopy;
	SelectedLevelsCopy.Append( SelectedLevels );

	//make sure the selected levels are made visible (and thus fully loaded) before merging
	ShowSelectedLevels_Executed();

	//restore the original selection and select all actors in the selected levels
	SetSelectedLevels( SelectedLevelsCopy );
	SelectActors_Executed();

	//Create a new level with the selected actors
	ULevel* NewLevel = EditorLevelUtils::CreateNewLevel( CurrentWorld.Get(),  true, AddedLevelStreamingClass );

	//If the new level was successfully created (ie the user did not cancel)
	if( ( NewLevel != NULL ) && ( CurrentWorld.IsValid() ) )
	{
		if( CurrentWorld->SetCurrentLevel( NewLevel ) )
		{
			FEditorDelegates::NewCurrentLevel.Broadcast();
		}
		GEditor->NoteSelectionChange();

		//restore the original selection and remove the levels that were merged
		SetSelectedLevels( SelectedLevelsCopy );
		RemoveSelectedLevels_Executed();

		//select the resulting merged level for feedback
		SetSelectedLevel( NewLevel->GetFName() );
	}
}

void FLevelCollectionViewModel::SetAddedLevelStreamingClass_Executed(UClass* InClass)
{
	AddedLevelStreamingClass = InClass;
}

bool FLevelCollectionViewModel::IsNewStreamingMethodChecked(UClass* InClass) const
{
	return AddedLevelStreamingClass == InClass;
}

bool FLevelCollectionViewModel::IsStreamingMethodChecked(UClass* InClass) const
{
	for (auto LevelIt = SelectedLevels.CreateConstIterator(); LevelIt; ++LevelIt )
	{
		const TSharedPtr<FLevelViewModel> Level = *LevelIt;
		const TWeakObjectPtr< ULevelStreaming > LevelStreaming = Level->GetLevelStreaming();
		if ( LevelStreaming.IsValid() && LevelStreaming->GetClass() == InClass )
		{
			return true;
		}
	}
	return false;
}

void FLevelCollectionViewModel::SetStreamingLevelsClass_Executed(UClass* InClass)
{
	// First prompt to save the selected levels, as changing the streaming method will unload/reload them
	SaveSelectedLevels_Executed();

	// Stash off a copy of the original array, as changing the streaming method can destroy the selection
	TArray< TSharedPtr< FLevelViewModel > > SelectedLevelsCopy;
	SelectedLevelsCopy.Append( SelectedLevels );

	// Apply the new streaming method to the selected levels
	for (auto LevelsToChangeIt = SelectedLevelsCopy.CreateIterator(); LevelsToChangeIt; ++LevelsToChangeIt )
	{
		(*LevelsToChangeIt)->SetStreamingClass(InClass);
	}

	SetSelectedLevels(SelectedLevelsCopy);
}

//level selection
void FLevelCollectionViewModel::SelectAllLevels_Executed()
{
	SetSelectedLevels( FilteredLevelViewModels );
}

void FLevelCollectionViewModel::DeselectAllLevels_Executed()
{
	TArray< TSharedPtr< FLevelViewModel > > NoLevels;
	SetSelectedLevels( NoLevels );
}

void FLevelCollectionViewModel::InvertSelection_Executed()
{
	TArray< TSharedPtr< FLevelViewModel > > InvertedLevels;

	for( auto LevelIt = FilteredLevelViewModels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only add the level if its valid
		if( (*LevelIt)->IsValid() )
		{
			if ( !SelectedLevels.Contains( *LevelIt ) )
			{
				InvertedLevels.Add( *LevelIt );
			}
		}
	}

	SetSelectedLevels( InvertedLevels );
}


//actors
void FLevelCollectionViewModel::SelectActors_Executed()
{
	//first clear any existing actor selection
	const FScopedTransaction Transaction( LOCTEXT("SelectActors", "Select Actors in Level") );
	GEditor->GetSelectedActors()->Modify();
	GEditor->SelectNone( false, true );

	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		if( (*LevelIt)->IsLevel() )
		{
			(*LevelIt)->SelectActors( /*bSelect*/ true, /*bNotify*/ true, /*bSelectEvenIfHidden*/ true );
		}
	}
}

void FLevelCollectionViewModel::DeselectActors_Executed()
{
	const FScopedTransaction Transaction( LOCTEXT("DeselectActors", "Deselect Actors in Level") );

	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		if( (*LevelIt)->IsLevel() )
		{
			(*LevelIt)->SelectActors( /*bSelect*/ false, /*bNotify*/ true, /*bSelectEvenIfHidden*/ true );
		}
	}
}

//streaming volumes
void FLevelCollectionViewModel::SelectStreamingVolumes_Executed()
{
	// Iterate over selected levels and make a list of volumes to select.
	TArray<ALevelStreamingVolume*> LevelStreamingVolumesToSelect;
	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only process the level if its valid.
		if( (*LevelIt)->IsValid() )
		{
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( (*LevelIt)->GetLevel().Get() ); 
			if ( StreamingLevel )
			{
				for ( int32 i = 0 ; i < StreamingLevel->EditorStreamingVolumes.Num() ; ++i )
				{
					ALevelStreamingVolume* LevelStreamingVolume = StreamingLevel->EditorStreamingVolumes[i];
					if ( LevelStreamingVolume )
					{
						LevelStreamingVolumesToSelect.Add( LevelStreamingVolume );
					}
				}
			}
		}
	}

	// Select the volumes.
	const FScopedTransaction Transaction( LOCTEXT("SelectAssociatedStreamingVolumes", "Select Associated Streaming Volumes") );
	GEditor->GetSelectedActors()->Modify();
	GEditor->SelectNone( false, true );

	for ( int32 i = 0 ; i < LevelStreamingVolumesToSelect.Num() ; ++i )
	{
		ALevelStreamingVolume* LevelStreamingVolume = LevelStreamingVolumesToSelect[i];
		GEditor->SelectActor( LevelStreamingVolume, /*bInSelected=*/ true, false, true );
	}


	GEditor->NoteSelectionChange();
}

void FLevelCollectionViewModel::AssembleSelectedLevelStreamingVolumes(TArray<ALevelStreamingVolume*>& OutLevelStreamingVolumes)
{
	OutLevelStreamingVolumes.Empty();
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		ALevelStreamingVolume* StreamingVolume = Cast<ALevelStreamingVolume>( *It );
		if ( StreamingVolume && StreamingVolume->GetLevel() == GWorld->PersistentLevel )
		{
			OutLevelStreamingVolumes.Add( StreamingVolume );
		}
	}
}

void FLevelCollectionViewModel::SetStreamingLevelVolumes_Executed()
{
	ClearStreamingLevelVolumes();
	AddStreamingLevelVolumes();
}

void FLevelCollectionViewModel::AddStreamingLevelVolumes()
{
	TArray<ALevelStreamingVolume*> LevelStreamingVolumes;
	AssembleSelectedLevelStreamingVolumes(LevelStreamingVolumes);
	
	if(LevelStreamingVolumes.Num() == 0)
	{
		return;
	}

	const FScopedTransaction Transaction( NSLOCTEXT("UnrealEd", "AddStreamingVolumes", "Add Streaming Volumes") );

	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		if( (*LevelIt)->IsLevel() )
		{
			ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( (*LevelIt)->GetLevel().Get() ); 
			if ( StreamingLevel )
			{
				StreamingLevel->Modify();
				for ( int32 i = 0 ; i < LevelStreamingVolumes.Num() ; ++i )
				{
					ALevelStreamingVolume* LevelStreamingVolume = LevelStreamingVolumes[i];

					// Associate the level to the volume.
					LevelStreamingVolume->Modify();
					LevelStreamingVolume->StreamingLevelNames.AddUnique( StreamingLevel->GetWorldAssetPackageFName() );

					// Associate the volume to the level.
					StreamingLevel->EditorStreamingVolumes.AddUnique( LevelStreamingVolume );
				}
			}
		}
	}
}

void FLevelCollectionViewModel::AddStreamingLevelVolumes_Executed()
{
	AddStreamingLevelVolumes();
}

void FLevelCollectionViewModel::ClearStreamingVolumes_Executed()
{
	int32 Result = FMessageDialog::Open( EAppMsgType::YesNo, LOCTEXT("ClearStreamingVolumesPrompt", "Are you sure you want to clear streaming volumes?") );

	// If the user wants to continue with the restart set the pending project to swtich to and close the editor
	if( Result == EAppReturnType::Yes )
	{
		ClearStreamingLevelVolumes();
	}
}


//visibility
void FLevelCollectionViewModel::SetVisible(TArray< TSharedPtr< FLevelViewModel > >& LevelViewModels, bool bVisible )
{
	TArray<TSharedPtr<FLevelViewModel>> CachedLevelViewModels = LevelViewModels;

	for (const TSharedPtr<FLevelViewModel>& Level : CachedLevelViewModels)
	{
		// Only set visibility if the level its valid.
		if (Level->IsValid())
		{
			Level->SetVisible(bVisible);
		}
	}
}

void FLevelCollectionViewModel::ShowSelectedLevels_Executed()
{
	//stash off a copy of the original array, as setting visibility can destroy the selection
	TArray< TSharedPtr< FLevelViewModel > > SelectedLevelsCopy;
	SelectedLevelsCopy.Append( SelectedLevels );

	SetVisible( SelectedLevelsCopy, true );

	//restore the selection 
	SetSelectedLevels( SelectedLevelsCopy );
}

void FLevelCollectionViewModel::HideSelectedLevels_Executed()
{
	//stash off a copy of the original array, as setting visibility can destroy the selection
	TArray< TSharedPtr< FLevelViewModel > > SelectedLevelsCopy;
	SelectedLevelsCopy.Append( SelectedLevels );

	SetVisible( SelectedLevelsCopy, false );
}

void FLevelCollectionViewModel::ShowOnlySelectedLevels_Executed()
{
	//stash off a copy of the original array, as setting visibility can destroy the selection
	TArray< TSharedPtr< FLevelViewModel > > SelectedLevelsCopy;
	SelectedLevelsCopy.Append( SelectedLevels );

	InvertSelection_Executed();
	HideSelectedLevels_Executed();
	SetSelectedLevels( SelectedLevelsCopy );
	ShowSelectedLevels_Executed();

	//restore the selection 
	SetSelectedLevels( SelectedLevelsCopy );
}

void FLevelCollectionViewModel::ShowAllLevels_Executed()
{
	SetVisible( AllLevelViewModels, true );
}

void FLevelCollectionViewModel::HideAllLevels_Executed()
{
	SetVisible( AllLevelViewModels, false );
}


//lock
void FLevelCollectionViewModel::LockSelectedLevels_Executed()
{
	const FText UndoTransactionText = (SelectedLevels.Num() == 1) ?
		LOCTEXT("LockLevel", "Lock Level") :
		LOCTEXT("LockMultipleLevels", "Lock Multiple Levels");

	const FScopedTransaction Transaction( UndoTransactionText );

	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only lock the level if its valid.
		if( (*LevelIt)->IsValid() )
		{
			(*LevelIt)->SetLocked( true );
		}
	}
}

void FLevelCollectionViewModel::UnockSelectedLevels_Executed()
{
	const FText UndoTransactionText = (SelectedLevels.Num() == 1) ?
		LOCTEXT("UnlockLevel", "Unlock Level") :
		LOCTEXT("UnlockMultipleLevels", "Unlock Multiple Levels");

	const FScopedTransaction Transaction( UndoTransactionText );

	for( auto LevelIt = SelectedLevels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only unlock the level if its valid.
		if( (*LevelIt)->IsValid() )
		{
			(*LevelIt)->SetLocked( false );
		}
	}
}

void FLevelCollectionViewModel::LockAllLevels_Executed()
{
	const FScopedTransaction Transaction( LOCTEXT("LockAllLevels", "Lock All Levels") );

	for( auto LevelIt = FilteredLevelViewModels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only lock the level if its valid.
		if( (*LevelIt)->IsValid() )
		{
			(*LevelIt)->SetLocked( true );
		}
	}
}

void FLevelCollectionViewModel::UnockAllLevels_Executed()
{
	const FScopedTransaction Transaction( LOCTEXT("UnlockAllLevels", "Unlock All Levels") );

	for( auto LevelIt = FilteredLevelViewModels.CreateIterator(); LevelIt; ++LevelIt )
	{
		// Only unlock the level if its valid.
		if( (*LevelIt)->IsValid() )
		{
			(*LevelIt)->SetLocked( false );
		}
	}
}

void FLevelCollectionViewModel::WorldAdded( UWorld* World )
{
	if( World )
	{
		// If the current world is not valid set this world to be the current one
		if( !CurrentWorld.IsValid() )
		{
			SetCurrentWorld( World );
		}
	}
}

void FLevelCollectionViewModel::WorldDestroyed( UWorld* InWorld )
{
	if( InWorld )
	{
		// Remove the level change handlers
		RemoveLevelChangeHandlers( InWorld );

		// If this world is the one we are showing levels for remove the reference
		if( InWorld == CurrentWorld.Get() )
		{
			CurrentWorld = NULL;
			
			if (InWorld != GEditor->GetEditorWorldContext().World())
			{
				SetCurrentWorld( GEditor->GetEditorWorldContext().World() );
			}
		}
	}

}

void FLevelCollectionViewModel::UpdateLevelActorsCount()
{
	for( auto It = AllLevelViewModels.CreateIterator(); It; ++It )
	{
		(*It)->UpdateLevelActorsCount();
	}

	bPendingUpdateActorsCount = false;
}

void FLevelCollectionViewModel::OnLevelActorAdded( AActor* InActor )
{
	if (InActor && 
		InActor->GetWorld() == CurrentWorld.Get()) // we care about our world only
	{
		bPendingUpdateActorsCount = true;
	}
}

void FLevelCollectionViewModel::OnLevelActorDeleted( AActor* InActor )
{
	bPendingUpdateActorsCount = true;
}

void FLevelCollectionViewModel::AddLevelChangeHandlers( UWorld* InWorld )
{
	if( ensure( InWorld ) )
	{
		InWorld->OnLevelsChanged().AddSP( this, &FLevelCollectionViewModel::Refresh );
		InWorld->OnSelectedLevelsChanged().AddSP( this, &FLevelCollectionViewModel::RefreshSelected );
	}
}

void FLevelCollectionViewModel::RemoveLevelChangeHandlers( UWorld* InWorld )
{
	if( ensure( InWorld ) )
	{
		InWorld->OnLevelsChanged().RemoveAll( this );
		InWorld->OnSelectedLevelsChanged().RemoveAll( this );
	}
}

void FLevelCollectionViewModel::SetCurrentWorld( UWorld* InWorld )
{
	if( InWorld == CurrentWorld.Get() )
	{
		return;
	}

	if( CurrentWorld.IsValid() )
	{
		RemoveLevelChangeHandlers( CurrentWorld.Get() );
	}
	CurrentWorld = InWorld;
	AddLevelChangeHandlers( InWorld );
	Refresh();
}

void FLevelCollectionViewModel::CacheCanExecuteSourceControlVars()
{
	bCanExecuteSCCCheckOut = false;
	bCanExecuteSCCOpenForAdd = false;
	bCanExecuteSCCCheckIn = false;
	bCanExecuteSCC = false;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	for( auto SelectedLevelIt = SelectedLevels.CreateIterator(); SelectedLevelIt; ++SelectedLevelIt )
	{
		if ( ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable() )
		{
			bCanExecuteSCC = true;

			// Check the SCC state for each package in the selected paths
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState((*SelectedLevelIt)->GetLevel().Get()->GetOutermost(), EStateCacheUsage::Use);

			if(SourceControlState.IsValid())
			{
				if ( SourceControlState->CanCheckout() )
				{
					bCanExecuteSCCCheckOut = true;
				}
				else if ( !SourceControlState->IsSourceControlled() )
				{
					bCanExecuteSCCOpenForAdd = true;
				}
				else if ( SourceControlState->IsCheckedOut() || SourceControlState->IsAdded() )
				{
					bCanExecuteSCCCheckIn = true;
				}
			}
		}

		if ( bCanExecuteSCCCheckOut
			&& bCanExecuteSCCOpenForAdd
			&& bCanExecuteSCCCheckIn
			)
		{
			// All options are available, no need to keep iterating
			break;
		}
	}
}

bool FLevelCollectionViewModel::IsValidMoveActorsToLevel()
{
	static bool bCachedIsValidActorMoveResult = false;
	if (bSelectionHasChanged)
	{
		bSelectionHasChanged = false;
		bCachedIsValidActorMoveResult = false;

		// We only operate on a single level
		if ( SelectedLevels.Num() == 1 )
		{
			ULevel* Level = SelectedLevels[0]->GetLevel().Get();
			if ( Level )
			{
				// Allow the move if at least one actor is in another level
				USelection* SelectedActors = GEditor->GetSelectedActors();
				for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
				{
					AActor* Actor = CastChecked<AActor>(*Iter);
					if (Actor != nullptr)
					{
						if (Actor->GetLevel() != Level)
						{
							bCachedIsValidActorMoveResult = true;
							break;
						}
					}
				}
			}
		}
	}

	// if non of the selected actors are in the level, just check the level is unlocked
	return bCachedIsValidActorMoveResult && AreAllSelectedLevelsUnlockedAndVisible();
}

void FLevelCollectionViewModel::OnActorSelectionChanged(UObject* obj)
{
	OnActorOrLevelSelectionChanged();
}

void FLevelCollectionViewModel::OnActorOrLevelSelectionChanged()
{
	bSelectionHasChanged = true;
}

#undef LOCTEXT_NAMESPACE
