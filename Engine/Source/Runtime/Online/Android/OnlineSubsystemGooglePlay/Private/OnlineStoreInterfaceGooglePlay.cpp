// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "OnlineAsyncTaskGooglePlayQueryInAppPurchases.h"
#include "TaskGraphInterfaces.h"
#include <jni.h>


////////////////////////////////////////////////////////////////////
/// FOnlineStoreGooglePlay implementation


FOnlineStoreGooglePlay::FOnlineStoreGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem)
	: Subsystem( InSubsystem )
	, CurrentQueryTask( nullptr )
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::FOnlineStoreGooglePlay" ));
	
	FString LicenseKey;
	GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("GooglePlayLicenseKey"), LicenseKey, GEngineIni);

	extern void AndroidThunkCpp_Iap_SetupIapService(const FString&);
	AndroidThunkCpp_Iap_SetupIapService(LicenseKey);
}


FOnlineStoreGooglePlay::~FOnlineStoreGooglePlay()
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::~FOnlineStoreGooglePlay" ));
}


bool FOnlineStoreGooglePlay::IsAllowedToMakePurchases()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::IsAllowedToMakePurchases"));

	extern bool AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
	return AndroidThunkCpp_Iap_IsAllowedToMakePurchases();
}


bool FOnlineStoreGooglePlay::QueryForAvailablePurchases(const TArray<FString>& ProductIds, FOnlineProductInformationReadRef& InReadObject)
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::QueryForAvailablePurchases" ));
	
	ReadObject = InReadObject;
	ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;

	TArray<bool> ConsumableFlags;
	ConsumableFlags.AddZeroed(ProductIds.Num());

	CurrentQueryTask = new FOnlineAsyncTaskGooglePlayQueryInAppPurchases(
		Subsystem,
		ProductIds,
		ConsumableFlags);
	Subsystem->QueueAsyncTask(CurrentQueryTask);

	return true;
}


extern "C" void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativeQueryComplete(JNIEnv* jenv, jobject thiz, jboolean bSuccess, jobjectArray productIDs, jobjectArray titles, jobjectArray descriptions, jobjectArray prices)
{
	TArray<FInAppPurchaseProductInfo> ProvidedProductInformation;

	if (jenv && bSuccess)
	{
		jsize NumProducts = jenv->GetArrayLength(productIDs);
		jsize NumTitles = jenv->GetArrayLength(titles);
		jsize NumDescriptions = jenv->GetArrayLength(descriptions);
		jsize NumPrices = jenv->GetArrayLength(prices);

		ensure((NumProducts == NumTitles) && (NumProducts == NumDescriptions) && (NumProducts == NumPrices));

		for (jsize Idx = 0; Idx < NumProducts; Idx++)
		{
			// Build the product information strings.

			FInAppPurchaseProductInfo NewProductInfo;

			jstring NextId = (jstring)jenv->GetObjectArrayElement(productIDs, Idx);
			const char* charsId = jenv->GetStringUTFChars(NextId, 0);
			NewProductInfo.Identifier = FString(UTF8_TO_TCHAR(charsId));
			jenv->ReleaseStringUTFChars(NextId, charsId);
			jenv->DeleteLocalRef(NextId);

			jstring NextTitle = (jstring)jenv->GetObjectArrayElement(titles, Idx);
			const char* charsTitle = jenv->GetStringUTFChars(NextTitle, 0);
			NewProductInfo.DisplayName = FString(UTF8_TO_TCHAR(charsTitle));
			jenv->ReleaseStringUTFChars(NextTitle, charsTitle);
			jenv->DeleteLocalRef(NextTitle);

			jstring NextDesc = (jstring)jenv->GetObjectArrayElement(descriptions, Idx);
			const char* charsDesc = jenv->GetStringUTFChars(NextDesc, 0);
			NewProductInfo.DisplayDescription = FString(UTF8_TO_TCHAR(charsDesc));
			jenv->ReleaseStringUTFChars(NextDesc, charsDesc);
			jenv->DeleteLocalRef(NextDesc);

			jstring NextPrice = (jstring)jenv->GetObjectArrayElement(prices, Idx);
			const char* charsPrice = jenv->GetStringUTFChars(NextPrice, 0);
			NewProductInfo.DisplayPrice = FString(UTF8_TO_TCHAR(charsPrice));
			jenv->ReleaseStringUTFChars(NextPrice, charsPrice);
			jenv->DeleteLocalRef(NextPrice);

			ProvidedProductInformation.Add(NewProductInfo);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\nProduct Identifier: %s, Name: %s, Description: %s, Price: %s\n"),
				*NewProductInfo.Identifier,
				*NewProductInfo.DisplayName,
				*NewProductInfo.DisplayDescription,
				*NewProductInfo.DisplayPrice);
		}
	}


	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessQueryIapResult"), STAT_FSimpleDelegateGraphTask_ProcessQueryIapResult, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get())
			{
				// call store implementation to process query results.
				if (FOnlineStoreGooglePlay* StoreInterface = (FOnlineStoreGooglePlay*)OnlineSub->GetStoreInterface().Get())
				{
					StoreInterface->ProcessQueryAvailablePurchasesResults(bSuccess, ProvidedProductInformation);
				}
			}
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("In-App Purchase query was completed  %s\n"), bSuccess ? TEXT("successfully") : TEXT("unsuccessfully"));
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessQueryIapResult), 
		nullptr, 
		ENamedThreads::GameThread
	);

}


