// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWmfMediaResolverCallbacks.h"
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
	FWmfMediaResolveState(EWmfMediaResolveType InType, const FString& InUrl, IWmfMediaResolverCallbacks& InCallbacks)
		: Callbacks(&InCallbacks)
		, Type(InType)
		, Url(InUrl)
		, RefCount(0)
	{ }

public:

	/** Invalidate the resolve state (resolve events will no longer be forwarded). */
	void Invalidate()
	{
		FScopeLock Lock(&CriticalSection);
		Callbacks = nullptr;
	}

	/**
	 * Forward the event for a completed media resolve.
	 *
	 * @param SourceObject The media source object that was resolved.
	 * @param ResolvedUrl The resolved media URL.
	 */
	void ResolveComplete(TComPtr<IUnknown> SourceObject, FString ResolvedUrl)
	{
		FScopeLock Lock(&CriticalSection);

		if (Callbacks != nullptr)
		{
			Callbacks->ProcessResolveComplete(SourceObject, ResolvedUrl);
		}
	}

	/**
	 * Forward the event for a failed media resolve.
	 *
	 * @param FailedUrl The media URL that couldn't be resolved.
	 */
	void ResolveFailed(FString FailedUrl)
	{
		FScopeLock Lock(&CriticalSection);

		if (Callbacks != nullptr)
		{
			Callbacks->ProcessResolveFailed(FailedUrl);
		}
	}

public:

	//~ IUnknown interface

	STDMETHODIMP_(ULONG) AddRef()
	{
		return FPlatformAtomics::InterlockedIncrement(&RefCount);
	}

#if _MSC_VER == 1900
	#pragma warning(push)
	#pragma warning(disable:4838)
#endif

	STDMETHODIMP QueryInterface(REFIID RefID, void** Object)
	{
		static const QITAB QITab[] =
		{
			QITABENT(FWmfMediaResolveState, IUnknown),
			{ 0 }
		};

		return QISearch(this, QITab, RefID, Object);
	}

#if _MSC_VER == 1900
	#pragma warning(pop)
#endif

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

	/** Object that receives event callbacks. */
	IWmfMediaResolverCallbacks* Callbacks;

	/** Critical section for gating access to Callbacks. */
	FCriticalSection CriticalSection;

	/** Holds a reference counter for this instance. */
	int32 RefCount;
};


#include "HideWindowsPlatformTypes.h"
