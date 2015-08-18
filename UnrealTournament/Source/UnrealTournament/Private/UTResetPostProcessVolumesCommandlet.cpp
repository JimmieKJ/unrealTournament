// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "EngineUtils.h"
#include "ISourceControlModule.h"
#include "UTResetPostProcessVolumesCommandlet.h"

#if UE_EDITOR
#include "UnrealEd.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogResetPostProcessVolumesCommandlet, Log, All);

UUTResetPostProcessVolumesCommandlet::UUTResetPostProcessVolumesCommandlet(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

int32 UUTResetPostProcessVolumesCommandlet::Main(const FString& Params)
{
#if UE_EDITOR
	TArray<FString> Maps;
	TArray<FString> AllPackageFilenames;
	FEditorFileUtils::FindAllPackageFiles(AllPackageFilenames);
	for (int32 PackageIndex = 0; PackageIndex < AllPackageFilenames.Num(); PackageIndex++)
	{
		const FString& Filename = AllPackageFilenames[PackageIndex];
		if (FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension())
		{
			Maps.Add(Filename);
		}
	}

	FScopedSourceControl SourceControl;

	for (int32 i = 0; i < Maps.Num(); i++)
	{
		const FString& MapName = Maps[i];

		FSourceControlStatePtr SourceControlState = SourceControl.GetProvider().GetState(MapName, EStateCacheUsage::ForceUpdate);

		if (SourceControlState.IsValid() && !SourceControlState->CanCheckout())
		{
			UE_LOG(LogResetPostProcessVolumesCommandlet, Warning, TEXT("Skipping %s: the file can't be checked out"), *MapName);
			continue;
		}

		UWorld* World = GWorld;
		// clean up any previous world
		if (World != NULL)
		{
			World->CleanupWorld();
			World->RemoveFromRoot();
		}

		// load the package
		UE_LOG(LogResetPostProcessVolumesCommandlet, Warning, TEXT("Loading %s..."), *MapName);
		UPackage* Package = LoadPackage(NULL, *MapName, LOAD_None);

		// load the world we're interested in
		World = UWorld::FindWorldInPackage(Package);
		if (World)
		{
			// We shouldnt need this - but just in case
			GWorld = World;
			// need to have a bool so we dont' save every single map
			bool bIsDirty = false;

			World->WorldType = EWorldType::Editor;

			// add the world to the root set so that the garbage collection to delete replaced actors doesn't garbage collect the whole world
			World->AddToRoot();
			// initialize the levels in the world
			World->InitWorld(UWorld::InitializationValues().AllowAudioPlayback(false));
			World->GetWorldSettings()->PostEditChange();
			World->UpdateWorldComponents(true, false);

			// iterate through all the post process volumes
			for (TActorIterator<APostProcessVolume> It(World, APostProcessVolume::StaticClass()); It; ++It)
			{
				It->Settings = FPostProcessSettings();
				Package->MarkPackageDirty();
				bIsDirty = true;
			}
			// collect garbage to delete replaced actors and any objects only referenced by them (components, etc)
			World->PerformGarbageCollectionAndCleanupActors();

			// save the world
			if ((Package->IsDirty() == true) && (bIsDirty == true))
			{
				SourceControlState = SourceControl.GetProvider().GetState(MapName, EStateCacheUsage::ForceUpdate);
				if (SourceControlState.IsValid() && SourceControlState->CanCheckout())
				{
					SourceControl.GetProvider().Execute(ISourceControlOperation::Create<FCheckOut>(), Package);
				}

				UE_LOG(LogResetPostProcessVolumesCommandlet, Warning, TEXT("Saving %s..."), *MapName);
				GEditor->SavePackage(Package, World, RF_NoFlags, *MapName, GWarn);
			}

			// clear GWorld by removing it from the root set and replacing it with a new one
			World->CleanupWorld();
			World->RemoveFromRoot();
			World = GWorld = NULL;
		}

		// get rid of the loaded world
		UE_LOG(LogResetPostProcessVolumesCommandlet, Warning, TEXT("GCing..."));
		CollectGarbage(RF_Native);
	}

	// UEditorEngine::FinishDestroy() expects GWorld to exist
	if (GWorld)
	{
		GWorld->DestroyWorld(false);
	}
	GWorld = UWorld::CreateWorld(EWorldType::Editor, false);
#endif
	return 0;
}