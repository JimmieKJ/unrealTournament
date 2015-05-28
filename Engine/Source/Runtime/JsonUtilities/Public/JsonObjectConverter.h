// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Class that handles converting Json objects to and from UStructs */
class JSONUTILITIES_API FJsonObjectConverter
{
public:
	
	/** FName case insensitivity can make the casing of UPROPERTIES unpredictable. Attempt to standardize output. */
	static FString StandardizeCase(const FString &StringIn);

	/**
	 * Converts from a UStruct to a Json Object, using exportText
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy out of
	 * @param JsonObject Json Object to be filled in with data from the ustruct
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties failed to write
	 */
	static bool UStructToJsonObject(const UStruct* StructDefinition, const void* Struct, TSharedRef<FJsonObject> OutJsonObject, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Converts from a UStruct to a json string containing an object, using exportText
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy out of
	 * @param JsonObject Json Object to be filled in with data from the ustruct
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 * @param Indent How many tabs to add to the json serializer
	 *
	 * @return False if any properties failed to write
	 */
	static bool UStructToJsonObjectString(const UStruct* StructDefinition, const void* Struct, FString& OutJsonString, int64 CheckFlags, int64 SkipFlags, int32 Indent = 0);

	/**
	 * Converts from a UStruct to a set of json attributes (possibly from within a JsonObject)
	 *
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy out of
	 * @param JsonAttributes Map of attributes to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties failed to write
	 */
	static bool UStructToJsonAttributes(const UStruct* StructDefinition, const void* Struct, TMap< FString, TSharedPtr<FJsonValue> >& OutJsonAttributes, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Converts from a Json Object to a UStruct, using importText
	 *
	 * @param JsonObject Json Object to copy data out of
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	static bool JsonObjectToUStruct(const TSharedRef<FJsonObject>& JsonObject, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Converts a set of json attributes (possibly from within a JsonObject) to a UStruct, using importText
	 *
	 * @param JsonAttributes Json Object to copy data out of
	 * @param StructDefinition UStruct definition that is looked over for properties
	 * @param Struct The UStruct instance to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	static bool JsonAttributesToUStruct(const TMap< FString, TSharedPtr<FJsonValue> >& JsonAttributes, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Converts a single JsonValue to the corresponding UProperty (this may recurse if the property is a UStruct for instance).
	 * 
	 * @param JsonValue The value to assign to this property
	 * @param Property The UProperty definition of the property we're setting.
	 * @param OutValue Pointer to the property instance to be modified.
	 * @param CheckFlags Only convert sub-properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip sub-properties that match any of these flags
	 *
	 * @return False if the property failed to serialize
	 */
	static bool JsonValueToUProperty(const TSharedPtr<FJsonValue> JsonValue, UProperty* Property, void* OutValue, int64 CheckFlags, int64 SkipFlags);

	/**
	 * Converts from a json string containing an object to a UStruct
	 *
	 * @param JsonString String containing JSON formatted data.
	 * @param OutStruct The UStruct instance to copy in to
	 * @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags Skip properties that match any of these flags
	 *
	 * @return False if any properties matched but failed to deserialize
	 */
	template<typename OutStructType>
	static bool JsonObjectStringToUStruct(const FString& JsonString, OutStructType* OutStruct, int64 CheckFlags, int64 SkipFlags)
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
		{
			UE_LOG(LogJson, Warning, TEXT("JsonObjectStringToUStruct - Unable to parse json=[%s]"), *JsonString);
			return false;
		}
		if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), OutStructType::StaticStruct(), OutStruct, CheckFlags, SkipFlags))
		{
			UE_LOG(LogJson, Warning, TEXT("JsonObjectStringToUStruct - Unable to deserialize. json=[%s]"), *JsonString);
			return false;
		}
		return true;
	}

	/**
	* Converts from a json string containing an array to an array of UStructs
	*
	* @param JsonString String containing JSON formatted data.
	* @param OutStructArray The UStruct array to copy in to
	* @param CheckFlags Only convert properties that match at least one of these flags. If 0 check all properties.
	* @param SkipFlags Skip properties that match any of these flags
	*
	* @return False if any properties matched but failed to deserialize
	*/
	template<typename OutStructType>
	static bool JsonArrayStringToUStruct(const FString& JsonString, TArray<OutStructType>* OutStructArray, int64 CheckFlags, int64 SkipFlags)
	{
		TArray<TSharedPtr<FJsonValue> > JsonArray;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(JsonReader, JsonArray))
		{
			UE_LOG(LogJson, Warning, TEXT("JsonArrayStringToUStruct - Unable to parse. json=[%s]"), *JsonString);
			return false;
		}
		OutStructArray->SetNum(JsonArray.Num());
		for (int32 i = 0; i < JsonArray.Num(); ++i)
		{
			const auto& Value = JsonArray[i];
			if (Value->Type != EJson::Object)
			{
				UE_LOG(LogJson, Warning, TEXT("JsonArrayStringToUStruct - One array element was not an object. json=[%s]"), *JsonString);
				return false;
			}
			if (!FJsonObjectConverter::JsonObjectToUStruct(Value->AsObject().ToSharedRef(), OutStructType::StaticStruct(), &(*OutStructArray)[i], CheckFlags, SkipFlags))
			{
				UE_LOG(LogJson, Warning, TEXT("JsonArrayStringToUStruct - Unable to deserialize. json=[%s]"), *JsonString);
				return false;
			}
		}
		return true;
	}

	/* * Converts from a UProperty to a Json Value using exportText
	 *
	 * @param Property			The property to export
	 * @param Value				Pointer to the value of the property
	 * @param CheckFlags		Only convert properties that match at least one of these flags. If 0 check all properties.
	 * @param SkipFlags			Skip properties that match any of these flags
	 *
	 * @return					The constructed JsonValue from the property
	 */
	static TSharedPtr<FJsonValue> UPropertyToJsonValue(UProperty* Property, const void* Value, int64 CheckFlags, int64 SkipFlags);
};

