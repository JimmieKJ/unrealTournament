// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"

//////////////////////////////////////////////////////////////////////////
// UInAppPurchaseQueryCallbackProxy

UInAppPurchaseQueryCallbackProxy::UInAppPurchaseQueryCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInAppPurchaseQueryCallbackProxy::TriggerQuery(APlayerController* PlayerController, const TArray<FString>& ProductIdentifiers)
{
	bFailedToEvenSubmit = true;

	WorldPtr = (PlayerController != nullptr) ? PlayerController->GetWorld() : nullptr;
	if (APlayerState* PlayerState = (PlayerController != NULL) ? PlayerController->PlayerState : nullptr)
	{
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get())
		{
			IOnlineStorePtr StoreInterface = OnlineSub->GetStoreInterface();
			if (StoreInterface.IsValid())
			{
				bFailedToEvenSubmit = false;

				// Register the completion callback
				InAppPurchaseReadCompleteDelegate = FOnQueryForAvailablePurchasesCompleteDelegate::CreateUObject(this, &UInAppPurchaseQueryCallbackProxy::OnInAppPurchaseRead);
				StoreInterface->AddOnQueryForAvailablePurchasesCompleteDelegate(InAppPurchaseReadCompleteDelegate);


				ReadObject = MakeShareable(new FOnlineProductInformationRead());
				FOnlineProductInformationReadRef ReadObjectRef = ReadObject.ToSharedRef();
				StoreInterface->QueryForAvailablePurchases(ProductIdentifiers, ReadObjectRef);
			}
			else
			{
				FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseQueryCallbackProxy::TriggerQuery - In App Purchases are not supported by Online Subsystem"), ELogVerbosity::Warning);
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseQueryCallbackProxy::TriggerQuery - Invalid or uninitialized OnlineSubsystem"), ELogVerbosity::Warning);
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseQueryCallbackProxy::TriggerQuery - Invalid player state"), ELogVerbosity::Warning);
	}

	if (bFailedToEvenSubmit && (PlayerController != NULL))
	{
		OnInAppPurchaseRead(false);
	}
}

void UInAppPurchaseQueryCallbackProxy::OnInAppPurchaseRead(bool bWasSuccessful)
{
	RemoveDelegate();

	if (bWasSuccessful && ReadObject.IsValid())
	{
		SavedProductInformation = ReadObject->ProvidedProductInformation;
		bSavedWasSuccessful = true;
	}
	else
	{
		bSavedWasSuccessful = false;
	}

	if (UWorld* World = WorldPtr.Get())
	{
		// Use a local timer handle as we don't need to store it for later but we don't need to look for something to clear
		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimer(this, &UInAppPurchaseQueryCallbackProxy::OnInAppPurchaseRead_Delayed, 0.001f, false);
	}

	ReadObject = NULL;
}

void UInAppPurchaseQueryCallbackProxy::OnInAppPurchaseRead_Delayed()
{
	if (bSavedWasSuccessful)
	{
		OnSuccess.Broadcast(SavedProductInformation);
	}
	else
	{
		OnFailure.Broadcast(SavedProductInformation);
	}
}

void UInAppPurchaseQueryCallbackProxy::RemoveDelegate()
{
	if (!bFailedToEvenSubmit)
	{
		if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get())
		{
			IOnlineStorePtr InAppPurchases = OnlineSub->GetStoreInterface();
			if (InAppPurchases.IsValid())
			{
				InAppPurchases->ClearOnQueryForAvailablePurchasesCompleteDelegate(InAppPurchaseReadCompleteDelegate);
			}
		}
	}
}

void UInAppPurchaseQueryCallbackProxy::BeginDestroy()
{
	ReadObject = NULL;
	RemoveDelegate();

	Super::BeginDestroy();
}

UInAppPurchaseQueryCallbackProxy* UInAppPurchaseQueryCallbackProxy::CreateProxyObjectForInAppPurchaseQuery(class APlayerController* PlayerController, const TArray<FString>& ProductIdentifiers)
{
	UInAppPurchaseQueryCallbackProxy* Proxy = NewObject<UInAppPurchaseQueryCallbackProxy>();
	Proxy->TriggerQuery(PlayerController, ProductIdentifiers);
	return Proxy;
}
