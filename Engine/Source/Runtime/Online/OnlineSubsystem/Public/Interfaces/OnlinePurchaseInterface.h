// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OnlineDelegateMacros.h"
#include "OnlineStoreInterfaceV2.h"
#include "OnlineEntitlementsInterface.h"
#include "OnlineError.h"

/**
 * Info needed for checkout
 */
class FPurchaseCheckoutRequest
{
public:
	/**
	 * Add a offer entry for purchase
	 *
	 * @param InNamespace namespace of offer to be purchased
	 * @param InOfferId id of offer to be purchased
	 * @param InQuantity number to purchase
	 */
	void AddPurchaseOffer(const FOfferNamespace& InNamespace, const FUniqueOfferId& InOfferId, int32 InQuantity)
	{ 
		PurchaseOffers.Add(FPurchaseOfferEntry(InNamespace, InOfferId, InQuantity)); 
	}
	/**
	 * Single offer entry for purchase 
	 */
	struct FPurchaseOfferEntry
	{
		FPurchaseOfferEntry(const FOfferNamespace& InOfferNamespace, const FUniqueOfferId& InOfferId, int32 InQuantity)
			: OfferNamespace(InOfferNamespace)
			, OfferId(InOfferId)
			, Quantity(InQuantity)
		{ }

		FOfferNamespace OfferNamespace;
		FUniqueOfferId OfferId;
		int32 Quantity;
	};	
	/** List of offers being purchased */
	TArray<FPurchaseOfferEntry> PurchaseOffers;
};

/**
 * State of a purchase transaction
 */
namespace EPurchaseTransactionState
{
	enum Type
	{
		/** processing has not started on the purchase */
		NotStarted,
		/** currently processing the purchase */
		Processing,
		/** purchase completed successfully */
		Purchased,
		/** purchase completed but failed */
		Failed,
		/** purchase canceled by user */
		Canceled,
		/** prior purchase that has been restored */
		Restored
	};
}

/**
 * Receipt result from checkout
 */
class FPurchaseReceipt
{
public:
	FPurchaseReceipt()
	: TransactionState(EPurchaseTransactionState::NotStarted)
	{
	}

	struct FLineItemInfo
	{
		/** The platform identifier of this purchase type */
		FString ItemName;

		/** unique identifier representing this purchased item (the specific instance owned by this account) */
		FUniqueEntitlementId UniqueId;

		/** platform-specific opaque validation info (required to verify UniqueId belongs to this account) */
		FString ValidationInfo;

		inline bool IsRedeemable() const { return !ValidationInfo.IsEmpty(); }
	};

	/**
	 * Single purchased offer offer
	 */
	struct FReceiptOfferEntry
	{
		FReceiptOfferEntry(const FOfferNamespace& InNamespace, const FUniqueOfferId& InOfferId, int32 InQuantity)
			: Namespace(InNamespace)
			, OfferId(InOfferId)
			, Quantity(InQuantity)
		{ }

		FOfferNamespace Namespace;
		FUniqueOfferId OfferId;
		int32 Quantity;

		/** Information about the individual items purchased */
		TArray<FLineItemInfo> LineItems;
	};

	/**
	 * Add a offer entry that has been purchased
	 *
	 * @param InNamespace of the offer that has been purchased
	 * @param InOfferId id of offer that has been purchased
	 * @param InQuantity number purchased
	 */
	void AddReceiptOffer(const FOfferNamespace& InNamespace, const FUniqueOfferId& InOfferId, int32 InQuantity)
	{ 
		ReceiptOffers.Add(FReceiptOfferEntry(InNamespace, InOfferId, InQuantity)); 
	}

	void AddReceiptOffer(const FReceiptOfferEntry& ReceiptOffer)
	{ 
		ReceiptOffers.Add(ReceiptOffer); 
	}

public:
	/** Unique Id for this transaction/order */
	FString TransactionId;
	/** Current state of the purchase */
	EPurchaseTransactionState::Type TransactionState;

	/** List of offers that were purchased */
	TArray<FReceiptOfferEntry> ReceiptOffers;
};

/**
 * Info needed for code redemption
 */
class FRedeemCodeRequest
{
public:
	/** Code to redeem */
	FString Code;
};

/**
 * Delegate called when checkout process completes
 */
DECLARE_DELEGATE_TwoParams(FOnPurchaseCheckoutComplete, const FOnlineError& /*Result*/, const TSharedRef<FPurchaseReceipt>& /*Receipt*/);

/**
 * Delegate called when code redemption process completes
 */
DECLARE_DELEGATE_TwoParams(FOnPurchaseRedeemCodeComplete, const FOnlineError& /*Result*/, const TSharedRef<FPurchaseReceipt>& /*Receipt*/);

/**
 * Delegate called when query receipt process completes
 */
DECLARE_DELEGATE_OneParam(FOnQueryReceiptsComplete, const FOnlineError& /*Result*/);

/**
 *	IOnlinePurchase - Interface for IAP (In App Purchases) services
 */
class IOnlinePurchase
{
public:
	virtual ~IOnlinePurchase() {}

	/**
	 * Determine if user is allowed to purchase from store 
	 *
	 * @param UserId user initiating the request
	 *
	 * @return true if user can make a purchase
	 */
	virtual bool IsAllowedToPurchase(const FUniqueNetId& UserId) = 0;

	/**
	 * Initiate the checkout process for purchasing offers via payment
	 *
	 * @param UserId user initiating the request
	 * @param CheckoutRequest info needed for the checkout request
	 * @param Delegate completion callback (guaranteed to be called)
	 */
	virtual void Checkout(const FUniqueNetId& UserId, const FPurchaseCheckoutRequest& CheckoutRequest, const FOnPurchaseCheckoutComplete& Delegate) = 0;

	/**
	 * Initiate the checkout process for obtaining offers via code redemption
	 *
	 * @param UserId user initiating the request
	 * @param RedeemCodeRequest info needed for the redeem request
	 * @param Delegate completion callback (guaranteed to be called)
	 */
	virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) = 0;

	/**
	 * Query for all of the user's receipts from prior purchases
	 *
	 * @param UserId user initiating the request
	 * @param Delegate completion callback (guaranteed to be called)
	 */
	virtual void QueryReceipts(const FUniqueNetId& UserId, const FOnQueryReceiptsComplete& Delegate) = 0;

	/**
	 * Get list of cached receipts for user (includes transactions currently being processed)
	 *
	 * @param UserId user initiating the request
	 * @param OutReceipts [out] list of receipts for the user 
	 */
	virtual void GetReceipts(const FUniqueNetId& UserId, TArray<FPurchaseReceipt>& OutReceipts) const = 0;
	
};