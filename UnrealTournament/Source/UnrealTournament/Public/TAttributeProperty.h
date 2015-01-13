// TAttributeProperty provides a way to link a TAttribute and Slate setter delegate directly to a UObject property
// without having to write "custom" (but really duplicated code) handlers for each such property
// note that since delegates hold weak pointers, you will need to keep a TSharedPtr reference to the constructed TAttributeProperty
// to keep everything working

// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

struct TAttributePropertyBase
{
	// base class so we can store a TArray of the below without regard for template parameter
};
template<typename T>
struct TAttributeProperty : public TAttributePropertyBase
{
	TWeakObjectPtr<UObject> Obj;
	T* Data;
	TAttributeProperty(UObject* InObj, T* InData)
		: Obj(InObj), Data(InData)
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
	}
};
// extras for bools to convert to check box type
struct TAttributePropertyBool : public TAttributeProperty<bool>
{
	TAttributePropertyBool(UObject* InObj, bool* InData)
	: TAttributeProperty<bool>(InObj, InData)
	{}
	ECheckBoxState GetAsCheckBox() const;
	void SetFromCheckBox(ECheckBoxState CheckedState);
};
// extras for FStrings for FText conversion
struct TAttributePropertyString : public TAttributeProperty<FString>
{
	TAttributePropertyString(UObject* InObj, FString* InData)
	: TAttributeProperty<FString>(InObj, InData)
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