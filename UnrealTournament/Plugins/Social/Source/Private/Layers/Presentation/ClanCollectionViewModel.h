// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FClanCollectionViewModel
	: public TSharedFromThis<FClanCollectionViewModel>
{
public:
	virtual ~FClanCollectionViewModel() {}

	virtual FText GetClanTitle() = 0;

	virtual void EnumerateJoinedClans(TArray<TSharedRef<class FClanInfoViewModel>>& OUTClanList) = 0;

	virtual int32 NumberOfJoinedClans() const = 0;

	virtual int32 NumberOfClanInvites() const = 0;

	DECLARE_EVENT_OneParam(FClanCollectionViewModel, FOpenClanDetails, const TSharedRef<class FClanInfoViewModel>&)
	virtual FOpenClanDetails& OpenClanDetails() = 0;
};

/**
 * Creates the implementation for an ClanViewModel.
 *
 * @return the newly created ClanViewModel implementation.
 */
FACTORY(TSharedRef< FClanCollectionViewModel >, FClanCollectionViewModel,
	const TSharedRef<class IClanRepository>& ClanRepository,
	const TSharedRef<class IFriendListFactory>& FriendsListFactory,
	const TSharedRef<class FChatSettingsService>& ChatSettingService);