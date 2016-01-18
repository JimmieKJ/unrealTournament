// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStructDeserializerBackend.h"
#include "Core.h"
#include "WebJSScripting.h"
#if WITH_CEF3

// forward declarations
class UProperty;
class UStruct;

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/cef_values.h"
#pragma pop_macro("OVERRIDE")
#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif


class ICefContainerWalker
	: public TSharedFromThis<ICefContainerWalker>
{
public:
	ICefContainerWalker(TSharedPtr<ICefContainerWalker> InParent)
		: Parent(InParent)
	{}
	virtual ~ICefContainerWalker() {}

	virtual TSharedPtr<ICefContainerWalker> GetNextToken( EStructDeserializerBackendTokens& OutToken, FString& PropertyName ) = 0;
	virtual bool ReadProperty(TSharedPtr<FWebJSScripting> Scripting, UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex) = 0;

	TSharedPtr<ICefContainerWalker> Parent;
};

class FCefListValueWalker
	: public ICefContainerWalker
{
public:
	FCefListValueWalker(TSharedPtr<ICefContainerWalker> InParent, CefRefPtr<CefListValue> InList)
		: ICefContainerWalker(InParent)
		, List(InList)
		, Index(-2)
	{}

	virtual TSharedPtr<ICefContainerWalker> GetNextToken( EStructDeserializerBackendTokens& OutToken, FString& PropertyName ) override;
	virtual bool ReadProperty(TSharedPtr<FWebJSScripting> Scripting, UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex ) override;

	CefRefPtr<CefListValue> List;
	size_t Index;
};

class FCefDictionaryValueWalker
	: public ICefContainerWalker
{
public:
	FCefDictionaryValueWalker(TSharedPtr<ICefContainerWalker> InParent, CefRefPtr<CefDictionaryValue> InDictionary)
		: ICefContainerWalker(InParent)
		, Dictionary(InDictionary)
		, Index(-2)
	{
		Dictionary->GetKeys(Keys);
	}

	virtual TSharedPtr<ICefContainerWalker> GetNextToken( EStructDeserializerBackendTokens& OutToken, FString& PropertyName ) override;
	virtual bool ReadProperty(TSharedPtr<FWebJSScripting> Scripting, UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex ) override;

private:
	CefRefPtr<CefDictionaryValue> Dictionary;
	size_t Index;
	CefDictionaryValue::KeyList Keys;
};

/**
 * Implements a writer for UStruct serialization using CefDictionary.
 */
class FWebJSStructDeserializerBackend
	: public IStructDeserializerBackend
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param Archive The archive to deserialize from.
	 */
	FWebJSStructDeserializerBackend(TSharedPtr<FWebJSScripting> InScripting, CefRefPtr<CefDictionaryValue> InDictionary)
		: Scripting(InScripting)
		, Walker(new FCefDictionaryValueWalker(nullptr, InDictionary))
		, CurrentPropertyName()
	{ }

public:

	// IStructDeserializerBackend interface

	virtual const FString& GetCurrentPropertyName() const override;
	virtual FString GetDebugString() const override;
	virtual const FString& GetLastErrorMessage() const override;
	virtual bool GetNextToken( EStructDeserializerBackendTokens& OutToken ) override;
	virtual bool ReadProperty( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex ) override;
	virtual void SkipArray() override;
	virtual void SkipStructure() override;

private:
	TSharedPtr<FWebJSScripting> Scripting;
	/** Holds the source CEF dictionary containing a serialized verion of the structure. */
	TSharedPtr<ICefContainerWalker> Walker;
	FString CurrentPropertyName;
};

#endif