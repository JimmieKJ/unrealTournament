// TAttributeProperty provides a way to link a TAttribute and Slate setter delegate directly to a UObject property
// without having to write "custom" (but really duplicated code) handlers for each such property
// note that since delegates hold weak pointers, you will need to keep a TSharedPtr reference to the constructed TAttributeProperty
// to keep everything working

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

DECLARE_DELEGATE(FAttributePropertyChangedDelegate);

// base class so we can store a TArray of the below without regard for template parameter
struct UNREALTOURNAMENT_API TAttributePropertyBase
{
	FAttributePropertyChangedDelegate OnChange;

	// accessors used when the UI interacts with the URL instead of a config file (e.g. hub)
	virtual FString GetURLString() const = 0;
	virtual FString GetURLKey() const = 0;
	virtual void SetFromString(const FString& InValue) = 0;

	virtual ~TAttributePropertyBase()
	{}
};
template<typename T>
struct UNREALTOURNAMENT_API TAttributeProperty : public TAttributePropertyBase
{
protected:
	TWeakObjectPtr<UObject> Obj;
	T* Data;
	const TCHAR* URLKey;
public:
	TAttributeProperty(UObject* InObj, T* InData, const TCHAR* InURLKey = NULL)
		: Obj(InObj), Data(InData), URLKey((InURLKey != NULL) ? InURLKey : TEXT("UNKNOWN"))
	{
		checkSlow(InObj != NULL);
		checkSlow(InData != NULL);
		checkSlow((uint8*)InObj + InObj->GetClass()->GetPropertiesSize() > (uint8*)InData + sizeof(T));
	}
	TOptional<T> GetOptional() const
	{
		if (Obj.IsValid())
		{
			return *Data;
		}
		else
		{
			return TOptional<T>();
		}
	}
	T Get() const
	{
		if (Obj.IsValid())
		{
			return *Data;
		}
		else
		{
			return T(0);
		}
	}
	FText GetAsText() const
	{
		if (Obj.IsValid())
		{
			return FText::FromString(TTypeToString<T>::ToSanitizedString(*Data));
		}
		else
		{
			return FText();
		}
	}

	/** returns key=value pair for use in travel URLs */
	virtual FString GetURLString() const override
	{
		return GetURLKey() + TEXT("=") + (Obj.IsValid() ? TTypeToString<T>::ToSanitizedString(*Data).Replace(TEXT("?"), TEXT("-")) : FString(TEXT("0")));
	}
	virtual FString GetURLKey() const override
	{
		return FString(URLKey);
	}
	virtual void SetFromString(const FString& InValue) override
	{
		TTypeFromString<T>::FromString(*Data, *InValue);
		OnChange.ExecuteIfBound();
	}

	void Set(T NewValue)
	{
		SetConstRef(NewValue);
	}
	void SetConstRef(const T& NewValue)
	{
		if (Obj.IsValid())
		{
			(*Data) = NewValue;
		}
		OnChange.ExecuteIfBound();
	}
};

namespace LexicalConversion
{
	/** Specialized for FString */
	template<>
	inline FString ToSanitizedString<FString>(const FString& value)
	{
		return FString(value);
	}
}

// extras for bools to convert to check box type
struct UNREALTOURNAMENT_API TAttributePropertyBool : public TAttributeProperty<bool>
{
	TAttributePropertyBool(UObject* InObj, bool* InData, const TCHAR* InURLKey = NULL)
	: TAttributeProperty<bool>(InObj, InData, InURLKey)
	{}
	ECheckBoxState GetAsCheckBox() const;
	void SetFromCheckBox(ECheckBoxState CheckedState);
	virtual FString GetURLString() const override
	{
		return GetURLKey() + TEXT("=") + ((Obj.IsValid() && *Data) ? TEXT("true") : TEXT("false"));
	}
};

// extras for FStrings for FText conversion
struct UNREALTOURNAMENT_API TAttributePropertyString : public TAttributeProperty<FString>
{
	TAttributePropertyString(UObject* InObj, FString* InData, const TCHAR* InURLKey = NULL)
	: TAttributeProperty<FString>(InObj, InData, InURLKey)
	{}
	FText GetAsText() const
	{
		return FText::FromString(*Data);
	}
	void SetFromText(const FText& NewValue)
	{
		SetConstRef(NewValue.ToString());
	}
};