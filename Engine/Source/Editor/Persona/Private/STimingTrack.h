// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "STrack.h"

//////////////////////////////////////////////////////////////////////////
// Specialised anim track which arranges overlapping nodes into groups
class STimingTrack : public STrack
{
public:

	SLATE_BEGIN_ARGS(STimingTrack)
	{}

	SLATE_ATTRIBUTE(float, ViewInputMin)
	SLATE_ATTRIBUTE(float, ViewInputMax)
	SLATE_ATTRIBUTE(float, TrackMaxValue)
	SLATE_ATTRIBUTE(float, TrackMinValue)
	SLATE_ATTRIBUTE(int32, TrackNumDiscreteValues)

	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

protected:
};