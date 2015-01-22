// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/** Type definition for shared pointers to instances of FDeviceDetailsFeature. */
typedef TSharedPtr<struct FDeviceDetailsFeature> FDeviceDetailsFeaturePtr;

/** Type definition for shared references to instances of FDeviceDetailsFeature. */
typedef TSharedRef<struct FDeviceDetailsFeature> FDeviceDetailsFeatureRef;


/**
 * Implements a view model for the device feature list.
 */
struct FDeviceDetailsFeature
{
	/** Holds the name of the feature. */
	FString FeatureName;

	/** Whether the feature is available. */
	bool Available;

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InFeatureName The name of the feature.
	 * @param InAvailable Whether the feature is available.
	 */
	FDeviceDetailsFeature( const FString& InFeatureName, bool InAvailable )
		: FeatureName(InFeatureName)
		, Available(InAvailable)
	{ }
};
