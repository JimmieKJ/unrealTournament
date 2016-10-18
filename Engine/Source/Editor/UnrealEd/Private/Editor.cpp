// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "UnrealEd.h"
#include "Matinee/MatineeActor.h"
#include "InteractiveFoliageActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/WorldComposition.h"
#include "EditorSupportDelegates.h"
#include "Factories.h"
#include "BSPOps.h"
#include "EditorCommandLineUtils.h"
#include "Net/NetworkProfiler.h"
#include "UObjectGlobals.h"

// needed for the RemotePropagator
#include "SoundDefinitions.h"
#include "Database.h"
#include "SurfaceIterators.h"
#include "ScopedTransaction.h"

#include "ISourceControlModule.h"
#include "PackageBackup.h"
#include "LevelUtils.h"
#include "Layers/Layers.h"
#include "EditorLevelUtils.h"

#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"
#include "AssetSelection.h"
#include "FXSystem.h"

#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Editor/Kismet/Public/BlueprintEditorModule.h"
#include "Engine/InheritableComponentHandler.h"

#include "BlueprintUtilities.h"

#include "AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "ISourceCodeAccessModule.h"

#include "Editor/MainFrame/Public/MainFrame.h"
#include "AnimationUtils.h"
#include "AudioDecompress.h"
#include "LevelEditor.h"
#include "SCreateAssetFromObject.h"

#include "Editor/ActorPositioning.h"

#include "Developer/DirectoryWatcher/Public/DirectoryWatcherModule.h"

#include "Runtime/Engine/Public/Slate/SceneViewport.h"
#include "Editor/LevelEditor/Public/ILevelViewport.h"

#include "ComponentReregisterContext.h"
#include "EngineModule.h"
#include "RendererInterface.h"

#if PLATFORM_WINDOWS
// For WAVEFORMATEXTENSIBLE
	#include "AllowWindowsPlatformTypes.h"
#include <mmreg.h>
	#include "HideWindowsPlatformTypes.h"
#endif

#include "AudioDerivedData.h"
#include "Projects.h"
#include "TargetPlatform.h"
#include "RemoteConfigIni.h"

#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "ObjectTools.h"
#include "MessageLogModule.h"

#include "GameProjectGenerationModule.h"
#include "ActorEditorUtils.h"
#include "SnappingUtils.h"
#include "EditorViewportCommands.h"
#include "MessageLog.h"

#include "MRUFavoritesList.h"
#include "EditorStyle.h"
#include "EngineBuildSettings.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"

// AIMdule
#include "BehaviorTree/BehaviorTreeManager.h"

#include "HotReloadInterface.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Engine/GameEngine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Light.h"
#include "Engine/LevelStreamingVolume.h"
#include "Sound/SoundCue.h"
#include "Components/BrushComponent.h"
#include "Engine/LocalPlayer.h"
#include "Components/SkyLightComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Engine/Polys.h"
#include "UnrealEngine.h"
#include "EngineStats.h"
#include "Engine/SimpleConstructionScript.h"
#include "K2Node_AddComponent.h"

#include "AutoReimport/AutoReimportUtilities.h"

#include "Settings/EditorSettings.h"

#define LOCTEXT_NAMESPACE "UnrealEd.Editor"

FSimpleMulticastDelegate								FEditorDelegates::NewCurrentLevel;
FEditorDelegates::FOnMapChanged							FEditorDelegates::MapChange;
FSimpleMulticastDelegate								FEditorDelegates::LayerChange;
FEditorDelegates::FOnModeChanged						FEditorDelegates::ChangeEditorMode;
FSimpleMulticastDelegate								FEditorDelegates::SurfProps;
FSimpleMulticastDelegate								FEditorDelegates::SelectedProps;
FEditorDelegates::FOnFitTextureToSurface				FEditorDelegates::FitTextureToSurface;
FSimpleMulticastDelegate								FEditorDelegates::ActorPropertiesChange;
FSimpleMulticastDelegate								FEditorDelegates::RefreshEditor;
FSimpleMulticastDelegate								FEditorDelegates::RefreshAllBrowsers;
FSimpleMulticastDelegate								FEditorDelegates::RefreshLayerBrowser;
FSimpleMulticastDelegate								FEditorDelegates::RefreshPrimitiveStatsBrowser;
FSimpleMulticastDelegate								FEditorDelegates::LoadSelectedAssetsIfNeeded;
FSimpleMulticastDelegate								FEditorDelegates::DisplayLoadErrors;
FEditorDelegates::FOnEditorModeTransitioned				FEditorDelegates::EditorModeEnter;
FEditorDelegates::FOnEditorModeTransitioned				FEditorDelegates::EditorModeExit;
FEditorDelegates::FOnPIEEvent							FEditorDelegates::PreBeginPIE;
FEditorDelegates::FOnPIEEvent							FEditorDelegates::BeginPIE;
FEditorDelegates::FOnPIEEvent							FEditorDelegates::EndPIE;
FEditorDelegates::FOnPIEEvent							FEditorDelegates::PausePIE;
FEditorDelegates::FOnPIEEvent							FEditorDelegates::ResumePIE;
FEditorDelegates::FOnPIEEvent							FEditorDelegates::SingleStepPIE;
FEditorDelegates::FOnPIEEvent							FEditorDelegates::OnSwitchBeginPIEAndSIE;
FSimpleMulticastDelegate								FEditorDelegates::PropertySelectionChange;
FSimpleMulticastDelegate								FEditorDelegates::PostLandscapeLayerUpdated;
FEditorDelegates::FOnPreSaveWorld						FEditorDelegates::PreSaveWorld;
FEditorDelegates::FOnPostSaveWorld						FEditorDelegates::PostSaveWorld;
FEditorDelegates::FOnFinishPickingBlueprintClass		FEditorDelegates::OnFinishPickingBlueprintClass;
FEditorDelegates::FOnNewAssetCreation					FEditorDelegates::OnConfigureNewAssetProperties;
FEditorDelegates::FOnNewAssetCreation					FEditorDelegates::OnNewAssetCreated;
FEditorDelegates::FOnAssetPreImport						FEditorDelegates::OnAssetPreImport;
FEditorDelegates::FOnAssetPostImport					FEditorDelegates::OnAssetPostImport;
FEditorDelegates::FOnAssetReimport						FEditorDelegates::OnAssetReimport;
FEditorDelegates::FOnNewActorsDropped					FEditorDelegates::OnNewActorsDropped;
FEditorDelegates::FOnGridSnappingChanged				FEditorDelegates::OnGridSnappingChanged;
FSimpleMulticastDelegate								FEditorDelegates::OnLightingBuildStarted;
FSimpleMulticastDelegate								FEditorDelegates::OnLightingBuildKept;
FSimpleMulticastDelegate								FEditorDelegates::OnLightingBuildFailed;
FSimpleMulticastDelegate								FEditorDelegates::OnLightingBuildSucceeded;
FEditorDelegates::FOnApplyObjectToActor					FEditorDelegates::OnApplyObjectToActor;
FEditorDelegates::FOnFocusViewportOnActors				FEditorDelegates::OnFocusViewportOnActors;
FEditorDelegates::FOnMapOpened							FEditorDelegates::OnMapOpened;
FEditorDelegates::FOnEditorCameraMoved					FEditorDelegates::OnEditorCameraMoved;
FEditorDelegates::FOnDollyPerspectiveCamera				FEditorDelegates::OnDollyPerspectiveCamera;
FSimpleMulticastDelegate								FEditorDelegates::OnShutdownPostPackagesSaved;
FEditorDelegates::FOnAssetsPreDelete					FEditorDelegates::OnAssetsPreDelete;
FEditorDelegates::FOnAssetsDeleted						FEditorDelegates::OnAssetsDeleted;
FEditorDelegates::FOnAssetDragStarted					FEditorDelegates::OnAssetDragStarted;
FSimpleMulticastDelegate								FEditorDelegates::OnActionAxisMappingsChanged;
FEditorDelegates::FOnAddLevelToWorld					FEditorDelegates::OnAddLevelToWorld;

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

