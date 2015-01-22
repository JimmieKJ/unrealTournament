// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** A window which allows the user to select additional content to add to the currently loaded project. */
class SAddContentDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SWindow)
	{}

	SLATE_END_ARGS()

	~SAddContentDialog();

	void Construct(const FArguments& InArgs);

private:
	/** The widget representing available content and which content the user has selected. */
	TSharedPtr<SAddContentWidget> AddContentWidget;
};