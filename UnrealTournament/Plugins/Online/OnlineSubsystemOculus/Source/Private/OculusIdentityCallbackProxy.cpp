// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "OnlineSubsystemOculusPrivatePCH.h"
#include "Online.h"
#include "OnlineIdentityOculus.h"

#include "OculusIdentityCallbackProxy.h"

UOculusIdentityCallbackProxy::UOculusIdentityCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UOculusIdentityCallbackProxy* UOculusIdentityCallbackProxy::GetOculusIdentity(int32 LocalUserNum)
{
	UOculusIdentityCallbackProxy* Proxy = NewObject<UOculusIdentityCallbackProxy>();
	Proxy->LocalUserNum = LocalUserNum;
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->Activate();
	return Proxy;
}

void UOculusIdentityCallbackProxy::Activate()
{
	auto OculusIdentityInterface = Online::GetIdentityInterface(OCULUS_SUBSYSTEM);

	if (OculusIdentityInterface.IsValid())
	{
		DelegateHandle = Online::GetIdentityInterface()->AddOnLoginCompleteDelegate_Handle(
			0, 
			FOnLoginCompleteDelegate::CreateUObject(this, &UOculusIdentityCallbackProxy::OnLoginCompleteDelegate)
		);
		OculusIdentityInterface->AutoLogin(LocalUserNum);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Oculus platform service not available to get the Oculus ID."));
		OnFailure.Broadcast();
	}
}

void UOculusIdentityCallbackProxy::OnLoginCompleteDelegate(int32 Unused, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& ErrorStr)
{
	Online::GetIdentityInterface()->ClearOnLoginCompleteDelegate_Handle(LocalUserNum, DelegateHandle);
	if (bWasSuccessful)
	{
    auto PlayerNickName = Online::GetIdentityInterface()->GetPlayerNickname(LocalUserNum);
		OnSuccess.Broadcast(UserId.ToString(), PlayerNickName);
	}
	else
	{
		OnFailure.Broadcast();
	}
}

