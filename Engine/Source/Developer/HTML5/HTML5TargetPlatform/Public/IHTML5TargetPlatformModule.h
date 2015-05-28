// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITargetPlatformModule.h"

class HTML5TARGETPLATFORM_API FHTML5SDKVersionNumber
{
public:
	FHTML5SDKVersionNumber()
	{
		// Zero is invalid, -1 is latest.
		VersionNumber.Major = 0;
		VersionNumber.Minor = 0;
		VersionNumber.Revision = 0;
	}

	// Directory of the SDK 
	FString VersionPath;

	//Version Number
	struct
	{
		int32 Major;
		int32 Minor;
		int32 Revision;
	} VersionNumber;

	void VersionNumberFromString(const TCHAR* VersionString)
	{
		// We expect the format "1.23.45"
		const TCHAR* VStr = VersionString;
		TCHAR* VEnd = nullptr;
		int32 Nums[3] = { 0 };
		VersionNumber.Major = 0;
		VersionNumber.Minor = 0;
		VersionNumber.Revision = 0;
		for (auto& N : Nums)
		{
			N = FCString::Strtoi(VStr, &VEnd, 10);
			if (!VEnd || VStr == VEnd || (*VEnd != '.' && *VEnd != 0))
			{
				return;
			}
			VStr = VEnd + 1;
		}
		VersionNumber.Major = Nums[0];
		VersionNumber.Minor = Nums[1];
		VersionNumber.Revision = Nums[2];
	}

	FString VersionNumberAsString() const
	{
		return FString::Printf(TEXT("%d.%d.%d"), VersionNumber.Major, VersionNumber.Minor, VersionNumber.Revision);
	}

	void MakeLatestVersionNumber()
	{
		VersionNumber.Major = -1;
		VersionNumber.Minor = -1;
		VersionNumber.Revision = -1;
	}

	bool operator < (const FHTML5SDKVersionNumber& RHS) const
	{
		return (VersionNumber.Major < RHS.VersionNumber.Major) ||
			(VersionNumber.Major == RHS.VersionNumber.Major && VersionNumber.Minor < RHS.VersionNumber.Minor) ||
			((VersionNumber.Major == RHS.VersionNumber.Major) && (VersionNumber.Minor == RHS.VersionNumber.Minor) && (VersionNumber.Revision < RHS.VersionNumber.Revision));
	}

	bool operator == (const FHTML5SDKVersionNumber& RHS) const
	{
		return VersionNumber.Major == RHS.VersionNumber.Major && VersionNumber.Minor == RHS.VersionNumber.Minor && VersionNumber.Revision == RHS.VersionNumber.Revision;
	}
};

/**
 * Interface for HTML5TargetPlatformModule module.
 */
class IHTML5TargetPlatformModule
	: public ITargetPlatformModule
{
public:
	/**
	 * Refresh the list of HTML5 browsers that exist on the system
	 */
	virtual void RefreshAvailableDevices() = 0;

	/**
	 * Fills a list of installed SDK version numbers
	 */
	virtual void GetInstalledSDKVersions(const TCHAR* SDKDirectory, TArray<FHTML5SDKVersionNumber>& OutSDKs) = 0;

protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IHTML5TargetPlatformModule( ) { }
};