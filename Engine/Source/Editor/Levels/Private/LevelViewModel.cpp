// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelsPrivatePCH.h"

#include "Engine/LevelScriptBlueprint.h"
#include "LevelViewModel.h"
#include "LevelUtils.h"
#include "EditorLevelUtils.h"
#include "Toolkits/AssetEditorManager.h"
#include "EditorSupportDelegates.h"
#include "SColorPicker.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "Level"

FLevelViewModel::FLevelViewModel( const TWeakObjectPtr< class ULevel >& InLevel, 
								  const TWeakObjectPtr< class ULevelStreaming >& InLevelStreaming, 
								  const TWeakObjectPtr< UEditorEngine >& InEditor )
	: Editor( InEditor )
	, Level( InLevel )
	, LevelStreaming( InLevelStreaming )
	, LevelActorsCount(0)
{
}

void FLevelViewModel::Initialize()
{
	Editor->RegisterForUndo( this );
}

FLevelViewModel::~FLevelViewModel()
{
	Editor->UnregisterForUndo( this );
}

void FLevelViewModel::RefreshStreamingLevelIndex()
{
	if ( IsLevel() )
	{
		UWorld *OwningWorld = Level->OwningWorld;
		check( OwningWorld );

		for( int32 LevelIndex = 0 ; LevelIndex < OwningWorld->StreamingLevels.Num() ; ++LevelIndex )
		{
			ULevelStreaming *StreamingLevel = OwningWorld->StreamingLevels[LevelIndex];
			if ( Level.Get() == StreamingLevel->GetLoadedLevel() )
			{
				StreamingLevelIndex = LevelIndex;
				LevelStreaming = StreamingLevel;
				break;
			}
		}
	}
}

FName FLevelViewModel::GetFName() const
{
	if ( IsLevel() )
	{
		return Level->GetOutermost()->GetFName();
	}

	return NAME_None;
}

FString FLevelViewModel::GetName(bool bForceDisplayPath /*=false*/, bool bDisplayTags /*=true*/) const
{
	FString DisplayName;

	if( IsLevel() )
	{
		if ( bDisplayTags && IsPersistent() )
		{
			DisplayName += LOCTEXT("PersistentTag", "Persistent Level").ToString();
		}
		else
		{
			DisplayName += Level->GetOutermost()->GetName();

			bool bDisplayPathsInLevelBrowser = (GetDefault<ULevelBrowserSettings>()->bDisplayPaths);

			if ( !(bDisplayPathsInLevelBrowser || bForceDisplayPath) )
			{
				DisplayName = FPaths::GetCleanFilename(DisplayName);
			}
		}

		if ( bDisplayTags && IsDirty() )
		{
			DisplayName += TEXT(" * ");
		}

		if ( bDisplayTags && (IsCurrent()) )
		{
			DisplayName += LOCTEXT("CurrentLevelTag", " [Current Level]").ToString();
		}
	}
	else if ( IsLevelStreaming() )
	{
		DisplayName += LevelStreaming->GetWorldAssetPackageName();

		bool bDisplayPathsInLevelBrowser = (GetDefault<ULevelBrowserSettings>()->bDisplayPaths);

		if ( !(bDisplayPathsInLevelBrowser || bForceDisplayPath) )
		{
			DisplayName = FPaths::GetCleanFilename(DisplayName);
		}
		
		if ( bDisplayTags && IsDirty() )
		{
			DisplayName += TEXT(" * ");
		}

		DisplayName += LOCTEXT("MissingLevelErrorText", " [Missing Level]").ToString();
	}

	return DisplayName;
}

FText FLevelViewModel::GetDisplayName() const
{
	return FText::FromString(GetName());
}

FText FLevelViewModel::GetActorCountString() const
{
	return FText::AsNumber(LevelActorsCount);
}

FText FLevelViewModel::GetLightmassSizeString() const
{
	ULevel* CurLevel = Level.Get();

	if ( CurLevel && GetDefault<ULevelBrowserSettings>()->bDisplayLightmassSize  )
	{
		// Update metrics
		static const float ByteConversion = 1.0f / 1024.0f;
		float LightmapSize = CurLevel->LightmapTotalSize * ByteConversion;
		
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::AsNumber(LightmapSize, &FormatOptions);
	}

	return FText::GetEmpty();
}

