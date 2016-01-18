// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameSubCatalogEditorPCH.h"
#include "CatalogDefinition.h"
#include "Runtime/JsonUtilities/Public/JsonUtilities.h"
#include "MultiLocHelper.h"

#define LOCTEXT_NAMESPACE "GameSubModule"

UCatalogMetaAsset::UCatalogMetaAsset(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

UCatalogFulfillment::UCatalogFulfillment(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, bCrossPromotionEnabled(false)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UniqueId = FGuid::NewGuid().ToString();
	}
}

void UCatalogFulfillment::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	UniqueId = FGuid::NewGuid().ToString();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

UCatalogDefinition::UCatalogDefinition(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	RefreshIntervalHrs = 24;
}

#if WITH_EDITOR
void UCatalogDefinition::PostEditChangeProperty(FPropertyChangedEvent & Event)
{
	// whenever any property changes, recompute the version
	ETag = FGuid::NewGuid().ToString();
	if (Event.Property == Event.MemberProperty && Event.Property->GetNameCPP() == TEXT("CatalogOffers") && Event.ChangeType == EPropertyChangeType::ValueSet)
	{
		TSet<FString> ExistingGuids;
		for (FCatalogOfferDefinition& CatalogOffer : CatalogOffers)
		{
			if (ExistingGuids.Contains(CatalogOffer.OfferId))
			{
				// make a new GUID (This element may have been Duplicated)
				CatalogOffer.OfferId = FGuid::NewGuid().ToString();
			}

			ExistingGuids.Add(CatalogOffer.OfferId);
		}
	}
	Super::PostEditChangeProperty(Event);
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR

#define GAME_SUBCATALOG_WRITE_FIELD(OBJ, FIELD_NAME) \
	JsonOut->WriteValue(FJsonObjectConverter::StandardizeCase(TEXT(#FIELD_NAME)), OBJ.FIELD_NAME)
#define GAME_SUBCATALOG_WRITE_FIELD_TEXT(OBJ, FIELD_NAME, CULTURE) \
	JsonOut->WriteValue(FJsonObjectConverter::StandardizeCase(TEXT(#FIELD_NAME)), MultiLocHelper->GetLocalizedString((CULTURE), OBJ.FIELD_NAME))

bool FCatalogExporter::ExportCatalog(JsonWriter& JsonOut, const UCatalogDefinition& CatalogDefinition, const TArray<const UCatalogFulfillment*>& Fulfillments) const
{
	bool bSuccess = true;

	// check for localization setup first
	if (!MultiLocHelper.IsValid() || MultiLocHelper->GetCultures().Num() <= 0)
	{
		UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Unable to properly localize catalog. Try MultiLocHelper->LoadAllArchives()"));
		bSuccess = false;
	}

	// copy the editor-defined catalog items
	for (const FCatalogOfferDefinition& Def : CatalogDefinition.CatalogOffers)
	{
		// validate this definition
		if (Def.bEnabledForExport)
		{
			if (!ValidateCatalogOffer(Def, Fulfillments))
			{
				UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Unable to validate Catalog offer definition %s"), *Def.OfferId);
				bSuccess = false;
			}
		}
	}

	// track all referenced AppStoreIds
	TMap<FString,FString> ReferencedAppStoreIds[EAppStore::MAX];

	// validate all fulfillments
	for (const UCatalogFulfillment* FulfillmentPtr : Fulfillments)
	{
		if (FulfillmentPtr != nullptr)
		{
			const UCatalogFulfillment& Fulfillment = *FulfillmentPtr;

			// make sure this fulfillment can grant something
			if (Fulfillment.ItemGrants.Num() <= 0 && Fulfillment.LootTableAward.IsEmpty())
			{
				UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Fulfillment %s grants neither items nor a loot table."), *Fulfillment.GetFullName());
				bSuccess = false;
			}

			// check the direct item grants for validity
			for (const FItemQuantity& ItemGrant : Fulfillment.ItemGrants)
			{
				if (ItemGrant.Quantity <= 0)
				{
					UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Fulfillment %s has item grant with invalid quantity %d x %s"), *Fulfillment.GetFullName(), ItemGrant.Quantity, *ItemGrant.TemplateId);
					bSuccess = false;
				}

				if (!IsValidTemplateId(ItemGrant.TemplateId))
				{
					UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Fulfillment %s has item grant with invalid template %d x %s"), *Fulfillment.GetFullName(), ItemGrant.Quantity, *ItemGrant.TemplateId);
					bSuccess = false;
				}
			}

			// check the loot table
			if (!Fulfillment.LootTableAward.IsEmpty())
			{
				if (!IsValidLootTable(Fulfillment.LootTableAward))
				{
					UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Fulfillment %s has invalid LootTableAward '%s'"), *Fulfillment.GetFullName(), *Fulfillment.LootTableAward);
					bSuccess = false;
				}
			}

			// make sure only one fulfillment references each app store ID
			for (int Idx=0;Idx<EAppStore::MAX;++Idx)
			{
				const FString& AppStoreId = Fulfillment.AppStoreId[Idx];
				if (!AppStoreId.IsEmpty())
				{
					const FString* OtherFulfillment = ReferencedAppStoreIds[Idx].Find(AppStoreId);
					if (OtherFulfillment != nullptr)
					{
						UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Fulfillment %s references the same AppStoreId as %s: %s %s"), *Fulfillment.GetFullName(), **OtherFulfillment, *EAppStore::ToString((EAppStore::Type)Idx), *AppStoreId);
						UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Do not reuse fulfillments or AppStore offers. Instead generate a new offer/fullfilment. Reusing existing offers may screw up accounting for people who have already purchased."));
						bSuccess = false;
					}
					else
					{
						// mark this guy down so the next reference is an error
						ReferencedAppStoreIds[Idx].Add(AppStoreId, Fulfillment.UniqueId);
					}
				}
			}
		}
	}

	// write the JSON
	if (bSuccess)
	{
		JsonOut->WriteObjectStart();
		JsonOut->WriteValue(TEXT("templateId"), TEXT("Data.Catalog"));
		JsonOut->WriteObjectStart(TEXT("attributes"));
		WriteCatalogToJson(JsonOut, CatalogDefinition, Fulfillments);
		JsonOut->WriteObjectEnd();
		JsonOut->WriteObjectEnd();
	}

	// return success
	return bSuccess;
}

bool FCatalogExporter::ValidateCatalogOffer(const FCatalogOfferDefinition& CatalogOffer, const TArray<const UCatalogFulfillment*>& Fulfillments) const
{
	// shouldn't make it to this function with this flag set, but just check in case
	if (!CatalogOffer.bEnabledForExport)
	{
		UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s is not enabled for export"), *CatalogOffer.OfferId);
		return false;
	}
	bool bSuccess = true;

	// validate various fields
	if (CatalogOffer.FilterWeight < 0)
	{
		UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has invalid filter weight %f"), *CatalogOffer.OfferId, CatalogOffer.FilterWeight);
		bSuccess = false;
	}

	// make sure the prices are valid
	bool bHasRealMoneyPrice = false;
	for (const FCatalogItemPrice& Price : CatalogOffer.Prices)
	{
		// check price valid
		if (Price.BasePrice < 0)
		{
			UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has a %s price which is < 0"), *CatalogOffer.OfferId, *EStoreCurrencyType::ToString(Price.CurrencyType));
			bSuccess = false;
		}

		switch (Price.CurrencyType)
		{
		case EStoreCurrencyType::RealMoney:
			{
				if (bHasRealMoneyPrice)
				{
					UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has multiple real money prices set."), *CatalogOffer.OfferId);
					bSuccess = false;
				}
				bHasRealMoneyPrice = true;

				// check currency type (make sure it includes country)
				int32 ColonIndex = INDEX_NONE;
				if (!Price.CurrencySubType.FindChar(':', ColonIndex))
				{
					UE_LOG(LogGameSubCatalogEditor, Error, 
						TEXT("Catalog offer %s has a RealMoney price with invalid currency sub-type '%s'. This should be country code followed by a colon followed by an ISO-4217 Currency Code. Ex: \"US:USD\""), 
						*CatalogOffer.OfferId,
						*Price.CurrencySubType);
					bSuccess = false;
				}

				// check the appstore ids for realmoney stuff
				if (CatalogOffer.Fulfillment == nullptr)
				{
					UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has a RealMoney price with no Fulfillment defined"), *CatalogOffer.OfferId);
					bSuccess = false;
				}
			}
			break;
		case EStoreCurrencyType::MtxCurrency:
			if (!Price.CurrencySubType.IsEmpty())
			{
				UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has a %s price with a currency sub-type defined (should be empty)"), *CatalogOffer.OfferId, *EStoreCurrencyType::ToString(Price.CurrencyType));
				bSuccess = false;
			}
			break;
		case EStoreCurrencyType::Other:
			break;
		case EStoreCurrencyType::GameItem:
			if (Price.CurrencySubType.IsEmpty())
			{
				UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has a GameItem price with no currency sub-type defined (should be an item TemplateID)"), *CatalogOffer.OfferId);
				bSuccess = false;
			}
			if (!IsValidTemplateId(Price.CurrencySubType))
			{
				UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has a GameItem price referencing invalid TemplateId %s"), *CatalogOffer.OfferId, *Price.CurrencySubType);
				bSuccess = false;
			}
			break;
		default:
			UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Unhandled EStoreCurrencyType"));
			bSuccess = false;
		}
	}

	// make sure the fulfillment is valid
	if (CatalogOffer.Fulfillment != nullptr)
	{
		// if a fulfillment is referenced it needs to be in our array
		if (!Fulfillments.Contains(CatalogOffer.Fulfillment))
		{
			UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s references Fulfillment %s which is not included in the exported fulfillments list"), *CatalogOffer.OfferId, *CatalogOffer.Fulfillment->GetFullName());
			bSuccess = false;
		}
	}
	else
	{
		UE_LOG(LogGameSubCatalogEditor, Error, TEXT("Catalog offer %s has no fulfillment package defined"), *CatalogOffer.OfferId);
		bSuccess = false;
	}

	return bSuccess;
}

static void WritePriceToJson(FCatalogExporter::JsonWriter& JsonOut, const FCatalogItemPrice& Price)
{
	JsonOut->WriteObjectStart();
	{
		// currency
		JsonOut->WriteValue(TEXT("currencyType"), EStoreCurrencyType::ToString(Price.CurrencyType));
		GAME_SUBCATALOG_WRITE_FIELD(Price, CurrencySubType);
		GAME_SUBCATALOG_WRITE_FIELD(Price, BasePrice);
	}
	JsonOut->WriteObjectEnd();
}

void FCatalogExporter::WriteCommonOfferFields(JsonWriter& JsonOut, const FCatalogOfferDefinition& CatalogOffer) const
{
	GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, OfferId);
	JsonOut->WriteArrayStart(TEXT("prices"));
	for (const FCatalogItemPrice& Price : CatalogOffer.Prices)
	{
		WritePriceToJson(JsonOut, Price);
	}
	JsonOut->WriteArrayEnd();
	JsonOut->WriteArrayStart(TEXT("categories"));
	for (const FString& Category : CatalogOffer.Categories)
	{
		JsonOut->WriteValue(Category);
	}
	JsonOut->WriteArrayEnd();
	GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, DailyLimit);
	GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, SinglePurchaseOnly);
	JsonOut->WriteArrayStart(TEXT("requiredFulfillments"));
	for (const FString& FulfillmentId : CatalogOffer.RequiredFulfillments)
	{
		JsonOut->WriteValue(FulfillmentId);
	}
	JsonOut->WriteArrayEnd();
	JsonOut->WriteValue(TEXT("fulfillmentId"), CatalogOffer.Fulfillment ? CatalogOffer.Fulfillment->UniqueId : FString());
}

void FCatalogExporter::WriteCatalogOfferToJson(JsonWriter& JsonOut, const FCatalogOfferDefinition& CatalogOffer) const
{
	check(CatalogOffer.bEnabledForExport);

	// main block is the server info
	WriteCommonOfferFields(JsonOut, CatalogOffer);
	GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, MatchFilter);
	GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, FilterWeight);
	JsonOut->WriteObjectStart(TEXT("meta"));
	for (const FCatalogKeyValue& Pair : CatalogOffer.MetaInfo)
	{
		JsonOut->WriteValue(Pair.Key, Pair.Value);
	}
	for (const FCatalogKeyValue& Pair : CatalogOffer.ServerMetaInfo)
	{
		JsonOut->WriteValue(Pair.Key, Pair.Value);
	}
	JsonOut->WriteObjectEnd();

	// get the meta-asset
	const UStruct* MetaStructType = nullptr;
	const void* MetaStructPtr = nullptr;
	if (CatalogOffer.MetaAsset)
	{
		CatalogOffer.MetaAsset->GetExportStruct(MetaStructType, MetaStructPtr);
	}

	// write out localized client info (for each locale)
	JsonOut->WriteObjectStart(TEXT("localized"));
	for (const FString& CultureCode : MultiLocHelper->GetCultures())
	{
		JsonOut->WriteObjectStart(CultureCode);
		{
			// write out localized fields
			// NOTE: the server just stores these as an opaque JSON blob. You can safely add new fields here without changing MCP.
			WriteCommonOfferFields(JsonOut, CatalogOffer);
			JsonOut->WriteArrayStart(TEXT("metaInfo"));
			for (const FCatalogKeyValue& Pair : CatalogOffer.MetaInfo)
			{
				JsonOut->WriteObjectStart();
				GAME_SUBCATALOG_WRITE_FIELD(Pair, Key);
				GAME_SUBCATALOG_WRITE_FIELD(Pair, Value);
				JsonOut->WriteObjectEnd();
			}
			JsonOut->WriteArrayEnd();
			if (MetaStructType != nullptr && MetaStructPtr != nullptr)
			{
				JsonOut->WriteObjectStart(TEXT("metaAssetInfo"));
				JsonOut->WriteValue(TEXT("structName"), MetaStructType->GetName());
				FString OutJson;
				if (FJsonObjectConverter::UStructToJsonObjectString(MetaStructType, MetaStructPtr, OutJson, 0, 0, JsonOut->GetIndentLevel()))
				{
					JsonOut->WriteRawJSONValue(TEXT("payload"), OutJson);
				}
				JsonOut->WriteObjectEnd();
			}
			GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, CatalogGroup);
			GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, CatalogGroupPriority);
			GAME_SUBCATALOG_WRITE_FIELD(CatalogOffer, SortPriority);
			GAME_SUBCATALOG_WRITE_FIELD_TEXT(CatalogOffer, Title, CultureCode);
			GAME_SUBCATALOG_WRITE_FIELD_TEXT(CatalogOffer, Description, CultureCode);
			JsonOut->WriteValue(TEXT("displayAssetPath"), CatalogOffer.DisplayAsset ? CatalogOffer.DisplayAsset->GetPathName() : FString());

			// app store IDs
			if (CatalogOffer.Fulfillment)
			{
				JsonOut->WriteArrayStart(TEXT("appStoreId"));
				for (int i = 0; i < EAppStore::MAX; ++i)
				{
					JsonOut->WriteValue(CatalogOffer.AppStoreId[i]);
				}
				JsonOut->WriteArrayEnd();
			}
		}
		JsonOut->WriteObjectEnd();
	}
	JsonOut->WriteObjectEnd();
}

