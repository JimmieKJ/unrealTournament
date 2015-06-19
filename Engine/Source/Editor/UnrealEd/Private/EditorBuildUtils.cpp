// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EditorBuildUtils.cpp: Utilities for building in the editor
=============================================================================*/

#include "UnrealEd.h"
#include "EditorBuildUtils.h"
#include "ISourceControlModule.h"
#include "LevelUtils.h"
#include "EditorLevelUtils.h"
#include "BusyCursor.h"
#include "Database.h"
#include "Dialogs/SBuildProgress.h"
#include "LightingBuildOptions.h"
#include "Dialogs/Dialogs.h"
#include "MainFrame.h"
#include "AssetToolsModule.h"
#include "MessageLog.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/WorldSettings.h"
#include "AI/Navigation/NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogEditorBuildUtils, Log, All);

#define LOCTEXT_NAMESPACE "EditorBuildUtils"

extern UNREALED_API bool GLightmassDebugMode; 
extern UNREALED_API bool GLightmassStatsMode;
extern FSwarmDebugOptions GSwarmDebugOptions;

bool FEditorBuildUtils::bBuildingNavigationFromUserRequest = false;
/** Constructor */
FEditorBuildUtils::FEditorAutomatedBuildSettings::FEditorAutomatedBuildSettings()
:	BuildErrorBehavior( ABB_PromptOnError ),
	UnableToCheckoutFilesBehavior( ABB_PromptOnError ),
	NewMapBehavior( ABB_PromptOnError ),
	FailedToSaveBehavior( ABB_PromptOnError ),
	bAutoAddNewFiles( true ),
	bShutdownEditorOnCompletion( false )
{}

/**
 * Start an automated build of all current maps in the editor. Upon successful conclusion of the build, the newly
 * built maps will be submitted to source control.
 *
 * @param	BuildSettings		Build settings used to dictate the behavior of the automated build
 * @param	OutErrorMessages	Error messages accumulated during the build process, if any
 *
 * @return	true if the build/submission process executed successfully; false if it did not
 */

