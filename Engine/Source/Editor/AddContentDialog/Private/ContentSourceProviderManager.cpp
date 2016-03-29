// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AddContentDialogPCH.h"

void FContentSourceProviderManager::RegisterContentSourceProvider(TSharedRef<IContentSourceProvider> ContentSourceProvider)
{
	ContentSourceProviders.Add(ContentSourceProvider);
}

const TArray<TSharedRef<IContentSourceProvider>>* FContentSourceProviderManager::GetContentSourceProviders()
{
	return &ContentSourceProviders;
}