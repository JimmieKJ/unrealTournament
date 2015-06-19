// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

class SProjectViewItem : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProjectViewItem) {}

	SLATE_ARGUMENT(FName, IconName)

	SLATE_ARGUMENT(FText, Text)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};