void FCatalogExporter::WriteFulfillmentToJson(JsonWriter& JsonOut, const UCatalogFulfillment& Fulfillment) const
{
	// fulfillment details are entirely server-side

	// unique id of this fulfillment type (used to track purchase history in conjunction with an actual transaction id)
	GAME_SUBCATALOG_WRITE_FIELD(Fulfillment, UniqueId);

	// write out the dev name
	JsonOut->WriteValue(TEXT("devName"), Fulfillment.GetName());

	// true if this fulfillment can be triggered by an entitlement sharing its uniqueId (within this game's namespace)
	JsonOut->WriteValue(TEXT("crossPromotionEnabled"), Fulfillment.bCrossPromotionEnabled);

	// game specific meta information
	JsonOut->WriteObjectStart(TEXT("meta"));
	for (const FCatalogKeyValue& Pair : Fulfillment.MetaInfo)
	{
		JsonOut->WriteValue(Pair.Key, Pair.Value);
	}
	JsonOut->WriteObjectEnd();

	// explicit item grants
	JsonOut->WriteArrayStart(TEXT("itemGrants"));
	for (const FItemQuantity& ItemGrant : Fulfillment.ItemGrants)
	{
		JsonOut->WriteObjectStart();
		GAME_SUBCATALOG_WRITE_FIELD(ItemGrant, TemplateId);
		GAME_SUBCATALOG_WRITE_FIELD(ItemGrant, Quantity);
		JsonOut->WriteObjectEnd();
	}
	JsonOut->WriteArrayEnd();

	// loot table roll on purchase
	GAME_SUBCATALOG_WRITE_FIELD(Fulfillment, LootTableAward);

	// app store IDs
	JsonOut->WriteArrayStart(TEXT("appStoreId"));
	for (int i = 0; i < EAppStore::MAX; ++i)
	{
		JsonOut->WriteValue(Fulfillment.AppStoreId[i]);
	}
	JsonOut->WriteArrayEnd();
}

