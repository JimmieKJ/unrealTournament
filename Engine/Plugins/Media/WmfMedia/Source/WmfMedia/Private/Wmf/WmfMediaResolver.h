// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


/** Delegate type that is executed when URL resolving has completed. */
DECLARE_DELEGATE_TwoParams(FWmfResolveCompletedDelegate, TComPtr<IUnknown> /*SourceObject*/, FString /*ResolvedUrl*/);

/** Delegate type that is executed when URL resolving has failed. */
DECLARE_DELEGATE_OneParam(FWmfResolveFailedDelegate, FString /*FailedUrl*/);


/**
 * Implements an asynchronous callback object for resolving media URLs.
 */
class FWmfMediaResolver
	: public IMFAsyncCallback
{
public:

	/** Default constructor. */
	FWmfMediaResolver();

public:

	/**
	 * Cancel the current media source resolve.
	 *
	 * Note: Because resolving media sources is an asynchronous operation, the
	 * callback delegates may still get invoked even after calling this method.
	 *
	 * @see ResolveByteStream, ResolveUrl
	 */
	void Cancel();

	/**
	 * Get a delegate that is executed when URL resolving has completed.
	 *
	 * @return The delegate.
	 */
	FWmfResolveCompletedDelegate& OnResolveComplete()
	{
		return ResolveCompletedDelegate;
	}

	/**
	 * Get a delegate that is executed when URL resolving has failed.
	 *
	 * @return The delegate.
	 */
	FWmfResolveFailedDelegate& OnResolveFailed()
	{
		return ResolveFailedDelegate;
	}

	/**
	 * Resolves a media source from a byte stream.
	 *
	 * @param Archive The archive holding the media data.
	 * @param OriginalUrl The original URL of the media that was loaded into the buffer.
	 * @return true if the byte stream will be resolved, false otherwise.
	 * @see Cancel, ResolveUrl
	 */
	bool ResolveByteStream(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl);

	/**
	 * Resolves a media source from a file or internet URL.
	 *
	 * @param Url The URL of the media to open (file name or web address).
	 * @return true if the URL will be resolved, false otherwise.
	 * @see Cancel, ResolveByteStream
	 */
	bool ResolveUrl(const FString& Url);

public:

	// IMFAsyncCallback interface

	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP GetParameters(unsigned long*, unsigned long*);
	STDMETHODIMP Invoke(IMFAsyncResult* AsyncResult);
	STDMETHODIMP QueryInterface(REFIID RefID, void** Object);
	STDMETHODIMP_(ULONG) Release();

private:

	/** Cancellation cookie object. */
	TComPtr<IUnknown> CancelCookie;

	/** Holds a reference counter for this instance. */
	int32 RefCount;

	/** The current resolve state. */
	TComPtr<FWmfMediaResolveState> ResolveState;

	/** Holds the actual source resolver. */
	TComPtr<IMFSourceResolver> SourceResolver;

private:

	/** A delegate that is executed when URL resolving has completed. */
	FWmfResolveCompletedDelegate ResolveCompletedDelegate;

	/** A delegate that is executed when URL resolving has failed. */
	FWmfResolveFailedDelegate ResolveFailedDelegate;
};


#include "HideWindowsPlatformTypes.h"