FText FLevelViewModel::GetFileSizeString() const
{
	ULevel* CurLevel = Level.Get();

	if ( CurLevel && GetDefault<ULevelBrowserSettings>()->bDisplayFileSize  )
	{
		// Update metrics
		static const float ByteConversion = 1.0f / 1024.0f;
		float FileSize = CurLevel->GetOutermost()->GetFileSize() * ByteConversion * ByteConversion;
		
		static const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		return FText::AsNumber(FileSize, &FormatOptions);
	}

	return FText::GetEmpty();
}

void FLevelViewModel::ClearDirtyFlag()
{
	if ( IsLevel() )
	{
		return Level->GetOutermost()->SetDirtyFlag(false);
	}
	else if ( IsLevelStreaming() )
	{
		return LevelStreaming->GetOutermost()->SetDirtyFlag(false);
	}
}

bool FLevelViewModel::IsDirty() const
{
	if ( IsLevel() )
	{
		return Level->GetOutermost()->IsDirty();
	}
	else if ( IsLevelStreaming() )
	{
		return LevelStreaming->GetOutermost()->IsDirty();
	}
	else
	{
		return false;
	}
}

bool FLevelViewModel::IsCurrent() const
{
	if ( IsLevel() )
	{
		return Level->IsCurrentLevel();
	}
	else
	{
		return false;
	}
}

bool FLevelViewModel::IsPersistent() const
{
	if( !Level.IsValid() )
	{
		return false;
	}

	return ( Level->IsPersistentLevel() );
}

bool FLevelViewModel::IsLevel() const
{
	return Level.IsValid();
}

bool FLevelViewModel::IsLevelStreaming() const
{
	return LevelStreaming.IsValid();
}

bool FLevelViewModel::IsVisible() const
{
	if( !Level.IsValid() )
	{
		return false;
	}

	return ( FLevelUtils::IsLevelVisible(Level.Get()) );

}

void FLevelViewModel::ToggleVisibility()
{
	if( !Level.IsValid() )
	{
		return;
	}

	SetVisible( !IsVisible() );
}

void FLevelViewModel::SetVisible(bool bVisible)
{
	//don't create unnecessary transactions
	if ( IsVisible() == bVisible )
	{
		return;
	}

	const bool oldIsDirty = IsDirty();

	const FScopedTransaction Transaction( LOCTEXT("ToggleVisibility", "Toggle Level Visibility") );

	//this call hides all owned actors, etc
	EditorLevelUtils::SetLevelVisibility( Level.Get(), bVisible, false );

	if( !oldIsDirty )
	{
		// don't set the dirty flag if we're just changing the visibility of the level within the editor
		ClearDirtyFlag();
	}
}

bool FLevelViewModel::IsReadOnly() const
{
	const UPackage* pPackage = Cast<UPackage>( Level->GetOutermost() );
	if ( pPackage )
	{
		FString PackageFileName;
		if ( FPackageName::DoesPackageExist( pPackage->GetName(), NULL, &PackageFileName ) )
		{
			return IFileManager::Get().IsReadOnly( *PackageFileName );
		}
	}

	return false;
}

bool FLevelViewModel::IsLocked() const
{
	if( !Level.IsValid() )
	{
		return false;
	}

	// Don't permit spawning in read only levels if they are locked
	if ( GIsEditor && !GIsEditorLoadingPackage )
	{
		if ( GEngine && GEngine->bLockReadOnlyLevels )
		{
			if(IsReadOnly())
			{
				return true;
			}
		}
	}

	return ( FLevelUtils::IsLevelLocked(Level.Get()) );

}

bool FLevelViewModel::IsValid() const
{
	bool bValid = Level.IsValid() || LevelStreaming.IsValid();
	return bValid;
}

void FLevelViewModel::ToggleLock()
{
	if( !Level.IsValid() )
	{
		return;
	}

	SetLocked( !IsLocked() );
}

/**
* Attempt to lock/unlock the level of this window
*
* @param	bLocked	If true, attempt to lock the level; If false, attempt to unlock the level
*/
void FLevelViewModel::SetLocked(bool bLocked)
{
	if( !Level.IsValid() )
	{
		return;
	}

	// Do nothing if attempting to set the level to the same locked state or if trying to lock/unlock the p-level
	if ( bLocked == IsLocked() || IsPersistent() )
	{
		return;
	}

	// If locking the level, deselect all of its actors and BSP surfaces
	if ( bLocked )
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		SelectedActors->Modify();

		// Deselect all level actors 
		for ( TArray<AActor*>::TConstIterator LevelActorIterator( Level->Actors ); LevelActorIterator; ++LevelActorIterator )
		{
			AActor* CurActor = *LevelActorIterator;
			if ( CurActor )
			{
				SelectedActors->Deselect( CurActor );
			}
		}

		// Deselect all level BSP surfaces
		EditorLevelUtils::DeselectAllSurfacesInLevel(Level.Get());

		// Tell the editor selection status was changed.
		GEditor->NoteSelectionChange();

		// If locking the current level, reset the p-level as the current level
		//@todo: fix this!
	}

	// Change the level's locked status
	FLevelUtils::ToggleLevelLock( Level.Get() );
}

