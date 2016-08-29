// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SExpanderArrow.h"


/**
 *
 *
 */
class SProfilerStatExpander : public SExpanderArrow
{
public:

	SLATE_BEGIN_ARGS( SProfilerStatExpander )
		: _StyleSet(&FCoreStyle::Get())
		, _IndentAmount(10)
		, _BaseIndentLevel(0)
	{ }
		SLATE_ARGUMENT(const ISlateStyle*, StyleSet)
		SLATE_ATTRIBUTE(float, IndentAmount)
		SLATE_ATTRIBUTE(int32, BaseIndentLevel)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedPtr<class ITableRow>& TableRow );

protected:

	/** Returns the statistic expander glyph */
	FText GetStatExpanderTextGlyph() const;

	/** Returns the statistic expander glyph color */
	FSlateColor GetStatExpanderColorAndOpacity() const;
};
