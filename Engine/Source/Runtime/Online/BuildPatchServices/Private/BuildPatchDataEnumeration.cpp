// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDataEnumeration, Log, All);
DEFINE_LOG_CATEGORY(LogDataEnumeration);

bool FBuildDataEnumeration::EnumerateManifestData(const FString& ManifestFilePath, const FString& OutputFile, bool bIncludeSizes)
{
	bool bSuccess = false;
	TArray<FGuid> DataList;
	FBuildPatchAppManifestRef AppManifest = MakeShareable(new FBuildPatchAppManifest());
	UE_LOG(LogDataEnumeration, Log, TEXT("Load manifest %s"), *ManifestFilePath);
	const double StartLoadManifest = FPlatformTime::Seconds();
	if (AppManifest->LoadFromFile(ManifestFilePath))
	{
		const double LoadManifestTime = FPlatformTime::Seconds() - StartLoadManifest;
		UE_LOG(LogDataEnumeration, Log, TEXT("Loaded manifest in %.1f seconds"), LoadManifestTime);

		// Enumerate
		AppManifest->GetDataList(DataList);

		// Output text file
		UE_LOG(LogDataEnumeration, Verbose, TEXT("Data file list:-"));
		FString FullList;
		for (FGuid& DataGuid : DataList)
		{
			FString OutputLine = FBuildPatchUtils::GetDataFilename(AppManifest, FString(), DataGuid);
			if (bIncludeSizes)
			{
				uint64 FileSize = AppManifest->GetDataSize(DataGuid);
				OutputLine += FString::Printf(TEXT("\t%u"), FileSize);
			}
			UE_LOG(LogDataEnumeration, Verbose, TEXT("%s"), *OutputLine);
			FullList += OutputLine + TEXT("\r\n");
		}
		if (FFileHelper::SaveStringToFile(FullList, *OutputFile))
		{
			UE_LOG(LogDataEnumeration, Log, TEXT("Saved out to %s"), *OutputFile);
			bSuccess = true;
		}
		else
		{
			UE_LOG(LogDataEnumeration, Error, TEXT("Failed to save output %s"), *OutputFile);
		}
	}
	else
	{
		UE_LOG(LogDataEnumeration, Error, TEXT("Failed to load manifest %s"), *ManifestFilePath);
	}
	return bSuccess;
}