bool FEditorBuildUtils::EditorAutomatedBuildAndSubmit( const FEditorAutomatedBuildSettings& BuildSettings, FText& OutErrorMessages )
{
	// Assume the build is successful to start
	bool bBuildSuccessful = true;
	
	// Keep a set of packages that should be submitted to source control at the end of a successful build. The build preparation and processing
	// will add and remove from the set depending on build settings, errors, etc.
	TSet<UPackage*> PackagesToSubmit;

	// Perform required preparations for the automated build process
	bBuildSuccessful = PrepForAutomatedBuild( BuildSettings, PackagesToSubmit, OutErrorMessages );

	// If the preparation went smoothly, attempt the actual map building process
	if ( bBuildSuccessful )
	{
		bBuildSuccessful = EditorBuild( GWorld, EBuildOptions::BuildAllSubmit );

		// If the map build failed, log the error
		if ( !bBuildSuccessful )
		{
			LogErrorMessage( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_BuildFailed", "The map build failed or was canceled."), OutErrorMessages );
		}
	}

	// If any map errors resulted from the build, process them according to the behavior specified in the build settings
	if ( bBuildSuccessful && FMessageLog("MapCheck").NumMessages( EMessageSeverity::Warning ) > 0 )
	{
		bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.BuildErrorBehavior, NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_MapErrors", "Map errors occurred while building.\n\nAttempt to continue the build?"), OutErrorMessages );
	}

	// If it's still safe to proceed, attempt to save all of the level packages that have been marked for submission
	if ( bBuildSuccessful )
	{
		UPackage* CurOutermostPkg = GWorld->PersistentLevel->GetOutermost();
		FString PackagesThatFailedToSave;

		// Try to save the p-level if it should be submitted
		if ( PackagesToSubmit.Contains( CurOutermostPkg ) && !FEditorFileUtils::SaveLevel( GWorld->PersistentLevel ) )
		{
			// If the p-level failed to save, remove it from the set of packages to submit
			PackagesThatFailedToSave += FString::Printf( TEXT("%s\n"), *CurOutermostPkg->GetName() );
			PackagesToSubmit.Remove( CurOutermostPkg );
		}
		
		// Try to save each streaming level (if they should be submitted)
		for ( TArray<ULevelStreaming*>::TIterator LevelIter( GWorld->StreamingLevels ); LevelIter; ++LevelIter )
		{
			ULevelStreaming* CurStreamingLevel = *LevelIter;
			if ( CurStreamingLevel != NULL )
			{
				ULevel* Level = CurStreamingLevel->GetLoadedLevel();
				if ( Level != NULL )
				{
					CurOutermostPkg = Level->GetOutermost();
					if ( PackagesToSubmit.Contains( CurOutermostPkg ) && !FEditorFileUtils::SaveLevel( Level ) )
					{
						// If a save failed, remove the streaming level from the set of packages to submit
						PackagesThatFailedToSave += FString::Printf( TEXT("%s\n"), *CurOutermostPkg->GetName() );
						PackagesToSubmit.Remove( CurOutermostPkg );
					}
				}
			}
		}

		// If any packages failed to save, process the behavior specified by the build settings to see how the process should proceed
		if ( PackagesThatFailedToSave.Len() > 0 )
		{
			bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.FailedToSaveBehavior,
				FText::Format( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_FilesFailedSave", "The following assets failed to save and cannot be submitted:\n\n{0}\n\nAttempt to continue the build?"), FText::FromString(PackagesThatFailedToSave) ),
				OutErrorMessages );
		}
	}

	// If still safe to proceed, make sure there are actually packages remaining to submit
	if ( bBuildSuccessful )
	{
		bBuildSuccessful = PackagesToSubmit.Num() > 0;
		if ( !bBuildSuccessful )
		{
			LogErrorMessage( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_NoValidLevels", "None of the current levels are valid for submission; automated build aborted."), OutErrorMessages );
		}
	}

	// Finally, if everything has gone smoothly, submit the requested packages to source control
	if ( bBuildSuccessful )
	{
		SubmitPackagesForAutomatedBuild( PackagesToSubmit, BuildSettings );
	}

	// Check if the user requested the editor shutdown at the conclusion of the automated build
	if ( BuildSettings.bShutdownEditorOnCompletion )
	{
		FPlatformMisc::RequestExit( false );
	}

	return bBuildSuccessful;
}

static bool IsBuildCancelled()
{
	return GEditor->GetMapBuildCancelled();
}

/**
 * Perform an editor build with behavior dependent upon the specified id
 *
 * @param	Id	Action Id specifying what kind of build is requested
 *
 * @return	true if the build completed successfully; false if it did not (or was manually canceled)
 */
bool FEditorBuildUtils::EditorBuild( UWorld* InWorld, EBuildOptions::Type Id, const bool bAllowLightingDialog )
{
	FMessageLog("MapCheck").NewPage(LOCTEXT("MapCheckNewPage", "Map Check"));

	// Make sure to set this flag to false before ALL builds.
	GEditor->SetMapBuildCancelled( false );

	// Will be set to false if, for some reason, the build does not happen.
	bool bDoBuild = true;
	// Indicates whether the persistent level should be dirtied at the end of a build.
	bool bDirtyPersistentLevel = true;

	// Stop rendering thread so we're not wasting CPU cycles.
	StopRenderingThread();

	// Hack: These don't initialize properly and if you pick BuildAll right off the
	// bat when opening a map you will get incorrect values in them.
	GSwarmDebugOptions.Touch();

	// Show option dialog first, before showing the DlgBuildProgress window.
	FLightingBuildOptions LightingBuildOptions;
	if ( Id == EBuildOptions::BuildLighting )
	{
		// Retrieve settings from ini.
		GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelected"),		LightingBuildOptions.bOnlyBuildSelected,			GEditorPerProjectIni );
		GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildCurrentLevel"),	LightingBuildOptions.bOnlyBuildCurrentLevel,		GEditorPerProjectIni );
		GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildSelectedLevels"),LightingBuildOptions.bOnlyBuildSelectedLevels,	GEditorPerProjectIni );
		GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("OnlyBuildVisibility"),	LightingBuildOptions.bOnlyBuildVisibility,		GEditorPerProjectIni );
		GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("UseErrorColoring"),		LightingBuildOptions.bUseErrorColoring,			GEditorPerProjectIni );
		GConfig->GetBool( TEXT("LightingBuildOptions"), TEXT("ShowLightingBuildInfo"),	LightingBuildOptions.bShowLightingBuildInfo,		GEditorPerProjectIni );
		int32 QualityLevel;
		GConfig->GetInt(  TEXT("LightingBuildOptions"), TEXT("QualityLevel"),			QualityLevel,						GEditorPerProjectIni );
		QualityLevel = FMath::Clamp<int32>(QualityLevel, Quality_Preview, Quality_Production);
		LightingBuildOptions.QualityLevel = (ELightingBuildQuality)QualityLevel;
	}

	// Show the build progress dialog.
	SBuildProgressWidget::EBuildType BuildType = SBuildProgressWidget::BUILDTYPE_Geometry;

	switch (Id)
	{
	case EBuildOptions::BuildGeometry:
	case EBuildOptions::BuildVisibleGeometry:
	case EBuildOptions::BuildAll:
	case EBuildOptions::BuildAllOnlySelectedPaths:
		BuildType = SBuildProgressWidget::BUILDTYPE_Geometry;
		break;
	case EBuildOptions::BuildLighting:
		BuildType = SBuildProgressWidget::BUILDTYPE_Lighting;
		break;
	case EBuildOptions::BuildAIPaths:
	case EBuildOptions::BuildSelectedAIPaths:
		BuildType = SBuildProgressWidget::BUILDTYPE_Paths;
		break;
	case EBuildOptions::BuildHierarchicalLOD:
		BuildType = SBuildProgressWidget::BUILDTYPE_LODs;
		break;
	default:
		BuildType = SBuildProgressWidget::BUILDTYPE_Unknown;	
		break;
	}

	TWeakPtr<class SBuildProgressWidget> BuildProgressWidget = GWarn->ShowBuildProgressWindow();
	BuildProgressWidget.Pin()->SetBuildType(BuildType);

	bool bShouldMapCheck = true;
	switch( Id )
	{
	case EBuildOptions::BuildGeometry:
		{
			// We can't set the busy cursor for all windows, because lighting
			// needs a cursor for the lighting options dialog.
			const FScopedBusyCursor BusyCursor;

			GUnrealEd->Exec( InWorld, TEXT("MAP REBUILD") );

			if (GetDefault<ULevelEditorMiscSettings>()->bNavigationAutoUpdate)
			{
				TriggerNavigationBuilder(InWorld, Id);
			}

			// No need to dirty the persient level if we're building BSP for a sub-level.
			bDirtyPersistentLevel = false;
			break;
		}
	case EBuildOptions::BuildVisibleGeometry:
		{
			// If any levels are hidden, prompt the user about how to proceed
			bDoBuild = GEditor->WarnAboutHiddenLevels( InWorld, true );
			if ( bDoBuild )
			{
				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				GUnrealEd->Exec( InWorld, TEXT("MAP REBUILD ALLVISIBLE") );

				if (GetDefault<ULevelEditorMiscSettings>()->bNavigationAutoUpdate)
				{
					TriggerNavigationBuilder(InWorld, Id);
				}
			}
			break;
		}

	case EBuildOptions::BuildLighting:
		{
			if( bDoBuild )
			{
				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				// BSP export to lightmass relies on current BSP state
				GUnrealEd->Exec( InWorld, TEXT("MAP REBUILD ALLVISIBLE") );

				GUnrealEd->BuildLighting( LightingBuildOptions );
				bShouldMapCheck = false;
			}
			break;
		}

	case EBuildOptions::BuildAIPaths:
		{
			bDoBuild = GEditor->WarnAboutHiddenLevels( InWorld, false );
			if ( bDoBuild )
			{
				GEditor->ResetTransaction( NSLOCTEXT("UnrealEd", "RebuildNavigation", "Rebuilding Navigation") );

				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				TriggerNavigationBuilder(InWorld, Id);
			}

			break;
		}

	case EBuildOptions::BuildHierarchicalLOD:
		{
			bDoBuild = GEditor->WarnAboutHiddenLevels( InWorld, false );
			if ( bDoBuild )
			{
				GEditor->ResetTransaction( NSLOCTEXT("UnrealEd", "RebuildLOD", "Rebuilding HierarchicalLOD") );

				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				TriggerHierarchicalLODBuilder(InWorld, Id);
			}

			break;
		}

	case EBuildOptions::BuildAll:
	case EBuildOptions::BuildAllSubmit:
		{
			bDoBuild = GEditor->WarnAboutHiddenLevels( InWorld, true );
			bool bLightingAlreadyRunning = GUnrealEd->WarnIfLightingBuildIsCurrentlyRunning();
			if ( bDoBuild && !bLightingAlreadyRunning )
			{
				// We can't set the busy cursor for all windows, because lighting
				// needs a cursor for the lighting options dialog.
				const FScopedBusyCursor BusyCursor;

				GUnrealEd->Exec( InWorld, TEXT("MAP REBUILD ALLVISIBLE") );

 				{
 					BuildProgressWidget.Pin()->SetBuildType(SBuildProgressWidget::BUILDTYPE_LODs);
					TriggerHierarchicalLODBuilder(InWorld, Id);
 				}

				{
					BuildProgressWidget.Pin()->SetBuildType(SBuildProgressWidget::BUILDTYPE_Paths);
					TriggerNavigationBuilder(InWorld, Id);
				}

				//Do a canceled check before moving on to the next step of the build.
				if( GEditor->GetMapBuildCancelled() )
				{
					break;
				}
				else
				{
					BuildProgressWidget.Pin()->SetBuildType(SBuildProgressWidget::BUILDTYPE_Lighting);

					FLightingBuildOptions LightingOptions;

					int32 QualityLevel;

					// Force automated builds to always use production lighting
					if ( Id == EBuildOptions::BuildAllSubmit )
					{
						QualityLevel = Quality_Production;
					}
					else
					{
						GConfig->GetInt( TEXT("LightingBuildOptions"), TEXT("QualityLevel"), QualityLevel, GEditorPerProjectIni);
						QualityLevel = FMath::Clamp<int32>(QualityLevel, Quality_Preview, Quality_Production);
					}
					LightingOptions.QualityLevel = (ELightingBuildQuality)QualityLevel;

					GUnrealEd->BuildLighting(LightingOptions);
					bShouldMapCheck = false;
				}
			}
			break;
		}

	default:
		UE_LOG(LogEditorBuildUtils, Warning, TEXT("Invalid build Id"));
		break;
	}

	// Check map for errors (only if build operation happened)
	if ( bShouldMapCheck && bDoBuild && !GEditor->GetMapBuildCancelled() )
	{
		GUnrealEd->Exec( InWorld, TEXT("MAP CHECK DONTDISPLAYDIALOG") );
	}

	// Re-start the rendering thread after build operations completed.
	if (GUseThreadedRendering)
	{
		StartRenderingThread();
	}

	if ( bDoBuild && InWorld->Scene )
	{
		// Invalidating lighting marked various components as needing a re-register
		// Propagate the re-registers before rendering the scene to get the latest state
		InWorld->SendAllEndOfFrameUpdates();
	}

	if ( bDoBuild )
	{
		// Display elapsed build time.
		UE_LOG(LogEditorBuildUtils, Log,  TEXT("Build time %s"), *BuildProgressWidget.Pin()->BuildElapsedTimeText().ToString() );
	}

	// Build completed, hide the build progress dialog.
	// NOTE: It's important to turn off modalness before hiding the window, otherwise a background
	//		 application may unexpectedly be promoted to the foreground, obscuring the editor.
	GWarn->CloseBuildProgressWindow();
	
	GUnrealEd->RedrawLevelEditingViewports();

	if ( bDoBuild )
	{
		if ( bDirtyPersistentLevel )
		{
			InWorld->MarkPackageDirty();
		}
		ULevel::LevelDirtiedEvent.Broadcast();
	}

	// Don't show map check if we cancelled build because it may have some bogus data
	const bool bBuildCompleted = bDoBuild && !GEditor->GetMapBuildCancelled();
	if( bBuildCompleted )
	{
		if (bShouldMapCheck)
		{
			FMessageLog("MapCheck").Open( EMessageSeverity::Warning );
		}
		FMessageLog("LightingResults").Notify(LOCTEXT("LightingErrorsNotification", "There were lighting errors."), EMessageSeverity::Error);
	}

	return bBuildCompleted;
}