IMPLEMENT_STRUCT(SlatePlayInEditorInfo);

//////////////////////////////////////////////////////////////////////////
// FReimportManager

FReimportManager* FReimportManager::Instance()
{
	static FReimportManager Inst;
	return &Inst;
}

void FReimportManager::RegisterHandler( FReimportHandler& InHandler )
{
	Handlers.AddUnique( &InHandler );
	bHandlersNeedSorting = true;
}

void FReimportManager::UnregisterHandler( FReimportHandler& InHandler )
{
	Handlers.Remove( &InHandler );
}

bool FReimportManager::CanReimport( UObject* Obj ) const
{
	if ( Obj )
	{
		TArray<FString> SourceFilenames;
		for( int32 HandlerIndex = 0; HandlerIndex < Handlers.Num(); ++HandlerIndex )
		{
			SourceFilenames.Empty();
			if ( Handlers[ HandlerIndex ]->CanReimport(Obj, SourceFilenames) )
			{
				return true;
			}
		}
	}
	return false;
}

void FReimportManager::UpdateReimportPaths( UObject* Obj, const TArray<FString>& InFilenames )
{
	if (Obj)
	{
		TArray<FString> UnusedExistingFilenames;
		auto* Handler = Handlers.FindByPredicate([&](FReimportHandler* InHandler){ return InHandler->CanReimport(Obj, UnusedExistingFilenames); });
		if (Handler)
		{
			(*Handler)->SetReimportPaths(Obj, InFilenames);
			Obj->MarkPackageDirty();
		}
	}
}

bool FReimportManager::Reimport( UObject* Obj, bool bAskForNewFileIfMissing, bool bShowNotification, FString PreferedReimportFile, FReimportHandler* SpecifiedReimportHandler)
{
	// Warn that were about to reimport, so prep for it
	PreReimport.Broadcast( Obj );

	bool bSuccess = false;
	if ( Obj )
	{
		if (bHandlersNeedSorting)
		{
			// Use > operator because we want higher priorities earlier in the list
			Handlers.Sort([](const FReimportHandler& A, const FReimportHandler& B) { return A.GetPriority() > B.GetPriority(); });
			bHandlersNeedSorting = false;
		}
		
		bool bValidSourceFilename = false;
		TArray<FString> SourceFilenames;

		FReimportHandler *CanReimportHandler = SpecifiedReimportHandler;
		if (CanReimportHandler == nullptr || !CanReimportHandler->CanReimport(Obj, SourceFilenames))
		{
			for (int32 HandlerIndex = 0; HandlerIndex < Handlers.Num(); ++HandlerIndex)
			{
				SourceFilenames.Empty();
				if (Handlers[HandlerIndex]->CanReimport(Obj, SourceFilenames))
				{
					CanReimportHandler = Handlers[HandlerIndex];
					break;
				}
			}
		}

		if(CanReimportHandler != nullptr)
		{
			// Check all filenames for missing files
			bool bMissingFiles = false;
			if (SourceFilenames.Num() > 0)
			{
				for (int32 FileIndex = 0; FileIndex < SourceFilenames.Num(); ++FileIndex)
				{
					if (SourceFilenames[FileIndex].IsEmpty() || IFileManager::Get().FileSize(*SourceFilenames[FileIndex]) == INDEX_NONE)
					{
						bMissingFiles = true;
						break;
					}
				}
			}
			else
			{
				bMissingFiles = true;
			}

			bValidSourceFilename = true;
			if ((bAskForNewFileIfMissing || !PreferedReimportFile.IsEmpty()) && bMissingFiles )
			{
				if (!bAskForNewFileIfMissing && !PreferedReimportFile.IsEmpty())
				{
					SourceFilenames.Empty();
					SourceFilenames.Add(PreferedReimportFile);
				}
				else
				{
					GetNewReimportPath(Obj, SourceFilenames);
				}
				if ( SourceFilenames.Num() == 0 )
				{
					// Failed to specify a new filename. Don't show a notification of the failure since the user exited on his own
					bValidSourceFilename = false;
					bShowNotification = false;
				}
				else
				{
					// A new filename was supplied, update the path
					CanReimportHandler->SetReimportPaths(Obj, SourceFilenames);
				}
			}

			if ( bValidSourceFilename )
			{
				// Do the reimport
				EReimportResult::Type Result = CanReimportHandler->Reimport( Obj );
				if( Result == EReimportResult::Succeeded )
				{
					Obj->PostEditChange();
					GEditor->BroadcastObjectReimported(Obj);
					if (FEngineAnalytics::IsAvailable())
					{
						TArray<FAnalyticsEventAttribute> Attributes;
						Attributes.Add( FAnalyticsEventAttribute( TEXT( "ObjectType" ), Obj->GetClass()->GetName() ) );
						FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.AssetReimported"), Attributes);
					}
					bSuccess = true;
				}
				else if( Result == EReimportResult::Cancelled )
				{
					bShowNotification = false;
				}
			}
		}

		if ( bShowNotification )
		{
			// Send a notification of the results
			FText NotificationText;
			if ( bSuccess )
			{
				if ( bValidSourceFilename )
				{
					const FString FirstLeafFilename = FPaths::GetCleanFilename(SourceFilenames[0]);

					if ( SourceFilenames.Num() == 1 )
					{
						FFormatNamedArguments Args;
						Args.Add( TEXT("ObjectName"), FText::FromString( Obj->GetName() ) );
						Args.Add( TEXT("ObjectType"), FText::FromString( Obj->GetClass()->GetName() ) );
						Args.Add( TEXT("SourceFile"), FText::FromString( FirstLeafFilename ) );
						NotificationText = FText::Format( LOCTEXT("ReimportSuccessfulFrom", "Successfully Reimported: {ObjectName} ({ObjectType}) from file ({SourceFile})"), Args );
					}
					else
					{
						FFormatNamedArguments Args;
						Args.Add( TEXT("ObjectName"), FText::FromString( Obj->GetName() ) );
						Args.Add( TEXT("ObjectType"), FText::FromString( Obj->GetClass()->GetName() ) );
						Args.Add( TEXT("SourceFile"), FText::FromString( FirstLeafFilename ) );
						Args.Add( TEXT("Number"), SourceFilenames.Num() - 1 );
						NotificationText = FText::Format( LOCTEXT("ReimportSuccessfulMultiple", "Successfuly Reimported: {ObjectName} ({ObjectType}) from file ({SourceFile}) and {Number} more"), Args );
					}
				}
				else
				{
					FFormatNamedArguments Args;
					Args.Add( TEXT("ObjectName"), FText::FromString( Obj->GetName() ) );
					Args.Add( TEXT("ObjectType"), FText::FromString( Obj->GetClass()->GetName() ) );
					NotificationText = FText::Format( LOCTEXT("ReimportSuccessful", "Successfully Reimported: {ObjectName} ({ObjectType})"), Args );
				}
			}
			else
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("ObjectName"), FText::FromString( Obj->GetName() ) );
				Args.Add( TEXT("ObjectType"), FText::FromString( Obj->GetClass()->GetName() ) );
				NotificationText = FText::Format( LOCTEXT("ReimportFailed", "Failed to Reimport: {ObjectName} ({ObjectType})"), Args );
			}

			FNotificationInfo Info(NotificationText);
			Info.ExpireDuration = 3.0f;
			Info.bUseLargeFont = false;
			TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
			if ( Notification.IsValid() )
			{
				Notification->SetCompletionState( bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail );
			}
		}
	}

	// Let listeners know whether the reimport was successful or not
	PostReimport.Broadcast( Obj, bSuccess );

	GEditor->RedrawAllViewports();

	return bSuccess;
}

