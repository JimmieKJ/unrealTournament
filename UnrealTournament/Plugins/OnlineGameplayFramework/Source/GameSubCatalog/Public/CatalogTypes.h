// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "McpSharedTypes.h"
#include "CatalogTypes.generated.h"

/** Different types of currency that can be used to purchase an item. */
UENUM()
namespace EStoreCurrencyType
{
	enum Type
	{
		/** Purchase this with real money. Use CurrencySubType to specify the currency code (e.g. "USD"). */
		RealMoney,
		/** Purchased currency. Divided into 3 types: Earned, PurchaseBonus, and Purchased (transparent to players). */
		MtxCurrency,
		/** Arbitrary item. Use CurrencySubType to specify a templateId to use. */
		GameItem,
		/** Game-specific interpretation. Use CurrencySubType to further qualify. */
		Other,

		MAX UMETA(Hidden)
	};
}
namespace EStoreCurrencyType
{
	GAMESUBCATALOG_API FString ToString(EStoreCurrencyType::Type CurrencyType);
}

/** Various app stores supported. */
UENUM()
namespace EAppStore
{
	enum Type
	{
		/** fake store that just short-circuits with success (this is only enabled in dev environments). */
		DebugStore = 0 UMETA(Hidden),
		EpicPurchasingService,
		IOSAppStore,
		WeChatAppStore,
		GooglePlayAppStore,
		KindleStore,
		PlayStationStore,
		XboxLiveStore,

		MAX UMETA(Hidden)
	};
}
namespace EAppStore
{
	GAMESUBCATALOG_API FString ToString(EAppStore::Type AppStore);
	GAMESUBCATALOG_API const TCHAR* ToReceiptPrefix(EAppStore::Type AppStore);
}

/** Structure containing purchasing-platform agnostic receipt information for a single purchased item */
USTRUCT()
struct GAMESUBCATALOG_API FCatalogReceiptInfo
{
	GENERATED_USTRUCT_BODY()
public:

	/** Which app store was this purchase made from */
	UPROPERTY()
	TEnumAsByte<EAppStore::Type> AppStore;

	/** The appstore-specific product identifier (see FCatalogOffer::AppStoreId) */
	UPROPERTY()
	FString AppStoreId;

	/** This is a transaction ID but needs to identify a single purchased product */
	UPROPERTY()
	FString ReceiptId;

	/** This is opaque data that can be passed to AppStore for validation. It correlates with AppStoreId and ReceiptId */
	UPROPERTY()
	FString ReceiptInfo;
};

/** Structure representing the parameters needed to make a purchase in virtual currency */
USTRUCT()
struct GAMESUBCATALOG_API FCatalogPurchaseInfo
{
	GENERATED_USTRUCT_BODY()
public:

	/** The unique offerID for the product */
	UPROPERTY()
	FString OfferId;

	/** How many do we want to buy */
	UPROPERTY()
	int32 PurchaseQuantity;
	
	/** The currency we want to purchase the product with */
	UPROPERTY()
	TEnumAsByte<EStoreCurrencyType::Type> Currency;
	
	/** Further specify the currency */
	UPROPERTY()
	FString CurrencySubType;
	
	/** What price does the client think this item is (so we can fail if the price has changed) */
	UPROPERTY()
	int32 ExpectedPrice;
};

/** Key/Value struct for CatalogOffer.MetaInfo */
USTRUCT()
struct GAMESUBCATALOG_API FCatalogKeyValue
{
	GENERATED_USTRUCT_BODY()
public:

	FCatalogKeyValue()
	{
	}

	FCatalogKeyValue(const FString& KeyIn, const FString& ValueIn) 
	: Key(KeyIn)
	, Value(ValueIn) 
	{
	}

	UPROPERTY(EditAnywhere, Category = "Catalog")
	FString Key;

	UPROPERTY(EditAnywhere, Category = "Catalog")
	FString Value;
};

