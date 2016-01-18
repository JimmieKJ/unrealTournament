// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameSubCatalogPCH.h"
#include "CatalogTypes.h"
#include "JsonObjectConverter.h"

bool FCatalogMetaAssetInfo::Parse(const UStruct* StructType, void* DataOut) const
{
	check(StructType != nullptr);
	check(DataOut != nullptr);

	// check the name
	if (StructType->GetName() != StructName)
	{
		UE_LOG(LogGameSubCatalog, Error, TEXT("Unable to Parse FCatalogMetaAssetInfo as %s because it is serialized as %s"), *StructType->GetName(), *StructName);
		return false;
	}

	// make sure we even have a payload
	if (!Payload.JsonObject.IsValid())
	{
		UE_LOG(LogGameSubCatalog, Error, TEXT("Unable to Parse FCatalogMetaAssetInfo as %s because no Json Payload was returned by the server"), *StructName);
		return false;
	}

	// deserialize from JSON
	if (!FJsonObjectConverter::JsonObjectToUStruct(Payload.JsonObject.ToSharedRef(), StructType, DataOut, 0, 0))
	{
		UE_LOG(LogGameSubCatalog, Error, TEXT("Unable to Parse FCatalogMetaAssetInfo as %s: see JSON errors above."), *StructName);
		return false;
	}

	// return success
	return true;
}

FString FCatalogOffer::GetMetaValue(const FString& Key) const
{
	FString SearchKey = Key.ToUpper();
	for (const FCatalogKeyValue& Pair : MetaInfo)
	{
		if (Pair.Key == SearchKey)
		{
			return Pair.Value;
		}
	}
	return FString();
}

const FCatalogItemPrice* FCatalogOffer::GetPriceIn(EStoreCurrencyType::Type Currency, const FString& SubType) const
{
	for (const FCatalogItemPrice& Price : Prices)
	{
		if (Price.CurrencyType == Currency && (SubType.IsEmpty() || Price.CurrencySubType == SubType))
		{
			return &Price;
		}
	}
	return nullptr;
}

FString FCatalogOffer::GetAppStoreId(EAppStore::Type Store) const
{
	switch (Store)
	{
	case EAppStore::MAX:
		return FString();
	case EAppStore::DebugStore:
		return FulfillmentId;
	default:
		return AppStoreId[Store];
	}
}

FString EStoreCurrencyType::ToString(EStoreCurrencyType::Type CurrencyType)
{
	return UEnum::GetValueAsString(TEXT("/Script/GameSubCatalog.EStoreCurrencyType"), CurrencyType);
}

FString EAppStore::ToString(EAppStore::Type AppStore)
{
	return UEnum::GetValueAsString(TEXT("/Script/GameSubCatalog.EAppStore"), AppStore);
}

const TCHAR* EAppStore::ToReceiptPrefix(EAppStore::Type AppStore)
{
	switch (AppStore)
	{
	case DebugStore:
		return TEXT("DEBUG");
	case EpicPurchasingService:
		return TEXT("EPIC");
	case IOSAppStore:
		return TEXT("APPLE");
	case WeChatAppStore:
		return TEXT("WECHAT");
	case GooglePlayAppStore:
		return TEXT("GOOGLE_PLAY");
	case KindleStore:
		return TEXT("KINDLE");
	case PlayStationStore:
		return TEXT("SONY");
	case XboxLiveStore:
		return TEXT("XBOX_LIVE");
	default:
		ensure(false);
		return TEXT("UNKNOWN");
	}
}

FStorefrontOffer::FStorefrontOffer(const TSharedPtr<FCatalogDownload>& InRootDownload, const FCatalogOffer& InCatalogOffer, const FCatalogItemPrice& InPrice)
: CatalogOffer(InCatalogOffer)
, Price(InPrice)
, RootDownload(InRootDownload)
{
}

const FString FCatalogPurchaseNotification::TypeStr(TEXT("CatalogPurchase"));

FText FCatalogItemPrice::AsText(int32 ActualPrice) const
{
	// let actual price default to BasePrice if unspecified
	if (ActualPrice < 0)
	{
		ActualPrice = BasePrice;
	}

	// check for real money
	if (CurrencyType == EStoreCurrencyType::RealMoney)
	{
		FString CurrencyCode;
		int32 ColonIndex = INDEX_NONE;
		if (CurrencySubType.FindChar(':', ColonIndex))
		{
			// take everything after the colon
			CurrencyCode = CurrencySubType.Mid(ColonIndex+1);
		}
		else
		{
			// just use the whole thing
			CurrencyCode = CurrencySubType;
		}
		return FText::AsCurrency(ActualPrice / 100.0, CurrencyCode);
	}

	// just return the base price
	return FText::AsNumber(ActualPrice);
}
