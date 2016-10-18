// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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


extern "C" void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativeQueryComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jobjectArray productIDs, jobjectArray titles, jobjectArray descriptions, jobjectArray prices, jfloatArray pricesRaw, jobjectArray currencyCodes)
{
	TArray<FInAppPurchaseProductInfo> ProvidedProductInformation;
	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)responseCode;
	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);

	if (jenv && bWasSuccessful)
	{
		jsize NumProducts = jenv->GetArrayLength(productIDs);
		jsize NumTitles = jenv->GetArrayLength(titles);
		jsize NumDescriptions = jenv->GetArrayLength(descriptions);
		jsize NumPrices = jenv->GetArrayLength(prices);
		jsize NumPricesRaw = jenv->GetArrayLength(pricesRaw);
		jsize NumCurrencyCodes = jenv->GetArrayLength(currencyCodes);

		ensure((NumProducts == NumTitles) && (NumProducts == NumDescriptions) && (NumProducts == NumPrices) && (NumProducts == NumPricesRaw) && (NumProducts == NumCurrencyCodes));

		jfloat *PricesRaw = jenv->GetFloatArrayElements(pricesRaw, 0);

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

			NewProductInfo.RawPrice = PricesRaw[Idx];

			jstring NextCurrencyCode = (jstring)jenv->GetObjectArrayElement(currencyCodes, Idx);
			const char* charsCurrencyCodes = jenv->GetStringUTFChars(NextCurrencyCode, 0);
			NewProductInfo.CurrencyCode = FString(UTF8_TO_TCHAR(charsCurrencyCodes));
			jenv->ReleaseStringUTFChars(NextCurrencyCode, charsCurrencyCodes);
			jenv->DeleteLocalRef(NextCurrencyCode);

			ProvidedProductInformation.Add(NewProductInfo);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\nProduct Identifier: %s, Name: %s, Description: %s, Price: %s, Price Raw: %f, Currency Code: %s\n"),
				*NewProductInfo.Identifier,
				*NewProductInfo.DisplayName,
				*NewProductInfo.DisplayDescription,
				*NewProductInfo.DisplayPrice,
				NewProductInfo.RawPrice,
				*NewProductInfo.CurrencyCode);
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
					StoreInterface->ProcessQueryAvailablePurchasesResults(EGPResponse, ProvidedProductInformation);
				}
			}
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("In-App Purchase query was completed  %s\n"), bWasSuccessful ? TEXT("successfully") : TEXT("unsuccessfully"));
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessQueryIapResult), 
		nullptr, 
		ENamedThreads::GameThread
	);

}


void FOnlineStoreGooglePlay::ProcessQueryAvailablePurchasesResults(EGooglePlayBillingResponseCode InResponseCode, const TArray<FInAppPurchaseProductInfo>& AvailablePurchases)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::ProcessQueryAvailablePurchasesResults"));

	if (ReadObject.IsValid())
	{
		ReadObject->ReadState = (InResponseCode == EGooglePlayBillingResponseCode::Ok) ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
		ReadObject->ProvidedProductInformation.Insert(AvailablePurchases, 0);
	}

	CurrentQueryTask->ProcessQueryAvailablePurchasesResults(InResponseCode == EGooglePlayBillingResponseCode::Ok);
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


