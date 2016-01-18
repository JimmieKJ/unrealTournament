// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Enum holding the display list selection
namespace EClanDisplayLists
{
	enum Type : uint8
	{
		DefaultDisplay,
		ClanRequestsDisplay,			// Clan request display
		MAX_None
	};

	inline const FText ToFText(EClanDisplayLists::Type EnumVal)
	{
		static const FText ClanList = NSLOCTEXT("ClanListTypes", "ClanList", "Clans");
		static const FText ClanRequestList = NSLOCTEXT("ClanListTypes", "InvitesList", "Invitations");

		switch (EnumVal)
		{
		case DefaultDisplay: return ClanList;
		case ClanRequestsDisplay: return ClanRequestList;
		}

		return FText::GetEmpty();
	}
};

class FClanListViewModel
	: public TSharedFromThis<FClanListViewModel>
{
public:
	virtual ~FClanListViewModel() {}
	virtual const TArray<TSharedRef<class FClanInfoViewModel>>& GetClanList() const = 0;
	virtual FText GetListCountText() const = 0;
	virtual int32 GetListCount() const = 0;
	virtual const FText GetListName() const = 0;
	virtual const EClanDisplayLists::Type GetListType() const = 0;
	virtual EVisibility GetListVisibility() const = 0;

	DECLARE_EVENT(FClanListViewModel, FClanListUpdated)
	virtual FClanListUpdated& OnClanListUpdated() = 0;
};

/**
 * Creates the implementation for an ClanListViewModel.
 *
 * @return the newly created FClanListViewModel implementation.
 */
FACTORY(TSharedRef< FClanListViewModel >, FClanListViewModel,const TSharedRef<class FClanCollectionViewModel>& ClanViewModel, EClanDisplayLists::Type ListType);