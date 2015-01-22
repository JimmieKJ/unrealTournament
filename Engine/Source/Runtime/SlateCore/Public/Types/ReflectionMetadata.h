// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

	FReflectionMetaData(FName InName, UClass* InClass, UObject* InAsset)
		: Name(InName)
		, Class(InClass)
		, Asset(InAsset)
	{
	}

	/** The name of the widget in the hierarchy */
	FName Name;
	
	/** The class the constructed the slate widget. */
	TWeakObjectPtr<UClass> Class;

	/** The asset that owns the widget and is responsible for its specific existance. */
	TWeakObjectPtr<UObject> Asset;
};
