// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemUtilsPrivatePCH.h"
#include "InAppPurchaseCallbackProxy.h"

//////////////////////////////////////////////////////////////////////////
// UInAppPurchaseCallbackProxy

UInAppPurchaseCallbackProxy::UInAppPurchaseCallbackProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PurchaseRequest = nullptr;
	WorldPtr = nullptr;
}


void UInAppPurchaseCallbackProxy::Trigger(APlayerController* PlayerController, const FInAppPurchaseProductRequest& ProductRequest)
{
	bFailedToEvenSubmit = true;

	WorldPtr = (PlayerController != nullptr) ? PlayerController->GetWorld() : nullptr;
	if (APlayerState* PlayerState = (PlayerController != nullptr) ? PlayerController->PlayerState : nullptr)
	{
		if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::IsLoaded() ? IOnlineSubsystem::Get() : nullptr)
		{
			IOnlineStorePtr StoreInterface = OnlineSub->GetStoreInterface();
			if (StoreInterface.IsValid())
			{
				bFailedToEvenSubmit = false;

				// Register the completion callback
				InAppPurchaseCompleteDelegate = FOnInAppPurchaseCompleteDelegate::CreateUObject(this, &UInAppPurchaseCallbackProxy::OnInAppPurchaseComplete);
				InAppPurchaseCompleteDelegateHandle = StoreInterface->AddOnInAppPurchaseCompleteDelegate_Handle(InAppPurchaseCompleteDelegate);

				// Set-up, and trigger the transaction through the store interface
				PurchaseRequest = MakeShareable(new FOnlineInAppPurchaseTransaction());
				FOnlineInAppPurchaseTransactionRef PurchaseRequestRef = PurchaseRequest.ToSharedRef();
				StoreInterface->BeginPurchase(ProductRequest, PurchaseRequestRef);
			}
			else
			{
				FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseCallbackProxy::Trigger - In-App Purchases are not supported by Online Subsystem"), ELogVerbosity::Warning);
			}
		}
		else
		{
			FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseCallbackProxy::Trigger - Invalid or uninitialized OnlineSubsystem"), ELogVerbosity::Warning);
		}
	}
	else
	{
		FFrame::KismetExecutionMessage(TEXT("UInAppPurchaseCallbackProxy::Trigger - Invalid player state"), ELogVerbosity::Warning);
	}

	if (bFailedToEvenSubmit && (PlayerController != NULL))
	{
		OnInAppPurchaseComplete(EInAppPurchaseState::Failed);
	}
}


void UInAppPurchaseCallbackProxy::OnInAppPurchaseComplete(EInAppPurchaseState::Type CompletionState)
{
	RemoveDelegate();
	SavedPurchaseState = CompletionState;
    
	if (UWorld* World = WorldPtr.Get())
	{
		DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.DelayInAppPurchaseComplete"), STAT_FSimpleDelegateGraphTask_DelayInAppPurchaseComplete, STATGROUP_TaskGraphTasks);

		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
			FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){

				OnInAppPurchaseComplete_Delayed();

			}),
			GET_STATID(STAT_FSimpleDelegateGraphTask_DelayInAppPurchaseComplete), 
			nullptr, 
			ENamedThreads::GameThread
		);
    }
    else
    {
        PurchaseRequest = nullptr;
    }
}


void UInAppPurchaseCallbackProxy::OnInAppPurchaseComplete_Delayed()
{
    /** Cached product details of the purchased product */
    FInAppPurchaseProductInfo ProductInformation; 

    if (SavedPurchaseState == EInAppPurchaseState::Success && PurchaseRequest.IsValid())
    {
        ProductInformation = PurchaseRequest->ProvidedProductInformation;
    }
    
	if (SavedPurchaseState == EInAppPurchaseState::Success)
	{
		OnSuccess.Broadcast(SavedPurchaseState, ProductInformation);
	}
	else
	{
		OnFailure.Broadcast(SavedPurchaseState, ProductInformation);
	}
    
    PurchaseRequest = nullptr;
}


void UInAppPurchaseCallbackProxy::RemoveDelegate()
{
	if (!bFailedToEvenSubmit)
	{
		if (IOnlineSubsystem* OnlineSub = IOnlineSubsystem::IsLoaded() ? IOnlineSubsystem::Get() : nullptr)
		{
			IOnlineStorePtr InAppPurchases = OnlineSub->GetStoreInterface();
			if (InAppPurchases.IsValid())
			{
				InAppPurchases->ClearOnInAppPurchaseCompleteDelegate_Handle(InAppPurchaseCompleteDelegateHandle);
			}
		}
	}
}


void UInAppPurchaseCallbackProxy::BeginDestroy()
{
	PurchaseRequest = nullptr;
	RemoveDelegate();

	Super::BeginDestroy();
}


UInAppPurchaseCallbackProxy* UInAppPurchaseCallbackProxy::CreateProxyObjectForInAppPurchase(class APlayerController* PlayerController, const FInAppPurchaseProductRequest& ProductRequest)
{
	UInAppPurchaseCallbackProxy* Proxy = NewObject<UInAppPurchaseCallbackProxy>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->Trigger(PlayerController, ProductRequest);
	return Proxy;
}
