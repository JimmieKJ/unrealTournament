// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class SGraphNodeSequencePlayer : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeSequencePlayer){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UK2Node* InNode);

	// SNodePanel::SNode interface
	virtual void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override;
	// End of SNodePanel::SNode interface

	// SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox) override;
	// End of SGraphNode interface

protected:
	/** Get the sequence player associated with this graph node */
	struct FAnimNode_SequencePlayer* GetSequencePlayer() const;
	EVisibility GetSliderVisibility() const;
	float GetSequencePositionRatio() const;
	void SetSequencePositionRatio(float NewRatio);

	FText GetPositionTooltip() const;

	bool GetSequencePositionInfo(float& Out_Position, float& Out_Length, int32& FrameCount) const;
};
