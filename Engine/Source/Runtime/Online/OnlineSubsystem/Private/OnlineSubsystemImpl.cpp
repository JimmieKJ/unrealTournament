// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPrivatePCH.h"
#include "OnlineSubsystemImpl.h"
#include "NamedInterfaces.h"
#include "OnlineIdentityInterface.h"

const FName FOnlineSubsystemImpl::DefaultInstanceName(TEXT("DefaultInstance"));

FOnlineSubsystemImpl::FOnlineSubsystemImpl() :
	InstanceName(DefaultInstanceName),
	bForceDedicated(false),
	NamedInterfaces(NULL)
{
}

FOnlineSubsystemImpl::FOnlineSubsystemImpl(FName InInstanceName) :
	InstanceName(InInstanceName),
	bForceDedicated(false),
	NamedInterfaces(NULL)
{
}

FOnlineSubsystemImpl::~FOnlineSubsystemImpl()
{
	if (NamedInterfaces)
	{
		NamedInterfaces->RemoveFromRoot();
		NamedInterfaces = NULL;
	}
}

void FOnlineSubsystemImpl::ExecuteDelegateNextTick(const FNextTickDelegate& Callback)
{
	NextTickQueue.Enqueue(Callback);
}

bool FOnlineSubsystemImpl::Tick(float DeltaTime)
{
	if (!NextTickQueue.IsEmpty())
	{
		// unload the next-tick queue into our buffer. Any further executes (from within callbacks) will happen NEXT frame (as intended)
		FNextTickDelegate Temp;
		while (NextTickQueue.Dequeue(Temp))
		{
			CurrentTickBuffer.Add(Temp);
		}

		// execute any functions in the current tick array
		for (const auto& Callback : CurrentTickBuffer)
		{
			Callback.ExecuteIfBound();
		}
		CurrentTickBuffer.SetNum(0, false); // keep the memory around
	}
	return true;
}

void FOnlineSubsystemImpl::InitNamedInterfaces()
{
	NamedInterfaces = NewObject<UNamedInterfaces>();
	if (NamedInterfaces)
	{
		NamedInterfaces->Initialize();
		NamedInterfaces->AddToRoot();
	}
}

UObject* FOnlineSubsystemImpl::GetNamedInterface(FName InterfaceName)
{
	if (!NamedInterfaces)
	{
		InitNamedInterfaces();
	}

	if (NamedInterfaces)
	{
		return NamedInterfaces->GetNamedInterface(InterfaceName);
	}

	return NULL;
}

void FOnlineSubsystemImpl::SetNamedInterface(FName InterfaceName, UObject* NewInterface)
{
	if (!NamedInterfaces)
	{
		InitNamedInterfaces();
	}

	if (NamedInterfaces)
	{
		return NamedInterfaces->SetNamedInterface(InterfaceName, NewInterface);
	}
}

bool FOnlineSubsystemImpl::IsServer() const
{
	FName WorldContextHandle = (InstanceName != NAME_None && InstanceName != DefaultInstanceName) ? InstanceName : NAME_None;
	return IsServerForOnlineSubsystems(WorldContextHandle);
}

bool FOnlineSubsystemImpl::IsLocalPlayer(const FUniqueNetId& UniqueId) const
{
	if (!IsDedicated())
	{
		IOnlineIdentityPtr IdentityInt = GetIdentityInterface();
		if (IdentityInt.IsValid())
		{
			for (int32 LocalUserNum = 0; LocalUserNum < MAX_LOCAL_PLAYERS; LocalUserNum++)
			{
				TSharedPtr<FUniqueNetId> LocalUniqueId = IdentityInt->GetUniquePlayerId(LocalUserNum);
				if (LocalUniqueId.IsValid() && UniqueId == *LocalUniqueId)
				{
					return true;
				}
			}
		}
	}

	return false;
}

