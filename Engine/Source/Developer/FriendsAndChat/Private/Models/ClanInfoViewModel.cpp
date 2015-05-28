// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ClanInfoViewModel.h"
#include "ClanInfo.h"
#include "ClanMemberList.h"
#include "FriendListViewModel.h"

class FClanInfoViewModelImpl
	: public FClanInfoViewModel
{
public:

	virtual FText GetClanTitle() override
	{
		return ClanInfo->GetTitle();
	}

	virtual int32 GetMemberCount() override
	{
		return ClanInfo->GetMemberList().Num();
	}

	virtual TSharedRef< FFriendListViewModel > GetFriendListViewModel() override
	{
		return FFriendListViewModelFactory::Create(FClanMemberListFactory::Create(ClanInfo), EFriendsDisplayLists::ClanMemberDisplay);
	}

	virtual FText GetListCountText() const override
	{
		return FText::AsNumber(ClanInfo->GetMemberList().Num());
	}

	virtual FText GetClanBrushName() const override
	{
		return ClanInfo->GetClanBrushName();
	}

private:
	void Initialize()
	{

	}

private:
	FClanInfoViewModelImpl(const TSharedRef<IClanInfo>& InClanInfo)
		: ClanInfo(InClanInfo)
	{
	}

	TSharedRef<IClanInfo> ClanInfo;
	friend FClanInfoViewModelFactory;
};

TSharedRef< FClanInfoViewModel > FClanInfoViewModelFactory::Create(
	const TSharedRef<IClanInfo>& ClanInfo
	)
{
	TSharedRef< FClanInfoViewModelImpl > ViewModel(new FClanInfoViewModelImpl(ClanInfo));
	ViewModel->Initialize();
	return ViewModel;
}