// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CatalogTypes.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "CatalogDefinition.generated.h"

class FMultiLocHelper;

/** container type for a grouping of templateId and quantity */
USTRUCT()
struct GAMESUBCATALOGEDITOR_API FItemQuantity
{
	GENERATED_USTRUCT_BODY()
public:

	FItemQuantity()
	: TemplateId()
	, Quantity(0)
	{
	}

	FItemQuantity(const FString& TemplateIdIn, int32 QuantityIn)
	: TemplateId(TemplateIdIn)
	, Quantity(QuantityIn)
	{
	}

	/** The template ID for the item */
	UPROPERTY(EditAnywhere, Category = "Item")
	FString TemplateId;

	/** How many of this item type to spend */
	UPROPERTY(EditAnywhere, Category = "Item")
	int32 Quantity;
};

UCLASS()
class GAMESUBCATALOGEDITOR_API UCatalogFulfillment : public UDataAsset
{
	GENERATED_UCLASS_BODY()
public:

	/** The fulfillment item's guid (for purchasing) */
	UPROPERTY(VisibleAnywhere, Category = "Catalog Info", DuplicateTransient)
	FString UniqueId;

	/** Can this fulfillment be granted from a different title or is it expected to only be referenced internally (by our CatalogItems) */
	UPROPERTY(EditAnywhere, Category = "Catalog Info")
	bool bCrossPromotionEnabled;

	/** Game-Specific Key value pairs */
	UPROPERTY(EditAnywhere, Category = "Fulfillment")
	TArray<FCatalogKeyValue> MetaInfo;

	/** Explicit items to grant when fulfilling this package. */
	UPROPERTY(EditAnywhere, Category = "Fulfillment")
	TArray<FItemQuantity> ItemGrants;

	/** An loot table to roll on and reward contents when this package is purchased. Leave blank to omit. */
	UPROPERTY(EditAnywhere, Category = "Fulfillment")
	FString LootTableAward;

	/** 
	 * Unique ID of the purchase itself. Platform specific:
	 *   Epic: This is the catalog Item ID (should be a guid)
	 *   Sony: This is the 6-character entitlement code (ex. "ENTXXX")
	 */
	UPROPERTY(EditAnywhere, Category = "MTX")
	FString AppStoreId[EAppStore::MAX];

	/** Called after duplication */
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
};

/** Derive game-specific meta info class from this (mostly used for some type safety of what belongs in MetaAsset) */
UCLASS(Abstract)
class GAMESUBCATALOGEDITOR_API UCatalogMetaAsset : public UDataAsset
{
	GENERATED_UCLASS_BODY()

	/** 
	this would go in the derived class

	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	PrimaryStruct Primary;

	virtual void GetExportStruct(const UStruct*& StructTypeOut, const void*& StructPtrOut) const override
	{
		StructTypeOut = PrimaryStruct::StaticStruct();
		StructPtrOut = &Primary;
	}

	*/

	/** Serialize your primary property to JSON using JsonUtilites or add custom behavior */
	virtual void GetExportStruct(const UStruct*& StructTypeOut, const void*& StructPtrOut) const
	{
		// override this
		StructTypeOut = nullptr;
		StructPtrOut = nullptr;
	}
};

/** Define a game catalog entry in the editor */ 
USTRUCT()
struct GAMESUBCATALOGEDITOR_API FCatalogOfferDefinition : public FCatalogOffer
{
	GENERATED_USTRUCT_BODY()
public:

	FCatalogOfferDefinition()
		: bEnabledForExport(true)
		, ServerMetaInfo()
		, DisplayAsset(nullptr)
		, Fulfillment(nullptr)
		, MatchFilter()
		, FilterWeight(1.0f)
	{
		OfferId = FGuid::NewGuid().ToString();
	}

	UPROPERTY(EditAnywhere, Category = "Meta Info")
	bool bEnabledForExport;

	/** Game-Specific Key value pairs that only the server can see */
	UPROPERTY(EditAnywhere, Category = "Meta Info")
	TArray<FCatalogKeyValue> ServerMetaInfo;

	/** The image asset for this item. This is only used when linking to assets in the editor */
	UPROPERTY(EditAnywhere, Category = "Display")
	const UObject* DisplayAsset;

	/** JSON serialized data asset */
	UPROPERTY(EditAnywhere, Category = "Display")
	const UCatalogMetaAsset* MetaAsset;

	/** Link to the fulfillment object which defines the rewards for purchasing this catalog item */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	const UCatalogFulfillment* Fulfillment;

	/** A filter string to match this catalog item for queries (usually for dynamically generating storefronts) */
	UPROPERTY(EditAnywhere, Category = "Selection")
	FString MatchFilter;

