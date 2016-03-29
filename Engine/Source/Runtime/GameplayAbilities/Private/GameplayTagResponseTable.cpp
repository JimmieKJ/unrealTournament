// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagResponseTable.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayTagReponseTable
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


UGameplayTagReponseTable::UGameplayTagReponseTable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Make an empty tag query. We will actually fill the tag out prior to evaluating the query with ::MakeQuery
	FGameplayTagContainer Container;
	Container.AddTagFast(FGameplayTag());
	Query = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(Container);
}

void UGameplayTagReponseTable::RegisterResponseForEvents(UAbilitySystemComponent* ASC)
{
	if (RegisteredASCs.Contains(ASC))
	{
		return;
	}

	TArray<FGameplayTagResponseAppliedInfo> AppliedInfoList;
	AppliedInfoList.SetNum(Entries.Num());

	RegisteredASCs.Add(ASC, AppliedInfoList );

	for (int32 idx=0; idx < Entries.Num(); ++idx)
	{
		const FGameplayTagResponseTableEntry& Entry = Entries[idx];
		if (Entry.Positive.Tag.IsValid())
		{
			ASC->RegisterGameplayTagEvent( Entry.Positive.Tag, EGameplayTagEventType::AnyCountChange ).AddUObject(this, &UGameplayTagReponseTable::TagResponseEvent, ASC, idx);
		}
		if (Entry.Negative.Tag.IsValid())
		{
			ASC->RegisterGameplayTagEvent( Entry.Negative.Tag, EGameplayTagEventType::AnyCountChange ).AddUObject(this, &UGameplayTagReponseTable::TagResponseEvent, ASC, idx);
		}
	}
}

void UGameplayTagReponseTable::TagResponseEvent(const FGameplayTag Tag, int32 NewCount, UAbilitySystemComponent* ASC, int32 idx)
{
	if (!ensure(Entries.IsValidIndex(idx)))
	{
		return;
	}

	const FGameplayTagResponseTableEntry& Entry = Entries[idx];
	int32 TotalCount = 0;

	{
		QUICK_SCOPE_CYCLE_COUNTER(ABILITY_TRT_CALC_COUNT);

		int32 Positive = GetCount(Entry.Positive, ASC);
		int32 Negative = GetCount(Entry.Negative, ASC);

		TotalCount = Positive - Negative;
	}

	TArray<FGameplayTagResponseAppliedInfo>& InfoList = RegisteredASCs.FindChecked(ASC);
	FGameplayTagResponseAppliedInfo& Info = InfoList[idx];

	if (TotalCount < 0)
	{
		Remove(ASC, Info.PositiveHandle);
		AddOrUpdate(ASC, Entry.Negative.ResponseGameplayEffect, TotalCount, Info.NegativeHandle);
	}
	else if (TotalCount > 0)
	{
		Remove(ASC, Info.NegativeHandle);
		AddOrUpdate(ASC, Entry.Positive.ResponseGameplayEffect, TotalCount, Info.PositiveHandle);
	}
	else if (TotalCount == 0)
	{
		Remove(ASC, Info.PositiveHandle);
		Remove(ASC, Info.NegativeHandle);
	}
}

int32 UGameplayTagReponseTable::GetCount(const FGameplayTagReponsePair& Pair, UAbilitySystemComponent* ASC) const
{
	int32 Count=0;
	if (Pair.Tag.IsValid())
	{
		Count = ASC->GetAggregatedStackCount(MakeQuery(Pair.Tag));
		if (Pair.SoftCountCap > 0)
		{
			Count = FMath::Min<int32>(Pair.SoftCountCap, Count);
		}
	}

	return Count;
}

void UGameplayTagReponseTable::Remove(UAbilitySystemComponent* ASC, FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(Handle);
		Handle.Invalidate();
	}
}

void UGameplayTagReponseTable::AddOrUpdate(UAbilitySystemComponent* ASC, const TSubclassOf<UGameplayEffect>& ResponseGameplayEffect, int32 TotalCount, FActiveGameplayEffectHandle& Handle)
{
	if (ResponseGameplayEffect)
	{
		if (Handle.IsValid())
		{
			ASC->SetActiveGameplayEffectLevel(Handle, TotalCount);
		}
		else
		{
			Handle = ASC->ApplyGameplayEffectToSelf(Cast<UGameplayEffect>(ResponseGameplayEffect->ClassDefaultObject), TotalCount, ASC->MakeEffectContext());
		}
	}
}
