// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#ifndef __ContentBrowserPCH_h__
#define __ContentBrowserPCH_h__

#include "UnrealEd.h"
#include "AssetSelection.h"
#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "CollectionManagerModule.h"
#include "IFilter.h"
#include "FilterCollection.h"
#include "TextFilter.h"
#include "SBreadcrumbTrail.h"

DECLARE_LOG_CATEGORY_EXTERN(LogContentBrowser, Verbose, All);

/** Called when a "Find in Asset Tree" is requested */
DECLARE_DELEGATE_OneParam(FOnFindInAssetTreeRequested, const TArray<FAssetData>& /*AssetsToFind*/);
/** Called when the user has committed a rename of one or more assets */
DECLARE_DELEGATE_OneParam(FOnAssetRenameCommitted, const TArray<FAssetData>& /*Assets*/);

#include "ContentBrowserDelegates.h"
#include "IContentBrowserSingleton.h"
#include "SourcesData.h"
#include "FrontendFilters.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"
#include "HistoryManager.h"
#include "SAssetSearchBox.h"
#include "SFilterList.h"
#include "SPathView.h"
#include "CollectionViewUtils.h"
#include "SCollectionView.h"
#include "AssetViewSortManager.h"
#include "SAssetView.h"
#include "SAssetPicker.h"
#include "SPathPicker.h"
#include "SCollectionPicker.h"
#include "SContentBrowser.h"

#endif // __ContentBrowserPCH_h__
