// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

#include "SlateBasics.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "MeshMergingTool"

FMeshMergingTool::FMeshMergingTool()
	: bReplaceSourceActors(false)
{
	SettingsObject = UMeshMergingSettingsObject::Get();
}

TSharedRef<SWidget> FMeshMergingTool::GetWidget()
{
	SAssignNew(MergingDialog, SMeshMergingDialog, this);
	return MergingDialog.ToSharedRef();
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

	FVector MergedActorLocation;
	TArray<UObject*> AssetsToSync;
	// Merge...
	{
		FScopedSlowTask SlowTask(0, LOCTEXT("MergingActorsSlowTask", "Merging actors..."));
		SlowTask.MakeDialog();

		// Extracting static mesh components from the selected mesh components in the dialog
		const TArray<TSharedPtr<FMeshComponentData>>& SelectedMeshComponents = MergingDialog->GetSelectedMeshComponents();
		TArray<UStaticMeshComponent*> MeshComponentsToMerge;

		for ( const TSharedPtr<FMeshComponentData>& SelectedMeshComponent : SelectedMeshComponents)
		{
			// Determine whether or not this component should be incorporated according the user settings
			if (SelectedMeshComponent->bShouldIncorporate)
			{
				MeshComponentsToMerge.Add(SelectedMeshComponent->MeshComponent.Get());
			}
		}
		
		UWorld* World = MeshComponentsToMerge[0]->GetWorld();	
		checkf(World != nullptr, TEXT("Invalid World retrieved from Mesh components"));
		const float ScreenAreaSize = TNumericLimits<float>::Max();
		MeshUtilities.MergeStaticMeshComponents(MeshComponentsToMerge, World, SettingsObject->Settings, nullptr, PackageName, AssetsToSync, MergedActorLocation, ScreenAreaSize, true);
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

	MergingDialog->Reset();

	return true;
}

bool FMeshMergingTool::CanMerge() const
{	
	return MergingDialog->GetNumSelectedMeshComponents() >= 1;
}

#undef LOCTEXT_NAMESPACE