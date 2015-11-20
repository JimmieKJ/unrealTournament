// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "WorldBrowserPrivatePCH.h"

#include "Editor/PropertyEditor/Public/IDetailsView.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "Editor/MainFrame/Public/MainFrame.h"
#include "LevelEditor.h"
#include "DesktopPlatformModule.h"
#include "AssetData.h"

#include "StreamingLevelCustomization.h"
#include "StreamingLevelCollectionModel.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Selection.h"
#include "Engine/LevelStreamingVolume.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/LevelStreamingKismet.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

FStreamingLevelCollectionModel::FStreamingLevelCollectionModel()
	: FLevelCollectionModel()
	, AddedLevelStreamingClass(ULevelStreamingKismet::StaticClass())
	, bAssetDialogOpen(false)
{
	const TSubclassOf<ULevelStreaming> DefaultLevelStreamingClass = GetDefault<ULevelEditorMiscSettings>()->DefaultLevelStreamingClass;
	if ( DefaultLevelStreamingClass )
	{
		AddedLevelStreamingClass = DefaultLevelStreamingClass;
	}
}

FStreamingLevelCollectionModel::~FStreamingLevelCollectionModel()
{
	GEditor->UnregisterForUndo( this );
}

void FStreamingLevelCollectionModel::Initialize(UWorld* InWorld)
{
	BindCommands();	
	GEditor->RegisterForUndo( this );
	
	FLevelCollectionModel::Initialize(InWorld);
}

void FStreamingLevelCollectionModel::OnLevelsCollectionChanged()
{
	InvalidSelectedLevels.Empty();
	
	// We have to have valid world
	if (!CurrentWorld.IsValid())
	{
		return;
	}

	// Add model for a persistent level
	TSharedPtr<FStreamingLevelModel> PersistentLevelModel = MakeShareable(new FStreamingLevelModel(*this, nullptr));
	PersistentLevelModel->SetLevelExpansionFlag(true);
	RootLevelsList.Add(PersistentLevelModel);
	AllLevelsList.Add(PersistentLevelModel);
	AllLevelsMap.Add(PersistentLevelModel->GetLongPackageName(), PersistentLevelModel);
		
	// Add models for each streaming level in the world
	for (auto It = CurrentWorld->StreamingLevels.CreateConstIterator(); It; ++It)
	{
		ULevelStreaming* StreamingLevel = (*It);
		if (StreamingLevel != nullptr)
		{
			TSharedPtr<FStreamingLevelModel> LevelModel = MakeShareable(new FStreamingLevelModel(*this, StreamingLevel));
			AllLevelsList.Add(LevelModel);
			AllLevelsMap.Add(LevelModel->GetLongPackageName(), LevelModel);

			PersistentLevelModel->AddChild(LevelModel);
			LevelModel->SetParent(PersistentLevelModel);
		}
	}
	
	FLevelCollectionModel::OnLevelsCollectionChanged();

	// Sync levels selection to world
	SetSelectedLevelsFromWorld();
}

void FStreamingLevelCollectionModel::OnLevelsSelectionChanged()
{
	InvalidSelectedLevels.Empty();

	for (TSharedPtr<FLevelModel> LevelModel : SelectedLevelsList)
	{
		if (!LevelModel->HasValidPackage())
		{
			InvalidSelectedLevels.Add(LevelModel);
		}
	}

	FLevelCollectionModel::OnLevelsSelectionChanged();
}

void FStreamingLevelCollectionModel::UnloadLevels(const FLevelModelList& InLevelList)
{
	if (IsReadOnly())
	{
		return;
	}

	// Persistent level cannot be unloaded
	if (InLevelList.Num() == 1 && InLevelList[0]->IsPersistent())
	{
		return;
	}
		
	bool bHaveDirtyLevels = false;
	for (auto It = InLevelList.CreateConstIterator(); It; ++It)
	{
		if ((*It)->IsDirty() && !(*It)->IsLocked() && !(*It)->IsPersistent())
		{
			// this level is dirty and can be removed from the world
			bHaveDirtyLevels = true;
			break;
		}
	}

	// Depending on the state of the level, create a warning message
	FText LevelWarning = LOCTEXT("RemoveLevel_Undo", "Removing levels cannot be undone.  Proceed?");
	if (bHaveDirtyLevels)
	{
		LevelWarning = LOCTEXT("RemoveLevel_Dirty", "Removing levels cannot be undone.  Any changes to these levels will be lost.  Proceed?");
	}

	// Ask the user if they really wish to remove the level(s)
	FSuppressableWarningDialog::FSetupInfo Info( LevelWarning, LOCTEXT("RemoveLevel_Message", "Remove Level"), "RemoveLevelWarning" );
	Info.ConfirmText = LOCTEXT( "RemoveLevel_Yes", "Yes");
	Info.CancelText = LOCTEXT( "RemoveLevel_No", "No");	
	FSuppressableWarningDialog RemoveLevelWarning( Info );
	if (RemoveLevelWarning.ShowModal() == FSuppressableWarningDialog::Cancel)
	{
		return;
	}
			
	// This will remove streaming levels from a persistent world, so we need to re-populate levels list
	FLevelCollectionModel::UnloadLevels(InLevelList);
	PopulateLevelsList();
}

