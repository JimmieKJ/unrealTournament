// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineStoreInterface.h"

// SK includes
#include <StoreKit/SKRequest.h>
#include <StoreKit/SKError.h>
#include <StoreKit/SKProduct.h>
#include <StoreKit/SKProductsRequest.h>
#include <StoreKit/SKReceiptRefreshRequest.h>
#include <StoreKit/SKPaymentTransaction.h>
#include <StoreKit/SKPayment.h>
#include <StoreKit/SKPaymentQueue.h>


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


/** Helper class, which allows us to manage IAP product information requests, AND transactions */
@interface FStoreKitHelper : NSObject<SKProductsRequestDelegate, SKPaymentTransactionObserver, SKRequestDelegate>
{
};
/** Store kit request object, holds information about the products we are purchasing, or querying. */
@property (nonatomic, strong) SKRequest *Request;
/** collection of available products attaced through a store kit request */
@property (nonatomic, strong) NSArray *AvailableProducts;

/** Helper fn to start a store kit purchase request */
-(void)makePurchase:(NSMutableSet*)productIDs;
/** Helper fn to start a store kit purchase information query request */
- (void) requestProductData:(NSMutableSet*) productIDs;

/** Helper fn to direct a product request response back to our store interface */
- (void)productsRequest:(SKProductsRequest *)request didReceiveResponse:(SKProductsResponse *)response;

/** Helper fn to restore previously purchased products */
-(void)restorePurchases;
@end


/**
 *	FOnlineStoreInterfaceIOS - Implementation of the online store for IOS
 */
class FOnlineStoreInterfaceIOS : public IOnlineStore
{
public:
	/** C-tor */
	FOnlineStoreInterfaceIOS();
	/** Destructor */
	virtual ~FOnlineStoreInterfaceIOS();

	//~ Begin IOnlineStore Interface
	virtual bool QueryForAvailablePurchases(const TArray<FString>& ProductIDs, FOnlineProductInformationReadRef& InReadObject) override;
	virtual bool BeginPurchase(const FInAppPurchaseProductRequest& ProductRequest, FOnlineInAppPurchaseTransactionRef& InReadObject) override;
	virtual bool IsAllowedToMakePurchases() override;
	virtual bool RestorePurchases(const TArray<FInAppPurchaseProductRequest>& ConsumableProductFlags, FOnlineInAppPurchaseRestoreReadRef& InReadObject) override;
	//~ End IOnlineStore Interface

	/**
	 * Process a response from the StoreKit
	 */
	void ProcessProductsResponse( SKProductsResponse* Response );
    
	/** Cached in-app purchase query object, used to provide the user with product information attained from the server */
	FOnlineProductInformationReadPtr CachedReadObject;
    
	/** Cached in-app purchase transaction object, used to provide details to the user, of the product that has just been purchased. */
	FOnlineInAppPurchaseTransactionPtr CachedPurchaseStateObject;

	/** Cached in-app purchase restore transaction object, used to provide details to the developer about what products should be restored */
	FOnlineInAppPurchaseRestoreReadPtr CachedPurchaseRestoreObject;


	/** Flags which determine the state of our transaction buffer, only one action at a time */
	bool bIsPurchasing, bIsProductRequestInFlight, bIsRestoringPurchases;

private:
	/** Access to the IOS Store kit interface */
	FStoreKitHelper* StoreHelper;

	/** Delegate fired when a query for purchases has completed, whether successful or unsuccessful */
	FOnQueryForAvailablePurchasesComplete OnQueryForAvailablePurchasesCompleteDelegate;

	/** Delegate fired when a purchase transaction has completed, whether successful or unsuccessful */
	FOnInAppPurchaseComplete OnPurchaseCompleteDelegate;

	/** Delegate fired when the purchase restoration has completed, whether successful or unsuccessful */
	FOnInAppPurchaseRestoreComplete OnPurchaseRestoreCompleteDelegate;
};

typedef TSharedPtr<FOnlineStoreInterfaceIOS, ESPMode::ThreadSafe> FOnlineStoreInterfaceIOSPtr;

