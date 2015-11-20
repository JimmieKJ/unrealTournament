// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Json.h"
#include "OnlineKeyValuePair.h"

/**
 * Macros used to generate a serialization function for a class derived from FJsonSerializable
 */
#define BEGIN_ONLINE_JSON_SERIALIZER \
	virtual void Serialize(FOnlineJsonSerializerBase& Serializer, bool bFlatObject) override \
	{ \
		if (!bFlatObject) { Serializer.StartObject(); }

#define END_ONLINE_JSON_SERIALIZER \
		if (!bFlatObject) { Serializer.EndObject(); } \
	}

#define ONLINE_JSON_SERIALIZE(JsonName, JsonValue) \
		Serializer.Serialize(TEXT(JsonName), JsonValue)

#define ONLINE_JSON_SERIALIZE_ARRAY(JsonName, JsonArray) \
	    Serializer.SerializeArray(TEXT(JsonName), JsonArray)

#define ONLINE_JSON_SERIALIZE_MAP(JsonName, JsonMap) \
		Serializer.SerializeMap(TEXT(JsonName), JsonMap)

#define ONLINE_JSON_SERIALIZE_SERIALIZABLE(JsonName, JsonValue) \
		JsonValue.Serialize(Serializer, false)

#define ONLINE_JSON_SERIALIZE_ARRAY_SERIALIZABLE(JsonName, JsonArray, ElementType) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasTypedField<EJson::Array>(JsonName)) \
			{ \
				for (auto It = Serializer.GetObject()->GetArrayField(JsonName).CreateConstIterator(); It; ++It) \
				{ \
					ElementType* Obj = new(JsonArray) ElementType(); \
					Obj->FromJson((*It)->AsObject()); \
				} \
			} \
		} \
		else \
		{ \
			Serializer.StartArray(JsonName); \
			for (auto It = JsonArray.CreateIterator(); It; ++It) \
			{ \
				It->Serialize(Serializer, false); \
			} \
			Serializer.EndArray(); \
		}

#define ONLINE_JSON_SERIALIZE_MAP_SERIALIZABLE(JsonName, JsonMap, ElementType) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasTypedField<EJson::Object>(JsonName)) \
			{ \
				TSharedPtr<FJsonObject> JsonObj = Serializer.GetObject()->GetObjectField(JsonName); \
				for (auto MapIt = JsonObj->Values.CreateConstIterator(); MapIt; ++MapIt) \
				{ \
					ElementType NewEntry; \
					NewEntry.FromJson(MapIt.Value()->AsObject()); \
					JsonMap.Add(MapIt.Key(), NewEntry); \
				} \
			} \
		} \
		else \
		{ \
			Serializer.StartObject(JsonName); \
			for (auto It = JsonMap.CreateIterator(); It; ++It) \
			{ \
				Serializer.StartObject(It.Key()); \
				It.Value().Serialize(Serializer, true); \
				Serializer.EndObject(); \
			} \
			Serializer.EndObject(); \
		}

#define ONLINE_JSON_SERIALIZE_OBJECT_SERIALIZABLE(JsonName, JsonSerializableObject) \
		Serializer.SerializeObject(TEXT(JsonName), JsonSerializableObject);

/** Array of string data */
typedef TArray<FString> FJsonSerializableArray;

/** Maps a key to a value */
typedef FOnlineKeyValuePairs<FString, FString> FJsonSerializableKeyValueMap;
typedef FOnlineKeyValuePairs<FString, int32> FJsonSerializableKeyValueMapInt;
typedef FOnlineKeyValuePairs<FString, FVariantData> FJsonSerializeableKeyValueMapVariant;

struct FOnlineJsonSerializable;

/**
 * Base interface used to serialize to/from JSON. Hides the fact there are separate read/write classes
 */
struct FOnlineJsonSerializerBase
{
	virtual bool IsLoading() const = 0;
	virtual bool IsSaving() const = 0;
	virtual void StartObject() = 0;
	virtual void StartObject(const FString& Name) = 0;
	virtual void EndObject() = 0;
	virtual void StartArray() = 0;
	virtual void StartArray(const FString& Name) = 0;
	virtual void EndArray() = 0;
	virtual void Serialize(const TCHAR* Name, int32& Value) = 0;
	virtual void Serialize(const TCHAR* Name, uint32& Value) = 0;
	virtual void Serialize(const TCHAR* Name, bool& Value) = 0;
	virtual void Serialize(const TCHAR* Name, FString& Value) = 0;
	virtual void Serialize(const TCHAR* Name, FText& Value) = 0;
	virtual void Serialize(const TCHAR* Name, float& Value) = 0;
	virtual void Serialize(const TCHAR* Name, double& Value) = 0;
	virtual void Serialize(const TCHAR* Name, FDateTime& Value) = 0;
	virtual void SerializeObject(const TCHAR* Name, FOnlineJsonSerializable& Value) = 0;
	virtual void SerializeArray(FJsonSerializableArray& Array) = 0;
	virtual void SerializeArray(const TCHAR* Name, FJsonSerializableArray& Value) = 0;
	virtual void SerializeMap(const TCHAR* Name, FJsonSerializableKeyValueMap& Map) = 0;
	virtual void SerializeMap(const TCHAR* Name, FJsonSerializableKeyValueMapInt& Map) = 0;
	virtual TSharedPtr<FJsonObject> GetObject() = 0;
};