void FReimportManager::GetNewReimportPath(UObject* Obj, TArray<FString>& InOutFilenames)
{
	TArray<UObject*> ReturnObjects;
	FString FileTypes;
	FString AllExtensions;
	TArray<UFactory*> Factories;

	// Determine whether we will allow multi select and clear old filenames
	bool bAllowMultiSelect = InOutFilenames.Num() > 1;
	InOutFilenames.Empty();

	// Get the list of valid factories
	for( TObjectIterator<UClass> It ; It ; ++It )
	{
		UClass* CurrentClass = (*It);

		if( CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->HasAnyClassFlags(CLASS_Abstract)) )
		{
			UFactory* Factory = Cast<UFactory>( CurrentClass->GetDefaultObject() );
			if( Factory->bEditorImport && Obj->GetClass()->IsChildOf(Factory->GetSupportedClass()) )
			{
				Factories.Add( Factory );
			}
		}
	}

	if ( Factories.Num() <= 0 )
	{
		// No matching factories for this asset, fail
		return;
	}

	TMultiMap<uint32, UFactory*> DummyFilterIndexToFactory;

	// Generate the file types and extensions represented by the selected factories
	ObjectTools::GenerateFactoryFileExtensions( Factories, FileTypes, AllExtensions, DummyFilterIndexToFactory );

	FileTypes = FString::Printf(TEXT("All Files (%s)|%s|%s"),*AllExtensions,*AllExtensions,*FileTypes);

	FString DefaultFolder;
	FString DefaultFile;
	
	TArray<FString> ExistingPaths = Utils::ExtractSourceFilePaths(Obj);
	if (ExistingPaths.Num() > 0)
	{
		DefaultFolder = FPaths::GetPath(ExistingPaths[0]);
		DefaultFile = FPaths::GetCleanFilename(ExistingPaths[0]);
	}

	// Prompt the user for the filenames
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

		const FString Title = FString::Printf(TEXT("%s: %s"), *NSLOCTEXT("ReimportManager", "ImportDialogTitle", "Import For").ToString(), *Obj->GetName());
		bOpened = DesktopPlatform->OpenFileDialog(
			ParentWindowWindowHandle,
			Title,
			*DefaultFolder,
			*DefaultFile,
			FileTypes,
			bAllowMultiSelect ? EFileDialogFlags::Multiple : EFileDialogFlags::None,
			OpenFilenames
			);
	}

	if ( bOpened )
	{
		for (int32 FileIndex = 0; FileIndex < OpenFilenames.Num(); ++FileIndex)
		{
			InOutFilenames.Add(OpenFilenames[FileIndex]);
		}
	}
}

FReimportManager::FReimportManager()
{
	// Create reimport handler for textures
	// NOTE: New factories can be created anywhere, inside or outside of editor
	// This is just here for convenience
	UReimportTextureFactory::StaticClass();

	// Create reimport handler for FBX static meshes
	UReimportFbxStaticMeshFactory::StaticClass();

	// Create reimport handler for FBX skeletal meshes
	UReimportFbxSkeletalMeshFactory::StaticClass();

	// Create reimport handler for FBX scene
	UReimportFbxSceneFactory::StaticClass();

	// Create reimport handler for APEX destructible meshes
	UReimportDestructibleMeshFactory::StaticClass();

	// Create reimport handler for sound node waves
	UReimportSoundFactory::StaticClass();

	// Create reimport handler for surround sound waves
	UReimportSoundSurroundFactory::StaticClass();
}

FReimportManager::~FReimportManager()
{
	Handlers.Empty();
}

int32 FReimportHandler::GetPriority() const
{
	return UFactory::GetDefaultImportPriority();
}

/*-----------------------------------------------------------------------------
	PIE helpers.
-----------------------------------------------------------------------------*/

/**
 * Sets GWorld to the passed in PlayWorld and sets a global flag indicating that we are playing
 * in the Editor.
 *
 * @param	PlayInEditorWorld		PlayWorld
 * @return	the original GWorld
 */
