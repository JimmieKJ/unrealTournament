// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTResetPostProcessVolumesCommandlet.h"

#if UE_EDITOR
#include "EngineUtils.h"
#include "ISourceControlModule.h"
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
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> SwitchParams;
	ParseCommandLine(*Params, Tokens, Switches, SwitchParams);

	bool bRestrictedOnly = false;
	bool bBloomOnly = false;
	for (auto It = Switches.CreateConstIterator(); It; ++It)
	{
		if (*It == TEXT("RestrictedOnly"))
		{
			bRestrictedOnly = true;
		}
		if (*It == TEXT("BloomOnly"))
		{
			bBloomOnly = true;
		}
	}

	TArray<FString> Maps;
	TArray<FString> AllPackageFilenames;
	FEditorFileUtils::FindAllPackageFiles(AllPackageFilenames);
	for (int32 PackageIndex = 0; PackageIndex < AllPackageFilenames.Num(); PackageIndex++)
	{
		const FString& Filename = AllPackageFilenames[PackageIndex];
		if (FPaths::GetExtension(Filename, true) == FPackageName::GetMapPackageExtension())
		{
			if (bRestrictedOnly && !Filename.Contains(TEXT("RestrictedAssets")))
			{
				continue;
			}

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
				if (bBloomOnly)
				{
					UE_LOG(LogResetPostProcessVolumesCommandlet, Warning, TEXT("Bloom only reset"));

					FPostProcessSettings DefaultSettings;

					It->Settings.bOverride_BloomIntensity = DefaultSettings.bOverride_BloomIntensity;
					It->Settings.bOverride_BloomThreshold = DefaultSettings.bOverride_BloomThreshold;
					It->Settings.bOverride_Bloom1Tint = DefaultSettings.bOverride_Bloom1Tint;
					It->Settings.bOverride_Bloom1Size = DefaultSettings.bOverride_Bloom1Size;
					It->Settings.bOverride_Bloom2Tint = DefaultSettings.bOverride_Bloom2Tint;
					It->Settings.bOverride_Bloom3Tint = DefaultSettings.bOverride_Bloom3Tint;
					It->Settings.bOverride_Bloom4Tint = DefaultSettings.bOverride_Bloom4Tint;
					It->Settings.bOverride_Bloom4Size = DefaultSettings.bOverride_Bloom4Size;
					It->Settings.bOverride_Bloom5Tint = DefaultSettings.bOverride_Bloom5Tint;
					It->Settings.bOverride_Bloom5Size = DefaultSettings.bOverride_Bloom5Size;
					It->Settings.bOverride_Bloom6Tint = DefaultSettings.bOverride_Bloom6Tint;
					It->Settings.bOverride_Bloom6Size = DefaultSettings.bOverride_Bloom6Size;

					It->Settings.bOverride_BloomSizeScale = DefaultSettings.bOverride_BloomSizeScale;
					It->Settings.bOverride_BloomDirtMaskIntensity = DefaultSettings.bOverride_BloomDirtMaskIntensity;
					It->Settings.bOverride_BloomDirtMaskTint = DefaultSettings.bOverride_BloomDirtMaskTint;
					It->Settings.bOverride_BloomDirtMask = DefaultSettings.bOverride_BloomDirtMask;


					It->Settings.BloomIntensity = DefaultSettings.BloomIntensity;
					It->Settings.BloomThreshold = DefaultSettings.BloomThreshold;
					It->Settings.BloomSizeScale = DefaultSettings.BloomSizeScale;

					It->Settings.Bloom1Size = DefaultSettings.Bloom1Size;
					It->Settings.Bloom2Size = DefaultSettings.Bloom2Size;
					It->Settings.Bloom3Size = DefaultSettings.Bloom3Size;
					It->Settings.Bloom4Size = DefaultSettings.Bloom4Size;
					It->Settings.Bloom5Size = DefaultSettings.Bloom5Size;
					It->Settings.Bloom6Size = DefaultSettings.Bloom6Size;

					It->Settings.Bloom1Tint = DefaultSettings.Bloom1Tint;
					It->Settings.Bloom2Tint = DefaultSettings.Bloom2Tint;
					It->Settings.Bloom3Tint = DefaultSettings.Bloom3Tint;
					It->Settings.Bloom4Tint = DefaultSettings.Bloom4Tint;
					It->Settings.Bloom5Tint = DefaultSettings.Bloom5Tint;
					It->Settings.Bloom6Tint = DefaultSettings.Bloom6Tint;

					It->Settings.BloomDirtMaskIntensity = DefaultSettings.BloomDirtMaskIntensity;
					It->Settings.BloomDirtMaskTint = DefaultSettings.BloomDirtMaskTint;
					It->Settings.BloomDirtMask = DefaultSettings.BloomDirtMask;
				}
				else
				{
					It->Settings = FPostProcessSettings();
				}

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
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
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