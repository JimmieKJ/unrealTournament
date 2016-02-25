// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPrivatePCH.h"
#include "OnlineSubsystemImpl.h"
#include "NamedInterfaces.h"
#include "OnlineIdentityInterface.h"

const FName FOnlineSubsystemImpl::DefaultInstanceName(TEXT("DefaultInstance"));

FOnlineSubsystemImpl::FOnlineSubsystemImpl() :
	InstanceName(DefaultInstanceName),
	bForceDedicated(false),
	NamedInterfaces(nullptr)
{
}

FOnlineSubsystemImpl::FOnlineSubsystemImpl(FName InInstanceName) :
	InstanceName(InInstanceName),
	bForceDedicated(false),
	NamedInterfaces(nullptr)
{
}

FOnlineSubsystemImpl::~FOnlineSubsystemImpl()
{	
}

bool FOnlineSubsystemImpl::Shutdown()
{
	OnNamedInterfaceCleanup();
	return true;
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
		NamedInterfaces->OnCleanup().AddRaw(this, &FOnlineSubsystemImpl::OnNamedInterfaceCleanup);
		NamedInterfaces->AddToRoot();
	}
}

void FOnlineSubsystemImpl::OnNamedInterfaceCleanup()
{
	if (NamedInterfaces)
	{
		UE_LOG_ONLINE(Display, TEXT("Removing %d named interfaces"), NamedInterfaces->GetNumInterfaces());
		NamedInterfaces->RemoveFromRoot();
		NamedInterfaces->OnCleanup().RemoveAll(this);
		NamedInterfaces = nullptr;
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

	return nullptr;
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
#if WITH_EDITOR
	FName WorldContextHandle = (InstanceName != NAME_None && InstanceName != DefaultInstanceName) ? InstanceName : NAME_None;
	return IsServerForOnlineSubsystems(WorldContextHandle);
#else
	return IsServerForOnlineSubsystems(NAME_None);
#endif
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
				TSharedPtr<const FUniqueNetId> LocalUniqueId = IdentityInt->GetUniquePlayerId(LocalUserNum);
				if (LocalUniqueId.IsValid() && UniqueId == *LocalUniqueId)
				{
					return true;
				}
			}
		}
	}

	return false;
}

