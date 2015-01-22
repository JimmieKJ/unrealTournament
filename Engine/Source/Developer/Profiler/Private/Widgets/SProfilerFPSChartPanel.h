// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A custom widget that acts as a container for widgets like SDataGraph or SEventTree. */
class SProfilerFPSChartPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SProfilerFPSChartPanel )
		{}

		SLATE_ARGUMENT( TSharedPtr<FFPSAnalyzer>, FPSAnalyzer )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param InArgs - The declaration data for this widget
	 */
	void Construct( const FArguments& InArgs );
};