extern "C" void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativePurchaseComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jstring productId, jstring receiptData, jstring signature)
{
	FString ProductId, ReceiptData, Signature;
	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)responseCode;
	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);

	if (bWasSuccessful)
	{
		const char* charsId = jenv->GetStringUTFChars(productId, 0);
		ProductId = FString(UTF8_TO_TCHAR(charsId));
		jenv->ReleaseStringUTFChars(productId, charsId);

		const char* charsReceipt = jenv->GetStringUTFChars(receiptData, 0);
		ReceiptData = FString(UTF8_TO_TCHAR(charsReceipt));
		jenv->ReleaseStringUTFChars(receiptData, charsReceipt);

		const char* charsSignature = jenv->GetStringUTFChars(signature, 0);
		Signature = FString(UTF8_TO_TCHAR(charsSignature));
		jenv->ReleaseStringUTFChars(signature, charsSignature);
	}
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("1... ProductId: %s, ReceiptData: %s, Signature: %s\n"), *ProductId, *ReceiptData, *Signature );

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessIapResult"), STAT_FSimpleDelegateGraphTask_ProcessIapResult, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=](){
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("In-App Purchase was completed  %s\n"), bWasSuccessful ? TEXT("successfully") : TEXT("unsuccessfully"));
			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get())
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("2... ProductId: %s, ReceiptData: %s, Signature: %s\n"), *ProductId, *ReceiptData, *Signature);
				// call store implementation to process query results.
				if (FOnlineStoreGooglePlay* StoreInterface = (FOnlineStoreGooglePlay*)OnlineSub->GetStoreInterface().Get())
				{
					StoreInterface->ProcessPurchaseResult(EGPResponse, ProductId, ReceiptData, Signature);
				}
			}
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessIapResult), 
		nullptr, 
		ENamedThreads::GameThread
	);

}


void FOnlineStoreGooglePlay::ProcessPurchaseResult(EGooglePlayBillingResponseCode InResponseCode, const FString& ProductId, const FString& InReceiptData, const FString& Signature)
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineStoreGooglePlay::ProcessPurchaseResult"));
	UE_LOG(LogOnline, Display, TEXT("3... ProductId: %s, ReceiptData: %s, Signature: %s\n"), *ProductId, *InReceiptData, *Signature);


	if (CachedPurchaseStateObject.IsValid())
	{
		FInAppPurchaseProductInfo& ProductInfo = CachedPurchaseStateObject->ProvidedProductInformation;
		ProductInfo.Identifier = ProductId;
		ProductInfo.DisplayName = TEXT("n/a");
		ProductInfo.DisplayDescription = TEXT("n/a");
		ProductInfo.DisplayPrice = TEXT("n/a");
		ProductInfo.ReceiptData = InReceiptData;
		ProductInfo.TransactionIdentifier = Signature;

		CachedPurchaseStateObject->ReadState = EOnlineAsyncTaskState::Done;
	}

	TriggerOnInAppPurchaseCompleteDelegates(ConvertGPResponseCodeToIAPState(InResponseCode));
}


bool FOnlineStoreGooglePlay::RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject)
{
	bool bSentAQueryRequest = false;
	CachedPurchaseRestoreObject = InReadObject;

	if (IsAllowedToMakePurchases())
	{
		TArray<FString> ProductIds;
		TArray<bool> IsConsumableFlags;

		for (int i = 0; i < ConsumableProductFlags.Num(); i++)
		{
			ProductIds.Add(ConsumableProductFlags[i].ProductIdentifier);
			IsConsumableFlags.Add(ConsumableProductFlags[i].bIsConsumable);
		}

		// Send JNI request
		extern bool AndroidThunkCpp_Iap_RestorePurchases(const TArray<FString>&, const TArray<bool>&);
		bSentAQueryRequest = AndroidThunkCpp_Iap_RestorePurchases(ProductIds, IsConsumableFlags);
	}
	else
	{
		UE_LOG(LogOnline, Display, TEXT("This device is not able to make purchases."));
		TriggerOnInAppPurchaseRestoreCompleteDelegates(EInAppPurchaseState::NotAllowed);
	}

	return bSentAQueryRequest;
}

