// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerEditTool.h"


class FSequencerEditTool_Default
	: public FSequencerEditTool
{
public:

	// ISequencerEditTool interface

	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	
protected:

	void PerformHotspotSelection(const FPointerEvent& MouseEvent);
	
	TSharedPtr<SWidget> OnSummonContextMenu( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );
};
