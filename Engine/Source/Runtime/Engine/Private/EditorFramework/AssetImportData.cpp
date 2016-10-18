// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

#include "EditorFramework/AssetImportData.h"

// This whole class is compiled out in non-editor
UAssetImportData::UAssetImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA

FOnImportDataChanged UAssetImportData::OnImportDataChanged;

FString FAssetImportInfo::ToJson() const
{
	FString Json;
	Json.Reserve(1024);
	Json += TEXT("[");

	for (int32 Index = 0; Index < SourceFiles.Num(); ++Index)
	{
		Json += FString::Printf(TEXT("{ \"RelativeFilename\" : \"%s\", \"Timestamp\" : \"%d\", \"FileMD5\" : \"%s\" }"),
			*SourceFiles[Index].RelativeFilename,
			SourceFiles[Index].Timestamp.ToUnixTimestamp(),
			*LexicalConversion::ToString(SourceFiles[Index].FileHash)
			);

		if (Index != SourceFiles.Num() - 1)
		{
			Json += TEXT(",");
		}
	}

	Json += TEXT("]");
	return Json;
}

TOptional<FAssetImportInfo> FAssetImportInfo::FromJson(FString InJsonString)
{
	// Load json
	TSharedRef<TJsonReader<TCHAR>> Reader = FJsonStringReader::Create(MoveTemp(InJsonString));

	TArray<TSharedPtr<FJsonValue>> JSONSourceFiles;
	if (!FJsonSerializer::Deserialize(Reader, JSONSourceFiles))
	{
		return TOptional<FAssetImportInfo>();
	}
	
	FAssetImportInfo Info;

	for (const auto& Value : JSONSourceFiles)
	{
		const TSharedPtr<FJsonObject>& SourceFile = Value->AsObject();
		if (!SourceFile.IsValid())
		{
			continue;
		}

		FString RelativeFilename, TimestampString, MD5String;
		SourceFile->TryGetStringField("RelativeFilename", RelativeFilename);
		SourceFile->TryGetStringField("Timestamp", TimestampString);
		SourceFile->TryGetStringField("FileMD5", MD5String);

		if (RelativeFilename.IsEmpty())
		{
			continue;
		}

		int64 UnixTimestamp = 0;
		LexicalConversion::FromString(UnixTimestamp, *TimestampString);

		FMD5Hash FileHash;
		LexicalConversion::FromString(FileHash, *MD5String);

		Info.SourceFiles.Emplace(MoveTemp(RelativeFilename), FDateTime::FromUnixTimestamp(UnixTimestamp), FileHash);
	}

	return Info;
}

void UAssetImportData::UpdateFilenameOnly(const FString& InPath)
{
	// Try and retain the MD5 and timestamp if possible
	if (SourceData.SourceFiles.Num() == 1)
	{
		SourceData.SourceFiles[0].RelativeFilename = SanitizeImportFilename(InPath);
	}
	else
	{
		SourceData.SourceFiles.Reset();
		SourceData.SourceFiles.Emplace(SanitizeImportFilename(InPath));
	}
}

void UAssetImportData::UpdateFilenameOnly(const FString& InPath, int32 Index)
{
	if (SourceData.SourceFiles.IsValidIndex(Index))
	{
		SourceData.SourceFiles[Index].RelativeFilename = SanitizeImportFilename(InPath);
	}
}

void UAssetImportData::Update(const FString& InPath, FMD5Hash *Md5Hash/* = nullptr*/)
{
	FAssetImportInfo Old = SourceData;

	// Reset our current data
	SourceData.SourceFiles.Reset();
	SourceData.SourceFiles.Emplace(
		SanitizeImportFilename(InPath),
		IFileManager::Get().GetTimeStamp(*InPath),
		(Md5Hash != nullptr) ? *Md5Hash : FMD5Hash::HashFile(*InPath)
		);
	
	OnImportDataChanged.Broadcast(Old, this);
}

FString UAssetImportData::GetFirstFilename() const
{
	return SourceData.SourceFiles.Num() > 0 ? ResolveImportFilename(SourceData.SourceFiles[0].RelativeFilename) : FString();
}