void FStreamingLevelCollectionModel::AddExistingLevelsFromAssetData(const TArray<FAssetData>& WorldList)
{
	HandleAddExistingLevelSelected(WorldList, false);
}

void FStreamingLevelCollectionModel::BindCommands()
{
	FLevelCollectionModel::BindCommands();
	
	const FLevelCollectionCommands& Commands = FLevelCollectionCommands::Get();
	FUICommandList& ActionList = *CommandList;

	//invalid selected levels
	ActionList.MapAction( Commands.FixUpInvalidReference,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::FixupInvalidReference_Executed ) );
	
	ActionList.MapAction( Commands.RemoveInvalidReference,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::RemoveInvalidSelectedLevels_Executed ));

	//levels
	ActionList.MapAction( Commands.World_CreateEmptyLevel,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::CreateEmptyLevel_Executed  ) );
	
	ActionList.MapAction( Commands.World_AddExistingLevel,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::AddExistingLevel_Executed ) );

	ActionList.MapAction( Commands.World_AddSelectedActorsToNewLevel,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::AddSelectedActorsToNewLevel_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionModel::AreActorsSelected ) );
	
	ActionList.MapAction( Commands.World_RemoveSelectedLevels,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::UnloadSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionModel::AreAllSelectedLevelsEditable ) );
	
	ActionList.MapAction( Commands.World_MergeSelectedLevels,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::MergeSelectedLevels_Executed  ),
		FCanExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::AreAllSelectedLevelsEditableAndNotPersistent ) );
	
	// new level streaming method
	ActionList.MapAction( Commands.SetAddStreamingMethod_Blueprint,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::SetAddedLevelStreamingClass_Executed, ULevelStreamingKismet::StaticClass()  ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FStreamingLevelCollectionModel::IsNewStreamingMethodChecked, ULevelStreamingKismet::StaticClass()));

	ActionList.MapAction( Commands.SetAddStreamingMethod_AlwaysLoaded,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::SetAddedLevelStreamingClass_Executed, ULevelStreamingAlwaysLoaded::StaticClass()  ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &FStreamingLevelCollectionModel::IsNewStreamingMethodChecked, ULevelStreamingAlwaysLoaded::StaticClass()));

	// change streaming method
	ActionList.MapAction( Commands.SetStreamingMethod_Blueprint,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::SetStreamingLevelsClass_Executed, ULevelStreamingKismet::StaticClass()  ),
		FCanExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::AreAllSelectedLevelsEditable ),
		FIsActionChecked::CreateSP( this, &FStreamingLevelCollectionModel::IsStreamingMethodChecked, ULevelStreamingKismet::StaticClass()));

	ActionList.MapAction( Commands.SetStreamingMethod_AlwaysLoaded,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::SetStreamingLevelsClass_Executed, ULevelStreamingAlwaysLoaded::StaticClass()  ),
		FCanExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::AreAllSelectedLevelsEditable ),
		FIsActionChecked::CreateSP( this, &FStreamingLevelCollectionModel::IsStreamingMethodChecked, ULevelStreamingAlwaysLoaded::StaticClass()));

	//streaming volume
	ActionList.MapAction( Commands.SelectStreamingVolumes,
		FExecuteAction::CreateSP( this, &FStreamingLevelCollectionModel::SelectStreamingVolumes_Executed  ),
		FCanExecuteAction::CreateSP( this, &FLevelCollectionModel::AreAllSelectedLevelsEditable));

}

