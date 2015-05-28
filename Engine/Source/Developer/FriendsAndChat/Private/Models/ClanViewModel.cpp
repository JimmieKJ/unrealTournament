// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FriendsAndChatPrivatePCH.h"
#include "ClanViewModel.h"
#include "ClanInfoViewModel.h"
#include "ClanRepository.h"
#include "ClanInfo.h"

class FClanViewModelImpl
	: public FClanViewModel
{
public:

	virtual FText GetClanTitle() override
	{
		if(ClanList.Num() > 0)
		{
			return ClanList[0]->GetTitle();
		}
		return FText::GetEmpty();
	}

	virtual TSharedPtr<FClanInfoViewModel> GetClanInfoViewModel() override
	{
		if(ClanList.Num() > 0)
		{
			return FClanInfoViewModelFactory::Create(ClanList[0]);
		}
		return nullptr;
	}


	virtual void EnumerateJoinedClans(TArray<TSharedRef<IClanInfo>>& OUTClanList) override
	{
		for (const auto& ClanItem : ClanList)
		{
			OUTClanList.Add(ClanItem);
		}
	}

private:
	void Initialize()
	{
		ClanRepository->EnumerateClanList(ClanList);
	}

private:
	FClanViewModelImpl(const TSharedRef<IClanRepository>& InClanRepository)
		: ClanRepository(InClanRepository)
	{
	}

	TSharedRef<IClanRepository> ClanRepository;
	TArray<TSharedRef<IClanInfo>> ClanList;
	friend FClanViewModelFactory;
};

TSharedRef< FClanViewModel > FClanViewModelFactory::Create(
	const TSharedRef<IClanRepository>& ClanRepository
	)
{
	TSharedRef< FClanViewModelImpl > ViewModel(new FClanViewModelImpl(ClanRepository));
	ViewModel->Initialize();
	return ViewModel;
}