/**
 * Private helper method to log an error both to GWarn and to the build's list of accumulated errors
 *
 * @param	InErrorMessage			Message to log to GWarn/add to list of errors
 * @param	OutAccumulatedErrors	List of errors accumulated during a build process so far
 */
void FEditorBuildUtils::LogErrorMessage( const FText& InErrorMessage, FText& OutAccumulatedErrors )
{
	OutAccumulatedErrors = FText::Format( LOCTEXT("AccumulateErrors", "{0}\n{1}"), OutAccumulatedErrors, InErrorMessage );
	UE_LOG(LogEditorBuildUtils, Warning, TEXT("%s"), *InErrorMessage.ToString() );
}

/**
 * Helper method to handle automated build behavior in the event of an error. Depending on the specified behavior, one of three
 * results are possible:
 *	a) User is prompted on whether to proceed with the automated build or not,
 *	b) The error is regarded as a build-stopper and the method returns failure,
 *	or
 *	c) The error is acknowledged but not regarded as a build-stopper, and the method returns success.
 * In any event, the error is logged for the user's information.
 *
 * @param	InBehavior				Behavior to use to respond to the error
 * @param	InErrorMsg				Error to log
 * @param	OutAccumulatedErrors	List of errors accumulated from the build process so far; InErrorMsg will be added to the list
 *
 * @return	true if the build should proceed after processing the error behavior; false if it should not
 */
