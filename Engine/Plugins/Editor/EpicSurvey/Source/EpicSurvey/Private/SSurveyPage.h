// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class FSurveyPage;

class SSurveyPage : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SSurveyPage )
	{ }
	SLATE_END_ARGS()


	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< FSurveyPage >& Page );
};
