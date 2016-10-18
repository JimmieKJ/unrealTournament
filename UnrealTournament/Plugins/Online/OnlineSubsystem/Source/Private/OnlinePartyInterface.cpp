// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemPrivatePCH.h"
#include "OnlinePartyInterface.h"

DEFINE_LOG_CATEGORY(LogOnlineParty);

void FOnlinePartyData::ToJsonFull(FString& JsonString) const
{
	JsonString.Empty();

	// iterate over key/val attrs and convert each entry to a json string
	TSharedRef<FJsonObject> JsonObject(new FJsonObject());
	TArray<TSharedPtr<FJsonValue> > JsonProperties;
	for (auto Iterator : KeyValAttrs)
	{
		const FString& PropertyName = Iterator.Key;
		const FVariantData& PropertyValue = Iterator.Value;

		TSharedRef<FJsonObject> PropertyJson = PropertyValue.ToJson();
		PropertyJson->SetStringField(TEXT("Name"), *PropertyName);
		JsonProperties.Add(MakeShareable(new FJsonValueObject(PropertyJson)));
	}
	JsonObject->SetNumberField(TEXT("Rev"), RevisionCount);
	JsonObject->SetArrayField(TEXT("Attrs"), JsonProperties);

	auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
}

void FOnlinePartyData::ToJsonDirty(FString& JsonString) const
{
	JsonString.Empty();

	// iterate over key/val attrs and convert each entry to a json string
	TSharedRef<FJsonObject> JsonObject(new FJsonObject());
	TArray<TSharedPtr<FJsonValue> > JsonProperties;
	for (auto& PropertyName : DirtyKeys)
	{
		const FVariantData* PropertyValue = KeyValAttrs.Find(PropertyName);
		check(PropertyValue);

		TSharedRef<FJsonObject> PropertyJson = PropertyValue->ToJson();
		PropertyJson->SetStringField(TEXT("Name"), *PropertyName);
		JsonProperties.Add(MakeShareable(new FJsonValueObject(PropertyJson)));
	}
	JsonObject->SetNumberField(TEXT("Rev"), RevisionCount);
	JsonObject->SetArrayField(TEXT("Attrs"), JsonProperties);

	auto JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
}

void FOnlinePartyData::FromJson(const FString& JsonString)
{
	// json string to key/val attrs
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(JsonReader, JsonObject) &&
		JsonObject.IsValid())
	{
		if (JsonObject->HasTypedField<EJson::Array>(TEXT("Attrs")))
		{
			const TArray<TSharedPtr<FJsonValue> >& JsonProperties = JsonObject->GetArrayField(TEXT("Attrs"));
			for (auto JsonPropertyValue : JsonProperties)
			{
				TSharedPtr<FJsonObject> JsonPropertyObject = JsonPropertyValue->AsObject();
				if (JsonPropertyObject.IsValid())
				{
					FString PropertyName;
					if (JsonPropertyObject->TryGetStringField(TEXT("Name"), PropertyName) &&
						!PropertyName.IsEmpty())
					{
						FVariantData PropertyData;
						if (PropertyData.FromJson(JsonPropertyObject.ToSharedRef()))
						{
							KeyValAttrs.Add(PropertyName, PropertyData);
						}
					}
				}
			}
		}

		if (JsonObject->HasTypedField<EJson::Number>(TEXT("Rev")))
		{
			int32 NewRevisionCount = JsonObject->GetIntegerField(TEXT("Rev"));
			if ((RevisionCount != 0) && (NewRevisionCount != RevisionCount) && (NewRevisionCount != (RevisionCount + 1)))
			{
				UE_LOG_ONLINEPARTY(Warning, TEXT("Unexpected revision received.  Current %d, new %d"), RevisionCount, NewRevisionCount);
			}
			RevisionCount = NewRevisionCount;
		}
	}
}