/** Represents a single pricing option for a catalog entry */
USTRUCT()
struct GAMESUBCATALOG_API FCatalogItemPrice
{
	GENERATED_USTRUCT_BODY()
public:

	FCatalogItemPrice()
		: CurrencyType(EStoreCurrencyType::MtxCurrency)
		, CurrencySubType()
		, BasePrice(-1)
	{
	}

	FCatalogItemPrice(int32 BasePriceIn, EStoreCurrencyType::Type CurrencyIn, const FString& SubTypeIn)
		: CurrencyType(CurrencyIn)
		, CurrencySubType(SubTypeIn)
		, BasePrice(BasePriceIn)
	{
	}

	/** The type of currency this item can be purchased with */
	UPROPERTY(EditAnywhere, Category = "Price")
	TEnumAsByte<EStoreCurrencyType::Type> CurrencyType;

	/** This field is a templateId if Currency = GameItem, it is an ISO 4217 Currency Code if Currency = RealMoney (e.g. "USD") */
	UPROPERTY(EditAnywhere, Category = "Price")
	FString CurrencySubType;

	/** Original price in whatever currency. If this is a real money price use the smallest denomination (i.e. cents for USD so 100 = $1) */
	UPROPERTY(EditAnywhere, Category = "Price")
	int32 BasePrice;

	/** Get the price as a localized currency if CurrencyType==RealMoney, otherwise just return the number. */
	FText AsText(int32 ActualPrice = -1) const;
};

/** Prices for various game services that a user can buy. These are things that don't necessarily correspond to receiving an item */
USTRUCT()
struct GAMESUBCATALOG_API FServicePrice
{
	GENERATED_USTRUCT_BODY()
public:

	/** Unique key that identifies the service */
	UPROPERTY(EditAnywhere, Category = "MTX Pricing")
	FString ServiceName;

	/** The price (in MTX gems) for access to the next marketplace page mid-day */
	UPROPERTY(EditAnywhere, Category = "MTX Pricing")
	FCatalogItemPrice Price;
};

/** Meta asset that can get JSON from the server and convert to a UStruct for display */
USTRUCT()
struct GAMESUBCATALOG_API FCatalogMetaAssetInfo
{
	GENERATED_USTRUCT_BODY()
public:
	template<typename STRUCT_T>
	FORCEINLINE bool Parse(STRUCT_T& DataOut) const
	{
		return Parse(STRUCT_T::StaticStruct(), &DataOut);
	}
	bool Parse(const UStruct* StructType, void* DataOut) const;

	UPROPERTY()
	FString StructName;

	UPROPERTY()
	FJsonObjectWrapper Payload;
};

/** An item that is offered for sale in the store */
USTRUCT()
struct GAMESUBCATALOG_API FCatalogOffer
{
	GENERATED_USTRUCT_BODY()
public:

	FCatalogOffer()
	: DailyLimit(-1)
	, SinglePurchaseOnly(false)
	, CatalogGroupPriority(0)
	, SortPriority(0)
	{
	}

	/** Pull a value from the MetaInfo (server guarantees uppercase keys) */
	FString GetMetaValue(const FString& Key) const;

	/** Checks currency type and subtype for a match */
	const FCatalogItemPrice* GetPriceIn(EStoreCurrencyType::Type Currency, const FString& SubType = FString()) const;

	/** Get the appstore-specific product ID for this catalog offer (only relevant for real money offers) */
	FString GetAppStoreId(EAppStore::Type Store) const;

	/** The catalog item's guid (for purchasing) */
	UPROPERTY(VisibleAnywhere, Category = "Meta Info")
	FString OfferId;

	/** Game-Specific Key value pairs (available to client and server) */
	UPROPERTY(EditAnywhere, Category = "Meta Info")
	TArray<FCatalogKeyValue> MetaInfo;

	/** Various prices this item can be offered at */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	TArray<FCatalogItemPrice> Prices;

	/** How many of this entry can be purchased in a given day. Use a daily limit of < 0 to indicate unlimited */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	int32 DailyLimit;