TSharedPtr<FLevelDragDropOp> FStreamingLevelCollectionModel::CreateDragDropOp() const
{
	TArray<TWeakObjectPtr<ULevelStreaming>> LevelsToDrag;

	for (auto It = SelectedLevelsList.CreateConstIterator(); It; ++It)
	{
		TSharedPtr<FStreamingLevelModel> TargetModel = StaticCastSharedPtr<FStreamingLevelModel>(*It);
		
		if (TargetModel->GetLevelStreaming().IsValid())
		{
			LevelsToDrag.Add(TargetModel->GetLevelStreaming());
		}
	}

	if (LevelsToDrag.Num())
	{
		return FLevelDragDropOp::New(LevelsToDrag);
	}
	else
	{
		return FLevelCollectionModel::CreateDragDropOp();
	}
}

void FStreamingLevelCollectionModel::BuildHierarchyMenu(FMenuBuilder& InMenuBuilder) const
{
	const FLevelCollectionCommands& Commands = FLevelCollectionCommands::Get();

	// We show the "level missing" commands, when missing level is selected solely
	if (IsOneLevelSelected() && InvalidSelectedLevels.Num() == 1)
	{
		InMenuBuilder.BeginSection("MissingLevel", LOCTEXT("ViewHeaderRemove", "Missing Level") );
		{
			InMenuBuilder.AddMenuEntry( Commands.FixUpInvalidReference );
			InMenuBuilder.AddMenuEntry( Commands.RemoveInvalidReference );
		}
		InMenuBuilder.EndSection();
	}

	// Add common commands
	InMenuBuilder.BeginSection("Levels", LOCTEXT("LevelsHeader", "Levels") );
	{
		// Make level current
		if (IsOneLevelSelected())
		{
			InMenuBuilder.AddMenuEntry( Commands.World_MakeLevelCurrent );
		}
		
		// Visibility commands
		InMenuBuilder.AddSubMenu( 
			LOCTEXT("VisibilityHeader", "Visibility"),
			LOCTEXT("VisibilitySubMenu_ToolTip", "Selected Level(s) visibility commands"),
			FNewMenuDelegate::CreateSP(this, &FStreamingLevelCollectionModel::FillVisibilitySubMenu ) );

		// Lock commands
		InMenuBuilder.AddSubMenu( 
			LOCTEXT("LockHeader", "Lock"),
			LOCTEXT("LockSubMenu_ToolTip", "Selected Level(s) lock commands"),
			FNewMenuDelegate::CreateSP(this, &FStreamingLevelCollectionModel::FillLockSubMenu ) );
		
		// Level streaming specific commands
		if (AreAnyLevelsSelected() && !(IsOneLevelSelected() && GetSelectedLevels()[0]->IsPersistent()))
		{
			InMenuBuilder.AddMenuEntry(Commands.World_RemoveSelectedLevels);
			//
			InMenuBuilder.AddSubMenu( 
				LOCTEXT("LevelsChangeStreamingMethod", "Change Streaming Method"),
				LOCTEXT("LevelsChangeStreamingMethod_Tooltip", "Changes the streaming method for the selected levels"),
				FNewMenuDelegate::CreateRaw(this, &FStreamingLevelCollectionModel::FillSetStreamingMethodSubMenu ));
		}
	}
	InMenuBuilder.EndSection();
	

	// Level selection commands
	InMenuBuilder.BeginSection("LevelsSelection", LOCTEXT("SelectionHeader", "Selection") );
	{
		InMenuBuilder.AddMenuEntry( Commands.SelectAllLevels );
		InMenuBuilder.AddMenuEntry( Commands.DeselectAllLevels );
		InMenuBuilder.AddMenuEntry( Commands.InvertLevelSelection );
	}
	InMenuBuilder.EndSection();
	
	// Level actors selection commands
	InMenuBuilder.BeginSection("Actors", LOCTEXT("ActorsHeader", "Actors") );
	{
		InMenuBuilder.AddMenuEntry( Commands.AddsActors );
		InMenuBuilder.AddMenuEntry( Commands.RemovesActors );

		// Move selected actors to a selected level
		if (IsOneLevelSelected())
		{
			InMenuBuilder.AddMenuEntry( Commands.MoveActorsToSelected );
			InMenuBuilder.AddMenuEntry( Commands.MoveFoliageToSelected );
		}

		if (AreAnyLevelsSelected() && !(IsOneLevelSelected() && SelectedLevelsList[0]->IsPersistent()))
		{
			InMenuBuilder.AddMenuEntry( Commands.SelectStreamingVolumes );
		}
	}
	InMenuBuilder.EndSection();
}

