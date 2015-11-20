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
		if (Ar.IsError())
		{
			return Ar;
		}

		Ar << DiskCachedAssetData.Timestamp;
		if (Ar.IsError())
		{
			return Ar;
		}

		Ar << DiskCachedAssetData.AssetDataList;
		if (Ar.IsError())
		{
			return Ar;
		}

		Ar << DiskCachedAssetData.DependencyData;
		
		return Ar;
	}
};