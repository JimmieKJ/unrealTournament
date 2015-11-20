// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MergeActorsPrivatePCH.h"
#include "MeshMergingTool.h"
#include "SMeshMergingDialog.h"
#include "PropertyEditorModule.h"
#include "Engine/TextureLODSettings.h"
#include "RawMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Selection.h"
#include "SystemSettings.h"
#include "Engine/TextureLODSettings.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "MeshMergingTool"


FMeshMergingTool::FMeshMergingTool()
	: bReplaceSourceActors(false)
	, bExportSpecificLOD(false)
	, ExportLODIndex(0)
{}


TSharedRef<SWidget> FMeshMergingTool::GetWidget()
{
	return SNew(SMeshMergingDialog, this);
}


FText FMeshMergingTool::GetTooltipText() const
{
	return LOCTEXT("MeshMergingToolTooltip", "Harvest geometry from selected actors and merge grouping them by materials.");
}


FString FMeshMergingTool::GetDefaultPackageName() const
{
	FString PackageName = FPackageName::FilenameToLongPackageName(FPaths::GameContentDir() + TEXT("SM_MERGED"));

	USelection* SelectedActors = GEditor->GetSelectedActors();
	// Iterate through selected actors and find first static mesh asset
	// Use this static mesh path as destination package name for a merged mesh
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			FString ActorName = Actor->GetName();
			PackageName = FString::Printf(TEXT("%s_%s"), *PackageName, *ActorName);
			break;
		}
	}

	if (PackageName.IsEmpty())
	{
		PackageName = MakeUniqueObjectName(NULL, UPackage::StaticClass(), *PackageName).ToString();
	}

	return PackageName;
}


bool FMeshMergingTool::RunMerge(const FString& PackageName)
{
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
			UniqueLevels.AddUnique(Actor->GetLevel());
		}
	}

	// This restriction is only for replacement of selected actors with merged mesh actor
	if (UniqueLevels.Num() > 1 && bReplaceSourceActors)
	{
		FText Message = NSLOCTEXT("UnrealEd", "FailedToMergeActorsSublevels_Msg", "The selected actors should be in the same level");
		OpenMsgDlgInt(EAppMsgType::Ok, Message, NSLOCTEXT("UnrealEd", "FailedToMergeActors_Title", "Unable to merge actors"));
		return false;
	}

	int32 TargetMeshLOD = bExportSpecificLOD ? ExportLODIndex : INDEX_NONE;
	FVector MergedActorLocation;
	TArray<UObject*> AssetsToSync;
	// Merge...
	{
		FScopedSlowTask SlowTask(0, LOCTEXT("MergingActorsSlowTask", "Merging actors..."));
		SlowTask.MakeDialog();

		MeshUtilities.MergeActors(Actors, MergingSettings, NULL, PackageName, TargetMeshLOD, AssetsToSync, MergedActorLocation);
	}

	if (AssetsToSync.Num())
	{
		FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		int32 AssetCount = AssetsToSync.Num();
		for (int32 AssetIndex = 0; AssetIndex < AssetCount; AssetIndex++)
		{
			AssetRegistry.AssetCreated(AssetsToSync[AssetIndex]);
			GEditor->BroadcastObjectReimported(AssetsToSync[AssetIndex]);
		}

		//Also notify the content browser that the new assets exists
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(AssetsToSync, true);

		// Place new mesh in the world
		if (bReplaceSourceActors)
		{
			UStaticMesh* MergedMesh = nullptr;
			if (AssetsToSync.FindItemByClass(&MergedMesh))
			{
				const FScopedTransaction Transaction(LOCTEXT("PlaceMergedActor", "Place Merged Actor"));
				UniqueLevels[0]->Modify();

				UWorld* World = UniqueLevels[0]->OwningWorld;
				FActorSpawnParameters Params;
				Params.OverrideLevel = UniqueLevels[0];
				FRotator MergedActorRotation(ForceInit);
								
				AStaticMeshActor* MergedActor = World->SpawnActor<AStaticMeshActor>(MergedActorLocation, MergedActorRotation, Params);
				MergedActor->GetStaticMeshComponent()->StaticMesh = MergedMesh;
				MergedActor->SetActorLabel(AssetsToSync[0]->GetName());

				// Remove source actors
				for (AActor* Actor : Actors)
				{
					Actor->Destroy();
				}
			}
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