UWorld* SetPlayInEditorWorld( UWorld* PlayInEditorWorld )
{
	check(!GIsPlayInEditorWorld);
	UWorld* SavedWorld = GWorld;
	GIsPlayInEditorWorld = true;
	GWorld = PlayInEditorWorld;

	return SavedWorld;
}

/**
 * Restores GWorld to the passed in one and reset the global flag indicating whether we are a PIE
 * world or not.
 *
 * @param EditorWorld	original world being edited
 */
void RestoreEditorWorld( UWorld* EditorWorld )
{
	check(GIsPlayInEditorWorld);
	GIsPlayInEditorWorld = false;
	GWorld = EditorWorld;
}

/**
 * Takes an FName and checks to see that it is unique among all loaded objects.
 *
 * @param	InName		The name to check
 * @param	Outer		The context for validating this object name. Should be a group/package, but could be ANY_PACKAGE if you want to check across the whole system (not recommended)
 * @param	InReason	If the check fails, this string is filled in with the reason why.
 *
 * @return	1 if the name is valid, 0 if it is not
 */

bool IsUniqueObjectName( const FName& InName, UObject* Outer, FText* InReason )
{
	// See if the name is already in use.
	if( StaticFindObject( UObject::StaticClass(), Outer, *InName.ToString() ) != NULL )
	{
		if ( InReason != NULL )
		{
			*InReason = NSLOCTEXT("UnrealEd", "NameAlreadyInUse", "Name is already in use by another object.");
		}
		return false;
	}

	return true;
}

/**
 * Takes an FName and checks to see that it is unique among all loaded objects.
 *
 * @param	InName		The name to check
 * @param	Outer		The context for validating this object name. Should be a group/package, but could be ANY_PACKAGE if you want to check across the whole system (not recommended)
 * @param	InReason	If the check fails, this string is filled in with the reason why.
 *
 * @return	1 if the name is valid, 0 if it is not
 */

bool IsUniqueObjectName( const FName& InName, UObject* Outer, FText& InReason )
{
	return IsUniqueObjectName(InName,Outer,&InReason);
}

//////////////////////////////////////////////////////////////////////////
// EditorUtilities

namespace EditorUtilities
{
	AActor* GetEditorWorldCounterpartActor( AActor* Actor )
	{
		const bool bIsSimActor = Actor->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
		if( bIsSimActor && GEditor->PlayWorld != NULL )
		{
			// Do we have a counterpart in the editor world?
			auto* SimWorldActor = Actor;
			if( GEditor->ObjectsThatExistInEditorWorld.Get( SimWorldActor ) )
			{
				// Find the counterpart level
				UWorld* EditorWorld = GEditor->EditorWorld;
				for( auto LevelIt( EditorWorld->GetLevelIterator() ); LevelIt; ++LevelIt )
				{
					auto* Level = *LevelIt;
					if( Level->GetFName() == SimWorldActor->GetLevel()->GetFName() )
					{
						// Find our counterpart actor
						const bool bExactClass = false;	// Don't match class exactly, because we support all classes derived from Actor as well!
						AActor* EditorWorldActor = FindObject<AActor>( Level, *SimWorldActor->GetFName().ToString(), bExactClass );
						if( EditorWorldActor )
						{
							return EditorWorldActor;
						}
					}
				}
			}
		}

		return NULL;
	}

	AActor* GetSimWorldCounterpartActor( AActor* Actor )
	{
		const bool bIsSimActor = Actor->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
		if( !bIsSimActor && GEditor->EditorWorld != NULL )
		{
			// Do we have a counterpart in the sim world?
			auto* EditorWorldActor = Actor;

			// Find the counterpart level
			UWorld* PlayWorld = GEditor->PlayWorld;
			for( auto LevelIt( PlayWorld->GetLevelIterator() ); LevelIt; ++LevelIt )
			{
				auto* Level = *LevelIt;
				if( Level->GetFName() == EditorWorldActor->GetLevel()->GetFName() )
				{
					// Find our counterpart actor
					const bool bExactClass = false;	// Don't match class exactly, because we support all classes derived from Actor as well!
					AActor* SimWorldActor = FindObject<AActor>( Level, *EditorWorldActor->GetFName().ToString(), bExactClass );
					if( SimWorldActor && GEditor->ObjectsThatExistInEditorWorld.Get( SimWorldActor ) )
					{
						return SimWorldActor;
					}
				}
			}
		}

		return NULL;
	}

