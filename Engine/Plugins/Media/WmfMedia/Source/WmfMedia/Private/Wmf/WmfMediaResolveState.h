// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


/** Enumerates media source resolve types. */
enum class EWmfMediaResolveType
{
	/** Not resolving right now. */
	None,

	/** Resolving a byte stream. */
	ByteStream,

	/** Resolving a file or web URL. */
	Url
};


/**
 * Implements media source resolve state information.
 */
struct FWmfMediaResolveState
	: public IUnknown
{
	/** The type of the media source being resolved. */
	EWmfMediaResolveType Type;

	/** The URL of the media source being resolved. */
	FString Url;

public:

	/** Creates and initializes a new instance. */
	FWmfMediaResolveState(EWmfMediaResolveType InType, const FString& InUrl)
		: Type(InType)
		, Url(InUrl)
		, RefCount(0)
	{ }

public:

	// IUnknown interface

	STDMETHODIMP_(ULONG) AddRef()
	{
		return FPlatformAtomics::InterlockedIncrement(&RefCount);
	}

#if _MSC_VER == 1900
#pragma warning(push)
#pragma warning(disable:4838)
#endif // _MSC_VER == 1900
	STDMETHODIMP QueryInterface(REFIID RefID, void** Object)
	{
		static const QITAB QITab[] =
		{
			QITABENT(FWmfMediaResolveState, IUnknown),
			{ 0 }
		};

		return QISearch( this, QITab, RefID, Object );
	}
#if _MSC_VER == 1900
#pragma warning(pop)
#endif // _MSC_VER == 1900

	STDMETHODIMP_(ULONG) Release()
	{
		int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);
	
		if (CurrentRefCount == 0)
		{
			delete this;
		}

		return CurrentRefCount;
	}

private:

	/** Holds a reference counter for this instance. */
	int32 RefCount;
};


#include "HideWindowsPlatformTypes.h"
