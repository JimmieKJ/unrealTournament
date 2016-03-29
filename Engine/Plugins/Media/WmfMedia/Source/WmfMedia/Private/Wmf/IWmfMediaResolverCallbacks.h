// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for classes that can handle media resolver callbacks.
 */
class IWmfMediaResolverCallbacks
{
public:

	/**
	 * Called when URL resolving has completed.
	 *
	 * @param SourceObject The media source object that was resolved.
	 * @param ResolvedUrl The resolved media URL.
	 */
	virtual void ProcessResolveComplete(TComPtr<IUnknown> SourceObject, FString ResolvedUrl) = 0;

	/**
	 * Called when URL resolving has failed.
	 *
	 * @param FailedUrl The media URL that couldn't be resolved.
	 */
	virtual void ProcessResolveFailed(FString FailedUrl) = 0;

public:

	/** Virtual destructor. */
	virtual ~IWmfMediaResolverCallbacks() { }
};