	// Searches through the target components array of the target actor for the source component
	// TargetComponents array is passed in populated to avoid repeated refetching and StartIndex 
	// is updated as an optimization based on the assumption that the standard use case is iterating 
	// over two component arrays that will be parallel in order
	template<class AllocatorType = FDefaultAllocator>
	UActorComponent* FindMatchingComponentInstance( UActorComponent* SourceComponent, AActor* TargetActor, const TArray<UActorComponent*, AllocatorType>& TargetComponents, int32& StartIndex )
	{
		UActorComponent* TargetComponent = StartIndex < TargetComponents.Num() ? TargetComponents[ StartIndex ] : nullptr;

		// If the source and target components do not match (e.g. context-specific), attempt to find a match in the target's array elsewhere
		const int32 NumTargetComponents = TargetComponents.Num();
		if( (SourceComponent != nullptr) 
			&& ((TargetComponent == nullptr) 
				|| (SourceComponent->GetFName() != TargetComponent->GetFName()) ))
		{
			const bool bSourceIsArchetype = SourceComponent->HasAnyFlags(RF_ArchetypeObject);
			// Reset the target component since it doesn't match the source
			TargetComponent = nullptr;

			if (NumTargetComponents > 0)
			{
				// Attempt to locate a match elsewhere in the target's component list
				const int32 StartingIndex = (bSourceIsArchetype ? StartIndex : StartIndex + 1);
				int32 FindTargetComponentIndex = (StartingIndex >= NumTargetComponents) ? 0 : StartingIndex;
				do
				{
					UActorComponent* FindTargetComponent = TargetComponents[ FindTargetComponentIndex ];

					if (FindTargetComponent->GetClass() == SourceComponent->GetClass())
					{
						// In the case that the SourceComponent is an Archetype there is a better than even chance the name won't match due to the way the SCS
						// is set up, so we're actually going to reverse search the archetype chain
						if (bSourceIsArchetype)
						{
							UActorComponent* CheckComponent = FindTargetComponent;
							while (CheckComponent)
							{
								if ( SourceComponent == CheckComponent->GetArchetype())
								{
									TargetComponent = FindTargetComponent;
									StartIndex = FindTargetComponentIndex;
									break;
								}
								CheckComponent = Cast<UActorComponent>(CheckComponent->GetArchetype());
							}
							if (TargetComponent)
							{
								break;
							}
						}
						else
						{
							// If we found a match, update the target component and adjust the target index to the matching position
							if( FindTargetComponent != NULL && SourceComponent->GetFName() == FindTargetComponent->GetFName() )
							{
								TargetComponent = FindTargetComponent;
								StartIndex = FindTargetComponentIndex;
								break;
							}
						}
					}

					// Increment the index counter, and loop back to 0 if necessary
					if( ++FindTargetComponentIndex >= NumTargetComponents )
					{
						FindTargetComponentIndex = 0;
					}

				} while( FindTargetComponentIndex != StartIndex );
			}

			// If we still haven't found a match and we're targeting a class default object what we're really looking
			// for is an Archetype
			if(TargetComponent == nullptr && TargetActor->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
			{
				if (bSourceIsArchetype)
				{
					UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(SourceComponent->GetOuter());

					// If the target actor's class is a child of our owner and we're both archetypes, then we're actually looking for an overridden version of ourselves
					if (BPGC && TargetActor->GetClass()->IsChildOf(BPGC))
					{
						TargetComponent = Cast<UActorComponent>(TargetActor->GetClass()->FindArchetype(SourceComponent->GetClass(), SourceComponent->GetFName()));

						// If it is us, then we're done, we don't need to find this
						if (TargetComponent == SourceComponent)
						{
							TargetComponent = nullptr;
						}
					}
				}
				else
				{
					TargetComponent = CastChecked<UActorComponent>(SourceComponent->GetArchetype(), ECastCheckedType::NullAllowed);

					// If the returned target component is not from the direct class of the actor we're targeting, we need to insert an inheritable component
					if (TargetComponent && (TargetComponent->GetOuter() != TargetActor->GetClass()))
					{
						// This component doesn't exist in the hierarchy anywhere and we're not going to modify the CDO, so we'll drop it
						if (TargetComponent->HasAnyFlags(RF_ClassDefaultObject))
						{
							TargetComponent = nullptr;
						}
						else
						{
							UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(TargetActor->GetClass());
							UBlueprint* Blueprint = CastChecked<UBlueprint>(BPGC->ClassGeneratedBy);
							UInheritableComponentHandler* InheritableComponentHandler = Blueprint->GetInheritableComponentHandler(true);
							if (InheritableComponentHandler)
							{
								FComponentKey Key;
								FName const SourceComponentName = SourceComponent->GetFName();

								BPGC = Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass());
								while (!Key.IsValid() && BPGC)
								{
									USCS_Node* SCSNode = BPGC->SimpleConstructionScript->FindSCSNode(SourceComponentName);
									if (!SCSNode)
									{
										UBlueprint* SuperBlueprint = CastChecked<UBlueprint>(BPGC->ClassGeneratedBy);
										for (UActorComponent* ComponentTemplate : BPGC->ComponentTemplates)
										{
											if (ComponentTemplate->GetFName() == SourceComponentName)
											{
												if (UEdGraph* UCSGraph = FBlueprintEditorUtils::FindUserConstructionScript(SuperBlueprint))
												{
													TArray<UK2Node_AddComponent*> ComponentNodes;
													UCSGraph->GetNodesOfClass<UK2Node_AddComponent>(ComponentNodes);

													for (UK2Node_AddComponent* UCSNode : ComponentNodes)
													{
														if (ComponentTemplate == UCSNode->GetTemplateFromNode())
														{
															Key = FComponentKey(SuperBlueprint, FUCSComponentId(UCSNode));
															break;
														}
													}
												}
												break;
											}
										}
									}
									else
									{
										Key = FComponentKey(SCSNode);
										break;
									}
									BPGC = Cast<UBlueprintGeneratedClass>(BPGC->GetSuperClass());
								}

								if (ensure(Key.IsValid()))
								{
									check(InheritableComponentHandler->GetOverridenComponentTemplate(Key) == nullptr);
									TargetComponent = InheritableComponentHandler->CreateOverridenComponentTemplate(Key);
								}
								else
								{
									TargetComponent = nullptr;
								}								
							}
						}
					}
				}
			}
		}

		return TargetComponent;
	}


	UActorComponent* FindMatchingComponentInstance( UActorComponent* SourceComponent, AActor* TargetActor )
	{
		UActorComponent* MatchingComponent = NULL;
		int32 StartIndex = 0;

		if (TargetActor)
		{
			TInlineComponentArray<UActorComponent*> TargetComponents;
			TargetActor->GetComponents(TargetComponents);
			MatchingComponent = FindMatchingComponentInstance( SourceComponent, TargetActor, TargetComponents, StartIndex );
		}

		return MatchingComponent;
	}


