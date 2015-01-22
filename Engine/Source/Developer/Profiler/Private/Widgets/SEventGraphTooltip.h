// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** An advanced tooltip used to show various informations in the event graph widget. */
class SEventGraphTooltip
{
protected:
	static EVisibility GetHotPathIconVisibility( const FEventGraphSamplePtr EventSample )
	{
		const bool bIsHotPathIconVisible = EventSample->_bIsHotPath;
		return bIsHotPathIconVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

public:
	static TSharedPtr<SToolTip> GetTableCellTooltip( const FEventGraphSamplePtr EventSample );

};