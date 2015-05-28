// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayPrediction.h"

bool FPredictionKey::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// Read bit for server initiated first
	uint8 ServerInitiatedByte = bIsServerInitiated;
	Ar.SerializeBits(&ServerInitiatedByte, 1);
	bIsServerInitiated = ServerInitiatedByte & 1;

	if (Ar.IsLoading())
	{
		Ar << Current;
		if (Current > 0)
		{
			Ar << Base;
		}
		if (!bIsServerInitiated)
		{
			PredictiveConnection = Map;
		}
	}
	else
	{
		/**
		 *	Only serialize the payload if we have no owning connection (Client sending to server)
		 *	or if the owning connection is this connection (Server only sends the prediction key to the client who gave it to us)
		 *  or if this is a server initiated key (valid on all connections)
		 */
		
		if (PredictiveConnection == nullptr || (Map == PredictiveConnection) || bIsServerInitiated)
		{
			Ar << Current;
			if (Current > 0)
			{
				Ar << Base;
			}
		}
		else
		{
			int16 Payload = 0;
			Ar << Payload;
		}
	}

	bOutSuccess = true;
	return true;
}

void FPredictionKey::GenerateNewPredictionKey()
{
	static KeyType GKey = 1;
	Current = GKey++;
	bIsStale = false;
}

void FPredictionKey::GenerateDependentPredictionKey()
{
	if (bIsServerInitiated)
	{
		// Can't have dependent keys on server keys, use same key
		return;
	}

	KeyType Previous = 0;
	if (Base == 0)
	{
		Base = Current;
	}
	else
	{
		Previous = Current;
	}

	GenerateNewPredictionKey();

	if (Previous > 0)
	{
		FPredictionKeyDelegates::AddDependency(Current, Previous);
	}
}

FPredictionKey FPredictionKey::CreateNewPredictionKey(UAbilitySystemComponent* OwningComponent)
{
	FPredictionKey NewKey;
	
	// We should never generate prediction keys on the authority
	if(OwningComponent->GetOwnerRole() != ROLE_Authority)
	{
		NewKey.GenerateNewPredictionKey();
	}
	return NewKey;
}

FPredictionKey FPredictionKey::CreateNewServerInitiatedKey(UAbilitySystemComponent* OwningComponent)
{
	FPredictionKey NewKey;

	// Only valid on the server
	if (OwningComponent->GetOwnerRole() == ROLE_Authority)
	{
		NewKey.GenerateNewPredictionKey();
		NewKey.bIsServerInitiated = true;
	}
	return NewKey;
}


FPredictionKeyEvent& FPredictionKey::NewRejectedDelegate()
{
	return FPredictionKeyDelegates::NewRejectedDelegate(Current);
}

FPredictionKeyEvent& FPredictionKey::NewCaughtUpDelegate()
{
	return FPredictionKeyDelegates::NewCaughtUpDelegate(Current);
}

void FPredictionKey::NewRejectOrCaughtUpDelegate(FPredictionKeyEvent Event)
{
	FPredictionKeyDelegates::NewRejectOrCaughtUpDelegate(Current, Event);
}

// -------------------------------------

FPredictionKeyDelegates& FPredictionKeyDelegates::Get()
{
	static FPredictionKeyDelegates StaticMap;
	return StaticMap;
}

FPredictionKeyEvent& FPredictionKeyDelegates::NewRejectedDelegate(FPredictionKey::KeyType Key)
{
	TArray<FPredictionKeyEvent>& DelegateList = Get().DelegateMap.FindOrAdd(Key).RejectedDelegates;
	DelegateList.Add(FPredictionKeyEvent());
	return DelegateList.Top();
}

FPredictionKeyEvent& FPredictionKeyDelegates::NewCaughtUpDelegate(FPredictionKey::KeyType Key)
{
	TArray<FPredictionKeyEvent>& DelegateList = Get().DelegateMap.FindOrAdd(Key).CaughtUpDelegates;
	DelegateList.Add(FPredictionKeyEvent());
	return DelegateList.Top();
}

void FPredictionKeyDelegates::NewRejectOrCaughtUpDelegate(FPredictionKey::KeyType Key, FPredictionKeyEvent NewEvent)
{
	FDelegates& Delegates = Get().DelegateMap.FindOrAdd(Key);
	Delegates.CaughtUpDelegates.Add(NewEvent);
	Delegates.RejectedDelegates.Add(NewEvent);
}

void FPredictionKeyDelegates::BroadcastRejectedDelegate(FPredictionKey::KeyType Key)
{
	TArray<FPredictionKeyEvent>& DelegateList = Get().DelegateMap.FindOrAdd(Key).RejectedDelegates;
	for (auto Delegate : DelegateList)
	{
		Delegate.ExecuteIfBound();
	}
}