	void CopySinglePropertyRecursive(const void* const InSourcePtr, void* const InTargetPtr, UObject* const InTargetObject, UProperty* const InProperty)
	{
		// Properties that are *object* properties are tricky
		// Sometimes the object will be a reference to a PIE-world object, and copying that reference back to an actor CDO asset is not a good idea
		// If the property is referencing an actor or actor component in the PIE world, then we can try and fix that reference up to the equivalent
		// from the editor world; otherwise we have to skip it
		bool bNeedsGenericCopy = true;
		if( UObjectPropertyBase* const ObjectProperty = Cast<UObjectPropertyBase>(InProperty) )
		{
			const int32 PropertyArrayDim = InProperty->ArrayDim;
			for (int32 ArrayIndex = 0; ArrayIndex < PropertyArrayDim; ArrayIndex++)
			{
				UObject* const SourceObjectPropertyValue = ObjectProperty->GetObjectPropertyValue_InContainer(InSourcePtr, ArrayIndex);
				if (SourceObjectPropertyValue && SourceObjectPropertyValue->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
				{
					// Not all the code paths below actually copy the object, but even if they don't we need to claim that they
					// did, as copying a reference to an object in a PIE world leads to crashes
					bNeedsGenericCopy = false;

					// REFERENCE an existing actor in the editor world from a REFERENCE in the PIE world
					if (SourceObjectPropertyValue->IsA(AActor::StaticClass()))
					{
						// We can try and fix-up an actor reference from the PIE world to instead be the version from the persistent world
						AActor* const EditorWorldActor = GetEditorWorldCounterpartActor(Cast<AActor>(SourceObjectPropertyValue));
						if (EditorWorldActor)
						{
							ObjectProperty->SetObjectPropertyValue_InContainer(InTargetPtr, EditorWorldActor, ArrayIndex);
						}
					}
					// REFERENCE an existing actor component in the editor world from a REFERENCE in the PIE world
					else if (SourceObjectPropertyValue->IsA(UActorComponent::StaticClass()) && InTargetObject->IsA(AActor::StaticClass()))
					{
						AActor* const TargetActor = Cast<AActor>(InTargetObject);
						TInlineComponentArray<UActorComponent*> TargetComponents;
						TargetActor->GetComponents(TargetComponents);

						// We can try and fix-up an actor component reference from the PIE world to instead be the version from the persistent world
						int32 TargetComponentIndex = 0;
						UActorComponent* const EditorWorldComponent = FindMatchingComponentInstance(Cast<UActorComponent>(SourceObjectPropertyValue), TargetActor, TargetComponents, TargetComponentIndex);
						if (EditorWorldComponent)
						{
							ObjectProperty->SetObjectPropertyValue_InContainer(InTargetPtr, EditorWorldComponent, ArrayIndex);
						}
					}
				}
			}
		}
		else if (UStructProperty* const StructProperty = Cast<UStructProperty>(InProperty))
		{
			// Ensure that the target struct is initialized before copying fields from the source.
			StructProperty->InitializeValue_InContainer(InTargetPtr);

			const int32 PropertyArrayDim = InProperty->ArrayDim;
			for (int32 ArrayIndex = 0; ArrayIndex < PropertyArrayDim; ArrayIndex++)
			{
				const void* const SourcePtr = StructProperty->ContainerPtrToValuePtr<void>(InSourcePtr, ArrayIndex);
				void* const TargetPtr = StructProperty->ContainerPtrToValuePtr<void>(InTargetPtr, ArrayIndex);

				for (TFieldIterator<UProperty> It(StructProperty->Struct); It; ++It)
				{
					UProperty* const InnerProperty = *It;
					CopySinglePropertyRecursive(SourcePtr, TargetPtr, InTargetObject, InnerProperty);
				}
			}

			bNeedsGenericCopy = false;
		}
		else if (UArrayProperty* const ArrayProperty = Cast<UArrayProperty>(InProperty))
		{
			check(InProperty->ArrayDim == 1);
			FScriptArrayHelper SourceArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(InSourcePtr));
			FScriptArrayHelper TargetArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(InTargetPtr));

			UProperty* InnerProperty = ArrayProperty->Inner;
			int32 Num = SourceArrayHelper.Num();

			// here we emulate UArrayProperty::CopyValuesInternal()
			if (!(InnerProperty->PropertyFlags & CPF_IsPlainOldData))
			{
				TargetArrayHelper.EmptyAndAddValues(Num);
			}
			else
			{
				TargetArrayHelper.EmptyAndAddUninitializedValues(Num);
			}
			
			for (int32 Index = 0; Index < Num; Index++)
			{
				CopySinglePropertyRecursive(SourceArrayHelper.GetRawPtr(Index), TargetArrayHelper.GetRawPtr(Index), InTargetObject, InnerProperty);
			}

			bNeedsGenericCopy = false;
		}
		
