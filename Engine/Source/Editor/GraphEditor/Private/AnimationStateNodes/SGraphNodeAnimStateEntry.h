// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

//
// Forward declarations.
//
class UAnimStateEntryNode;

class SGraphNodeAnimStateEntry : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeAnimStateEntry){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UAnimStateEntryNode* InNode);

	// SNodePanel::SNode interface
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	// End of SNodePanel::SNode interface

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	
	// End of SGraphNode interface

	
protected:
	FSlateColor GetBorderBackgroundColor() const;

	FText GetPreviewCornerText() const;
};