	/** If checked, this offer can only be purchased once per account (enforced ONLY BY UI for real money purchases, since they are outside our control on some platforms) */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	bool SinglePurchaseOnly;

	/** Not purchaseable unless the user has purchased all fulfillments in this list (enforced ONLY BY UI for real money purchases, since they are outside our control on some platforms) */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	TArray<FString> RequiredFulfillments;

	/** Category tags used for filtering on marketplace and price engine. E.x. assets, assets/environments, projects/completeprojects */
	UPROPERTY(EditAnywhere, Category = "Display")
	TArray<FString> Categories;

	/** Only the highest priority item in a given catalog group will be shown by the UI */
	UPROPERTY(EditAnywhere, Category = "Display")
	FString CatalogGroup;

	/** Only the highest priority item in a given catalog group will be shown by the UI */
	UPROPERTY(EditAnywhere, Category = "Display")
	int32 CatalogGroupPriority;

	/** Used to sort catalog items after they become FStorefrontOffers */
	UPROPERTY(EditAnywhere, Category = "Display")
	int32 SortPriority;

	/** The title of this store item */
	UPROPERTY(EditAnywhere, Category = "Display")
	FText Title;

	/** The description of this store item */
	UPROPERTY(EditAnywhere, Category = "Display")
	FText Description;

	/**
	 * App store specific ID that should be used to purchase this offer (only relevant for RealMoney offers)
	 * Epic: This is the catalog OfferId (should be a guid)
	 * Sony: This is the end of the SKU for the service entitlement product "<licenseeId>-<titleId>-<what-you-want>" (ex. "ENTXXX0000000000-UYYY")
	 */
	UPROPERTY(EditAnywhere, Category = "Display")
	FString AppStoreId[EAppStore::MAX];

	/** Display meta asset info */
	UPROPERTY()
	FCatalogMetaAssetInfo MetaAssetInfo;

	/** UObject path to the DisplayAsset (necessary since this struct is deserialized from JSON) */
	UPROPERTY()
	FString DisplayAssetPath;

	/** This property comes down from the server letting us know what "product" this catalog offer corresponds to */
	UPROPERTY()
	FString FulfillmentId;
};

/** download structure for a storefront (list of items for sale) */
USTRUCT()
struct GAMESUBCATALOG_API FStorefront
{
	GENERATED_USTRUCT_BODY()
public:

	/** The storefront name */
	UPROPERTY()
	FString Name;

	/** Active catalog entries in this storefront */
	UPROPERTY()
	TArray<FCatalogOffer> CatalogEntries;
};

/** download structure for the store catalog */
USTRUCT()
struct GAMESUBCATALOG_API FCatalogDownload
{
	GENERATED_USTRUCT_BODY()
public:

	/** How often the catalog storefronts regenerate */
	UPROPERTY()
	int32 RefreshIntervalHrs;

	/** Prices for various game services (as opposed to goods) like a continue, or refresh energy */
	UPROPERTY()
	TArray<FServicePrice> ServicePricing;

	/** Active catalog entries */
	UPROPERTY()
	TArray<FStorefront> Storefronts;
};

USTRUCT()
struct GAMESUBCATALOG_API FCatalogPurchaseNotification
{
	GENERATED_USTRUCT_BODY()
public:

	static const FString TypeStr;

	UPROPERTY()
	FMcpLootResult LootResult;
};

/** This struct is used by UI to reference a particular offer in a specific version of the catalog */
struct FStorefrontOffer : TSharedFromThis<FStorefrontOffer>
{
public:
	FStorefrontOffer(const TSharedPtr<FCatalogDownload>& RootDownload, const FCatalogOffer& CatalogOffer, const FCatalogItemPrice& Price);

	const FCatalogOffer& CatalogOffer;
	const FCatalogItemPrice& Price;
private:
	TSharedPtr<const FCatalogDownload> RootDownload; // keeping this ensures our references stay valid
};