void FCatalogExporter::WriteCatalogToJson(JsonWriter& JsonOut, const UCatalogDefinition& CatalogDefinition, const TArray<const UCatalogFulfillment*>& Fulfillments) const
{
	// write out the etag
	JsonOut->WriteValue(TEXT("etag"), CatalogDefinition.ETag);
	JsonOut->WriteValue(TEXT("refreshIntervalHrs"), CatalogDefinition.RefreshIntervalHrs);

	// write the list of culture codes we're exporting
	JsonOut->WriteArrayStart(TEXT("cultures"));
	for (const FString& CultureCode : MultiLocHelper->GetCultures())
	{
		JsonOut->WriteValue(CultureCode);
	}
	JsonOut->WriteArrayEnd();

	// write all defined catalog items
	JsonOut->WriteArrayStart(TEXT("catalogOffers"));
	for (const FCatalogOfferDefinition& CatalogOffer : CatalogDefinition.CatalogOffers)
	{
		// write the item if it's enabled for export
		if (CatalogOffer.bEnabledForExport)
		{
			JsonOut->WriteObjectStart();
			WriteCatalogOfferToJson(JsonOut, CatalogOffer);
			JsonOut->WriteObjectEnd();
		}
	}
	JsonOut->WriteArrayEnd();
	
	// write any referenced fulfillments
	JsonOut->WriteArrayStart(TEXT("fulfillments"));
	for (const UCatalogFulfillment* Fulfillment : Fulfillments)
	{
		JsonOut->WriteObjectStart();
		WriteFulfillmentToJson(JsonOut, *Fulfillment);
		JsonOut->WriteObjectEnd();
	}
	JsonOut->WriteArrayEnd();

	// write out storefront filters block
	WriteStorefrontFilters(JsonOut, CatalogDefinition.StorefrontFilters);

	// write out service pricing block
	WriteServicePricing(JsonOut, CatalogDefinition.ServicePricing);
}