/**
 * Implements the abstract serializer interface hiding the underlying writer object
 */
template <class CharType = TCHAR, class PrintPolicy = TPrettyJsonPrintPolicy<CharType> >
class FOnlineJsonSerializerWriter :
	public FOnlineJsonSerializerBase
{
	/** The object to write the JSON output to */
	TSharedRef<TJsonWriter<CharType, PrintPolicy> > JsonWriter;

public:

	/**
	 * Initializes the writer object
	 *
	 * @param InJsonWriter the object to write the JSON output to
	 */
	FOnlineJsonSerializerWriter(TSharedRef<TJsonWriter<CharType, PrintPolicy> > InJsonWriter) :
		JsonWriter(InJsonWriter)
	{
	}

    /** Is the JSON being read from */
	virtual bool IsLoading() const override { return false; }
    /** Is the JSON being written to */
	virtual bool IsSaving() const override { return true; }
	/** Access to the root object */
	virtual TSharedPtr<FJsonObject> GetObject() override { return TSharedPtr<FJsonObject>(); }

	/**
	 * Starts a new object "{"
	 */
	virtual void StartObject() override
	{
		JsonWriter->WriteObjectStart();
	}

	/**
	 * Starts a new object "{"
	 */
	virtual void StartObject(const FString& Name) override
	{
		JsonWriter->WriteObjectStart(Name);
	}
	/**
	 * Completes the definition of an object "}"
	 */
	virtual void EndObject() override
	{
		JsonWriter->WriteObjectEnd();
	}

	virtual void StartArray() override
	{
		JsonWriter->WriteArrayStart();
	}

	virtual void StartArray(const FString& Name) override
	{
		JsonWriter->WriteArrayStart(Name);
	}

	virtual void EndArray() override
	{
		JsonWriter->WriteArrayEnd();
	}
	/**
	 * Writes the field name and the corresponding value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @param Value the value to write out
	 */
	virtual void Serialize(const TCHAR* Name, int32& Value) override
	{
		JsonWriter->WriteValue(Name, Value);
	}
	/**
	 * Writes the field name and the corresponding value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @param Value the value to write out
	 */
	virtual void Serialize(const TCHAR* Name, uint32& Value) override
	{
		JsonWriter->WriteValue(Name, static_cast<int64>(Value));
	}
	/**
	 * Writes the field name and the corresponding value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @param Value the value to write out
	 */
	virtual void Serialize(const TCHAR* Name, bool& Value) override
	{
		JsonWriter->WriteValue(Name, Value);
	}
	/**
	 * Writes the field name and the corresponding value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @param Value the value to write out
	 */
	virtual void Serialize(const TCHAR* Name, FString& Value) override
	{
		JsonWriter->WriteValue(Name, Value);
	}
	/**
	 * Writes the field name and the corresponding value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @param Value the value to write out
	 */
	virtual void Serialize(const TCHAR* Name, FText& Value) override
	{
		JsonWriter->WriteValue(Name, Value.ToString());
	}
	/**
	 * Writes the field name and the corresponding value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @param Value the value to write out
	 */
	virtual void Serialize(const TCHAR* Name, float& Value) override
	{
		JsonWriter->WriteValue(Name, Value);
	}
	/**
	* Writes the field name and the corresponding value to the JSON data
	*
	* @param Name the field name to write out
	* @param Value the value to write out
	*/
	virtual void Serialize(const TCHAR* Name, double& Value) override
	{
		JsonWriter->WriteValue(Name, Value);
	}
	/**
	* Writes the field name and the corresponding value to the JSON data
	*
	* @param Name the field name to write out
	* @param Value the value to write out
	*/
	virtual void Serialize(const TCHAR* Name, FDateTime& Value) override
	{
		if (Value.GetTicks() > 0)
		{
			JsonWriter->WriteValue(Name, Value.ToIso8601());
		}
	}
	/**
	 * Writes the field name and corresponding object value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @param Object the object to write out
	 */
	virtual void SerializeObject(const TCHAR* Name, FOnlineJsonSerializable& Object) override;
	/**
	 * Serializes an array of values
	 *
	 * @param Name the name of the property to serialize
	 * @param Array the array to serialize
	 */
	virtual void SerializeArray(FJsonSerializableArray& Array) override
	{
		JsonWriter->WriteArrayStart();
		// Iterate all of values
		for (FJsonSerializableArray::TIterator ArrayIt(Array); ArrayIt; ++ArrayIt)
		{
			JsonWriter->WriteValue(*ArrayIt);
		}
		JsonWriter->WriteArrayEnd();
	}
	/**
	 * Serializes an array of values with an identifier
	 *
	 * @param Name the name of the property to serialize
	 * @param Array the array to serialize
	 */
	virtual void SerializeArray(const TCHAR* Name, FJsonSerializableArray& Array) override
	{
		JsonWriter->WriteArrayStart(Name);
		// Iterate all of values
		for (FJsonSerializableArray::TIterator ArrayIt(Array); ArrayIt; ++ArrayIt)
		{
			JsonWriter->WriteValue(*ArrayIt);
		}
		JsonWriter->WriteArrayEnd();
	}
	/**
	 * Serializes the keys & values for map
	 *
	 * @param Name the name of the property to serialize
	 * @param Map the map to serialize
	 */
	virtual void SerializeMap(const TCHAR* Name, FJsonSerializableKeyValueMap& Map) override
	{
		JsonWriter->WriteObjectStart(Name);
		// Iterate all of the keys and their values
		for (FJsonSerializableKeyValueMap::TIterator KeyValueIt(Map); KeyValueIt; ++KeyValueIt)
		{
			Serialize(*(KeyValueIt.Key()), KeyValueIt.Value());
		}
		JsonWriter->WriteObjectEnd();
	}
	/**
	 * Serializes the keys & values for map
	 *
	 * @param Name the name of the property to serialize
	 * @param Map the map to serialize
	 */
	virtual void SerializeMap(const TCHAR* Name, FJsonSerializableKeyValueMapInt& Map) override
	{
		JsonWriter->WriteObjectStart(Name);
		// Iterate all of the keys and their values
		for (FJsonSerializableKeyValueMapInt::TIterator KeyValueIt(Map); KeyValueIt; ++KeyValueIt)
		{
			Serialize(*(KeyValueIt.Key()), KeyValueIt.Value());
		}
		JsonWriter->WriteObjectEnd();
	}
};

