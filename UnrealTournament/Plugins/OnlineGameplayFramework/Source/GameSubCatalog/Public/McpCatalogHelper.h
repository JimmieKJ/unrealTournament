// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "CatalogTypes.h"

class UMcpProfile;
class UMcpProfileGroup;
class IHttpRequest;
class FOnlineHttpRequest;
class FPurchaseReceipt;
class FUniqueNetId;
namespace FHttpRetrySystem
{
	class FRequest;
}

/**
 * Helper class for adding catalog operations to your game's McpProfile
 */
struct GAMESUBCATALOG_API FMcpCatalogHelper : public TSharedFromThis<FMcpCatalogHelper>
{
public:
	FMcpCatalogHelper(UMcpProfile* OwningProfile, const FString& CatalogRouteOverride = FString(), const FString& EpicReceiptRouteOverride = FString());
	struct FPurchaseRedemptionState;

	// error codes
	static const FString UserCancelledErrorCode;
	static const FString CatalogOutOfDateErrorCode;
	static const FString UnableToParseErrorCode;

	/** Get the owning profile for this helper. Profile lifespan should outlast this object since it owns it. */
	FORCEINLINE UMcpProfile& GetProfile() const { return *Profile; }

	/** Get the owning profile's profile group */
	UMcpProfileGroup& GetProfileGroup() const;

	/** Get rid of any cached catalog data */
	void FlushCache();

	/** Triggers a refresh if the cache has expired. Callback will be called immediately if the cache is valid. */
	void FreshenCache(const FMcpQueryComplete& Callback = FMcpQueryComplete());

	/** Get the latest store catalog from MCP - WIll need to be called if CatalogOutOfDateException is thrown - if prices are not matching between our data and IOS data */
	void Refresh(const FMcpQueryComplete& Callback);

	/** Get a date stamp representing the last time the catalog refresh occurred */
	FORCEINLINE const FDateTime& GetLastRefreshDate() const { return LastDownloadTime; }
	FORCEINLINE const FDateTime& GetCacheExpirationTime() const { return CacheExpirationTime; }
	FTimespan GetCatalogCacheAge() const;

	/** Check if a catalog download is currently in progress */
	FORCEINLINE bool IsDownloadPending() const { return PendingHttpRequest.IsValid(); }

	/** Check if receipt validation is currently in progress */
	FORCEINLINE bool IsPurchaseRedemptionPending() const { return PendingPurchaseRedemption.IsValid(); }

	/** Delegate for when our cache of the store catalog changes (via any method) */
	FSimpleMulticastDelegate OnCatalogChanged;

	/** Time until the marketplace refreshes */
	FTimespan GetTimeUntilMarketplaceRefresh() const;

	/** Time until daily purchase limits reset (basically time until end of day in server timezone) */
	FTimespan GetTimeUntilDailyLimitReset() const;

	/** Start the process of redeeming paid purchases. May return null if a redeem operation is already pending, or in the case where it could not proceed without showing UI (iOS) and bAllowUi is false */
	bool RedeemPurchases(const FMcpQueryComplete& OnComplete);

	/** Start the process of purchasing an offerId */
	void PurchaseCatalogEntry(const FString& GameNamespace, const TSharedRef<FStorefrontOffer>& Offer, int32 Quantity, int32 ExpectedPrice, const FMcpQueryComplete& OnComplete);

	/** Get the user country code (Real money offers are automatically filtered to this country). */
	FString GetUserCountryCode() const;

public:

	/** Get the list of possible storefront names (This is intended only for debug, your code should know which storefronts it wants already) */
	TArray<FString> DebugGetStorefrontNames() const;

	/** Get the list of items for sale on the named storefront */
	const FStorefront* GetStorefront(const FString& StorefrontName) const;

	/** Gets storefront entries for every catalog item-price combo on the named storefront (this is for UI) */
	TArray< TSharedRef<FStorefrontOffer> > GetStorefrontOffers(const FString& StorefrontName) const;

	/** Get the current price for the named service (or NULL if the service couldn't be found) */
	const FCatalogItemPrice* GetServicePrice(const FString& ServiceName) const;

	/** When will the daily purchase limits be refreshed */
	FDateTime GetDailyLimitRefreshTime() const;

	/** 
	 * Get the number of these catalog items that still remain for the user to purchase today.
	 * @param CatalogOffer The catalog item to check
	 * @return -1 for infinite stock, 0 for out of stock, positive numbers indicate remaining stock.
	 */
	int32 GetAmountLeftInStock(const FCatalogOffer& CatalogOffer) const;

	/** Has the player ever purchased the fulfillment package specified */
	bool HasPurchasedItem(const FString& FulfillmentId) const;

	/** Check a receipt to see if it has already been redeemed on this profile */
	bool CanRedeemReceipt(EAppStore::Type AppStore, const FString& ReceiptId) const;

protected:

	void RefreshCatalog_HttpRequestComplete(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest, bool bSucceeded);

	const TMap<FString, int32>& GetDailyPurchases() const;

private:

	/** Callback from the purchasing system */
	void OnRealMoneyPurchaseComplete(const FUniqueNetId& UserId, bool bWasSuccessful, const TSharedRef<FPurchaseReceipt>& Receipt, const FString& Error, EAppStore::Type AppStore);

	/** Called when a purchase request completes  */
	void OnVirtualCurrencyPurchaseComplete(const FMcpQueryResult& Response, FMcpQueryComplete Callback);

	// called after auto-refreshing the catalog to unset bPendingPurchase
	void OnCatalogRefreshDone(const FMcpQueryResult& Result, FMcpQueryComplete Callback);

private:

	/** State machine to handle enumerating receipts and redeeming any necessary ones */
	TSharedPtr<FPurchaseRedemptionState> PendingPurchaseRedemption;

	/** Weak pointer to the profile */
	TWeakObjectPtr<UMcpProfile> Profile;

	/** The current version of StoreCatalog that we have */
	FString CatalogETag;

	/** The cached copy of the current MCP store catalog */
	TSharedPtr<FCatalogDownload> CatalogDownload;

	/** when the catalog was last refreshed */
	FDateTime LastDownloadTime;

	/** When this cached version of the catalog will expire */
	FDateTime CacheExpirationTime;

	/** HTTP route to get the catalog */
	FString CatalogRoute;

	/** HTTP route to get epic purchase receipts */
	FString EpicReceiptRoute;

	/** Store anything that has asked for a refresh here and fire when the refresh finishes */
	TArray<FMcpQueryComplete> OnRefreshCompleted;

	/** A pending HTTP request reference */
	TSharedPtr<FOnlineHttpRequest> PendingHttpRequest;

	// cache for daily purchases
	mutable int64 DailyPurchaseCacheVersion;
	mutable FDateTime DailyPurchaseExpirationTime;
	mutable TMap<FString, int32> DailyPurchaseCache;
};