void FStreamingLevelCollectionModel::FillSetStreamingMethodSubMenu(FMenuBuilder& InMenuBuilder)
{
	const FLevelCollectionCommands& Commands = FLevelCollectionCommands::Get();
	InMenuBuilder.AddMenuEntry( Commands.SetStreamingMethod_Blueprint, NAME_None, LOCTEXT("SetStreamingMethodBlueprintOverride", "Blueprint") );
	InMenuBuilder.AddMenuEntry( Commands.SetStreamingMethod_AlwaysLoaded, NAME_None, LOCTEXT("SetStreamingMethodAlwaysLoadedOverride", "Always Loaded") );
}

void FStreamingLevelCollectionModel::CustomizeFileMainMenu(FMenuBuilder& InMenuBuilder) const
{
	FLevelCollectionModel::CustomizeFileMainMenu(InMenuBuilder);

	const FLevelCollectionCommands& Commands = FLevelCollectionCommands::Get();
		
	InMenuBuilder.BeginSection("LevelsAddLevel");
	{
		InMenuBuilder.AddSubMenu( 
			LOCTEXT("LevelsStreamingMethod", "Default Streaming Method"),
			LOCTEXT("LevelsStreamingMethod_Tooltip", "Changes the default streaming method for a new levels"),
			FNewMenuDelegate::CreateRaw(this, &FStreamingLevelCollectionModel::FillDefaultStreamingMethodSubMenu ) );
		
		InMenuBuilder.AddMenuEntry( Commands.World_CreateEmptyLevel );
		InMenuBuilder.AddMenuEntry( Commands.World_AddExistingLevel );
		InMenuBuilder.AddMenuEntry( Commands.World_AddSelectedActorsToNewLevel );
		InMenuBuilder.AddMenuEntry( Commands.World_MergeSelectedLevels );
	}
	InMenuBuilder.EndSection();
}

void FStreamingLevelCollectionModel::FillDefaultStreamingMethodSubMenu(FMenuBuilder& InMenuBuilder)
{
	const FLevelCollectionCommands& Commands = FLevelCollectionCommands::Get();
	InMenuBuilder.AddMenuEntry( Commands.SetAddStreamingMethod_Blueprint, NAME_None, LOCTEXT("SetAddStreamingMethodBlueprintOverride", "Blueprint") );
	InMenuBuilder.AddMenuEntry( Commands.SetAddStreamingMethod_AlwaysLoaded, NAME_None, LOCTEXT("SetAddStreamingMethodAlwaysLoadedOverride", "Always Loaded") );
}

void FStreamingLevelCollectionModel::RegisterDetailsCustomization(FPropertyEditorModule& InPropertyModule, 
																	TSharedPtr<IDetailsView> InDetailsView)
{
	TSharedRef<FStreamingLevelCollectionModel> WorldModel = StaticCastSharedRef<FStreamingLevelCollectionModel>(this->AsShared());

	InDetailsView->RegisterInstancedCustomPropertyLayout(ULevelStreaming::StaticClass(), 
		FOnGetDetailCustomizationInstance::CreateStatic(&FStreamingLevelCustomization::MakeInstance,  WorldModel)
		);
}

void FStreamingLevelCollectionModel::UnregisterDetailsCustomization(FPropertyEditorModule& InPropertyModule,
																	TSharedPtr<IDetailsView> InDetailsView)
{
	InDetailsView->UnregisterInstancedCustomPropertyLayout(ULevelStreaming::StaticClass());
}

const FLevelModelList& FStreamingLevelCollectionModel::GetInvalidSelectedLevels() const 
{ 
	return InvalidSelectedLevels;
}

//levels
void FStreamingLevelCollectionModel::CreateEmptyLevel_Executed()
{
	EditorLevelUtils::CreateNewLevel( CurrentWorld.Get(), false, AddedLevelStreamingClass );

	// Force a cached level list rebuild
	PopulateLevelsList();
}

void FStreamingLevelCollectionModel::AddExistingLevel_Executed()
{
	AddExistingLevel();
}