extern "C" void Java_com_epicgames_ue4_GooglePlayStoreHelper_nativeRestorePurchasesComplete(JNIEnv* jenv, jobject thiz, jsize responseCode, jobjectArray ProductIDs, jobjectArray ReceiptsData, jobjectArray Signatures)
{
	TArray<FInAppPurchaseRestoreInfo> RestoredPurchaseInfo;

	EGooglePlayBillingResponseCode EGPResponse = (EGooglePlayBillingResponseCode)responseCode;
	bool bWasSuccessful = (EGPResponse == EGooglePlayBillingResponseCode::Ok);

	if (jenv && bWasSuccessful)
	{
		jsize NumProducts = jenv->GetArrayLength(ProductIDs);
		jsize NumReceipts = jenv->GetArrayLength(ReceiptsData);
		jsize NumSignatures = jenv->GetArrayLength(Signatures);

		ensure((NumProducts == NumReceipts) && (NumProducts == NumSignatures));

		for (jsize Idx = 0; Idx < NumProducts; Idx++)
		{
			// Build the restore product information strings.
			FInAppPurchaseRestoreInfo RestoreInfo;

			jstring NextId = (jstring)jenv->GetObjectArrayElement(ProductIDs, Idx);
			const char* charsId = jenv->GetStringUTFChars(NextId, 0);
			RestoreInfo.Identifier = FString(UTF8_TO_TCHAR(charsId));
			jenv->ReleaseStringUTFChars(NextId, charsId);
			jenv->DeleteLocalRef(NextId);

			jstring NextReceipt = (jstring)jenv->GetObjectArrayElement(ReceiptsData, Idx);
			const char* charsReceipt = jenv->GetStringUTFChars(NextReceipt, 0);
			RestoreInfo.ReceiptData = FString(UTF8_TO_TCHAR(charsReceipt));
			jenv->ReleaseStringUTFChars(NextReceipt, charsReceipt);
			jenv->DeleteLocalRef(NextReceipt);

			jstring NextSignature = (jstring)jenv->GetObjectArrayElement(Signatures, Idx);
			const char* charsSignature = jenv->GetStringUTFChars(NextSignature, 0);
			RestoreInfo.TransactionIdentifier = FString(UTF8_TO_TCHAR(charsSignature));
			jenv->ReleaseStringUTFChars(NextSignature, charsSignature);
			jenv->DeleteLocalRef(NextSignature);

			RestoredPurchaseInfo.Add(RestoreInfo);

			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("\nRestored Product Identifier: %s\n"), *RestoreInfo.Identifier);
		}
	}

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.RestorePurchases"), STAT_FSimpleDelegateGraphTask_RestorePurchases, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Restoring In-App Purchases was completed  %s\n"), bWasSuccessful ? TEXT("successfully") : TEXT("unsuccessfully"));

			if (IOnlineSubsystem* const OnlineSub = IOnlineSubsystem::Get())
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Sending result back to OnlineSubsystem.\n"));
				// call store implementation to process query results.
				if (FOnlineStoreGooglePlay* StoreInterface = (FOnlineStoreGooglePlay*)OnlineSub->GetStoreInterface().Get())
				{
					if (StoreInterface->CachedPurchaseRestoreObject.IsValid())
					{
						StoreInterface->CachedPurchaseRestoreObject->ProvidedRestoreInformation = RestoredPurchaseInfo;
						StoreInterface->CachedPurchaseRestoreObject->ReadState = bWasSuccessful ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
					}

					EInAppPurchaseState::Type IAPState = bWasSuccessful ? EInAppPurchaseState::Restored : StoreInterface->ConvertGPResponseCodeToIAPState(EGPResponse);

					StoreInterface->TriggerOnInAppPurchaseRestoreCompleteDelegates(IAPState);
				}
			}
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_RestorePurchases),
		nullptr,
		ENamedThreads::GameThread
	);
}

EInAppPurchaseState::Type FOnlineStoreGooglePlay::ConvertGPResponseCodeToIAPState(const EGooglePlayBillingResponseCode InResponseCode)
{
	switch (InResponseCode)
	{
	case EGooglePlayBillingResponseCode::Ok:
		return EInAppPurchaseState::Success;
	case EGooglePlayBillingResponseCode::UserCancelled:
		return EInAppPurchaseState::Cancelled;
	case EGooglePlayBillingResponseCode::ItemAlreadyOwned:
		return EInAppPurchaseState::AlreadyOwned;
	case EGooglePlayBillingResponseCode::ItemNotOwned:
		return EInAppPurchaseState::NotAllowed;
	case EGooglePlayBillingResponseCode::ServiceUnavailable:
	case EGooglePlayBillingResponseCode::BillingUnavailable:
	case EGooglePlayBillingResponseCode::ItemUnavailable:
	case EGooglePlayBillingResponseCode::DeveloperError:
	case EGooglePlayBillingResponseCode::Error:
	default:
		return EInAppPurchaseState::Failed;
	}
}
