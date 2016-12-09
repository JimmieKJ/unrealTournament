// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Engine/FontFace.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

UFontFace::UFontFace()
{
}

void UFontFace::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	// Only count the memory size for fonts that will be pre-loaded
	if (LoadingPolicy == EFontLoadingPolicy::PreLoad)
	{
#if WITH_EDITORONLY_DATA
		{
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FontFaceData.Num());
		}
#else  // WITH_EDITORONLY_DATA
		{
			const int64 FileSize = IFileManager::Get().FileSize(*SourceFilename);
			if (FileSize > 0)
			{
				CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FileSize);
			}
		}
#endif // WITH_EDITORONLY_DATA
	}
}

#if WITH_EDITOR
void UFontFace::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FSlateApplication::Get().GetRenderer()->FlushFontCache();
}

void UFontFace::PostEditUndo()
{
	Super::PostEditUndo();

	FSlateApplication::Get().GetRenderer()->FlushFontCache();
}

void UFontFace::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	FAssetImportInfo ImportInfo;
	ImportInfo.Insert(FAssetImportInfo::FSourceFile(SourceFilename));
	OutTags.Add(FAssetRegistryTag(SourceFileTagName(), ImportInfo.ToJson(), FAssetRegistryTag::TT_Hidden));
}

void UFontFace::CookAdditionalFiles(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform)
{
	Super::CookAdditionalFiles(PackageFilename, TargetPlatform);

	// We replace the package name with the cooked font face name
	// Note: This must match the replacement logic in UFontFace::GetCookedFilename
	const FString CookedFontFilename = FPaths::GetPath(PackageFilename) / GetName() + TEXT(".ufont");
	FFileHelper::SaveArrayToFile(FontFaceData, *CookedFontFilename);
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UFontFace::InitializeFromBulkData(const FString& InFilename, const EFontHinting InHinting, const void* InBulkDataPtr, const int32 InBulkDataSizeBytes)
{
	check(InBulkDataPtr && InBulkDataSizeBytes > 0 && FontFaceData.Num() == 0);

	SourceFilename = InFilename;
	Hinting = InHinting;
	LoadingPolicy = EFontLoadingPolicy::PreLoad;
	FontFaceData.Append(static_cast<const uint8*>(InBulkDataPtr), InBulkDataSizeBytes);
}
#endif // WITH_EDITORONLY_DATA

const FString& UFontFace::GetFontFilename() const
{
	return SourceFilename;
}

EFontHinting UFontFace::GetHinting() const
{
	return Hinting;
}

EFontLoadingPolicy UFontFace::GetLoadingPolicy() const
{
	return LoadingPolicy;
}

#if WITH_EDITORONLY_DATA
const TArray<uint8>& UFontFace::GetFontFaceData() const
{
	return FontFaceData;
}
#endif // WITH_EDITORONLY_DATA

FString UFontFace::GetCookedFilename() const
{
	// UFontFace assets themselves can't be localized, however that doesn't mean the package they're in isn't localized (ie, when they're upgraded into a UFont asset)
	FString PackageName = GetOutermost()->GetName();
	PackageName = FPackageName::GetLocalizedPackagePath(PackageName);
	
	// Note: This must match the replacement logic in UFontFace::CookAdditionalFiles
	const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, TEXT(".uasset"));
	return FPaths::GetPath(PackageFilename) / GetName() + TEXT(".ufont");
}
