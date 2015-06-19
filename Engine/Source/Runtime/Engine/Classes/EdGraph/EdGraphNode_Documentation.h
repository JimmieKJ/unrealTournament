// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraphNode_Documentation.generated.h"

UCLASS(MinimalAPI)
class UEdGraphNode_Documentation : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	/** Documentation Link */
	UPROPERTY(meta=(DisplayName="Documentation Link"))
	FString Link;

	/** Documentation Excerpt */
	UPROPERTY(meta=(DisplayName="Documentation Excerpt"))
	FString Excerpt;

public:

#if WITH_EDITOR
	// Begin UEdGraphNode interface
	virtual void AllocateDefaultPins() override {}
	virtual bool ShouldOverridePinNames() const override { return true; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor( 0.2f, 1.f, 0.2f ); }
	ENGINE_API virtual FText GetTooltipText() const override;
	ENGINE_API virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	ENGINE_API virtual FText GetPinNameOverride(const UEdGraphPin& Pin) const override;
	ENGINE_API virtual void ResizeNode(const FVector2D& NewSize) override;
	ENGINE_API virtual void PostPlacedNewNode() override;
	ENGINE_API virtual void OnRenameNode(const FString& NewName) override;
	ENGINE_API virtual TSharedPtr<class INameValidatorInterface> MakeNameValidator() const override;
	ENGINE_API virtual FString GetDocumentationLink() const override { return Link; }
	ENGINE_API virtual FString GetDocumentationExcerptName() const override { return Excerpt; }
	ENGINE_API virtual FName GetPaletteIcon(FLinearColor& OutColor) const override { return TEXT("GraphEditor.Documentation_16x");  }
	// End UEdGraphNode interface

	/** Set the Bounds for the comment node */
	ENGINE_API void SetBounds(const class FSlateRect& Rect);

#endif
};

