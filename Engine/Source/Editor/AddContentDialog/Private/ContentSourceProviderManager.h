// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class IContentSourceProvider;

/** Provides methods for registering and getting registered content source providers. */
class FContentSourceProviderManager
{
public:
	/** Registers a content source provider. */
	void RegisterContentSourceProvider(TSharedRef<IContentSourceProvider> ContentSourceProvider);

	/** Gets and array of the currently registered content source providers. */
	const TArray<TSharedRef<IContentSourceProvider>>* GetContentSourceProviders();

private:
	/** The registered content source providers. */
	TArray<TSharedRef<IContentSourceProvider>> ContentSourceProviders;
};