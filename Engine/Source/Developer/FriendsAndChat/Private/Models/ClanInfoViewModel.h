// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FClanInfoViewModel
	: public TSharedFromThis<FClanInfoViewModel>
{
public:
	virtual ~FClanInfoViewModel() {}

	virtual FText GetClanTitle() = 0;
	virtual int32 GetMemberCount() = 0;
	virtual TSharedRef<class FFriendListViewModel > GetFriendListViewModel() = 0;
	virtual FText GetListCountText() const = 0;
	virtual FText GetClanBrushName() const = 0;
};

/**
 * Creates the implementation for an ClanInfoViewModel.
 *
 * @return the newly created ClanInfoViewModel implementation.
 */
FACTORY(TSharedRef< FClanInfoViewModel >, FClanInfoViewModel,
	const TSharedRef<class IClanInfo>& ClanInfo);