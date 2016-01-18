// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AutoReimportUtilities.h"
#include "EditorReimportHandler.h"
#include "ARFilter.h"
#include "IAssetRegistry.h"
#include "AssetSourceFilenameCache.h"

namespace Utils
{
	TArray<FAssetData> FindAssetsPertainingToFile(const IAssetRegistry& Registry, const FString& AbsoluteFilename)
	{
		return FAssetSourceFilenameCache::Get().GetAssetsPertainingToFile(Registry, AbsoluteFilename);
	}

	TArray<FString> ExtractSourceFilePaths(UObject* Object)
	{
		TArray<FString> Temp;
		ExtractSourceFilePaths(Object, Temp);
		return Temp;
	}

	void ExtractSourceFilePaths(UObject* Object, TArray<FString>& OutSourceFiles)
	{
		TArray<UObject::FAssetRegistryTag> TagList;
		Object->GetAssetRegistryTags(TagList);

		const FName TagName = UObject::SourceFileTagName();
		for (const auto& Tag : TagList)
		{
			if (Tag.Name == TagName)
			{
				int32 PreviousNum = OutSourceFiles.Num();

				TOptional<FAssetImportInfo> ImportInfo = FAssetImportInfo::FromJson(Tag.Value);
				if (ImportInfo.IsSet())
				{
					for (const auto& File : ImportInfo->SourceFiles)
					{
						OutSourceFiles.Emplace(UAssetImportData::ResolveImportFilename(File.RelativeFilename, Object->GetOutermost()));
					}
				}
				break;
			}
		}
	}
}