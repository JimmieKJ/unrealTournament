// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraphNode_Comment.h"
#include "MaterialGraph/MaterialGraph.h"

#include "MaterialGraphNode_Comment.generated.h"

UCLASS(MinimalAPI)
class UMaterialGraphNode_Comment : public UEdGraphNode_Comment
{
	GENERATED_UCLASS_BODY()

	/** Material Comment that this node represents */
	UPROPERTY()
	class UMaterialExpressionComment* MaterialExpressionComment;

	/** Marks the Material Editor as dirty so that user prompted to apply change */
	FSetMaterialDirty MaterialDirtyDelegate;

	/** Fix up the node's owner after being copied */
	UNREALED_API void PostCopyNode();

	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	// End of UObject interface

	// Begin UEdGraphNode interface.
	virtual void PrepareForCopying() override;
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	virtual bool CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const override;
	virtual void PostPlacedNewNode() override;
	virtual void OnRenameNode(const FString& NewName) override;
	virtual void ResizeNode(const FVector2D& NewSize) override;
	// End UEdGraphNode interface.

private:
	/** Make sure the MaterialExpressionComment is owned by the Material */
	void ResetMaterialExpressionOwner();
};
