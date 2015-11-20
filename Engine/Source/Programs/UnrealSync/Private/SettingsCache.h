// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Map.h"
#include "JsonSerializer.h"
#include "JsonObject.h"
#include "JsonReader.h"

/**
 * Cache class for file settings.
 */
class FSettingsCache
{
public:
	/**
	 * Instance getter.
	 */
	static FSettingsCache& Get();

	/**
	 * Gets setting of given type.
	 *
	 * @param Value Output parameter. If succeeded this parameter will be overwritten with setting JSON value.
	 * @param Name Name to look for.
	 *
	 * @returns True if setting was found. False otherwise.
	 */
	bool GetSetting(TSharedPtr<FJsonValue>& Value, const FString& Name);

	/**
	 * Sets setting of given type to given value.
	 *
	 * @param Name Name of the setting.
	 * @param Value Value of the setting.
	 */
	void SetSetting(const FString& Name, TSharedRef<FJsonValue> Value);

	/**
	 * Saves setting to file.
	 */
	void Save();

	/**
	 * TAddConstIf trait to conditionally add const to reference type.
	 */
	template <bool bCondition, typename Type>
	struct TAddConstIf;

	template <bool bCondition, typename Type>
	struct TAddConstIf<bCondition, Type&>
	{
		typedef typename TChooseClass<bCondition, const Type&, Type&>::Result Result;
	};

	/**
	 * Pointer traits to strip TSharedRef or TSharedPtr and return simple
	 * object reference (T&) to unify calls.
	 */
	template <typename T, bool bIsConst>
	struct TPointerTraits
	{
		typedef typename TAddConstIf<bIsConst, T&>::Result ObjectRefType;
		typedef typename TAddConstIf<bIsConst, T&>::Result ParamType;

		static ObjectRefType GetObject(ParamType Object) { return Object; }
	};

	template <typename T, bool bIsConst>
	struct TPointerTraits<TSharedRef<T>, bIsConst>
	{
		typedef typename TAddConstIf<bIsConst, T&>::Result ObjectRefType;
		typedef typename TAddConstIf<bIsConst, TSharedRef<T>&>::Result ParamType;

		static ObjectRefType GetObject(ParamType Object) { return Object.Get(); }
	};

	template <typename T, bool bIsConst>
	struct TPointerTraits<TSharedPtr<T>, bIsConst>
	{
		typedef typename TAddConstIf<bIsConst, T&>::Result ObjectRefType;
		typedef typename TAddConstIf<bIsConst, TSharedPtr<T>&>::Result ParamType;

		static ObjectRefType GetObject(ParamType Object) { return *Object.Get(); }
	};

	/**
	 * Forwards GetStateAsJSON call to preservable object.
	 *
	 * @param Object Object to forward call for.
	 * @returns Returned JSON value from forwarded call.
	 */
	template <typename TPreservableType>
	static TSharedPtr<FJsonValue> GetStateAsJSON(const TPreservableType Object)
	{
		typedef TPointerTraits<TPreservableType, true> T;
		typename T::ObjectRefType Ref = T::GetObject(Object);

		return Ref.GetStateAsJSON();
	}

	/**
	 * Forwards LoadStateFromJSON call to preservable object.
	 *
	 * @param Object Object to forward call for.
	 * @param Value JSON vale to forward.
	 */
	template <typename TPreservableType, typename TValueType>
	static void LoadStateFromJSON(TPreservableType Object, const TValueType Value)
	{
		typedef TPointerTraits<TPreservableType, false> PT;
		typename PT::ObjectRefType Ref = PT::GetObject(Object);

		typedef TPointerTraits<TValueType, true> VT;
		typename VT::ObjectRefType ValRef = VT::GetObject(Value);

		Ref.LoadStateFromJSON(ValRef);
	}

	/**
	 * Saves single object to JSON.
	 *
	 * @param JSONObject JSON object to save data into.
	 * @param FieldName JSON field name for object's data.
	 * @param Object Object to save.
	 */
	template <typename TPreservableType>
	static void SaveObjectToJSON(TSharedRef<FJsonObject> JSONObject, const FString& FieldName, const TPreservableType Object)
	{
		TSharedPtr<FJsonValue> Value = FSettingsCache::GetStateAsJSON(Object);

		if (Value.IsValid())
		{
			JSONObject->SetField(FieldName, Value);
		}
	}