		// Handle copying properties that either aren't an object, or aren't part of the PIE world
		if( bNeedsGenericCopy )
		{
			InProperty->CopyCompleteValue_InContainer(InTargetPtr, InSourcePtr);
		}
	}

	void CopySingleProperty(const UObject* const InSourceObject, UObject* const InTargetObject, UProperty* const InProperty)
	{
		CopySinglePropertyRecursive(InSourceObject, InTargetObject, InTargetObject, InProperty);
	}

	int32 CopyActorProperties( AActor* SourceActor, AActor* TargetActor, const FCopyOptions& Options )
	{
		check( SourceActor != nullptr && TargetActor != nullptr );

		const bool bIsPreviewing = ( Options.Flags & ECopyOptions::PreviewOnly ) != 0;

		int32 CopiedPropertyCount = 0;

		// The actor's classes should be compatible, right?
		UClass* ActorClass = SourceActor->GetClass();
		check( TargetActor->GetClass()->IsChildOf(ActorClass) );

		// Get archetype instances for propagation (if requested)
		TArray<AActor*> ArchetypeInstances;
		if( Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances )
		{
			TArray<UObject*> ObjectArchetypeInstances;
			TargetActor->GetArchetypeInstances(ObjectArchetypeInstances);

			for (UObject* ObjectArchetype : ObjectArchetypeInstances)
			{
				if (AActor* ActorArchetype = Cast<AActor>(ObjectArchetype))
				{
					ArchetypeInstances.Add(ActorArchetype);
				}
			}
		}

		bool bTransformChanged = false;

		// Copy non-component properties from the old actor to the new actor
		// @todo sequencer: Most of this block of code was borrowed (pasted) from UEditorEngine::ConvertActors().  If we end up being able to share these code bodies, that would be nice!
		TSet<UObject*> ModifiedObjects;
		for( UProperty* Property = ActorClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
		{
			const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
			const bool bIsComponentContainer = !!( Property->PropertyFlags & CPF_ContainsInstancedReference );
			const bool bIsComponentProp = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
			const bool bIsBlueprintReadonly = !!(Options.Flags & ECopyOptions::FilterBlueprintReadOnly) && !!( Property->PropertyFlags & CPF_BlueprintReadOnly );
			const bool bIsIdentical = Property->Identical_InContainer( SourceActor, TargetActor );

			if( !bIsTransient && !bIsIdentical && !bIsComponentContainer && !bIsComponentProp && !bIsBlueprintReadonly)
			{
				const bool bIsSafeToCopy = !( Options.Flags & ECopyOptions::OnlyCopyEditOrInterpProperties ) || ( Property->HasAnyPropertyFlags( CPF_Edit | CPF_Interp ) );
				if( bIsSafeToCopy )
				{
					if (!Options.CanCopyProperty(*Property, *SourceActor))
					{
						continue;
					}

					if( !bIsPreviewing )
					{
						if( !ModifiedObjects.Contains(TargetActor) )
						{
							// Start modifying the target object
							TargetActor->Modify();
							ModifiedObjects.Add(TargetActor);
						}

						if( Options.Flags & ECopyOptions::CallPostEditChangeProperty )
						{
							TargetActor->PreEditChange( Property );
						}

						// Determine which archetype instances match the current property value of the target actor (before it gets changed). We only want to propagate the change to those instances.
						TArray<UObject*> ArchetypeInstancesToChange;
						if( Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances )
						{
							for( AActor* ArchetypeInstance : ArchetypeInstances )
							{
								if( ArchetypeInstance != nullptr && Property->Identical_InContainer( ArchetypeInstance, TargetActor ) )
								{
									ArchetypeInstancesToChange.Add( ArchetypeInstance );
								}
							}
						}

						CopySingleProperty(SourceActor, TargetActor, Property);

						if( Options.Flags & ECopyOptions::CallPostEditChangeProperty )
						{
							FPropertyChangedEvent PropertyChangedEvent( Property );
							TargetActor->PostEditChangeProperty( PropertyChangedEvent );
						}

						if( Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances )
						{
							for( int32 InstanceIndex = 0; InstanceIndex < ArchetypeInstancesToChange.Num(); ++InstanceIndex )
							{
								UObject* ArchetypeInstance = ArchetypeInstancesToChange[InstanceIndex];
								if( ArchetypeInstance != nullptr )
								{
									if( !ModifiedObjects.Contains(ArchetypeInstance) )
									{
										ArchetypeInstance->Modify();
										ModifiedObjects.Add(ArchetypeInstance);
									}

									CopySingleProperty( TargetActor, ArchetypeInstance, Property );
								}
							}
						}
					}

					++CopiedPropertyCount;
				}
			}
		}

		// Copy component properties from source to target if they match. Note that the component lists may not be 1-1 due to context-specific components (e.g. editor-only sprites, etc.).
		TInlineComponentArray<UActorComponent*> SourceComponents;
		TInlineComponentArray<UActorComponent*> TargetComponents;

		SourceActor->GetComponents(SourceComponents);
		TargetActor->GetComponents(TargetComponents);


		int32 TargetComponentIndex = 0;
		for( auto SourceComponentIter( SourceComponents.CreateConstIterator() ); SourceComponentIter; ++SourceComponentIter )
		{
			UActorComponent* SourceComponent = *SourceComponentIter;
			if (SourceComponent->CreationMethod == EComponentCreationMethod::UserConstructionScript)
			{
				continue;
			}
			UActorComponent* TargetComponent = FindMatchingComponentInstance( SourceComponent, TargetActor, TargetComponents, TargetComponentIndex );

			if( SourceComponent != nullptr && TargetComponent != nullptr )
			{
				UClass* ComponentClass = SourceComponent->GetClass();
				check( ComponentClass == TargetComponent->GetClass() );

				// Build a list of matching component archetype instances for propagation (if requested)
				TArray<UActorComponent*> ComponentArchetypeInstances;
				if( Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances )
				{
					for( AActor* ArchetypeInstance : ArchetypeInstances )
					{
						if( ArchetypeInstance != nullptr )
						{
							UActorComponent* ComponentArchetypeInstance = FindMatchingComponentInstance( TargetComponent, ArchetypeInstance );
							if( ComponentArchetypeInstance != nullptr )
							{
								ComponentArchetypeInstances.AddUnique( ComponentArchetypeInstance );
							}
						}
					}
				}

				TSet<const UProperty*> SourceUCSModifiedProperties;
				SourceComponent->GetUCSModifiedProperties(SourceUCSModifiedProperties);

				TArray<UActorComponent*> ComponentInstancesToReregister;

				// Copy component properties
				for( UProperty* Property = ComponentClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext )
				{
					const bool bIsTransient = !!( Property->PropertyFlags & CPF_Transient );
					const bool bIsIdentical = Property->Identical_InContainer( SourceComponent, TargetComponent );
					const bool bIsComponent = !!( Property->PropertyFlags & ( CPF_InstancedReference | CPF_ContainsInstancedReference ) );
					const bool bIsTransform =
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeScale3D) ||
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeLocation) ||
						Property->GetFName() == GET_MEMBER_NAME_CHECKED(USceneComponent, RelativeRotation);

					if( !bIsTransient && !bIsIdentical && !bIsComponent && !SourceUCSModifiedProperties.Contains(Property)
						&& ( !bIsTransform || SourceComponent != SourceActor->GetRootComponent() || ( !SourceActor->HasAnyFlags( RF_ClassDefaultObject | RF_ArchetypeObject ) && !TargetActor->HasAnyFlags( RF_ClassDefaultObject | RF_ArchetypeObject ) ) ) )
					{
						const bool bIsSafeToCopy = !( Options.Flags & ECopyOptions::OnlyCopyEditOrInterpProperties ) || ( Property->HasAnyPropertyFlags( CPF_Edit | CPF_Interp ) );
						if( bIsSafeToCopy )
						{
							if (!Options.CanCopyProperty(*Property, *SourceActor))
							{
								continue;
							}
							
							if( !bIsPreviewing )
							{
								if( !ModifiedObjects.Contains(TargetComponent) )
								{
									TargetComponent->SetFlags(RF_Transactional);
									TargetComponent->Modify();
									ModifiedObjects.Add(TargetComponent);
								}

								if( Options.Flags & ECopyOptions::CallPostEditChangeProperty )
								{
									// @todo simulate: Should we be calling this on the component instead?
									TargetActor->PreEditChange( Property );
								}

								// Determine which component archetype instances match the current property value of the target component (before it gets changed). We only want to propagate the change to those instances.
								TArray<UActorComponent*> ComponentArchetypeInstancesToChange;
								if( Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances )
								{
									for (UActorComponent* ComponentArchetypeInstance : ComponentArchetypeInstances)
									{
										if( ComponentArchetypeInstance != nullptr && Property->Identical_InContainer( ComponentArchetypeInstance, TargetComponent ) )
										{
											bool bAdd = true;
											// We also need to double check that either the direct archetype of the target is also identical
											if (ComponentArchetypeInstance->GetArchetype() != TargetComponent)
											{
												UActorComponent* CheckComponent = CastChecked<UActorComponent>(ComponentArchetypeInstance->GetArchetype());
												while (CheckComponent != ComponentArchetypeInstance)
												{
													if (!Property->Identical_InContainer( CheckComponent, TargetComponent ))
													{
														bAdd = false;
														break;
													}
													CheckComponent = CastChecked<UActorComponent>(CheckComponent->GetArchetype());
												}
											}
											
											if (bAdd)
											{
												ComponentArchetypeInstancesToChange.Add( ComponentArchetypeInstance );
											}
										}
									}
								}

								CopySingleProperty(SourceComponent, TargetComponent, Property);

								if( Options.Flags & ECopyOptions::CallPostEditChangeProperty )
								{
									FPropertyChangedEvent PropertyChangedEvent( Property );
									TargetActor->PostEditChangeProperty( PropertyChangedEvent );
								}

								if( Options.Flags & ECopyOptions::PropagateChangesToArchetypeInstances )
								{
									for( int32 InstanceIndex = 0; InstanceIndex < ComponentArchetypeInstancesToChange.Num(); ++InstanceIndex )
									{
										UActorComponent* ComponentArchetypeInstance = ComponentArchetypeInstancesToChange[InstanceIndex];
										if( ComponentArchetypeInstance != nullptr )
										{
											if( !ModifiedObjects.Contains(ComponentArchetypeInstance) )
											{
												// Ensure that this instance will be included in any undo/redo operations, and record it into the transaction buffer.
												// Note: We don't do this for components that originate from script, because they will be re-instanced from the template after an undo, so there is no need to record them.
												if (!ComponentArchetypeInstance->IsCreatedByConstructionScript())
												{
													ComponentArchetypeInstance->SetFlags(RF_Transactional);
													ComponentArchetypeInstance->Modify();
													ModifiedObjects.Add(ComponentArchetypeInstance);
												}

												// We must also modify the owner, because we'll need script components to be reconstructed as part of an undo operation.
												AActor* Owner = ComponentArchetypeInstance->GetOwner();
												if( Owner != nullptr && !ModifiedObjects.Contains(Owner))
												{
													Owner->Modify();
													ModifiedObjects.Add(Owner);
												}
											}

											if (ComponentArchetypeInstance->IsRegistered())
											{
												ComponentArchetypeInstance->UnregisterComponent();
												ComponentInstancesToReregister.Add(ComponentArchetypeInstance);
											}

											CopySingleProperty( TargetComponent, ComponentArchetypeInstance, Property );
										}
									}
								}
							}

							++CopiedPropertyCount;

							if( bIsTransform )
							{
								bTransformChanged = true;
							}
						}
					}
				}

				for (UActorComponent* ModifiedComponentInstance : ComponentInstancesToReregister)
				{
					ModifiedComponentInstance->RegisterComponent();
				}
			}
		}

		if (!bIsPreviewing && CopiedPropertyCount > 0 && TargetActor->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject) && TargetActor->GetClass()->HasAllClassFlags(CLASS_CompiledFromBlueprint))
		{
			FBlueprintEditorUtils::PostEditChangeBlueprintActors(CastChecked<UBlueprint>(TargetActor->GetClass()->ClassGeneratedBy));
		}

		// If one of the changed properties was part of the actor's transformation, then we'll call PostEditMove too.
		if( !bIsPreviewing && bTransformChanged )
		{
			if( Options.Flags & ECopyOptions::CallPostEditMove )
			{
				const bool bFinishedMove = true;
				TargetActor->PostEditMove( bFinishedMove );
			}
		}

		return CopiedPropertyCount;
	}
}

