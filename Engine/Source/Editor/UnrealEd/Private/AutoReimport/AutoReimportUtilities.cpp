// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "AutoReimportUtilities.h"
#include "EditorReimportHandler.h"
#include "ARFilter.h"
#include "IAssetRegistry.h"

namespace Utils
{
	TArray<FAssetData> FindAssetsPertainingToFile(const IAssetRegistry& Registry, const FString& AbsoluteFilename)
	{
		TArray<FAssetData> Assets;

		const FString LeafName = FPaths::GetCleanFilename(AbsoluteFilename);
		const FName TagName = UObject::SourceFileTagName();

		FARFilter Filter;
		Filter.SourceFilenames.Add(*LeafName);
		Filter.bIncludeOnlyOnDiskAssets = true;
		Registry.GetAssets(Filter, Assets);

		Assets.RemoveAll([&](const FAssetData& Asset){
			for (const auto& Pair : Asset.TagsAndValues)
			{
				// We don't compare numbers on FNames for this check because the tag "ReimportPath" may exist multiple times
				const bool bCompareNumber = false;
				if (Pair.Key.IsEqual(TagName, ENameCase::IgnoreCase, bCompareNumber))
				{
					// Either relative to the asset itself, or relative to the base path, or absolute.
					// Would ideally use FReimportManager::ResolveImportFilename here, but we don't want to ask the file system whether the file exists.
					auto RelativeToPackagePath = FPackageName::LongPackageNameToFilename(Asset.PackagePath.ToString() / TEXT("")) / Pair.Value;
					if (AbsoluteFilename == FPaths::ConvertRelativePathToFull(RelativeToPackagePath) ||
						AbsoluteFilename == FPaths::ConvertRelativePathToFull(Pair.Value))
					{
						return false;
					}
				}
			}
			return true;
		});
		
		return Assets;
	}

	void ExtractSourceFilePaths(UObject* Object, TArray<FString>& OutSourceFiles)
	{
		TArray<UObject::FAssetRegistryTag> TagList;
		Object->GetAssetRegistryTags(TagList);

		const FName TagName = UObject::SourceFileTagName();
		for (const auto& Tag : TagList)
		{
			// We don't compare numbers on FNames for this check because the tag "ReimportPath" may exist multiple times
			const bool bCompareNumber = false;
			if (Tag.Name.IsEqual(TagName, ENameCase::IgnoreCase, bCompareNumber))
			{
				OutSourceFiles.Add(FReimportManager::ResolveImportFilename(Tag.Value, Object));
			}
		}
	}
}