void UAssetImportData::ExtractFilenames(TArray<FString>& AbsoluteFilenames) const
{
	for (const auto& File : SourceData.SourceFiles)
	{
		AbsoluteFilenames.Add(ResolveImportFilename(File.RelativeFilename));
	}
}

TArray<FString> UAssetImportData::ExtractFilenames() const
{
	TArray<FString> Temp;
	ExtractFilenames(Temp);
	return Temp;
}

FString UAssetImportData::SanitizeImportFilename(const FString& InPath) const
{
	const UPackage* Package = GetOutermost();
	if (Package)
	{
		const bool		bIncludeDot = true;
		const FString	PackagePath	= Package->GetPathName();
		const FName		MountPoint	= FPackageName::GetPackageMountPoint(PackagePath);
		const FString	PackageFilename = FPackageName::LongPackageNameToFilename(PackagePath, FPaths::GetExtension(InPath, bIncludeDot));
		const FString	AbsolutePath = FPaths::ConvertRelativePathToFull(InPath);

		if ((MountPoint == FName("Engine") && AbsolutePath.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::EngineContentDir()))) ||
			(MountPoint == FName("Game") &&	AbsolutePath.StartsWith(FPaths::ConvertRelativePathToFull(FPaths::GameDir()))))
		{
			FString RelativePath = InPath;
			FPaths::MakePathRelativeTo(RelativePath, *PackageFilename);
			return RelativePath;
		}
	}

	return IFileManager::Get().ConvertToRelativePath(*InPath);
}

FString UAssetImportData::ResolveImportFilename(const FString& InRelativePath, const UPackage* Outermost)
{
	FString RelativePath = InRelativePath;

	if (Outermost)
	{
		// Relative to the package filename?
		const FString PathRelativeToPackage = FPaths::GetPath(FPackageName::LongPackageNameToFilename(Outermost->GetPathName())) / InRelativePath;
		if (FPaths::FileExists(PathRelativeToPackage))
		{
			FString FullConvertPath = FPaths::ConvertRelativePathToFull(PathRelativeToPackage);
			//FileExist return true when testing Path like c:/../folder1/filename. ConvertRelativePathToFull specify having .. in front of a drive letter is an error.
			//It is relative to package only if the conversion to full path is successful.
			if (FullConvertPath.Find(TEXT("..")) == INDEX_NONE)
			{
				return FullConvertPath;
			}
		}
	}

	// Convert relative paths
	return FPaths::ConvertRelativePathToFull(RelativePath);	
}

FString UAssetImportData::ResolveImportFilename(const FString& InRelativePath) const
{
	return ResolveImportFilename(InRelativePath, GetOutermost());
}

void UAssetImportData::Serialize(FArchive& Ar)
{
	if (Ar.UE4Ver() >= VER_UE4_ASSET_IMPORT_DATA_AS_JSON)
	{
		
		if (!Ar.IsFilterEditorOnly())
		{
			FString Json;
			if (Ar.IsLoading())
			{
				Ar << Json;
				TOptional<FAssetImportInfo> Copy = FAssetImportInfo::FromJson(MoveTemp(Json));
				if (Copy.IsSet())
				{
					SourceData = MoveTemp(Copy.GetValue());
				}
			}
			else if (Ar.IsSaving())
			{
				Json = SourceData.ToJson();
				Ar << Json;
			}
		}
	}

	Super::Serialize(Ar);
}

void UAssetImportData::PostLoad()
{
	if (!SourceFilePath_DEPRECATED.IsEmpty() && SourceData.SourceFiles.Num() == 0)
	{
		FDateTime SourceDateTime;
		if (!FDateTime::Parse(SourceFileTimestamp_DEPRECATED,SourceDateTime))
		{
			SourceDateTime = 0;
		}

		SourceData.SourceFiles.Add(FAssetImportInfo::FSourceFile(MoveTemp(SourceFilePath_DEPRECATED),SourceDateTime));

		SourceFilePath_DEPRECATED.Empty();
		SourceFileTimestamp_DEPRECATED.Empty();
	}

	Super::PostLoad();
}

#endif // WITH_EDITORONLY_DATA