/**
 * Implements the abstract serializer interface hiding the underlying reader object
 */
class FOnlineJsonSerializerReader :
	public FOnlineJsonSerializerBase
{
	/** The object that holds the parsed JSON data */
	TSharedPtr<FJsonObject> JsonObject;

public:
	/**
	 * Inits the base JSON object that is being read from
	 *
	 * @param InJsonObject the JSON object to serialize from
	 */
	FOnlineJsonSerializerReader(TSharedPtr<FJsonObject> InJsonObject) :
		JsonObject(InJsonObject)
	{
	}

    /** Is the JSON being read from */
	virtual bool IsLoading() const override { return true; }
    /** Is the JSON being written to */
	virtual bool IsSaving() const override { return false; }
	/** Access to the root Json object being read */
	virtual TSharedPtr<FJsonObject> GetObject() override { return JsonObject; }

	/** Ignored */
	virtual void StartObject() override
	{
		// Empty on purpose
	}
	/** Ignored */
	virtual void StartObject(const FString& Name) override
	{
		// Empty on purpose
	}
	/** Ignored */
	virtual void EndObject() override
	{
		// Empty on purpose
	}
	/** Ignored */
	virtual void StartArray() override
	{
		// Empty on purpose
	}
	/** Ignored */
	virtual void StartArray(const FString& Name) override
	{
		// Empty on purpose
	}
	/** Ignored */
	virtual void EndArray() override
	{
		// Empty on purpose
	}
	/**
	 * If the underlying json object has the field, it is read into the value
	 *
	 * @param Name the name of the field to read
	 * @param Value the out value to read the data into
	 */
	virtual void Serialize(const TCHAR* Name, int32& Value) override
	{
		if (JsonObject->HasTypedField<EJson::Number>(Name))
		{
			JsonObject->TryGetNumberField(Name, Value);
		}
	}
	/**
	 * If the underlying json object has the field, it is read into the value
	 *
	 * @param Name the name of the field to read
	 * @param Value the out value to read the data into
	 */
	virtual void Serialize(const TCHAR* Name, uint32& Value) override
	{
		if (JsonObject->HasTypedField<EJson::Number>(Name))
		{
			JsonObject->TryGetNumberField(Name, Value);
		}
	}
	/**
	 * If the underlying json object has the field, it is read into the value
	 *
	 * @param Name the name of the field to read
	 * @param Value the out value to read the data into
	 */
	virtual void Serialize(const TCHAR* Name, bool& Value) override
	{
		if (JsonObject->HasTypedField<EJson::Boolean>(Name))
		{
			Value = JsonObject->GetBoolField(Name);
		}
	}
	/**
	 * If the underlying json object has the field, it is read into the value
	 *
	 * @param Name the name of the field to read
	 * @param Value the out value to read the data into
	 */
	virtual void Serialize(const TCHAR* Name, FString& Value) override
	{
		if (JsonObject->HasTypedField<EJson::String>(Name))
		{
			Value = JsonObject->GetStringField(Name);
		}
	}
	/**
	 * If the underlying json object has the field, it is read into the value
	 *
	 * @param Name the name of the field to read
	 * @param Value the out value to read the data into
	 */
	virtual void Serialize(const TCHAR* Name, FText& Value) override
	{
		if (JsonObject->HasTypedField<EJson::String>(Name))
		{
			Value = FText::FromString(JsonObject->GetStringField(Name));
		}
	}
	/**
	 * If the underlying json object has the field, it is read into the value
	 *
	 * @param Name the name of the field to read
	 * @param Value the out value to read the data into
	 */
	virtual void Serialize(const TCHAR* Name, float& Value) override
	{
		if (JsonObject->HasTypedField<EJson::Number>(Name))
		{
			Value = JsonObject->GetNumberField(Name);
		}
	}
	/**
	* If the underlying json object has the field, it is read into the value
	*
	* @param Name the name of the field to read
	* @param Value the out value to read the data into
	*/
	virtual void Serialize(const TCHAR* Name, double& Value) override
	{
		if (JsonObject->HasTypedField<EJson::Number>(Name))
		{
			Value = JsonObject->GetNumberField(Name);
		}
	}
	/**
	* Writes the field name and the corresponding value to the JSON data
	*
	* @param Name the field name to write out
	* @param Value the value to write out
	*/
	virtual void Serialize(const TCHAR* Name, FDateTime& Value) override
	{
		if (JsonObject->HasTypedField<EJson::String>(Name))
		{
			FDateTime::ParseIso8601(*JsonObject->GetStringField(Name), Value);
		}
	}
	/**
	 * Writes the field name and the corresponding object value to the JSON data
	 *
	 * @param Name the field name to write out
	 * @Object Value the object to write out
	 */
	virtual void SerializeObject(const TCHAR* Name, FOnlineJsonSerializable& Object) override;
	/**
	 * Serializes an array of values
	 *
	 * @param Name the name of the property to serialize
	 * @param Array the array to serialize
	 */
	virtual void SerializeArray(FJsonSerializableArray& Array) override
	{
		// @todo - higher level serialization is expecting a Json Object
		check(0 && TEXT("Not implemented"));
	}
	/**
	 * Serializes an array of values with an identifier
	 *
	 * @param Name the name of the property to serialize
	 * @param Array the array to serialize
	 */
	virtual void SerializeArray(const TCHAR* Name, FJsonSerializableArray& Array) override
	{
		if (JsonObject->HasTypedField<EJson::Array>(Name))
		{
			TArray< TSharedPtr<FJsonValue> > JsonArray = JsonObject->GetArrayField(Name);
			// Iterate all of the keys and their values
			for (auto ArrayIt = JsonArray.CreateConstIterator(); ArrayIt; ++ArrayIt)
			{
				Array.Add((*ArrayIt)->AsString());
			}
		}
	}
	/**
	 * Serializes the keys & values for map
	 *
	 * @param Name the name of the property to serialize
	 * @param Map the map to serialize
	 */
	virtual void SerializeMap(const TCHAR* Name, FJsonSerializableKeyValueMap& Map) override
	{
		if (JsonObject->HasTypedField<EJson::Object>(Name))
		{
			TSharedPtr<FJsonObject> JsonMap = JsonObject->GetObjectField(Name);
			// Iterate all of the keys and their values
			for (auto KeyValueIt = JsonMap->Values.CreateConstIterator(); KeyValueIt; ++KeyValueIt)
			{
				FString Value = JsonMap->GetStringField(KeyValueIt.Key());
				Map.Add(KeyValueIt.Key(), Value);
			}
		}
	}

	/**
	 * Serializes the keys & values for map
	 *
	 * @param Name the name of the property to serialize
	 * @param Map the map to serialize
	 */
	virtual void SerializeMap(const TCHAR* Name, FJsonSerializableKeyValueMapInt& Map) override
	{
		if (JsonObject->HasTypedField<EJson::Object>(Name))
		{
			TSharedPtr<FJsonObject> JsonMap = JsonObject->GetObjectField(Name);
			// Iterate all of the keys and their values
			for (auto KeyValueIt = JsonMap->Values.CreateConstIterator(); KeyValueIt; ++KeyValueIt)
			{
				int32 Value = JsonMap->GetNumberField(KeyValueIt.Key());
				Map.Add(KeyValueIt.Key(), Value);
			}
		}
	}
};