void FLevelViewModel::SetStreamingClass( UClass *LevelStreamingClass )
{
	if ( IsPersistent() || !IsLevel() )
	{
		// Invalid operations for the persistent level
		return;
	}

	ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( Level.Get() );
	if ( StreamingLevel )
	{
		EditorLevelUtils::SetStreamingClassForLevel( StreamingLevel, LevelStreamingClass );
	}
}

void FLevelViewModel::MakeLevelCurrent()
{
	// If something is selected and not already current . . .
	if ( Level.IsValid() && !Level.Get()->IsCurrentLevel() )
	{
		EditorLevelUtils::MakeLevelCurrent(Level.Get());
	}

	Refresh();
}

void FLevelViewModel::Save()
{
	if ( !IsLevel() )
	{
		return;
	}

	if ( !IsVisible() )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "UnableToSaveInvisibleLevels", "Save aborted.  Levels must be made visible before they can be saved.") );
		return;
	}
	else if ( IsLocked() )
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "UnableToSaveLockedLevels", "Save aborted.  Level must be unlocked before it can be saved.") );
		return;
	}

	// Prompt the user to check out the level from source control before saving if it's currently under source control
	if ( FEditorFileUtils::PromptToCheckoutLevels( false, Level.Get() ) )
	{
		FEditorFileUtils::SaveLevel( Level.Get() );
	}
}

bool FLevelViewModel::HasKismet() const
{
	if( !Level.IsValid() )
	{
		return false;
	}
	
	return true;
}

void FLevelViewModel::OpenKismet()
{
	if( !Level.IsValid() )
	{
		return;
	}

	ULevelScriptBlueprint* LevelScriptBlueprint = Level.Get()->GetLevelScriptBlueprint();

	if( LevelScriptBlueprint )
	{
		FAssetEditorManager::Get().OpenEditorForAsset(LevelScriptBlueprint);
	}
	else
	{
		FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "UnableToCreateLevelScript", "Unable to find or create a level blueprint for this level.") );
	}
}

FSlateColor FLevelViewModel::GetColor() const
{
	if( Level.IsValid() && !Level->IsPersistentLevel() )
	{
		ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( Level.Get() );
		if ( StreamingLevel )
		{
			return StreamingLevel->LevelColor;
		}
	}

	return FLinearColor::White;
}

void FLevelViewModel::OnColorPickerCancelled(FLinearColor Color)
{
	bColorPickerOK = false;
}

void FLevelViewModel::ChangeColor(const TSharedRef<SWidget>& InPickerParentWidget)
{
	if( !Level.IsValid() )
	{
		return;
	}

	if ( !Level->IsPersistentLevel())
	{
		// Initialize the color data for the picker window.
		ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel( Level.Get() );
		check( StreamingLevel );

		FLinearColor NewColor = StreamingLevel->LevelColor;
		TArray<FLinearColor*> ColorArray;
		ColorArray.Add(&NewColor);

		FColorPickerArgs PickerArgs;
		PickerArgs.bIsModal = true;
		PickerArgs.DisplayGamma = TAttribute<float>::Create( TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma) );
		PickerArgs.LinearColorArray = &ColorArray;
		PickerArgs.OnColorPickerCancelled = FOnColorPickerCancelled::CreateSP(this, &FLevelViewModel::OnColorPickerCancelled);
		PickerArgs.ParentWidget = InPickerParentWidget;

		// ensure this is true, will be set to false in OnColorPickerCancelled if necessary
		bColorPickerOK = true;
		if (OpenColorPicker(PickerArgs))
		{
			if ( bColorPickerOK )
			{
				StreamingLevel->LevelColor = NewColor;
				StreamingLevel->Modify();

				// Update the loaded level's components so the change in color will apply immediately
				ULevel* LoadedLevel = StreamingLevel->GetLoadedLevel();
				if ( LoadedLevel )
				{
					LoadedLevel->UpdateLevelComponents(false);
				}

				ULevel::LevelDirtiedEvent.Broadcast();
			}
			FEditorSupportDelegates::RedrawAllViewports.Broadcast();
		}
	}
}