bool FEditorBuildUtils::ProcessAutomatedBuildBehavior( EAutomatedBuildBehavior InBehavior, const FText& InErrorMsg, FText& OutAccumulatedErrors )
{
	// Assume the behavior should result in the build being successful/proceeding to start
	bool bSuccessful = true;

	switch ( InBehavior )
	{
		// In the event the user should be prompted for the error, display a modal dialog describing the error and ask the user
		// if the build should proceed or not
	case ABB_PromptOnError:
		{
			bSuccessful = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, InErrorMsg);
		}
		break;

		// In the event that the specified error should abort the build, mark the processing as a failure
	case ABB_FailOnError:
		bSuccessful = false;
		break;
	}

	// Log the error message so the user is aware of it
	LogErrorMessage( InErrorMsg, OutAccumulatedErrors );

	// If the processing resulted in the build inevitably being aborted, write to the log about the abortion
	if ( !bSuccessful )
	{
		LogErrorMessage( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_AutomatedBuildAborted", "Automated build aborted."), OutAccumulatedErrors );
	}

	return bSuccessful;
}



/**
 * Helper method designed to perform the necessary preparations required to complete an automated editor build
 *
 * @param	BuildSettings		Build settings that will be used for the editor build
 * @param	OutPkgsToSubmit		Set of packages that need to be saved and submitted after a successful build
 * @param	OutErrorMessages	Errors that resulted from the preparation (may or may not force the build to stop, depending on build settings)
 *
 * @return	true if the preparation was successful and the build should continue; false if the preparation failed and the build should be aborted
 */
