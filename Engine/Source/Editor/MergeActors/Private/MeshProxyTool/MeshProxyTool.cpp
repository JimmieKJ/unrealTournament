// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "MergeActorsPrivatePCH.h"
#include "MeshProxyTool.h"
#include "SMeshProxyDialog.h"
#include "MeshUtilities.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Selection.h"


#define LOCTEXT_NAMESPACE "MeshProxyTool"


TSharedRef<SWidget> FMeshProxyTool::GetWidget()
{
	return SNew(SMeshProxyDialog, this);
}


FText FMeshProxyTool::GetTooltipText() const
{
	return LOCTEXT("MeshProxyToolTooltip", "Harvest geometry from selected actors and merge them into single mesh.");
}


FString FMeshProxyTool::GetDefaultPackageName() const
{
	FString PackageName;

	USelection* SelectedActors = GEditor->GetSelectedActors();

	// Iterate through selected actors and find first static mesh asset
	// Use this static mesh path as destination package name for a merged mesh
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			TInlineComponentArray<UStaticMeshComponent*> SMComponets;
			Actor->GetComponents<UStaticMeshComponent>(SMComponets);
			for (UStaticMeshComponent* Component : SMComponets)
			{
				if (Component->StaticMesh)
				{
					PackageName = FPackageName::GetLongPackagePath(Component->StaticMesh->GetOutermost()->GetName());
					PackageName += FString(TEXT("/PROXY_")) + Component->StaticMesh->GetName();
					break;
				}
			}
		}

		if (!PackageName.IsEmpty())
		{
			break;
		}
	}

	if (PackageName.IsEmpty())
	{
		PackageName = FPackageName::FilenameToLongPackageName(FPaths::GameContentDir() + TEXT("PROXY"));
		PackageName = MakeUniqueObjectName(NULL, UPackage::StaticClass(), *PackageName).ToString();
	}

	return PackageName;
}


bool FMeshProxyTool::RunMerge(const FString& PackageName)
{
	TArray<AActor*> Actors;
	TArray<UObject*> AssetsToSync;

	//Get the module for the proxy mesh utilities
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	USelection* SelectedActors = GEditor->GetSelectedActors();
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
		}
	}

	if (Actors.Num())
	{
		GWarn->BeginSlowTask(LOCTEXT("MeshProxy_CreatingProxy", "Creating Mesh Proxy"), true);
		GEditor->BeginTransaction(LOCTEXT("MeshProxy_Create", "Creating Mesh Proxy"));

		FVector ProxyLocation = FVector::ZeroVector;
		MeshUtilities.CreateProxyMesh(Actors, ProxySettings, NULL, PackageName, AssetsToSync, ProxyLocation);

		GEditor->EndTransaction();
		GWarn->EndSlowTask();

		//Update the asset registry that a new static mash and material has been created
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
		}
	}

	return true;
}


#undef LOCTEXT_NAMESPACE