void FStreamingLevelCollectionModel::AddExistingLevel(bool bRemoveInvalidSelectedLevelsAfter)
{
	if (UEditorEngine::IsUsingWorldAssets())
	{
		if (!bAssetDialogOpen)
		{
			bAssetDialogOpen = true;
			FEditorFileUtils::FOnLevelsChosen LevelsChosenDelegate = FEditorFileUtils::FOnLevelsChosen::CreateSP(this, &FStreamingLevelCollectionModel::HandleAddExistingLevelSelected, bRemoveInvalidSelectedLevelsAfter);
			FEditorFileUtils::FOnLevelPickingCancelled LevelPickingCancelledDelegate = FEditorFileUtils::FOnLevelPickingCancelled::CreateSP(this, &FStreamingLevelCollectionModel::HandleAddExistingLevelCancelled);
			const bool bAllowMultipleSelection = true;
			FEditorFileUtils::OpenLevelPickingDialog(LevelsChosenDelegate, LevelPickingCancelledDelegate, bAllowMultipleSelection);
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
			for (const auto& Filename : Filenames)
			{
				const FString& PackageName = FPackageName::FilenameToLongPackageName(Filename);
				PackageNames.Add(PackageName);
			}

			// Save or selected list, adding a new level will clean it up
			FLevelModelList SavedInvalidSelectedLevels = InvalidSelectedLevels;

			EditorLevelUtils::AddLevelsToWorld(CurrentWorld.Get(), PackageNames, AddedLevelStreamingClass);
			
			// Force a cached level list rebuild
			PopulateLevelsList();
			
			if (bRemoveInvalidSelectedLevelsAfter)
			{
				InvalidSelectedLevels = SavedInvalidSelectedLevels;
				RemoveInvalidSelectedLevels_Executed();
			}
		}
	}
}

void FStreamingLevelCollectionModel::HandleAddExistingLevelSelected(const TArray<FAssetData>& SelectedAssets, bool bRemoveInvalidSelectedLevelsAfter)
{
	bAssetDialogOpen = false;

	TArray<FString> PackageNames;
	for (const auto& AssetData : SelectedAssets)
	{
		PackageNames.Add(AssetData.PackageName.ToString());
	}

	// Save or selected list, adding a new level will clean it up
	FLevelModelList SavedInvalidSelectedLevels = InvalidSelectedLevels;

	EditorLevelUtils::AddLevelsToWorld(CurrentWorld.Get(), PackageNames, AddedLevelStreamingClass);

	// Force a cached level list rebuild
	PopulateLevelsList();

	if (bRemoveInvalidSelectedLevelsAfter)
	{
		InvalidSelectedLevels = SavedInvalidSelectedLevels;
		RemoveInvalidSelectedLevels_Executed();
	}
}

void FStreamingLevelCollectionModel::HandleAddExistingLevelCancelled()
{
	bAssetDialogOpen = false;
}

void FStreamingLevelCollectionModel::AddSelectedActorsToNewLevel_Executed()
{
	EditorLevelUtils::CreateNewLevel( CurrentWorld.Get(), true, AddedLevelStreamingClass );

	// Force a cached level list rebuild
	PopulateLevelsList();
}

void FStreamingLevelCollectionModel::FixupInvalidReference_Executed()
{
	// Browsing is essentially the same as adding an existing level
	const bool bRemoveInvalidSelectedLevelsAfter = true;
	AddExistingLevel(bRemoveInvalidSelectedLevelsAfter);
}

void FStreamingLevelCollectionModel::RemoveInvalidSelectedLevels_Executed()
{
	for (TSharedPtr<FLevelModel> LevelModel : InvalidSelectedLevels)
	{
		TSharedPtr<FStreamingLevelModel> TargetModel = StaticCastSharedPtr<FStreamingLevelModel>(LevelModel);
		ULevelStreaming* LevelStreaming = TargetModel->GetLevelStreaming().Get();

		if (LevelStreaming)
		{
			EditorLevelUtils::RemoveInvalidLevelFromWorld(LevelStreaming);
		}
	}

	// Force a cached level list rebuild
	PopulateLevelsList();
}

