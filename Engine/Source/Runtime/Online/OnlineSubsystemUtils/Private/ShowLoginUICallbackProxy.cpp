// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "Classes/ShowLoginUICallbackProxy.h"
#include "OnlineExternalUIInterface.h"

UShowLoginUICallbackProxy::UShowLoginUICallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WorldContextObject(nullptr)
{
}

UShowLoginUICallbackProxy* UShowLoginUICallbackProxy::ShowExternalLoginUI(UObject* WorldContextObject, class APlayerController* InPlayerController)
{
	UShowLoginUICallbackProxy* Proxy = NewObject<UShowLoginUICallbackProxy>();
	Proxy->PlayerControllerWeakPtr = InPlayerController;
	Proxy->WorldContextObject = WorldContextObject;
	return Proxy;
}

void UShowLoginUICallbackProxy::Activate()
{
	if (!PlayerControllerWeakPtr.IsValid())
	{
		FFrame::KismetExecutionMessage(TEXT("A player controller must be provided in order to show the external login UI."), ELogVerbosity::Warning);
		OnFailure.Broadcast(PlayerControllerWeakPtr.Get());
		return;
	}

	FOnlineSubsystemBPCallHelper Helper(TEXT("ShowLoginUI"), GEngine->GetWorldFromContextObject(WorldContextObject));

	if (Helper.OnlineSub == nullptr)
	{
		OnFailure.Broadcast(PlayerControllerWeakPtr.Get());
		return;
	}
		
	IOnlineExternalUIPtr OnlineExternalUI = Helper.OnlineSub->GetExternalUIInterface();
	if (!OnlineExternalUI.IsValid())
	{
		FFrame::KismetExecutionMessage(TEXT("External UI not supported by the current online subsystem"), ELogVerbosity::Warning);
		OnFailure.Broadcast(PlayerControllerWeakPtr.Get());
		return;
	}

	const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerControllerWeakPtr->Player);

	if (LocalPlayer == nullptr)
	{
		FFrame::KismetExecutionMessage(TEXT("Can only show login UI for local players"), ELogVerbosity::Warning);
		OnFailure.Broadcast(PlayerControllerWeakPtr.Get());
		return;
	}
		
	const bool bWaitForDelegate = OnlineExternalUI->ShowLoginUI(LocalPlayer->GetControllerId(), false,
		FOnLoginUIClosedDelegate::CreateUObject(this, &UShowLoginUICallbackProxy::OnShowLoginUICompleted));

	if (!bWaitForDelegate)
	{
		FFrame::KismetExecutionMessage(TEXT("The online subsystem couldn't show its login UI"), ELogVerbosity::Log);
		OnFailure.Broadcast(PlayerControllerWeakPtr.Get());
	}
}

void UShowLoginUICallbackProxy::OnShowLoginUICompleted(TSharedPtr<const FUniqueNetId> UniqueId, int LocalPlayerNum)
{
	// Update the cached unique ID for the local player and the player state.
	APlayerController* PlayerController = PlayerControllerWeakPtr.Get();
	
	if (PlayerController != nullptr)
	{
		ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
		if (LocalPlayer != nullptr)
		{
			LocalPlayer->SetCachedUniqueNetId(UniqueId);
		}
		
		if (PlayerController->PlayerState != nullptr)
		{
			PlayerController->PlayerState->SetUniqueId(UniqueId);
		}
	}

	if (UniqueId.IsValid())
	{
		OnSuccess.Broadcast(PlayerControllerWeakPtr.Get());
	}
	else
	{
		OnFailure.Broadcast(PlayerControllerWeakPtr.Get());
	}
}
