// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphNodeUtils.h" // for FNodeTextCache
#include "EdGraphNode_Comment.generated.h"

typedef TArray<class UObject*> FCommentNodeSet;

UENUM()
namespace ECommentBoxMode
{
	enum Type
	{
		// This comment box will move any fully contained nodes when it moves
		GroupMovement UMETA(DisplayName="Group Movement"),

		// This comment box has no effect on nodes contained inside it
		NoGroupMovement UMETA(DisplayName="Comment")
	};
}

UCLASS(MinimalAPI)
class UEdGraphNode_Comment : public UEdGraphNode
{
public:
	GENERATED_BODY()

public:
	UEdGraphNode_Comment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Color to style comment with */
	UPROPERTY(EditAnywhere, Category=Comment)
	FLinearColor CommentColor;

	/** Whether to use Comment Color to color the background of the comment bubble shown when zoomed out. */
	UPROPERTY(EditAnywhere, Category=Comment, meta=(DisplayName="Color Bubble"))
	uint32 bColorCommentBubble:1;

	/** Whether the comment should move any fully enclosed nodes around when it is moved */
	UPROPERTY(EditAnywhere, Category=Comment)
	TEnumAsByte<ECommentBoxMode::Type> MoveMode;

	/** comment Depth */
	UPROPERTY()
	int32 CommentDepth;

public:

	// Begin UObject Interface
	UNREALED_API static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// End UObject Interface

	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override {}
	UNREALED_API virtual FText GetTooltipText() const override;
	UNREALED_API virtual FLinearColor GetNodeCommentColor() const override;
	UNREALED_API virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool ShouldOverridePinNames() const override { return true; }
	UNREALED_API virtual FText GetPinNameOverride(const UEdGraphPin& Pin) const override;
	UNREALED_API virtual void ResizeNode(const FVector2D& NewSize) override;
	UNREALED_API virtual void PostPlacedNewNode() override;
	UNREALED_API virtual void OnRenameNode(const FString& NewName) override;
	UNREALED_API virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const override;
	virtual bool ShouldDrawNodeAsComment() const override { return true; }
	UNREALED_API virtual FString GetDocumentationLink() const override;
	UNREALED_API virtual FString GetDocumentationExcerptName() const override;
	UNREALED_API virtual FName GetPaletteIcon(FLinearColor& OutColor) const override;
	// End UEdGraphNode interface

	/** Add a node that will be dragged when this comment is dragged */
	UNREALED_API void	AddNodeUnderComment(UObject* Object);

	/** Clear all of the nodes which are currently being dragged with this comment */
	UNREALED_API void	ClearNodesUnderComment();

	/** Set the Bounds for the comment node */
	UNREALED_API void SetBounds(const class FSlateRect& Rect);

	/** Return the set of nodes underneath the comment */
	UNREALED_API const FCommentNodeSet& GetNodesUnderComment() const;

private:
	/** Nodes currently within the region of the comment */
	FCommentNodeSet	NodesUnderComment;

	/** Constructing FText strings can be costly, so we cache the node's tooltip */
	FNodeTextCache CachedTooltip;
};