void FStreamingLevelCollectionModel::MergeSelectedLevels_Executed()
{
	if (SelectedLevelsList.Num() <= 1)
	{
		return;
	}
	
	// Stash off a copy of the original array, so the selection can be restored
	FLevelModelList SelectedLevelsCopy = SelectedLevelsList;

	//make sure the selected levels are made visible (and thus fully loaded) before merging
	ShowSelectedLevels_Executed();

	//restore the original selection and select all actors in the selected levels
	SetSelectedLevels(SelectedLevelsCopy);
	SelectActors_Executed();

	//Create a new level with the selected actors
	ULevel* NewLevel = EditorLevelUtils::CreateNewLevel(CurrentWorld.Get(),  true, AddedLevelStreamingClass);

	//If the new level was successfully created (ie the user did not cancel)
	if ((NewLevel != NULL) && (CurrentWorld.IsValid()))
	{
		if (CurrentWorld->SetCurrentLevel(NewLevel))
		{
			FEditorDelegates::NewCurrentLevel.Broadcast();
		}

		GEditor->NoteSelectionChange();

		//restore the original selection and remove the levels that were merged
		SetSelectedLevels(SelectedLevelsCopy);
		UnloadSelectedLevels_Executed();
	}

	// Force a cached level list rebuild
	PopulateLevelsList();
}

void FStreamingLevelCollectionModel::SetAddedLevelStreamingClass_Executed(UClass* InClass)
{
	AddedLevelStreamingClass = InClass;
}

bool FStreamingLevelCollectionModel::IsNewStreamingMethodChecked(UClass* InClass) const
{
	return AddedLevelStreamingClass == InClass;
}

bool FStreamingLevelCollectionModel::IsStreamingMethodChecked(UClass* InClass) const
{
	for (auto It = SelectedLevelsList.CreateConstIterator(); It; ++It)
	{
		TSharedPtr<FStreamingLevelModel> TargetModel = StaticCastSharedPtr<FStreamingLevelModel>(*It);
		ULevelStreaming* LevelStreaming = TargetModel->GetLevelStreaming().Get();
		
		if (LevelStreaming && LevelStreaming->GetClass() == InClass)
		{
			return true;
		}
	}
	return false;
}

void FStreamingLevelCollectionModel::SetStreamingLevelsClass_Executed(UClass* InClass)
{
	// First prompt to save the selected levels, as changing the streaming method will unload/reload them
	SaveSelectedLevels_Executed();

	// Stash off a copy of the original array, as changing the streaming method can destroy the selection
	FLevelModelList SelectedLevelsCopy = GetSelectedLevels();

	// Apply the new streaming method to the selected levels
	for (auto It = SelectedLevelsCopy.CreateIterator(); It; ++It)
	{
		TSharedPtr<FStreamingLevelModel> TargetModel = StaticCastSharedPtr<FStreamingLevelModel>(*It);
		TargetModel->SetStreamingClass(InClass);
	}

	SetSelectedLevels(SelectedLevelsCopy);

	// Force a cached level list rebuild
	PopulateLevelsList();
}

//streaming volumes
void FStreamingLevelCollectionModel::SelectStreamingVolumes_Executed()
{
	// Iterate over selected levels and make a list of volumes to select.
	TArray<ALevelStreamingVolume*> LevelStreamingVolumesToSelect;
	for (auto It = SelectedLevelsList.CreateIterator(); It; ++It)
	{
		TSharedPtr<FStreamingLevelModel> TargetModel = StaticCastSharedPtr<FStreamingLevelModel>(*It);
		ULevelStreaming* StreamingLevel = TargetModel->GetLevelStreaming().Get();

		if (StreamingLevel)
		{
			for (int32 i = 0; i < StreamingLevel->EditorStreamingVolumes.Num(); ++i)
			{
				ALevelStreamingVolume* LevelStreamingVolume = StreamingLevel->EditorStreamingVolumes[i];
				if (LevelStreamingVolume)
				{
					LevelStreamingVolumesToSelect.Add(LevelStreamingVolume);
				}
			}
		}
	}

	// Select the volumes.
	const FScopedTransaction Transaction( LOCTEXT("SelectAssociatedStreamingVolumes", "Select Associated Streaming Volumes") );
	GEditor->GetSelectedActors()->Modify();
	GEditor->SelectNone(false, true);

	for (int32 i = 0 ; i < LevelStreamingVolumesToSelect.Num() ; ++i)
	{
		ALevelStreamingVolume* LevelStreamingVolume = LevelStreamingVolumesToSelect[i];
		GEditor->SelectActor(LevelStreamingVolume, /*bInSelected=*/ true, false, true);
	}
	
	GEditor->NoteSelectionChange();
}

#undef LOCTEXT_NAMESPACE