/**
 * Base class for a JSON serializable object
 */
struct FOnlineJsonSerializable
{
	/**
	 * Used to allow serialization of a const ref
	 *
	 * @return the corresponding json string
	 */
	inline const FString ToJson() const
	{
		// Strip away const, because we use a single method that can read/write which requires non-const semantics
		// Otherwise, we'd have to have 2 separate macros for declaring const to json and non-const from json
		return ((FOnlineJsonSerializable*)this)->ToJson();
	}
	/**
	 * Serializes this object to its JSON string form
	 *
	 * @param bPrettyPrint - If true, will use the pretty json formatter
	 * @return the corresponding json string
	 */
	virtual const FString ToJson(bool bPrettyPrint=true)
	{
		FString JsonStr;
		if (bPrettyPrint)
		{
			TSharedRef<TJsonWriter<> > JsonWriter = TJsonWriterFactory<>::Create(&JsonStr);
			FOnlineJsonSerializerWriter<> Serializer(JsonWriter);
			Serialize(Serializer, false);
			JsonWriter->Close();
		}
		else
		{
			TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy< TCHAR > > > JsonWriter = TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy< TCHAR > >::Create( &JsonStr );
			FOnlineJsonSerializerWriter<TCHAR, TCondensedJsonPrintPolicy< TCHAR >> Serializer(JsonWriter);
			Serialize(Serializer, false);
			JsonWriter->Close();
		}
		return JsonStr;
	}
	virtual void ToJson(TSharedRef<TJsonWriter<> >& JsonWriter, bool bFlatObject) const
	{
		FOnlineJsonSerializerWriter<> Serializer(JsonWriter);
		((FOnlineJsonSerializable*)this)->Serialize(Serializer, bFlatObject);
	}
	virtual void ToJson(TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy< TCHAR > > >& JsonWriter, bool bFlatObject) const
	{
		FOnlineJsonSerializerWriter<TCHAR, TCondensedJsonPrintPolicy< TCHAR >> Serializer(JsonWriter);
		((FOnlineJsonSerializable*)this)->Serialize(Serializer, bFlatObject);
	}
	/**
	 * Serializes the contents of a JSON string into this object
	 *
	 * @param Json the JSON data to serialize from
	 */
	virtual bool FromJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(JsonReader,JsonObject) &&
			JsonObject.IsValid())
		{
			FOnlineJsonSerializerReader Serializer(JsonObject);
			Serialize(Serializer, false);
			return true;
		}
		return false;
	}
	virtual bool FromJson(TSharedPtr<FJsonObject> JsonObject)
	{
		if (JsonObject.IsValid())
		{
			FOnlineJsonSerializerReader Serializer(JsonObject);
			Serialize(Serializer, false);
			return true;
		}
		return false;
	}

	/**
	 * Abstract method that needs to be supplied using the macros
	 *
	 * @param Serializer the object that will perform serialization in/out of JSON
	 * @param bFlatObject if true then no object wrapper is used
	 */
	virtual void Serialize(FOnlineJsonSerializerBase& Serializer, bool bFlatObject) = 0;
};

inline void FOnlineJsonSerializerReader::SerializeObject(const TCHAR* Name, FOnlineJsonSerializable& Value)
{
	/* Read in the value from the Name field */
	if (GetObject()->HasTypedField<EJson::Object>(Name))
	{
		TSharedPtr<FJsonObject> JsonObj = GetObject()->GetObjectField(Name);
		if (JsonObj.IsValid())
		{
			Value.FromJson(JsonObj);
		}
	}
}

template <class CharType, class PrintPolicy>
inline void FOnlineJsonSerializerWriter<CharType, PrintPolicy>::SerializeObject(const TCHAR* Name, FOnlineJsonSerializable& Object)
{
	/* Write the value to the Name field */
	JsonWriter->WriteIdentifierPrefix(Name);
	Object.Serialize(*this, true);
}
