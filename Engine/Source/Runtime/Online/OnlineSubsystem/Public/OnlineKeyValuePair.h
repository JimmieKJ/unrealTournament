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
class FOnlineKeyValuePairs
{

private:

	struct FKeyValuePairInternal 
	{
		KeyType Key;
		ValueType Value;

		FKeyValuePairInternal(KeyType InKey, const ValueType& InValue) :
			Key(InKey),
			Value(InValue)
		{
		}

		/**
		 *	Comparison operator
		 */
		bool operator==(const FKeyValuePairInternal& Other) const
		{
			// Only the key matters for equality
			return Key == Other.Key;
		}
	};
	
	/** Array of all key value pairs */
	TArray<FKeyValuePairInternal> KeyValuePairs;

public:
	/**
	 *	Finds a key value pair by key name
	 * @param Key key to search for
	 * @return pointer to value paired with key
	 */
	const ValueType* Find(KeyType Key) const
	{
		// Search for the individual element
		for (int32 PairIdx = 0; PairIdx < KeyValuePairs.Num(); ++PairIdx)
		{
			const FKeyValuePairInternal& Setting = KeyValuePairs[PairIdx];
			if (Setting.Key == Key)
			{
				return &Setting.Value;
			}
		}

		return NULL;
	}

	/**
	 *	Finds a key value pair by key name
	 * @param Key key to search for
	 * @return pointer to value paired with key
	 */
	ValueType* Find(KeyType Key)
	{
		// Search for the individual element
		for (int32 PairIdx = 0; PairIdx < KeyValuePairs.Num(); ++PairIdx)
		{
			FKeyValuePairInternal& Setting = KeyValuePairs[PairIdx];
			if (Setting.Key == Key)
			{
				return &Setting.Value;
			}
		}

		return NULL;
	}

	/**
	 *	Add an element to the grouping, overwriting existing values
	 *
	 * @param Key key to assign to the value
	 * @param Value value to store
	 */
	ValueType* Add(KeyType Key, const ValueType& Value)
	{
		ValueType* ExistingElement = Find(Key);
		if (ExistingElement)
		{
			*ExistingElement = Value;
			return ExistingElement;
		}
		else
		{
			int32 Idx = KeyValuePairs.Add(FKeyValuePairInternal(Key, Value));
			return &KeyValuePairs[Idx].Value;
		}
	}

	/**
	 *	Remove an element from the grouping
	 *
	 * @param Key key to assign to the value
	 *
	 * @return true if found and removed, false otherwise
	 */
	bool Remove(KeyType Key)
	{
		for (int32 PairIdx = 0; PairIdx < KeyValuePairs.Num(); ++PairIdx)
		{
			FKeyValuePairInternal& Setting = KeyValuePairs[PairIdx];
			if (Setting.Key == Key)
			{
				KeyValuePairs.RemoveAtSwap(PairIdx);
				return true;
			}
		}

		return false;
	}

	/**
	 *	Append another FOnlineKeyValuePairs 
	 *
	 * @param Source key value pairs to append
	 */
	void Append(const FOnlineKeyValuePairs& Source)
	{
		KeyValuePairs.Append(Source.KeyValuePairs);
	}

	/**
	 *	Remove all elements from the grouping
	 */
	void Empty()
	{
		KeyValuePairs.Empty();
	}

	/** @return number of elements in grouping */
	int32 Num() const
	{
		return KeyValuePairs.Num();
	}

	/** Base class for const/nonconst iterators */
	template<bool bConst>
	class TBaseIterator
	{
	private:

		/** Define the data type to iterate on as const/nonconst */
		typedef typename TChooseClass<bConst, const ValueType, ValueType>::Result PairsType;
		/** Define the internal iterator as const/nonconst */
		typedef typename TChooseClass<bConst, typename TArray<FKeyValuePairInternal>::TConstIterator, typename TArray<FKeyValuePairInternal>::TIterator>::Result PairsItType;

		/** Internal iterator over array */
		PairsItType PairsIt;

	public:

		/** Initialization constructor. */
		TBaseIterator(const FOnlineKeyValuePairs& InPair, int32 StartIndex) :
			PairsIt(InPair.KeyValuePairs)
		{
			PairsIt += StartIndex;
		}

		/** Initialization constructor. */
		TBaseIterator(FOnlineKeyValuePairs& InPair, int32 StartIndex) :
			PairsIt(InPair.KeyValuePairs)
		{
			PairsIt += StartIndex;
		}

		/** Prefix op */
		TBaseIterator& operator++()	{ ++PairsIt; return *this; }

		SAFE_BOOL_OPERATORS(TBaseIterator<bConst>)

		/** Conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE_EXPLICIT_OPERATOR_BOOL() const
		{ 
			return !!PairsIt; 
		}
		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const 
		{
			return !(bool)*this;
		}

		/** @return the current key */
		KeyType Key() const		{ return PairsIt->Key; }
		/** @return the current value */
		PairsType& Value() const	{ return PairsIt->Value; }
	};

	/** Used to iterate over the elements of a const array of pairs. */
	class TConstIterator : public TBaseIterator<true>
	{
	public:
		TConstIterator(const FOnlineKeyValuePairs& InPairs,int32 StartIndex = 0) :
		  TBaseIterator<true>(InPairs, StartIndex)
		{
		}
	};

	/** Used to iterate over the elements of an array of pairs. */
	class TIterator : public TBaseIterator<false>
	{
	public:
		TIterator(FOnlineKeyValuePairs& InPairs,int32 StartIndex = 0) :
			TBaseIterator<false>(InPairs, StartIndex)
		{
		}
	};
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