//@todo: add actor drag+drop support
bool FLevelViewModel::CanAssignActors( const TArray< TWeakObjectPtr<AActor> > Actors, FText& OutMessage ) const
{
	return false;
}
bool FLevelViewModel::CanAssignActor(const TWeakObjectPtr<AActor> Actor, FText& OutMessage) const
{
	return true;
}
void FLevelViewModel::AppendActors( TArray< TWeakObjectPtr< AActor > >& InActors ) const
{

}
void FLevelViewModel::AppendActorsOfSpecificType( TArray< TWeakObjectPtr< AActor > >& InActors, const TWeakObjectPtr< UClass >& Class )
{

}
void FLevelViewModel::AddActor( const TWeakObjectPtr< AActor >& Actor )
{

}
void FLevelViewModel::AddActors( const TArray< TWeakObjectPtr< AActor > >& Actors )
{

}
void FLevelViewModel::RemoveActors( const TArray< TWeakObjectPtr< AActor > >& Actors )
{

}
void FLevelViewModel::RemoveActor( const TWeakObjectPtr< AActor >& Actor )
{

}


void FLevelViewModel::SelectActors( bool bSelect, bool bNotify, bool bSelectEvenIfHidden, const TSharedPtr< IFilter< const TWeakObjectPtr< AActor >& > >& Filter )
{
	if( !Level.IsValid() || IsLocked() )
	{
		return;
	}

	Editor->GetSelectedActors()->BeginBatchSelectOperation();
	bool bChangesOccurred = false;

	// Iterate over all actors, looking for actors in this level.
	ULevel* RawLevel = Level.Get();
	for ( int32 ActorIndex = 2 ; ActorIndex < RawLevel->Actors.Num() ; ++ActorIndex )
	{
		AActor* Actor = RawLevel->Actors[ ActorIndex ];
		if ( Actor )
		{
			if( Filter.IsValid() && !Filter->PassesFilter( Actor ) )
			{
				continue;
			}

			bool bNotifyForActor = false;
			Editor->GetSelectedActors()->Modify();
			Editor->SelectActor( Actor, bSelect, bNotifyForActor, bSelectEvenIfHidden );
			bChangesOccurred = true;
		}
	}

	Editor->GetSelectedActors()->EndBatchSelectOperation();

	if( bNotify )
	{
		Editor->NoteSelectionChange();
	}
}

void FLevelViewModel::SelectActorsOfSpecificType( const TWeakObjectPtr< UClass >& Class )
{

}

void FLevelViewModel::SetDataSource( const TWeakObjectPtr< ULevel >& InLevel )
{
	if( Level == InLevel )
	{
		return;
	}

	Level = InLevel;
	Refresh();
}


const TWeakObjectPtr< ULevel > FLevelViewModel::GetLevel()
{
	return Level;
}

const TWeakObjectPtr< ULevelStreaming > FLevelViewModel::GetLevelStreaming()
{
	return LevelStreaming;
}

void FLevelViewModel::OnLevelsChanged( const ELevelsAction::Type Action, const ULevel* ChangedLevel, const FName& ChangedProperty )
{
	ChangedEvent.Broadcast();
}

void FLevelViewModel::Refresh()
{
	OnLevelsChanged( ELevelsAction::Reset, NULL, NAME_None );
}

 void FLevelViewModel::UpdateLevelActorsCount()
 {
	LevelActorsCount = 0;
	ULevel* CurLevel = Level.Get();
	
	if (CurLevel)
	{
		// Count the actors contained in these levels
		// NOTE: We subtract two here to omit "default actors" in the count (default brush, and WorldSettings)
		LevelActorsCount = CurLevel->Actors.Num()-2;

		// Count deleted actors
		int32 NumDeletedActors = 0;
		for (int32 ActorIdx = 0; ActorIdx < CurLevel->Actors.Num(); ++ActorIdx)
		{
			if (!CurLevel->Actors[ActorIdx])
			{
				++NumDeletedActors;
			}
		}
		
		// Subtract deleted actors from the actor count
		LevelActorsCount -= NumDeletedActors;
	}
 }



#undef LOCTEXT_NAMESPACE