void FOnlineStoreGooglePlay::ProcessQueryAvailablePurchasesResults(bool bInSuccessful, const TArray<FInAppPurchaseProductInfo>& AvailablePurchases)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::ProcessQueryAvailablePurchasesResults"));

	if (ReadObject.IsValid())
	{
		ReadObject->ReadState = bInSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
		ReadObject->ProvidedProductInformation.Insert(AvailablePurchases, 0);
	}

	CurrentQueryTask->ProcessQueryAvailablePurchasesResults(bInSuccessful);
}


bool FOnlineStoreGooglePlay::BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InPurchaseStateObject)
{
	UE_LOG(LogOnline, Display, TEXT( "FOnlineStoreGooglePlay::BeginPurchase" ));
	
	bool bCreatedNewTransaction = false;
	
	if (IsAllowedToMakePurchases())
	{
		CachedPurchaseStateObject = InPurchaseStateObject;

		extern bool AndroidThunkCpp_Iap_BeginPurchase(const FString&, const bool);
		bCreatedNewTransaction = AndroidThunkCpp_Iap_BeginPurchase(ProductRequest.ProductIdentifier, ProductRequest.bIsConsumable);
		UE_LOG(LogOnline, Display, TEXT("Created Transaction? - %s"), 
			bCreatedNewTransaction ? TEXT("Created a transaction.") : TEXT("Failed to create a transaction."));

		if (!bCreatedNewTransaction)
		{
			UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::BeginPurchase - Could not create a new transaction."));
			CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Failed;
			TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::Invalid);
		}
		else
		{
			CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::InProgress;
		}
	}
	else
	{
		UE_LOG(LogOnline, Display, TEXT("This device is not able to make purchases."));

		InPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Failed;
		TriggerOnInAppPurchaseCompleteDelegates(EInAppPurchaseState::NotAllowed);
	}


	return bCreatedNewTransaction;
}


extern "C" void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativePurchaseComplete(JNIEnv* jenv, jobject thiz, jboolean bSuccess, jstring productId, jstring receiptData)
{
	FString ProductId, ReceiptData;
	if (bSuccess)
	{
		const char* charsId = jenv->GetStringUTFChars(productId, 0);
		ProductId = FString(UTF8_TO_TCHAR(charsId));
		jenv->ReleaseStringUTFChars(productId, charsId);

		const char* charsReceipt = jenv->GetStringUTFChars(receiptData, 0);
		ReceiptData = FString(UTF8_TO_TCHAR(charsReceipt));
		jenv->ReleaseStringUTFChars(receiptData, charsReceipt);
	}
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("1... ProductId: %s, ReceiptData: %s\n"), *ProductId, *ReceiptData );

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessIapResult"), STAT_FSimpleDelegateGraphTask_ProcessIapResult, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("In-App Purchase was completed  %s\n"), bSuccess ? TEXT("successfully") : TEXT("unsuccessfully"));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get())
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("2... ProductId: %s, ReceiptData: %s\n"), *ProductId, *ReceiptData);
				// call store implementation to process query results.
				if (FOnlineStoreGooglePlay* StoreInterface = (FOnlineStoreGooglePlay*)OnlineSub->GetStoreInterface().Get())
				{
					StoreInterface->ProcessPurchaseResult(bSuccess, ProductId, ReceiptData);
				}
			}
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessIapResult), 
		nullptr, 
		ENamedThreads::GameThread
	);

}


void FOnlineStoreGooglePlay::ProcessPurchaseResult(bool bInSuccessful, const FString& ProductId, const FString& InReceiptData)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::ProcessPurchaseResult"));
	UE_LOG(LogOnline, Display, TEXT("3... ProductId: %s, ReceiptData: %s\n"), *ProductId, *InReceiptData);


	if (CachedPurchaseStateObject.IsValid())
	{
		FInAppPurchaseProductInfo& ProductInfo = CachedPurchaseStateObject->ProvidedProductInformation;
		ProductInfo.Identifier = ProductId;
		ProductInfo.DisplayName = TEXT("n/a");
		ProductInfo.DisplayDescription = TEXT("n/a");
		ProductInfo.DisplayPrice = TEXT("n/a");
		ProductInfo.ReceiptData = InReceiptData;

		CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Done;
	}

	TriggerOnInAppPurchaseCompleteDelegates(bInSuccessful ? EInAppPurchaseState::Success : EInAppPurchaseState::Failed);
}