// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "AssetRegistryModule.h"
#include "AssetThumbnail.h"

UEdGraph_ReferenceViewer::UEdGraph_ReferenceViewer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AssetThumbnailPool = MakeShareable( new FAssetThumbnailPool(1024) );

	MaxSearchDepth = 1;
	MaxSearchBreadth = 15;

	bLimitSearchDepth = true;
	bLimitSearchBreadth = true;
	bIsShowSoftReferences = true;
	bIsShowSoftDependencies = true;
	bIsShowHardReferences = true;
	bIsShowHardDependencies = true;
}

void UEdGraph_ReferenceViewer::BeginDestroy()
{
	if ( AssetThumbnailPool.IsValid() )
	{
		AssetThumbnailPool->ReleaseResources();
		AssetThumbnailPool.Reset();
	}

	Super::BeginDestroy();
}

void UEdGraph_ReferenceViewer::SetGraphRoot(const TArray<FName>& GraphRootPackageNames, const FIntPoint& GraphRootOrigin)
{
	CurrentGraphRootPackageNames = GraphRootPackageNames;
	CurrentGraphRootOrigin = GraphRootOrigin;
}

const TArray<FName>& UEdGraph_ReferenceViewer::GetCurrentGraphRootPackageNames() const
{
	return CurrentGraphRootPackageNames;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::RebuildGraph()
{
	RemoveAllNodes();
	UEdGraphNode_Reference* NewRootNode = ConstructNodes(CurrentGraphRootPackageNames, CurrentGraphRootOrigin);
	NotifyGraphChanged();

	return NewRootNode;
}

bool UEdGraph_ReferenceViewer::IsSearchDepthLimited() const
{
	return bLimitSearchDepth;
}

bool UEdGraph_ReferenceViewer::IsSearchBreadthLimited() const
{
	return bLimitSearchBreadth;
}

bool UEdGraph_ReferenceViewer::IsShowSoftReferences() const
{
	return bIsShowSoftReferences;
}

bool UEdGraph_ReferenceViewer::IsShowSoftDependencies() const
{
	return bIsShowSoftDependencies;
}

bool UEdGraph_ReferenceViewer::IsShowHardReferences() const
{
	return bIsShowHardReferences;
}

bool UEdGraph_ReferenceViewer::IsShowHardDependencies() const
{
	return bIsShowHardDependencies;
}

void UEdGraph_ReferenceViewer::SetSearchDepthLimitEnabled(bool newEnabled)
{
	bLimitSearchDepth = newEnabled;
}

void UEdGraph_ReferenceViewer::SetSearchBreadthLimitEnabled(bool newEnabled)
{
	bLimitSearchBreadth = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowSoftReferencesEnabled(bool newEnabled)
{
	bIsShowSoftReferences = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowSoftDependenciesEnabled(bool newEnabled)
{
	bIsShowSoftDependencies = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowHardReferencesEnabled(bool newEnabled)
{
	bIsShowHardReferences = newEnabled;
}

void UEdGraph_ReferenceViewer::SetShowHardDependenciesEnabled(bool newEnabled)
{
	bIsShowHardDependencies = newEnabled;
}

int32 UEdGraph_ReferenceViewer::GetSearchDepthLimit() const
{
	return MaxSearchDepth;
}

int32 UEdGraph_ReferenceViewer::GetSearchBreadthLimit() const
{
	return MaxSearchBreadth;
}

void UEdGraph_ReferenceViewer::SetSearchDepthLimit(int32 NewDepthLimit)
{
	MaxSearchDepth = NewDepthLimit;
}

void UEdGraph_ReferenceViewer::SetSearchBreadthLimit(int32 NewBreadthLimit)
{
	MaxSearchBreadth = NewBreadthLimit;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::ConstructNodes(const TArray<FName>& GraphRootPackageNames, const FIntPoint& GraphRootOrigin )
{
	UEdGraphNode_Reference* RootNode = NULL;

	if ( GraphRootPackageNames.Num() > 0 )
	{
		TMap<FName, int32> ReferencerNodeSizes;
		TSet<FName> VisitedReferencerSizeNames;
		int32 ReferencerDepth = 1;
		RecursivelyGatherSizes(/*bReferencers=*/true, GraphRootPackageNames, ReferencerDepth, VisitedReferencerSizeNames, ReferencerNodeSizes);

		TMap<FName, int32> DependencyNodeSizes;
		TSet<FName> VisitedDependencySizeNames;
		int32 DependencyDepth = 1;
		RecursivelyGatherSizes(/*bReferencers=*/false, GraphRootPackageNames, DependencyDepth, VisitedDependencySizeNames, DependencyNodeSizes);

		TSet<FName> AllPackageNames = VisitedReferencerSizeNames;
		AllPackageNames.Append(VisitedDependencySizeNames);
		TMap<FName, FAssetData> PackagesToAssetDataMap;
		GatherAssetData(AllPackageNames, PackagesToAssetDataMap);

		// Create the root node
		RootNode = CreateReferenceNode();
		RootNode->SetupReferenceNode(GraphRootOrigin, GraphRootPackageNames, PackagesToAssetDataMap.FindRef(GraphRootPackageNames[0]));

		TSet<FName> VisitedReferencerNames;
		int32 VisitedReferencerDepth = 1;
		RecursivelyConstructNodes(/*bReferencers=*/true, RootNode, GraphRootPackageNames, GraphRootOrigin, ReferencerNodeSizes, PackagesToAssetDataMap, VisitedReferencerDepth, VisitedReferencerNames);

		TSet<FName> VisitedDependencyNames;
		int32 VisitedDependencyDepth = 1;
		RecursivelyConstructNodes(/*bReferencers=*/false, RootNode, GraphRootPackageNames, GraphRootOrigin, DependencyNodeSizes, PackagesToAssetDataMap, VisitedDependencyDepth, VisitedDependencyNames);
	}

	return RootNode;
}

int32 UEdGraph_ReferenceViewer::RecursivelyGatherSizes(bool bReferencers, const TArray<FName>& PackageNames, int32 CurrentDepth, TSet<FName>& VisitedNames, TMap<FName, int32>& OutNodeSizes) const
{
	check(PackageNames.Num() > 0);

	VisitedNames.Append(PackageNames);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> ReferenceNames;

	if ( bReferencers )
	{
		for ( auto NameIt = PackageNames.CreateConstIterator(); NameIt; ++NameIt )
		{
			AssetRegistryModule.Get().GetReferencers(*NameIt, ReferenceNames);
		}
	}
	else
	{
		for ( auto NameIt = PackageNames.CreateConstIterator(); NameIt; ++NameIt )
		{
			AssetRegistryModule.Get().GetDependencies(*NameIt, ReferenceNames);
		}
	}

	int32 NodeSize = 0;
	if ( ReferenceNames.Num() > 0 && !ExceedsMaxSearchDepth(CurrentDepth) )
	{
		int32 NumReferencesMade = 0;
		int32 NumReferencesExceedingMax = 0;

		// Since there are referencers, use the size of all your combined referencers.
		// Do not count your own size since there could just be a horizontal line of nodes
		for ( auto RefIt = ReferenceNames.CreateConstIterator(); RefIt; ++RefIt )
		{
			if ( !VisitedNames.Contains(*RefIt) )
			{
				if ( !ExceedsMaxSearchBreadth(NumReferencesMade) )
				{
					TArray<FName> NewPackageNames;
					NewPackageNames.Add(*RefIt);
					NodeSize += RecursivelyGatherSizes(bReferencers, NewPackageNames, CurrentDepth + 1, VisitedNames, OutNodeSizes);
					NumReferencesMade++;
				}
				else
				{
					NumReferencesExceedingMax++;
				}
			}
		}

		if ( NumReferencesExceedingMax > 0 )
		{
			// Add one size for the collapsed node
			NodeSize++;
		}
	}

	if ( NodeSize == 0 )
	{
		// If you have no valid children, the node size is just 1 (counting only self to make a straight line)
		NodeSize = 1;
	}

	OutNodeSizes.Add(PackageNames[0], NodeSize);
	return NodeSize;
}

void UEdGraph_ReferenceViewer::GatherAssetData(const TSet<FName>& AllPackageNames, TMap<FName, FAssetData>& OutPackageToAssetDataMap) const
{
	// Take a guess to find the asset instead of searching for it. Most packages have a single asset in them with the same name as the package.
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	for ( auto PackageIt = AllPackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		const FString& PackageName = (*PackageIt).ToString();
		const FString& PackagePath = PackageName + TEXT(".") + FPackageName::GetLongPackageAssetName(PackageName);
		Filter.ObjectPaths.Add( FName(*PackagePath) );
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);
	for ( auto AssetIt = AssetDataList.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		OutPackageToAssetDataMap.Add((*AssetIt).PackageName, *AssetIt);
	}
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::RecursivelyConstructNodes(bool bReferencers, UEdGraphNode_Reference* RootNode, const TArray<FName>& PackageNames, const FIntPoint& NodeLoc, const TMap<FName, int32>& NodeSizes, const TMap<FName, FAssetData>& PackagesToAssetDataMap, int32 CurrentDepth, TSet<FName>& VisitedNames)
{
	check(PackageNames.Num() > 0);

	VisitedNames.Append(PackageNames);

	UEdGraphNode_Reference* NewNode = NULL;
	if ( RootNode->GetPackageName() == PackageNames[0] )
	{
		// Don't create the root node. It is already created!
		NewNode = RootNode;
	}
	else
	{
		NewNode = CreateReferenceNode();
		NewNode->SetupReferenceNode(NodeLoc, PackageNames, PackagesToAssetDataMap.FindRef(PackageNames[0]));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FName> ReferenceNames;
	TArray<FName> HardReferenceNames;
	if ( bReferencers )
	{
		for ( auto NameIt = PackageNames.CreateConstIterator(); NameIt; ++NameIt )
		{
			AssetRegistryModule.Get().GetReferencers(*NameIt, HardReferenceNames, EAssetRegistryDependencyType::Hard);
			if (bIsShowSoftReferences && bIsShowHardReferences)
			{
				AssetRegistryModule.Get().GetReferencers(*NameIt, ReferenceNames, EAssetRegistryDependencyType::All);
			}
			else if (bIsShowSoftReferences && !bIsShowHardReferences)
			{
				AssetRegistryModule.Get().GetReferencers(*NameIt, ReferenceNames, EAssetRegistryDependencyType::Soft);
			}
			else
			{
				AssetRegistryModule.Get().GetReferencers(*NameIt, ReferenceNames, EAssetRegistryDependencyType::Hard);
			}
		}
	}
	else
	{
		for ( auto NameIt = PackageNames.CreateConstIterator(); NameIt; ++NameIt )
		{
			AssetRegistryModule.Get().GetDependencies(*NameIt, HardReferenceNames, EAssetRegistryDependencyType::Hard);
			if (bIsShowSoftDependencies && bIsShowHardDependencies)
			{
				AssetRegistryModule.Get().GetDependencies(*NameIt, ReferenceNames, EAssetRegistryDependencyType::All);
			}
			else if (bIsShowSoftDependencies && !bIsShowHardDependencies)
			{
				AssetRegistryModule.Get().GetDependencies(*NameIt, ReferenceNames, EAssetRegistryDependencyType::Soft);
			}
			else
			{
				AssetRegistryModule.Get().GetDependencies(*NameIt, ReferenceNames, EAssetRegistryDependencyType::Hard);
			}
		}
	}

	if ( ReferenceNames.Num() > 0 && !ExceedsMaxSearchDepth(CurrentDepth) )
	{
		FIntPoint ReferenceNodeLoc = NodeLoc;

		if ( bReferencers )
		{
			// Referencers go left
			ReferenceNodeLoc.X -= 800;
		}
		else
		{
			// Dependencies go right
			ReferenceNodeLoc.X += 800;
		}

		const int32 NodeSizeY = 200;
		const int32 TotalReferenceSizeY = NodeSizes.FindChecked(PackageNames[0]) * NodeSizeY;

		ReferenceNodeLoc.Y -= TotalReferenceSizeY * 0.5f;
		ReferenceNodeLoc.Y += NodeSizeY * 0.5f;

		int32 NumReferencesMade = 0;
		int32 NumReferencesExceedingMax = 0;
		for ( int32 RefIdx = 0; RefIdx < ReferenceNames.Num(); ++RefIdx )
		{
			FName ReferenceName = ReferenceNames[RefIdx];

			if ( !VisitedNames.Contains(ReferenceName) )
			{
				if ( !ExceedsMaxSearchBreadth(NumReferencesMade) )
				{
					const int32 RefSizeY = NodeSizes.FindChecked(ReferenceName);
					FIntPoint RefNodeLoc;
					RefNodeLoc.X = ReferenceNodeLoc.X;
					RefNodeLoc.Y = ReferenceNodeLoc.Y + RefSizeY * NodeSizeY * 0.5 - NodeSizeY * 0.5;
					TArray<FName> NewPackageNames;
					NewPackageNames.Add(ReferenceName);
					UEdGraphNode_Reference* ReferenceNode = RecursivelyConstructNodes(bReferencers, RootNode, NewPackageNames, RefNodeLoc, NodeSizes, PackagesToAssetDataMap, CurrentDepth + 1, VisitedNames);
					for (FName NodeName : HardReferenceNames)
					{
						FName NewNodeFName = FName(*ReferenceNode->NodeComment);
						if (NodeName == FName(*ReferenceNode->NodeComment))
						{
							if (bReferencers)
							{
								ReferenceNode->GetDependencyPin()->PinType.PinCategory = TEXT("hard");
							}
							else
							{
								ReferenceNode->GetReferencerPin()->PinType.PinCategory = TEXT("hard");
							}
						}
					}
					if ( ensure(ReferenceNode) )
					{
						if ( bReferencers )
						{
							NewNode->AddReferencer( ReferenceNode );
						}
						else
						{
							ReferenceNode->AddReferencer( NewNode );
						}

						ReferenceNodeLoc.Y += RefSizeY * NodeSizeY;
					}

					NumReferencesMade++;
				}
				else
				{
					NumReferencesExceedingMax++;
				}
			}
		}

		if ( NumReferencesExceedingMax > 0 )
		{
			// There are more references than allowed to be displayed. Make a collapsed node.
			UEdGraphNode_Reference* ReferenceNode = CreateReferenceNode();
			FIntPoint RefNodeLoc;
			RefNodeLoc.X = ReferenceNodeLoc.X;
			RefNodeLoc.Y = ReferenceNodeLoc.Y;

			if ( ensure(ReferenceNode) )
			{
				ReferenceNode->SetReferenceNodeCollapsed(RefNodeLoc, NumReferencesExceedingMax);

				if ( bReferencers )
				{
					NewNode->AddReferencer( ReferenceNode );
				}
				else
				{
					ReferenceNode->AddReferencer( NewNode );
				}
			}
		}
	}

	return NewNode;
}

const TSharedPtr<FAssetThumbnailPool>& UEdGraph_ReferenceViewer::GetAssetThumbnailPool() const
{
	return AssetThumbnailPool;
}

bool UEdGraph_ReferenceViewer::ExceedsMaxSearchDepth(int32 Depth) const
{
	return bLimitSearchDepth && Depth > MaxSearchDepth;
}

bool UEdGraph_ReferenceViewer::ExceedsMaxSearchBreadth(int32 Breadth) const
{
	return bLimitSearchBreadth && Breadth > MaxSearchBreadth;
}

UEdGraphNode_Reference* UEdGraph_ReferenceViewer::CreateReferenceNode()
{
	const bool bSelectNewNode = false;
	return Cast<UEdGraphNode_Reference>(CreateNode(UEdGraphNode_Reference::StaticClass(), bSelectNewNode));
}

void UEdGraph_ReferenceViewer::RemoveAllNodes()
{
	TArray<UEdGraphNode*> NodesToRemove = Nodes;
	for (int32 NodeIndex = 0; NodeIndex < NodesToRemove.Num(); ++NodeIndex)
	{
		RemoveNode(NodesToRemove[NodeIndex]);
	}
}