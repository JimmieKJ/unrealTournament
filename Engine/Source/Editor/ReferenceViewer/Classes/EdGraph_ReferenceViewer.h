// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph_ReferenceViewer.generated.h"

UCLASS()
class UEdGraph_ReferenceViewer : public UEdGraph
{
	GENERATED_UCLASS_BODY()

public:
	// UObject implementation
	virtual void BeginDestroy() override;
	// End UObject implementation

	void SetGraphRoot(const TArray<FName>& GraphRootPackageNames, const FIntPoint& GraphRootOrigin = FIntPoint(ForceInitToZero));
	const TArray<FName>& GetCurrentGraphRootPackageNames() const;
	class UEdGraphNode_Reference* RebuildGraph();

	bool IsSearchDepthLimited() const;
	bool IsSearchBreadthLimited() const;

	void SetSearchDepthLimitEnabled(bool newEnabled);
	void SetSearchBreadthLimitEnabled(bool newEnabled);

	int32 GetSearchDepthLimit() const;
	int32 GetSearchBreadthLimit() const;

	void SetSearchDepthLimit(int32 NewDepthLimit);
	void SetSearchBreadthLimit(int32 NewBreadthLimit);

	/** Accessor for the thumbnail pool in this graph */
	const TSharedPtr<class FAssetThumbnailPool>& GetAssetThumbnailPool() const;

private:
	UEdGraphNode_Reference* ConstructNodes(const TArray<FName>& GraphRootPackageNames, const FIntPoint& GraphRootOrigin);
	int32 RecursivelyGatherSizes(bool bReferencers, const TArray<FName>& PackageNames, int32 CurrentDepth, TSet<FName>& VisitedNames, TMap<FName, int32>& OutNodeSizes) const;
	void GatherAssetData(const TSet<FName>& AllPackageNames, TMap<FName, FAssetData>& OutPackageToAssetDataMap) const;
	class UEdGraphNode_Reference* RecursivelyConstructNodes(bool bReferencers, UEdGraphNode_Reference* RootNode, const TArray<FName>& PackageNames, const FIntPoint& NodeLoc, const TMap<FName, int32>& NodeSizes, const TMap<FName, FAssetData>& PackagesToAssetDataMap, int32 CurrentDepth, TSet<FName>& VisitedNames);

	bool ExceedsMaxSearchDepth(int32 Depth) const;
	bool ExceedsMaxSearchBreadth(int32 Breadth) const;

	UEdGraphNode_Reference* CreateReferenceNode();

	/** Removes all nodes from the graph */
	void RemoveAllNodes();

private:
	/** Pool for maintaining and rendering thumbnails */
	TSharedPtr<class FAssetThumbnailPool> AssetThumbnailPool;

	TArray<FName> CurrentGraphRootPackageNames;
	FIntPoint CurrentGraphRootOrigin;

	int32 MaxSearchDepth;
	int32 MaxSearchBreadth;

	bool bLimitSearchDepth;
	bool bLimitSearchBreadth;
};