	/** A weight to report when matching a filter. This is used for random selection. */
	UPROPERTY(EditAnywhere, Category = "Selection")
	float FilterWeight;
};

USTRUCT()
struct GAMESUBCATALOGEDITOR_API FCatalogTableFilter
{
	GENERATED_USTRUCT_BODY()
public:

	FCatalogTableFilter()
	: PullCount(-1)
	{
	}

	/** The game-relevant name of this filter (e.g. "Marketplace[0]", or "SecretShop") might not be unique if you want multiple pull/matches for a given filter */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	FString Name;

	/** The value to match against the MatchFilter column in FCatalogTableEntry or FCatalogOfferDefinition */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	FString Match;

	/** How many pulls from CatalogEntries will occur every day. Use < 0 to ignore weights and include all items. */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	int32 PullCount;
};

/** Definition of the current store catalog */
UCLASS()
class GAMESUBCATALOGEDITOR_API UCatalogDefinition : public UDataAsset
{
	GENERATED_UCLASS_BODY()
public:

	/** Version number updated after each change */
	UPROPERTY(VisibleAnywhere, Category = "Meta", DuplicateTransient)
	FString ETag;

	/** How many hours between regenerating marketplace filters */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	int32 RefreshIntervalHrs;

	/** The items that should appear in the store catalog */
	UPROPERTY(EditAnywhere, Category = "Catalog")
	TArray<FCatalogOfferDefinition> CatalogOffers;

	/** Each day, PullCount catalog items will be selected from each marketplace level */
	UPROPERTY(EditAnywhere, Category = "Daily Rotating Catalog")
	TArray<FCatalogTableFilter> StorefrontFilters;

	/** Prices for various services (as opposed to goods) offered by the game */
	UPROPERTY(EditAnywhere, Category = "Service Pricing")
	TArray<FServicePrice> ServicePricing;

protected:
	// UObject interface
#if WITH_EDITOR

	/** Fired when a property or sub-property changes (re-rolls ETag) */
	virtual void PostEditChangeProperty(FPropertyChangedEvent & Event) override;

#endif
};

#if WITH_EDITOR
struct GAMESUBCATALOGEDITOR_API FCatalogExporter
{
public:
	typedef TPrettyJsonPrintPolicy<TCHAR> JsonPrintPolicy; // use pretty printing since we're checking these in (results in better diffs)
	typedef TSharedRef< TJsonWriter<TCHAR, JsonPrintPolicy > > JsonWriter;

	/** Export this catalog definition to JSON */
	bool ExportCatalog(JsonWriter& JsonOut, const UCatalogDefinition& CatalogDefinition, const TArray<const UCatalogFulfillment*>& Fulfillments) const;

	/** Provide bindings for template and loot table validation */
	DECLARE_DELEGATE_RetVal_OneParam(bool, ValidateIdCallback, const FString&);

	/** Override this to provide template validation per-game */
	ValidateIdCallback OnIsValidTemplateId;
	
	/** Override this to provide loot table validation per-game */
	ValidateIdCallback OnIsValidLootTable;

	/** Localization helper (can be shared with other export code) */
	TSharedPtr<FMultiLocHelper> MultiLocHelper;

protected:
	virtual bool IsValidTemplateId(const FString& TemplateId) const { return OnIsValidTemplateId.IsBound() ? OnIsValidTemplateId.Execute(TemplateId) : false; }

	virtual bool IsValidLootTable(const FString& LootTableId) const { return OnIsValidLootTable.IsBound() ? OnIsValidLootTable.Execute(LootTableId) : false; }

	/** General catalog item validation. Base method should be complete, but you may want to also check your own MetaInfo properties. */
	virtual bool ValidateCatalogOffer(const FCatalogOfferDefinition& CatalogOffer, const TArray<const UCatalogFulfillment*>& Fulfillments) const;

	virtual void WriteCatalogOfferToJson(JsonWriter& JsonOut, const FCatalogOfferDefinition& CatalogOffer) const;

	virtual void WriteCatalogToJson(JsonWriter& JsonOut, const UCatalogDefinition& CatalogDefinition, const TArray<const UCatalogFulfillment*>& Fulfillments) const;

	virtual void WriteFulfillmentToJson(JsonWriter& JsonOut, const UCatalogFulfillment& Fulfillment) const;

	virtual void WriteStorefrontFilters(JsonWriter& JsonOut, const TArray<FCatalogTableFilter>& StoreFrontFilters) const;

	virtual void WriteServicePricing(JsonWriter& JsonOut, const TArray<FServicePrice>& ServicePricing) const;

protected:
	void WriteCommonOfferFields(JsonWriter& JsonOut, const FCatalogOfferDefinition& CatalogOffer) const;
};
#endif
