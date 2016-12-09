// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SkeletonTreeBuilder.h"
#include "IPersonaPreviewScene.h"
#include "SkeletonTreeBoneItem.h"
#include "SkeletonTreeSocketItem.h"
#include "SkeletonTreeAttachedAssetItem.h"
#include "SkeletonTreeVirtualBoneItem.h"
#include "Animation/DebugSkelMeshComponent.h"

void FSkeletonTreeBuilderOutput::Add(const TSharedPtr<class ISkeletonTreeItem>& InItem, const FName& InParentName, const FName& InParentType)
{
	Add(InItem, InParentName, TArray<FName, TInlineAllocator<1>>({ InParentType }));
}

void FSkeletonTreeBuilderOutput::Add(const TSharedPtr<class ISkeletonTreeItem>& InItem, const FName& InParentName, TArrayView<const FName> InParentTypes)
{
	TSharedPtr<ISkeletonTreeItem> ParentItem = Find(InParentName, InParentTypes);
	if (ParentItem.IsValid())
	{
		ParentItem->GetChildren().Add(InItem);
	}
	else
	{
		Items.Add(InItem);
	}

	LinearItems.Add(InItem);
}

TSharedPtr<class ISkeletonTreeItem> FSkeletonTreeBuilderOutput::Find(const FName& InName, const FName& InType) const
{
	return Find(InName, TArray<FName, TInlineAllocator<1>>({ InType }));
}

TSharedPtr<class ISkeletonTreeItem> FSkeletonTreeBuilderOutput::Find(const FName& InName, TArrayView<const FName> InTypes) const
{
	for (const TSharedPtr<ISkeletonTreeItem>& Item : LinearItems)
	{
		bool bPassesType = (InTypes.Num() == 0);
		for (const FName& TypeName : InTypes)
		{
			if (Item->IsOfTypeByName(TypeName))
			{
				bPassesType = true;
				break;
			}
		}

		if (bPassesType && Item->GetAttachName() == InName)
		{
			return Item;
		}
	}

	return nullptr;
}

FSkeletonTreeBuilder::FSkeletonTreeBuilder(const FSkeletonTreeBuilderArgs& InBuilderArgs, FOnFilterSkeletonTreeItem InOnFilterSkeletonTreeItem, const TSharedRef<class ISkeletonTree>& InSkeletonTree, const TSharedPtr<class IPersonaPreviewScene>& InPreviewScene)
	: BuilderArgs(InBuilderArgs)
	, OnFilterSkeletonTreeItem(InOnFilterSkeletonTreeItem)
	, SkeletonTreePtr(InSkeletonTree)
	, EditableSkeletonPtr(InSkeletonTree->GetEditableSkeleton())
	, PreviewScenePtr(InPreviewScene)
{
}

void FSkeletonTreeBuilder::Build(FSkeletonTreeBuilderOutput& Output)
{
	AddBones(Output);

	if (BuilderArgs.bShowSockets)
	{
		AddSockets(Output);
	}
	
	if (BuilderArgs.bShowAttachedAssets)
	{
		AddAttachedAssets(Output);
	}

	if (BuilderArgs.bShowVirtualBones)
	{
		AddVirtualBones(Output);
	}
}

void FSkeletonTreeBuilder::Filter(const FSkeletonTreeFilterArgs& InArgs, const TArray<TSharedPtr<ISkeletonTreeItem>>& InItems, TArray<TSharedPtr<ISkeletonTreeItem>>& OutFilteredItems)
{
	OutFilteredItems.Empty();

	for (const TSharedPtr<ISkeletonTreeItem>& Item : InItems)
	{
		if (InArgs.bWillFilter && InArgs.bFlattenHierarchyOnFilter)
		{
			FilterRecursive(InArgs, Item, OutFilteredItems);
		}
		else
		{
			ESkeletonTreeFilterResult FilterResult = FilterRecursive(InArgs, Item, OutFilteredItems);
			if (FilterResult != ESkeletonTreeFilterResult::Hidden)
			{
				OutFilteredItems.Add(Item);
			}

			Item->SetFilterResult(FilterResult);
		}
	}
}

