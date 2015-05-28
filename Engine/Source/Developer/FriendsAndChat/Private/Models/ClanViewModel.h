// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FClanViewModel
	: public TSharedFromThis<FClanViewModel>
{
public:
	virtual ~FClanViewModel() {}

	virtual FText GetClanTitle() = 0;

	virtual TSharedPtr<class FClanInfoViewModel> GetClanInfoViewModel() = 0;

	virtual void EnumerateJoinedClans(TArray<TSharedRef<class IClanInfo>>& OUTClanList) = 0;
};

/**
 * Creates the implementation for an ClanViewModel.
 *
 * @return the newly created ClanViewModel implementation.
 */
FACTORY(TSharedRef< FClanViewModel >, FClanViewModel,
	const TSharedRef<class IClanRepository>& ClanRepository);