	/**
	 * Loads single object from JSON.
	 *
	 * @param JSONObject JSON object that's containing data to load.
	 * @param FieldName JSON field name for object's data.
	 * @param Object Object to load.
	 */
	template <typename TPreservableType>
	static void LoadObjectFromJSON(const TSharedRef<FJsonObject> JSONObject, const FString& FieldName, TPreservableType Object)
	{
		TSharedPtr<FJsonValue> Field = JSONObject->TryGetField(FieldName);

		if (Field.IsValid())
		{
			LoadStateFromJSON(Object, Field);
		}
	}

	/**
	 * Save objects serialization action policy.
	 */
	struct FSaveObjectsSerializationActionPolicy
	{
	public:
		FSaveObjectsSerializationActionPolicy(TSharedRef<FJsonObject> InJSON)
			: JSON(MoveTemp(InJSON))
		{}

		/**
		 * Tells what to do on serialization action. It just saves serialized
		 * object to JSON.
		 *
		 * @param FieldName JSON field name.
		 * @param Object Object to save.
		 */
		template <class TPreservableType>
		void Serialize(const FString& FieldName, const TPreservableType Object) const
		{
			FSettingsCache::SaveObjectToJSON(JSON, FieldName, Object);
		}

	private:
		/* Stored JSON object for serialization. */
		TSharedRef<FJsonObject> JSON;
	};

	/**
	 * Saves object to JSON using TSerializationPolicy.
	 *
	 * @template_param TSerializationPolicy Policy that tells the method how to
	 *		serialize Object. This method uses const version, marked by first
	 *		template parameter equals to true.
	 * @param JSON Destination JSON object.
	 * @param Object Source object.
	 */
	template <template <bool> class TSerializationPolicy, typename TObjectToSerializeType>
	static void SerializeToJSON(TSharedRef<FJsonObject> JSON, const TObjectToSerializeType& Object)
	{
		FSaveObjectsSerializationActionPolicy Action(JSON);
		TSerializationPolicy<true>::Serialize(Action, Object);
	}

	/**
	 * Load objects serialization action policy.
	 */
	struct FLoadObjectsSerializationActionPolicy
	{
	public:
		FLoadObjectsSerializationActionPolicy(TSharedRef<FJsonObject> InJSON)
			: JSON(MoveTemp(InJSON))
		{}

		/**
		 * Tells what to do on serialization action. It just loads serialized
		 * object from JSON.
		 *
		 * @param FieldName JSON field name.
		 * @param Object Object to load.
		 */
		template <class TPreservableType>
		void Serialize(const FString& FieldName, TPreservableType Object)
		{
			FSettingsCache::LoadObjectFromJSON(JSON, FieldName, Object);
		}

	private:
		/* Stored JSON object for serialization. */
		TSharedRef<FJsonObject> JSON;
	};

	/**
	 * Loads object from JSON using TSerializationPolicy.
	 *
	 * @template_param TSerializationPolicy Policy that tells the method how to
	 *		serialize Object. This method uses non-const version, marked by
	 *		first template parameter equals to false.
	 * @param JSON Data source.
	 * @param Object Destination object.
	 */
	template <template <bool> class TSerializationPolicy, typename TObjectToSerializeType>
	static void SerializeFromJSON(TSharedRef<FJsonObject> JSON, TObjectToSerializeType& Object)
	{
		FLoadObjectsSerializationActionPolicy Action(JSON);
		TSerializationPolicy<false>::Serialize(Action, Object);
	}

private:
	/**
	 * Constructor.
	 *
	 * Creates the cache from value read from settings file if it exists.
	 */
	FSettingsCache();

	/**
	 * Gets settings file name. For now it's hardcoded.
	 *
	 * @returns Settings file name.
	 */
	static const FString& GetSettingsFileName();

	/** Map of settings' JSON values. */
	TMap<FString, TSharedRef<FJsonValue> > Settings;
};