ESkeletonTreeFilterResult FSkeletonTreeBuilder::FilterRecursive(const FSkeletonTreeFilterArgs& InArgs, const TSharedPtr<ISkeletonTreeItem>& InItem, TArray<TSharedPtr<ISkeletonTreeItem>>& OutFilteredItems)
{
	ESkeletonTreeFilterResult FilterResult = ESkeletonTreeFilterResult::Shown;

	InItem->GetFilteredChildren().Empty();

	if (InArgs.bWillFilter && InArgs.bFlattenHierarchyOnFilter)
	{
		FilterResult = OnFilterSkeletonTreeItem.Execute(InItem);
		InItem->SetFilterResult(FilterResult);

		if (FilterResult != ESkeletonTreeFilterResult::Hidden)
		{
			OutFilteredItems.Add(InItem);
		}

		for (const TSharedPtr<ISkeletonTreeItem>& Item : InItem->GetChildren())
		{
			FilterRecursive(InArgs, Item, OutFilteredItems);
		}
	}
	else
	{
		// check to see if we have any children that pass the filter
		ESkeletonTreeFilterResult DescendantsFilterResult = ESkeletonTreeFilterResult::Hidden;
		for (const TSharedPtr<ISkeletonTreeItem>& Item : InItem->GetChildren())
		{
			ESkeletonTreeFilterResult ChildResult = FilterRecursive(InArgs, Item, OutFilteredItems);
			if (ChildResult != ESkeletonTreeFilterResult::Hidden)
			{
				InItem->GetFilteredChildren().Add(Item);
			}
			if (ChildResult > DescendantsFilterResult)
			{
				DescendantsFilterResult = ChildResult;
			}
		}

		FilterResult = OnFilterSkeletonTreeItem.Execute(InItem);
		if (DescendantsFilterResult > FilterResult)
		{
			FilterResult = ESkeletonTreeFilterResult::ShownDescendant;
		}

		InItem->SetFilterResult(FilterResult);
	}

	return FilterResult;
}

void FSkeletonTreeBuilder::AddBones(FSkeletonTreeBuilderOutput& Output)
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();

	const FReferenceSkeleton& RefSkeleton = Skeleton.GetReferenceSkeleton();
	for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetRawBoneNum(); ++BoneIndex)
	{
		const FName& BoneName = RefSkeleton.GetBoneName(BoneIndex);

		FName ParentName = NAME_None;
		int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
		if (ParentIndex != INDEX_NONE)
		{
			ParentName = RefSkeleton.GetBoneName(ParentIndex);
		}
		TSharedRef<ISkeletonTreeItem> DisplayBone = CreateBoneTreeItem(BoneName);
		Output.Add(DisplayBone, ParentName, FSkeletonTreeBoneItem::GetTypeId());

		// @TODO: Persona2.0: Item expansion should be saved
		//SkeletonTreeView->SetItemExpansion(DisplayBone, true);
	}
}

void FSkeletonTreeBuilder::AddSockets(FSkeletonTreeBuilderOutput& Output)
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();

	// Add the sockets for the skeleton
	AddSocketsFromData(Skeleton.Sockets, ESocketParentType::Skeleton, Output);

	// Add the sockets for the mesh
	if (PreviewScenePtr.IsValid())
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
		if (USkeletalMesh* const SkeletalMesh = PreviewMeshComponent->SkeletalMesh)
		{
			AddSocketsFromData(SkeletalMesh->GetMeshOnlySocketList(), ESocketParentType::Mesh, Output);
		}
	}
}

void FSkeletonTreeBuilder::AddAttachedAssets(FSkeletonTreeBuilderOutput& Output)
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();

	// Mesh attached items...
	if (PreviewScenePtr.IsValid())
	{
		UDebugSkelMeshComponent* PreviewMeshComponent = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
		if (USkeletalMesh* const SkeletalMesh = PreviewMeshComponent->SkeletalMesh)
		{
			AddAttachedAssetContainer(SkeletalMesh->PreviewAttachedAssetContainer, Output);
		}
	}

	// ...skeleton attached items
	AddAttachedAssetContainer(Skeleton.PreviewAttachedAssetContainer, Output);
}

