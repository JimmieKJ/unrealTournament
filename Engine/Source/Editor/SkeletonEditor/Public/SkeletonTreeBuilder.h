// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISkeletonTreeBuilder.h"
#include "IEditableSkeleton.h"
#include "ISkeletonTreeItem.h"

class IPersonaPreviewScene;
class ISkeletonTree;
class USkeletalMeshSocket;

/** Options for skeleton building */
struct FSkeletonTreeBuilderArgs
{
	FSkeletonTreeBuilderArgs()
		: bShowSockets(true)
		, bShowAttachedAssets(true)
		, bShowVirtualBones(true)
	{}

	/** Whether we should show sockets */
	bool bShowSockets;

	/** Whether we should show attached assets */
	bool bShowAttachedAssets;

	/** Whether we should show virtual bones */
	bool bShowVirtualBones;
};

/** Enum which tells us what type of bones we should be showing */
enum class EBoneFilter : int32
{
	All,
	Mesh,
	LOD,
	Weighted, /** only showing weighted bones of current LOD */
	None,
	Count
};

/** Enum which tells us what type of sockets we should be showing */
enum class ESocketFilter : int32
{
	Active,
	Mesh,
	Skeleton,
	All,
	None,
	Count
};

/** Delegate used to filter an item. */
DECLARE_DELEGATE_RetVal_OneParam(ESkeletonTreeFilterResult, FOnFilterSkeletonTreeItem, const TSharedPtr<class ISkeletonTreeItem>& /*InItem*/);

class SKELETONEDITOR_API FSkeletonTreeBuilder : public ISkeletonTreeBuilder
{
public:
	FSkeletonTreeBuilder(const FSkeletonTreeBuilderArgs& InBuilderArgs, FOnFilterSkeletonTreeItem InOnFilterSkeletonTreeItem, const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedPtr<class IPersonaPreviewScene>& InPreviewScene);

	/** ISkeletonTreeBuilder interface */
	virtual void Build(FSkeletonTreeBuilderOutput& Output) override;
	virtual void Filter(const FSkeletonTreeFilterArgs& InArgs, const TArray<TSharedPtr<class ISkeletonTreeItem>>& InItems, TArray<TSharedPtr<class ISkeletonTreeItem>>& OutFilteredItems) override;

private:
	/** Add Bones */
	void AddBones(FSkeletonTreeBuilderOutput& Output);

	/** Add Sockets */
	void AddSockets(FSkeletonTreeBuilderOutput& Output);

	void AddAttachedAssets(FSkeletonTreeBuilderOutput& Output);

	void AddSocketsFromData(const TArray< USkeletalMeshSocket* >& SocketArray, ESocketParentType ParentType, FSkeletonTreeBuilderOutput& Output);

	void AddAttachedAssetContainer(const FPreviewAssetAttachContainer& AttachedObjects, FSkeletonTreeBuilderOutput& Output);

	void AddVirtualBones(FSkeletonTreeBuilderOutput& Output);

	/** Create an item for a bone */
	TSharedRef<class ISkeletonTreeItem> CreateBoneTreeItem(const FName& InBoneName);

	/** Create an item for a socket */
	TSharedRef<class ISkeletonTreeItem> CreateSocketTreeItem(USkeletalMeshSocket* InSocket, ESocketParentType ParentType, bool bIsCustomized);

	/** Create an item for an attached asset */
	TSharedRef<class ISkeletonTreeItem> CreateAttachedAssetTreeItem(UObject* InAsset, const FName& InAttachedTo);

	/** Create an item for a virtual bone */
	TSharedRef<class ISkeletonTreeItem> CreateVirtualBoneTreeItem(const FName& InBoneName);

	/** Helper function for filtering */
	ESkeletonTreeFilterResult FilterRecursive(const FSkeletonTreeFilterArgs& InArgs, const TSharedPtr<ISkeletonTreeItem>& InItem, TArray<TSharedPtr<ISkeletonTreeItem>>& OutFilteredItems);

private:
	/** Options for building */
	FSkeletonTreeBuilderArgs BuilderArgs;

	/** Delegate used for filtering */
	FOnFilterSkeletonTreeItem OnFilterSkeletonTreeItem;

	/** The skeleton tree we will build against */
	TWeakPtr<class ISkeletonTree> SkeletonTreePtr;

	/** The editable skeleton that the skeleton tree represents */
	TWeakPtr<class IEditableSkeleton> EditableSkeletonPtr;

	/** The (optional) preview scene we will build against */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
};
