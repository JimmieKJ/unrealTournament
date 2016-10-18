// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/** This class is used to control which FKeys should move focus */
class SLATE_API FNavigationConfig : public TSharedFromThis<FNavigationConfig>
{
public:
	/** Keys that should move focus left */
	TSet<FKey> Left;

	/** Keys that should move focus right */
	TSet<FKey> Right;

	/** Keys that should move focus up*/
	TSet<FKey> Up;

	/** Keys that should move focus down*/
	TSet<FKey> Down;

	FNavigationConfig();

	virtual ~FNavigationConfig();

	virtual EUINavigation GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const;
};
