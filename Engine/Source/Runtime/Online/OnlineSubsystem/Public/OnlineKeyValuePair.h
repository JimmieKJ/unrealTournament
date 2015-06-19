// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "OnlineSubsystemPackage.h"

namespace EOnlineKeyValuePairDataType
{
	enum Type
	{
		/** Means the data in the OnlineData value fields should be ignored */
		Empty,
		/** 32 bit integer */
		Int32,
		/** 64 bit integer */
		Int64,
		/** Double (8 byte) */
		Double,
		/** Unicode string */
		String,
		/** Float (4 byte) */
		Float,
		/** Binary data */
		Blob,
		/** bool data (1 byte) */
		Bool,
		MAX
	};

	/** @return the stringified version of the enum passed in */
	inline const TCHAR* ToString(EOnlineKeyValuePairDataType::Type EnumVal)
	{
		switch (EnumVal)
		{
		case Empty:
			return TEXT("Empty");
		case Int32:
			return TEXT("Int32");
		case Int64:
			return TEXT("Int64");
		case Double:
			return TEXT("Double");
		case String:
			return TEXT("String");
		case Float:
			return TEXT("Float");
		case Blob:
			return TEXT("Blob");
		case Bool:
			return TEXT("Bool");
		default:
			return TEXT("");
		}		
	}
}

/**
 *	Associative container for key value pairs
 */
template<class KeyType, class ValueType>
class FOnlineKeyValuePairs : public TMap<KeyType,ValueType>
{
	typedef TMap<KeyType, ValueType> Super;

public:

	FORCEINLINE FOnlineKeyValuePairs() {}
	FORCEINLINE FOnlineKeyValuePairs(FOnlineKeyValuePairs&& Other) : Super(MoveTemp(Other)) {}
	FORCEINLINE FOnlineKeyValuePairs(const FOnlineKeyValuePairs&  Other) : Super(Other) {}
	FORCEINLINE FOnlineKeyValuePairs& operator=(FOnlineKeyValuePairs&& Other) { Super::operator=(MoveTemp(Other)); return *this; }
	FORCEINLINE FOnlineKeyValuePairs& operator=(const FOnlineKeyValuePairs&  Other) { Super::operator=(Other); return *this; }

};

/**
 *	Container for storing data of variable type
 */
class ONLINESUBSYSTEM_API FVariantData
{

private:

	/** Current data type */
	EOnlineKeyValuePairDataType::Type Type;
	/** Union of all possible types that can be stored */
	union ValueUnion
	{
		bool AsBool;
		int32 AsInt;
		float AsFloat;
		uint64 AsInt64;
		double AsDouble;
		TCHAR* AsTCHAR;
		struct 
		{ 
			uint8* BlobData;
			uint32 BlobSize;
		} AsBlob;

		ValueUnion() { FMemory::Memset( this, 0, sizeof( ValueUnion ) ); }
	} Value;

public:

	/** Constructor */
	FVariantData() :
		Type(EOnlineKeyValuePairDataType::Empty)
	{
	}

	/** Constructor starting with an initialized value/type */
	template<typename ValueType> 
	FVariantData(const ValueType& InData) :
		Type(EOnlineKeyValuePairDataType::Empty)
	{
		SetValue(InData);
	}

	/**
	 * Copy constructor. Copies the other into this object
	 *
	 * @param Other the other structure to copy
	 */
	FVariantData(const FVariantData& Other);

	/**
	 * Assignment operator. Copies the other into this object
	 *
	 * @param Other the other structure to copy
	 */
	FVariantData& operator=(const FVariantData& Other);

	/**
	 * Cleans up the data to prevent leaks
	 */
	~FVariantData()
	{
		Empty();
	}

	/**
	 * Cleans up the existing data and sets the type to ODT_Empty
	 */
	void Empty();

	/**
	 *	Get the key for this key value pair
	 */
	const EOnlineKeyValuePairDataType::Type GetType() const
	{
		return Type;
	}

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(const FString& InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(const TCHAR* InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(int32 InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(bool InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(double InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(float InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(const TArray<uint8>& InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param Size the length of the buffer to copy
	 * @param InData the new data to assign
	 */
	void SetValue(uint32 Size, const uint8* InData);

	/**
	 * Copies the data and sets the type
	 *
	 * @param InData the new data to assign
	 */
	void SetValue(uint64 InData);

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(FString& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(int32& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(bool& OutData) const;

	/**
	  Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(uint64& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(float& OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(TArray<uint8>& OutData) const;

	/**
	 * Copies the data after verifying the type.
	 * NOTE: Performs a deep copy so you are responsible for freeing the data
	 *
	 * @param OutSize out value that receives the size of the copied data
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(uint32& OutSize,uint8** OutData) const;

	/**
	 * Copies the data after verifying the type
	 *
	 * @param OutData out value that receives the copied data
	 */
	void GetValue(double& OutData) const;

	/**
	 * Increments the value by the specified amount
	 * 
	 * @param IncBy the amount to increment by
	 */
	template<typename TYPE, EOnlineKeyValuePairDataType::Type ENUM_TYPE>
	FORCEINLINE void Increment(TYPE IncBy)
	{
		checkSlow(Type == EOnlineKeyValuePairDataType::Int32 || Type == EOnlineKeyValuePairDataType::Int64 ||
					Type == EOnlineKeyValuePairDataType::Float || Type == EOnlineKeyValuePairDataType::Double);
		if (Type == ENUM_TYPE)
		{
			*(TYPE*)&Value += IncBy;
		}
	}

	/**
	 * Decrements the value by the specified amount
	 *
	 * @param DecBy the amount to decrement by
	 */
	template<typename TYPE, EOnlineKeyValuePairDataType::Type ENUM_TYPE>
	FORCEINLINE void Decrement(TYPE DecBy)
	{
		checkSlow(Type == EOnlineKeyValuePairDataType::Int32 || Type == EOnlineKeyValuePairDataType::Int64 ||
					Type == EOnlineKeyValuePairDataType::Float || Type == EOnlineKeyValuePairDataType::Double);
		if (Type == ENUM_TYPE)
		{
			*(TYPE*)&Value -= DecBy;
		}
	}

	/**
	 * Converts the data into a string representation
	 */
	FString ToString() const;

	/**
	 * Converts the string to the specified type of data for this setting
	 *
	 * @param NewValue the string value to convert
	 *
	 * @return true if it was converted, false otherwise
	 */
	bool FromString(const FString& NewValue);

	/** @return The type as a string */
	const TCHAR* GetTypeString() const
	{
		return EOnlineKeyValuePairDataType::ToString(Type);
	}

	/**
	 * Convert variant data to json object with "type,value" fields
	 *
	 * @return json object representation
	 */
	TSharedRef<class FJsonObject> ToJson() const;
	
	/**
	 * Convert json object to variant data from "type,value" fields
	 *
	 * @param JsonObject json to convert from
	 * @return true if conversion was successful
	 */
	bool FromJson(const TSharedRef<class FJsonObject>& JsonObject);

	/**
	* Comparison of two settings data classes
	*
	* @param Other the other settings data to compare against
	*
	* @return true if they are equal, false otherwise
	*/
	bool operator==(const FVariantData& Other) const;
	bool operator!=(const FVariantData& Other) const;
};

