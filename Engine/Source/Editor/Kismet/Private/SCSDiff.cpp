// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "SCSDiff.h"
#include "SKismetInspector.h"
#include "SSCSEditor.h"
#include "BlueprintEditorUtils.h"

#include <vector>

FSCSDiff::FSCSDiff(const UBlueprint* InBlueprint)
{
	if (!FBlueprintEditorUtils::SupportsConstructionScript(InBlueprint) || InBlueprint->SimpleConstructionScript == NULL)
	{
		ContainerWidget = SNew(SBox);
		return;
	}

	TSharedRef<SKismetInspector> Inspector = SNew(SKismetInspector)
		.HideNameArea(true)
		.ViewIdentifier(FName("BlueprintInspector"))
		.IsPropertyEditingEnabledDelegate(FIsPropertyEditingEnabled::CreateStatic([] { return false; }));

	ContainerWidget = SNew(SSplitter)
		.Orientation(Orient_Vertical)
		+ SSplitter::Slot()
		[
			SAssignNew(SCSEditor, SSCSEditor, TSharedPtr<FBlueprintEditor>(), InBlueprint->SimpleConstructionScript, const_cast<UBlueprint*>(InBlueprint), Inspector)
		]
		+ SSplitter::Slot()
		[
			Inspector
		];
}

void FSCSDiff::HighlightProperty(FName VarName, FPropertySoftPath Property)
{
	if (SCSEditor.IsValid())
	{
		check(VarName != FName());
		SCSEditor->HighlightTreeNode(VarName, FPropertyPath());
	}
}

TSharedRef< SWidget > FSCSDiff::TreeWidget()
{
	return ContainerWidget.ToSharedRef();
}

void GetDisplayedHierarchyRecursive(TArray< int32 >& TreeAddress, const FSCSEditorTreeNode& Node, TArray< FSCSResolvedIdentifier >& OutResult)
{
	FSCSIdentifier Identifier = { Node.GetVariableName(), TreeAddress };
	FSCSResolvedIdentifier ResolvedIdentifier = { Identifier, Node.GetComponentTemplate() };
	OutResult.Push(ResolvedIdentifier);
	const auto& Children = Node.GetChildren();
	for (int32 Iter = 0; Iter != Children.Num(); ++Iter)
	{
		TreeAddress.Push(Iter);
		GetDisplayedHierarchyRecursive(TreeAddress, *Children[Iter], OutResult);
		TreeAddress.Pop();
	}
}

TArray< FSCSResolvedIdentifier > FSCSDiff::GetDisplayedHierarchy() const
{
	TArray< FSCSResolvedIdentifier > Ret;

	if( SCSEditor.IsValid() )
	{
		for (int32 Iter = 0; Iter != SCSEditor->RootNodes.Num(); ++Iter)
		{
			TArray< int32 > TreeAddress;
			TreeAddress.Push(Iter);
			GetDisplayedHierarchyRecursive(TreeAddress, *SCSEditor->RootNodes[Iter], Ret);
		}
	}

	return Ret;
}

