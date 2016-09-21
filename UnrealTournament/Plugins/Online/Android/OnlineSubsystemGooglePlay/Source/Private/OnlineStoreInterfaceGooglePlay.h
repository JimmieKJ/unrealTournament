// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineStoreInterface.h"


enum class EGooglePlayBillingResponseCode
{
	Ok					= 0,
	UserCancelled		= 1,
	ServiceUnavailable	= 2,
	BillingUnavailable	= 3,
	ItemUnavailable		= 4,
	DeveloperError		= 5,
	Error				= 6,
	ItemAlreadyOwned	= 7,
	ItemNotOwned		= 8,
};


/**
 * The resulting state of an iap transaction
 */
namespace EInAppPurchaseResult
{
	enum Type
	{
		Succeeded = 0,
		RestoredFromServer,
		Failed,
		Cancelled,
	};
}


/**
 * Implementation of the Platform Purchase receipt. For this we provide an identifier and the encrypted data.
 */
class FGooglePlayPurchaseReceipt : public IPlatformPurchaseReceipt
{
public:
	// Product identifier
	FString Identifier;

	// The encrypted receipt data
	FString Data;
};


/**
 *	FOnlineStoreGooglePlay - Implementation of the online store for GooglePlay
 */
class FOnlineStoreGooglePlay : public IOnlineStore
{
public:
	/** C-tor */
	FOnlineStoreGooglePlay(FOnlineSubsystemGooglePlay* InSubsystem);
	/** Destructor */
	virtual ~FOnlineStoreGooglePlay();

	// Begin IOnlineStore 
	virtual bool QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject) override;
	virtual bool BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InReadObject) override;
	virtual bool IsAllowedToMakePurchases() override;
	virtual bool RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject) override;
	// End IOnlineStore 

	void ProcessQueryAvailablePurchasesResults(EGooglePlayBillingResponseCode InResponseCode, const TArray<FInAppPurchaseProductInfo>& AvailablePurchases);
	void ProcessPurchaseResult(EGooglePlayBillingResponseCode InResponseCode, const FString& InProductId, const FString& InReceiptData, const FString& Signature);

	EInAppPurchaseState::Type ConvertGPResponseCodeToIAPState(const EGooglePlayBillingResponseCode InResponseCode);

	/** Cached in-app purchase restore transaction object, used to provide details to the developer about what products should be restored */
	FOnlineInAppPurchaseRestoreReadPtr CachedPurchaseRestoreObject;

private:

	/** Pointer to owning subsystem */
	FOnlineSubsystemGooglePlay* Subsystem;

	/** The current query for iap async task */
	class FOnlineAsyncTaskGooglePlayQueryInAppPurchases* CurrentQueryTask;

	/** Delegate fired when a query for purchases has completed, whether successful or unsuccessful */
	FOnQueryForAvailablePurchasesComplete OnQueryForAvailablePurchasesCompleteDelegate;

	/** Delegate fired when a purchase transaction has completed, whether successful or unsuccessful */
	FOnInAppPurchaseComplete OnPurchaseCompleteDelegate;

	/** Cached in-app purchase query object, used to provide the user with product information attained from the server */
	FOnlineProductInformationReadPtr ReadObject;

	/** Cached in-app purchase transaction object, used to provide details to the user, of the product that has just been purchased. */
	FOnlineInAppPurchaseTransactionPtr CachedPurchaseStateObject;
};

typedef TSharedPtr<FOnlineStoreGooglePlay, ESPMode::ThreadSafe> FOnlineStoreGooglePlayPtr;