bool FEditorBuildUtils::PrepForAutomatedBuild( const FEditorAutomatedBuildSettings& BuildSettings, TSet<UPackage*>& OutPkgsToSubmit, FText& OutErrorMessages )
{
	// Assume the preparation is successful to start
	bool bBuildSuccessful = true;

	OutPkgsToSubmit.Empty();

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Source control is required for the automated build, so ensure that SCC support is compiled in and
	// that the server is enabled and available for use
	if ( !ISourceControlModule::Get().IsEnabled() || !SourceControlProvider.IsAvailable() )
	{
		bBuildSuccessful = false;
		LogErrorMessage( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_SCCError", "Cannot connect to source control; automated build aborted."), OutErrorMessages );
	}

	// Empty changelists aren't allowed; abort the build if one wasn't provided
	if ( bBuildSuccessful && BuildSettings.ChangeDescription.Len() == 0 )
	{
		bBuildSuccessful = false;
		LogErrorMessage( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_NoCLDesc", "A changelist description must be provided; automated build aborted."), OutErrorMessages );
	}

	TArray<UPackage*> PreviouslySavedWorldPackages;
	TArray<UPackage*> PackagesToCheckout;
	TArray<ULevel*> LevelsToSave;

	if ( bBuildSuccessful )
	{
		TArray<UWorld*> AllWorlds;
		FString UnsavedWorlds;
		EditorLevelUtils::GetWorlds( GWorld, AllWorlds, true );

		// Check all of the worlds that will be built to ensure they have been saved before and have a filename
		// associated with them. If they don't, they won't be able to be submitted to source control.
		FString CurWorldPkgFileName;
		for ( TArray<UWorld*>::TConstIterator WorldIter( AllWorlds ); WorldIter; ++WorldIter )
		{
			const UWorld* CurWorld = *WorldIter;
			check( CurWorld );

			UPackage* CurWorldPackage = CurWorld->GetOutermost();
			check( CurWorldPackage );

			if ( FPackageName::DoesPackageExist( CurWorldPackage->GetName(), NULL, &CurWorldPkgFileName ) )
			{
				PreviouslySavedWorldPackages.AddUnique( CurWorldPackage );

				// Add all packages which have a corresponding file to the set of packages to submit for now. As preparation continues
				// any packages that can't be submitted due to some error will be removed.
				OutPkgsToSubmit.Add( CurWorldPackage );
			}
			else
			{
				UnsavedWorlds += FString::Printf( TEXT("%s\n"), *CurWorldPackage->GetName() );
			}
		}

		// If any of the worlds haven't been saved before, process the build setting's behavior to see if the build
		// should proceed or not
		if ( UnsavedWorlds.Len() > 0 )
		{
			bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.NewMapBehavior, 
				FText::Format( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_UnsavedMap", "The following levels have never been saved before and cannot be submitted:\n\n{0}\n\nAttempt to continue the build?"), FText::FromString(UnsavedWorlds) ),
				OutErrorMessages );
		}
	}

	// Load the asset tools module
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	if ( bBuildSuccessful )
	{
		// Update the source control status of any relevant world packages in order to determine which need to be
		// checked out, added to the depot, etc.
		SourceControlProvider.Execute( ISourceControlOperation::Create<FUpdateStatus>(), SourceControlHelpers::PackageFilenames(PreviouslySavedWorldPackages) );

		FString PkgsThatCantBeCheckedOut;
		for ( TArray<UPackage*>::TConstIterator PkgIter( PreviouslySavedWorldPackages ); PkgIter; ++PkgIter )
		{
			UPackage* CurPackage = *PkgIter;
			const FString CurPkgName = CurPackage->GetName();
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(CurPackage, EStateCacheUsage::ForceUpdate);

			if( !SourceControlState.IsValid() ||
				(!SourceControlState->IsSourceControlled() &&
				 !SourceControlState->IsUnknown() &&
				 !SourceControlState->IsIgnored()))
			{
				FString CurFilename;
				if ( FPackageName::DoesPackageExist( CurPkgName, NULL, &CurFilename ) )
				{
					if ( IFileManager::Get().IsReadOnly( *CurFilename ) )
					{
						PkgsThatCantBeCheckedOut += FString::Printf( TEXT("%s\n"), *CurPkgName );
						OutPkgsToSubmit.Remove( CurPackage );
					}
				}
			}
			else if(SourceControlState->CanCheckout())
			{
				PackagesToCheckout.Add( CurPackage );
			}
			else
			{
				PkgsThatCantBeCheckedOut += FString::Printf( TEXT("%s\n"), *CurPkgName );
				OutPkgsToSubmit.Remove( CurPackage );
			}
		}

		// If any of the packages can't be checked out or are read-only, process the build setting's behavior to see if the build
		// should proceed or not
		if ( PkgsThatCantBeCheckedOut.Len() > 0 )
		{
			bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.UnableToCheckoutFilesBehavior,
				FText::Format( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_UnsaveableFiles", "The following assets cannot be checked out of source control (or are read-only) and cannot be submitted:\n\n{0}\n\nAttempt to continue the build?"), FText::FromString(PkgsThatCantBeCheckedOut) ),
				OutErrorMessages );
		}
	}

	if ( bBuildSuccessful )
	{
		// Check out all of the packages from source control that need to be checked out
		if ( PackagesToCheckout.Num() > 0 )
		{
			TArray<FString> PackageFilenames = SourceControlHelpers::PackageFilenames(PackagesToCheckout);
			SourceControlProvider.Execute( ISourceControlOperation::Create<FCheckOut>(), PackageFilenames );

			// Update the package status of the packages that were just checked out to confirm that they
			// were actually checked out correctly
			SourceControlProvider.Execute(  ISourceControlOperation::Create<FUpdateStatus>(), PackageFilenames );

			FString FilesThatFailedCheckout;
			for ( TArray<UPackage*>::TConstIterator CheckedOutIter( PackagesToCheckout ); CheckedOutIter; ++CheckedOutIter )
			{
				UPackage* CurPkg = *CheckedOutIter;
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(CurPkg, EStateCacheUsage::ForceUpdate);

				// If any of the packages failed to check out, remove them from the set of packages to submit
				if ( !SourceControlState.IsValid() || (!SourceControlState->IsCheckedOut() && !SourceControlState->IsAdded() && SourceControlState->IsSourceControlled()) )
				{
					FilesThatFailedCheckout += FString::Printf( TEXT("%s\n"), *CurPkg->GetName() );
					OutPkgsToSubmit.Remove( CurPkg );
				}
			}

			// If any of the packages failed to check out correctly, process the build setting's behavior to see if the build
			// should proceed or not
			if ( FilesThatFailedCheckout.Len() > 0 )
			{
				bBuildSuccessful = ProcessAutomatedBuildBehavior( BuildSettings.UnableToCheckoutFilesBehavior,
					FText::Format( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_FilesFailedCheckout", "The following assets failed to checkout of source control and cannot be submitted:\n{0}\n\nAttempt to continue the build?"), FText::FromString(FilesThatFailedCheckout)),
					OutErrorMessages );
			}
		}
	}

	// Verify there are still actually any packages left to submit. If there aren't, abort the build and warn the user of the situation.
	if ( bBuildSuccessful )
	{
		bBuildSuccessful = OutPkgsToSubmit.Num() > 0;
		if ( !bBuildSuccessful )
		{
			LogErrorMessage( NSLOCTEXT("UnrealEd", "AutomatedBuild_Error_NoValidLevels", "None of the current levels are valid for submission; automated build aborted."), OutErrorMessages );
		}
	}

	// If the build is safe to commence, force all of the levels visible to make sure the build operates correctly
	if ( bBuildSuccessful )
	{
		bool bVisibilityToggled = false;
		if ( !FLevelUtils::IsLevelVisible( GWorld->PersistentLevel ) )
		{
			EditorLevelUtils::SetLevelVisibility( GWorld->PersistentLevel, true, false );
			bVisibilityToggled = true;
		}
		for ( TArray<ULevelStreaming*>::TConstIterator LevelIter( GWorld->StreamingLevels ); LevelIter; ++LevelIter )
		{
			ULevelStreaming* CurStreamingLevel = *LevelIter;
			if ( CurStreamingLevel && !FLevelUtils::IsLevelVisible( CurStreamingLevel ) )
			{
				CurStreamingLevel->bShouldBeVisibleInEditor = true;
				bVisibilityToggled = true;
			}
		}
		if ( bVisibilityToggled )
		{
			GWorld->FlushLevelStreaming();
		}
	}

	return bBuildSuccessful;
}


/**
 * Helper method to submit packages to source control as part of the automated build process
 *
 * @param	InPkgsToSubmit	Set of packages which should be submitted to source control
 * @param	BuildSettings	Build settings used during the automated build
 */
void FEditorBuildUtils::SubmitPackagesForAutomatedBuild( const TSet<UPackage*>& InPkgsToSubmit, const FEditorAutomatedBuildSettings& BuildSettings )
{
	TArray<FString> LevelsToAdd;
	TArray<FString> LevelsToSubmit;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// first update the status of the packages
	SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), SourceControlHelpers::PackageFilenames(InPkgsToSubmit.Array()));

	// Iterate over the set of packages to submit, determining if they need to be checked in or
	// added to the depot for the first time
	for ( TSet<UPackage*>::TConstIterator PkgIter( InPkgsToSubmit ); PkgIter; ++PkgIter )
	{
		const UPackage* CurPkg = *PkgIter;
		const FString PkgName = CurPkg->GetName();
		const FString PkgFileName = SourceControlHelpers::PackageFilename(CurPkg);

		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(CurPkg, EStateCacheUsage::ForceUpdate);
		if(SourceControlState.IsValid())
		{
			if ( SourceControlState->IsCheckedOut() || SourceControlState->IsAdded() )
			{
				LevelsToSubmit.Add( PkgFileName );
			}
			else if ( BuildSettings.bAutoAddNewFiles && !SourceControlState->IsSourceControlled() && !SourceControlState->IsIgnored() )
			{
				LevelsToSubmit.Add( PkgFileName );
				LevelsToAdd.Add( PkgFileName );
			}
		}
	}

	// Then, if we've also opted to check in any packages, iterate over that list as well
	if(BuildSettings.bCheckInPackages)
	{
		TArray<FString> PackageNames = BuildSettings.PackagesToCheckIn;
		for ( TArray<FString>::TConstIterator PkgIterName(PackageNames); PkgIterName; PkgIterName++ )
		{
			const FString& PkgName = *PkgIterName;
			const FString PkgFileName = SourceControlHelpers::PackageFilename(PkgName);
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(PkgFileName, EStateCacheUsage::ForceUpdate);
			if(SourceControlState.IsValid())
			{
				if ( SourceControlState->IsCheckedOut() || SourceControlState->IsAdded() )
				{
					LevelsToSubmit.Add( PkgFileName );
				}
				else if ( !SourceControlState->IsSourceControlled() && !SourceControlState->IsIgnored() )
				{
					// note we add the files we need to add to the submit list as well
					LevelsToSubmit.Add( PkgFileName );
					LevelsToAdd.Add( PkgFileName );
				}
			}
		}
	}

	// first add files that need to be added
	SourceControlProvider.Execute( ISourceControlOperation::Create<FMarkForAdd>(), LevelsToAdd, EConcurrency::Synchronous );

	// Now check in all the changes, including the files we added above
	TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation = StaticCastSharedRef<FCheckIn>(ISourceControlOperation::Create<FCheckIn>());
	CheckInOperation->SetDescription(NSLOCTEXT("UnrealEd", "AutomatedBuild_AutomaticSubmission", "[Automatic Submission]"));
	SourceControlProvider.Execute( CheckInOperation, LevelsToSubmit, EConcurrency::Synchronous );
}

void FEditorBuildUtils::TriggerNavigationBuilder(UWorld* InWorld, EBuildOptions::Type Id)
{
	if( InWorld->GetWorldSettings()->bEnableNavigationSystem &&
		InWorld->GetNavigationSystem() )
	{
		switch( Id )
		{
		case EBuildOptions::BuildAIPaths:
		case EBuildOptions::BuildSelectedAIPaths:
		case EBuildOptions::BuildAllOnlySelectedPaths:
		case EBuildOptions::BuildAll:
		case EBuildOptions::BuildAllSubmit:
			{
				bBuildingNavigationFromUserRequest = true;
				break;
			}
		default:
			{
				bBuildingNavigationFromUserRequest = false;
				break;
			}
		}

		// Invoke navmesh generator
		InWorld->GetNavigationSystem()->Build();
	}
}

void FEditorBuildUtils::TriggerHierarchicalLODBuilder(UWorld* InWorld, EBuildOptions::Type Id)
{
	// Invoke HLOD generator
	InWorld->HierarchicalLODBuilder.Build();
}
#undef LOCTEXT_NAMESPACE