void FCatalogExporter::WriteStorefrontFilters(JsonWriter& JsonOut, const TArray<FCatalogTableFilter>& StoreFrontFilters) const
{
	// group the filters by name
	TMap<FString, TArray<FCatalogTableFilter> > FilterGroups;
	for (const FCatalogTableFilter& Filter : StoreFrontFilters)
	{
		FilterGroups.FindOrAdd(Filter.Name).Add(Filter);
	}

	// export as an object with named keys
	JsonOut->WriteObjectStart(TEXT("storefronts"));
	for (const auto& It : FilterGroups)
	{
		JsonOut->WriteArrayStart(It.Key);
		for (const FCatalogTableFilter& Filter : It.Value)
		{
			JsonOut->WriteObjectStart();
			JsonOut->WriteValue(TEXT("match"), Filter.Match);
			JsonOut->WriteValue(TEXT("pullCount"), Filter.PullCount);
			JsonOut->WriteObjectEnd();
		}
		JsonOut->WriteArrayEnd();
	}
	JsonOut->WriteObjectEnd();
}

void FCatalogExporter::WriteServicePricing(JsonWriter& JsonOut, const TArray<FServicePrice>& ServicePricing) const
{
	JsonOut->WriteArrayStart(TEXT("servicePricing"));
	for (const FServicePrice& ServicePrice : ServicePricing)
	{
		JsonOut->WriteObjectStart();
		JsonOut->WriteValue(TEXT("serviceName"), ServicePrice.ServiceName);
		JsonOut->WriteIdentifierPrefix(TEXT("price"));
		WritePriceToJson(JsonOut, ServicePrice.Price);
		JsonOut->WriteObjectEnd();
	}
	JsonOut->WriteArrayEnd();
}

#endif

#undef LOCTEXT_NAMESPACE