void FPredictionKeyDelegates::BroadcastCaughtUpDelegate(FPredictionKey::KeyType Key)
{
	TArray<FPredictionKeyEvent>& DelegateList = Get().DelegateMap.FindOrAdd(Key).CaughtUpDelegates;
	for (auto Delegate : DelegateList)
	{
		Delegate.ExecuteIfBound();
	}
}

void FPredictionKeyDelegates::Reject(FPredictionKey::KeyType Key)
{
	FDelegates* DelPtr = Get().DelegateMap.Find(Key);
	if (DelPtr)
	{
		for (auto Delegate : DelPtr->RejectedDelegates)
		{
			Delegate.ExecuteIfBound();
		}

		Get().DelegateMap.Remove(Key);
	}
}

void FPredictionKeyDelegates::CatchUpTo(FPredictionKey::KeyType Key)
{
	for (auto MapIt = Get().DelegateMap.CreateIterator(); MapIt; ++MapIt)
	{
		if (MapIt.Key() <= Key)
		{		
			for (auto Delegate : MapIt.Value().CaughtUpDelegates)
			{
				Delegate.ExecuteIfBound();
			}
			
			MapIt.RemoveCurrent();
		}
	}
}

void FPredictionKeyDelegates::CaughtUp(FPredictionKey::KeyType Key)
{
	FDelegates* DelPtr = Get().DelegateMap.Find(Key);
	if (DelPtr)
	{
		for (auto Delegate : DelPtr->CaughtUpDelegates)
		{
			Delegate.ExecuteIfBound();
		}
		Get().DelegateMap.Remove(Key);
	}
}

void FPredictionKeyDelegates::AddDependency(FPredictionKey::KeyType ThisKey, FPredictionKey::KeyType DependsOn)
{
	NewRejectedDelegate(DependsOn).BindStatic(&FPredictionKeyDelegates::Reject, ThisKey);
	NewCaughtUpDelegate(DependsOn).BindStatic(&FPredictionKeyDelegates::CaughtUp, ThisKey);
}

// -------------------------------------

FScopedPredictionWindow::FScopedPredictionWindow(UAbilitySystemComponent* AbilitySystemComponent, FPredictionKey InPredictionKey)
{
	if (!ensure(AbilitySystemComponent != NULL))
	{
		return;
	}

	// This is used to set an already generated prediction key as the current scoped prediction key.
	// Should be called on the server for logical scopes where a given key is valid. E.g, "client gave me this key, we both are going to run Foo()".
	
	if (AbilitySystemComponent->IsNetSimulating() == false)
	{
		Owner = AbilitySystemComponent;
		check(Owner.IsValid());
		Owner->ScopedPredictionKey = InPredictionKey;
		ClearScopedPredictionKey = true;
		SetReplicatedPredictionKey = true;
	}
}

FScopedPredictionWindow::FScopedPredictionWindow(UAbilitySystemComponent* InAbilitySystemComponent, bool bCanGenerateNewKey)
{
	// On the server, this will do nothing since he is authoritative and doesn't need a prediction key for anything.
	// On the client, this will generate a new prediction key if bCanGenerateNewKey is true, and we have a invalid prediction key.

	ClearScopedPredictionKey = false;
	SetReplicatedPredictionKey = false;
	Owner = InAbilitySystemComponent;

	if (!ensure(Owner.IsValid()) || Owner->IsNetSimulating() == false)
	{
		return;
	}
	
	// InAbilitySystemComponent->GetPredictionKey().IsValidForMorePrediction() == false && 
	if (bCanGenerateNewKey)
	{
		check(InAbilitySystemComponent != NULL); // Should have bailed above with ensure(Owner.IsValid())
		ClearScopedPredictionKey = true;
		RestoreKey = InAbilitySystemComponent->ScopedPredictionKey;
		InAbilitySystemComponent->ScopedPredictionKey.GenerateDependentPredictionKey();
		
	}
}

FScopedPredictionWindow::~FScopedPredictionWindow()
{
	if (Owner.IsValid())
	{
		if (SetReplicatedPredictionKey)
		{
			// It is important to not set the ReplicatedPredictionKey unless it is valid (>0).
			// If we werent given a new prediction key for this scope from the client, then setting the
			// replicated prediction key back to 0 could cause OnReps to be missed on the client during high PL.
			// (for example, predict w/ key 100 -> prediction key replication dropped -> predict w/ invalid key -> next rep of prediction key is 0).
			if (Owner->ScopedPredictionKey.IsValidKey())
			{
				Owner->ReplicatedPredictionKey = Owner->ScopedPredictionKey;
			}
		}
		if (ClearScopedPredictionKey)
		{
			Owner->ScopedPredictionKey = RestoreKey;
		}
	}
}
