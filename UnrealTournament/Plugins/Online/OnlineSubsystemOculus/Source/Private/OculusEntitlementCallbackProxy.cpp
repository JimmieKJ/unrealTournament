// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusPrivatePCH.h"
#include "Online.h"
#include "OnlineIdentityOculus.h"

#include "OculusEntitlementCallbackProxy.h"

UOculusEntitlementCallbackProxy::UOculusEntitlementCallbackProxy(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

UOculusEntitlementCallbackProxy* UOculusEntitlementCallbackProxy::VerifyEntitlement()
{
    UOculusEntitlementCallbackProxy* Proxy = NewObject<UOculusEntitlementCallbackProxy>();
    Proxy->SetFlags(RF_StrongRefOnFrame);
    Proxy->Activate();
    return Proxy;
}

void UOculusEntitlementCallbackProxy::Activate()
{
    auto OculusIdentityInterface = Online::GetIdentityInterface(OCULUS_SUBSYSTEM);

    if (OculusIdentityInterface.IsValid())
    {
        auto Unused = new FUniqueNetIdString("UNUSED");
        OculusIdentityInterface->GetUserPrivilege(
            *Unused,
            EUserPrivileges::CanPlay,
            IOnlineIdentity::FOnGetUserPrivilegeCompleteDelegate::CreateUObject(this, &UOculusEntitlementCallbackProxy::OnUserPrivilegeCompleteDelegate)
        );
    }
    else
    {
        UE_LOG_ONLINE(Warning, TEXT("Oculus platform service not available. Skipping entitlement check."));
        OnFailure.Broadcast();
    }
}

void UOculusEntitlementCallbackProxy::OnUserPrivilegeCompleteDelegate(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, uint32 Result)
{
    if (Result == (uint32)IOnlineIdentity::EPrivilegeResults::NoFailures)
    {
        OnSuccess.Broadcast();
    }
    else
    {
        OnFailure.Broadcast();
    }
}