void FSkeletonTreeBuilder::AddSocketsFromData(const TArray< USkeletalMeshSocket* >& SocketArray, ESocketParentType ParentType, FSkeletonTreeBuilderOutput& Output)
{
	const USkeleton& Skeleton = EditableSkeletonPtr.Pin()->GetSkeleton();

	for (auto SocketIt = SocketArray.CreateConstIterator(); SocketIt; ++SocketIt)
	{
		USkeletalMeshSocket* Socket = *(SocketIt);

		bool bIsCustomized = false;

		if (ParentType == ESocketParentType::Mesh)
		{
			bIsCustomized = EditableSkeletonPtr.Pin()->DoesSocketAlreadyExist(nullptr, FText::FromName(Socket->SocketName), ESocketParentType::Skeleton, nullptr);
		}
		else
		{
			if (PreviewScenePtr.IsValid())
			{
				UDebugSkelMeshComponent* PreviewMeshComponent = PreviewScenePtr.Pin()->GetPreviewMeshComponent();
				if (USkeletalMesh* const SkeletalMesh = PreviewMeshComponent->SkeletalMesh)
				{
					bIsCustomized = EditableSkeletonPtr.Pin()->DoesSocketAlreadyExist(nullptr, FText::FromName(Socket->SocketName), ESocketParentType::Mesh, SkeletalMesh);
				}
			}
		}

		Output.Add(CreateSocketTreeItem(Socket, ParentType, bIsCustomized), Socket->BoneName, FSkeletonTreeBoneItem::GetTypeId());

		// @TODO: Persona2.0: Item expansion should be saved
	//	SkeletonTreeView->SetItemExpansion(DisplaySocket, true);
	}
}

void FSkeletonTreeBuilder::AddAttachedAssetContainer(const FPreviewAssetAttachContainer& AttachedObjects, FSkeletonTreeBuilderOutput& Output)
{
	for (auto Iter = AttachedObjects.CreateConstIterator(); Iter; ++Iter)
	{
		const FPreviewAttachedObjectPair& Pair = (*Iter);

		Output.Add(CreateAttachedAssetTreeItem(Pair.GetAttachedObject(), Pair.AttachedTo), Pair.AttachedTo, { FSkeletonTreeSocketItem::GetTypeId(), FSkeletonTreeSocketItem::GetTypeId() });
	}
}

void FSkeletonTreeBuilder::AddVirtualBones(FSkeletonTreeBuilderOutput& Output)
{
	const TArray<FVirtualBone>& VirtualBones = EditableSkeletonPtr.Pin()->GetSkeleton().GetVirtualBones();
	for (const FVirtualBone& VirtualBone : VirtualBones)
	{
		Output.Add(CreateVirtualBoneTreeItem(VirtualBone.VirtualBoneName), VirtualBone.SourceBoneName, FSkeletonTreeBoneItem::GetTypeId());
	}
}

TSharedRef<class ISkeletonTreeItem> FSkeletonTreeBuilder::CreateBoneTreeItem(const FName& InBoneName)
{
	return MakeShareable(new FSkeletonTreeBoneItem(InBoneName, SkeletonTreePtr.Pin().ToSharedRef()));
}

TSharedRef<class ISkeletonTreeItem> FSkeletonTreeBuilder::CreateSocketTreeItem(USkeletalMeshSocket* InSocket, ESocketParentType InParentType, bool bInIsCustomized)
{
	return MakeShareable(new FSkeletonTreeSocketItem(InSocket, InParentType, bInIsCustomized, SkeletonTreePtr.Pin().ToSharedRef()));
}

TSharedRef<class ISkeletonTreeItem> FSkeletonTreeBuilder::CreateAttachedAssetTreeItem(UObject* InAsset, const FName& InAttachedTo)
{
	return MakeShareable(new FSkeletonTreeAttachedAssetItem(InAsset, InAttachedTo, SkeletonTreePtr.Pin().ToSharedRef()));
}

TSharedRef<class ISkeletonTreeItem> FSkeletonTreeBuilder::CreateVirtualBoneTreeItem(const FName& InBoneName)
{
	return MakeShareable(new FSkeletonTreeVirtualBoneItem(InBoneName, SkeletonTreePtr.Pin().ToSharedRef()));
}
