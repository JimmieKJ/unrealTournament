// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISlateMetaData.h"

/**
 * Reflection meta-data that can be used by the widget reflector to determine
 * additional information about slate widgets that are constructed by UObject
 * classes for UMG.
 */
class FReflectionMetaData : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FReflectionMetaData, ISlateMetaData)

	FReflectionMetaData(FName InName, UClass* InClass, const UObject* InAsset)
		: Name(InName)
		, Class(InClass)
		, Asset(InAsset)
	{
	}

	/** The name of the widget in the hierarchy */
	FName Name;
	
	/** The class the constructed the slate widget. */
	TWeakObjectPtr<UClass> Class;

	/** The asset that owns the widget and is responsible for its specific existence. */
	TWeakObjectPtr<const UObject> Asset;

public:

	static FString GetWidgetDebugInfo(const SWidget* InWidget)
	{
		// UMG widgets have meta-data to help track them
		TSharedPtr<FReflectionMetaData> MetaData = InWidget->GetMetaData<FReflectionMetaData>();
		if ( MetaData.IsValid() && MetaData->Asset.Get() != nullptr )
		{
			const FName AssetName = MetaData->Asset->GetFName();
			const FName WidgetName = MetaData->Name;

			return FString::Printf(TEXT("%s [%s]"), *AssetName.ToString(), *WidgetName.ToString());
		}
		else
		{
			return InWidget->GetReadableLocation();
		}
	}
};
