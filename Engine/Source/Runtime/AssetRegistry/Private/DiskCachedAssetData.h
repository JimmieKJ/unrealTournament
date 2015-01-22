// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FDiskCachedAssetData
{
public:
	FName PackageName;
	FDateTime Timestamp;
	TArray<FAssetData> AssetDataList;
	FPackageDependencyData DependencyData;

	FDiskCachedAssetData()
	{}

	FDiskCachedAssetData(FName InPackageName, const FDateTime& InTimestamp)
		: PackageName(InPackageName), Timestamp(InTimestamp)
	{}

	/** Operator for serialization */
	friend FArchive& operator<<(FArchive& Ar, FDiskCachedAssetData& DiskCachedAssetData)
	{
		Ar << DiskCachedAssetData.PackageName;
		Ar << DiskCachedAssetData.Timestamp;
		Ar << DiskCachedAssetData.AssetDataList;
		Ar << DiskCachedAssetData.DependencyData;
		
		return Ar;
	}
};