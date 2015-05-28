// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SNodePanel.h"
#include "SGraphNode.h"
#include "KismetNodes/SGraphNodeK2Base.h"
#include "KismetNodes/SGraphNodeK2Var.h"

class SGraphNodeK2GameplayEffectVar : public SGraphNodeK2Var
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2Var){}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, UK2Node* InNode );

	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	// End of SGraphNode interface

protected:
	TSharedRef<SWidget> OpenGameplayEffectButtonContent(FText ButtonText, FText ButtonTooltipText, FString DocumentationExcerpt = FString());
	
	FReply OnOpenGameplayEffect();
	EVisibility IsOpenGameplayEffectButtonVisible() const;

	FSlateColor GetVariableColor() const;
};