//////////////////////////////////////////////////////////////////////////
// FCachedActorLabels

FCachedActorLabels::FCachedActorLabels()
{
	
}

FCachedActorLabels::FCachedActorLabels(UWorld* World, const TSet<AActor*>& IgnoredActors)
{
	Populate(World, IgnoredActors);
}

void FCachedActorLabels::Populate(UWorld* World, const TSet<AActor*>& IgnoredActors)
{
	ActorLabels.Empty();

	for (FActorIterator It(World); It; ++It)
	{
		if (!IgnoredActors.Contains(*It))
		{
			ActorLabels.Add(It->GetActorLabel());
		}
	}
	ActorLabels.Shrink();
}

//////////////////////////////////////////////////////////////////////////

void ExecuteInvalidateCachedShaders(const TArray< FString >& Args)
{
	if(Args.Num() == 0)
	{
		// todo: log error, at least one command is needed
		UE_LOG(LogConsoleResponse, Display, TEXT("r.InvalidateCachedShaders failed\nAs this command should not be executed accidentally it requires you to specify an extra parameter."));
		return;
	}

	FString FileName = FPaths::EngineDir() + TEXT("Shaders/ShaderVersion.usf");

	FileName = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FileName);

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	SourceControlProvider.Init();

	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(FileName, EStateCacheUsage::ForceUpdate);
	if(SourceControlState.IsValid())
	{
		if( SourceControlState->CanCheckout() || SourceControlState->IsCheckedOutOther() )
		{
			if(SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FileName) == ECommandResult::Failed)
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("r.InvalidateCachedShaders failed\nCouldn't check out \"ShaderVersion.usf\""));
				return;
			}
		}
		else if(!SourceControlState->IsSourceControlled())
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("r.InvalidateCachedShaders failed\n\"ShaderVersion.usf\" is not under source control."));
		}
		else if(SourceControlState->IsCheckedOutOther())
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("r.InvalidateCachedShaders failed\n\"ShaderVersion.usf\" is already checked out by someone else\n(UE4 SourceControl needs to be fixed to allow multiple checkout.)"));
			return;
		}
		else if(SourceControlState->IsDeleted())
		{
			UE_LOG(LogConsoleResponse, Display, TEXT("r.InvalidateCachedShaders failed\n\"ShaderVersion.usf\" is marked for delete"));
			return;
		}
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	IFileHandle* FileHandle = PlatformFile.OpenWrite(*FileName);
	if(FileHandle)
	{
		FString Guid = FString(
			TEXT("// This file is automatically generated by the console command r.InvalidateCachedShaders\n")
			TEXT("// Each time the console command is executed it generates a new GUID. As this file is included\n")
			TEXT("// in common.usf (which should be included in any shader) it allows to invalidate the shader DDC.\n")
			TEXT("// \n")
			TEXT("// GUID = "))
			+ FGuid::NewGuid().ToString();

		FileHandle->Write((const uint8*)TCHAR_TO_ANSI(*Guid), Guid.Len());
		delete FileHandle;

		UE_LOG(LogConsoleResponse, Display, TEXT("r.InvalidateCachedShaders succeeded\n\"ShaderVersion.usf\" was updated.\n"));
	}
	else
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("r.InvalidateCachedShaders failed\nCouldn't open \"ShaderVersion.usf\".\n"));
	}
}

FAutoConsoleCommand InvalidateCachedShaders(
	TEXT("r.InvalidateCachedShaders"),
	TEXT("Invalidate shader cache by making a unique change to ShaderVersion.usf which is included in common.usf.")
	TEXT("To initiate actual the recompile of all shaders use \"recompileshaders changed\" or press \"Ctrl Shift .\".\n")
	TEXT("The ShaderVersion.usf file should be automatically checked out but  it needs to be checked in to have effect on other machines."),
	FConsoleCommandWithArgsDelegate::CreateStatic(ExecuteInvalidateCachedShaders)
	);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
