// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "PackageHelperFunctions.h"
#include "DerivedDataCacheInterface.h"
#include "ISourceControlModule.h"
#include "GlobalShader.h"
#include "TargetPlatform.h"
#include "IConsoleManager.h"
#include "Developer/PackageDependencyInfo/Public/PackageDependencyInfo.h"
#include "AssetRegistryModule.h"
#include "UnrealEdMessages.h"
#include "GameDelegates.h"
#include "ChunkManifestGenerator.h"
#include "ChunkDependencyInfo.h"
#include "IPlatformFileSandboxWrapper.h"

#include "JsonWriter.h"
#include "JsonReader.h"
#include "JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogChunkManifestGenerator, Log, All);

FChunkManifestGenerator::FChunkManifestGenerator(const TArray<ITargetPlatform*>& InPlatforms)
	: AssetRegistry(FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	, Platforms(InPlatforms)
	, bGenerateChunks(false)
{
	DependencyInfo = GetMutableDefault<UChunkDependencyInfo>();
}

FChunkManifestGenerator::~FChunkManifestGenerator()
{
	for (auto ChunkSet : ChunkManifests)
	{
		delete ChunkSet;
	}
	ChunkManifests.Empty();
	for (auto ChunkSet : FinalChunkManifests)
	{
		delete ChunkSet;
	}
	FinalChunkManifests.Empty();
}

void FChunkManifestGenerator::OnAssetLoaded(UObject* Asset)
{
	if (Asset != NULL)
	{
		UPackage* AssetPackage = CastChecked<UPackage>(Asset->GetOutermost());
		if (!AssetsLoadedWithLastPackage.Contains(AssetPackage->GetFName()))
		{
			AssetsLoadedWithLastPackage.Add(AssetPackage->GetFName());
		}
	}
}

void FChunkManifestGenerator::OnLastPackageLoaded( UPackage* Package )
{
	if ( !AssetsLoadedWithLastPackage.Contains(Package->GetFName()))
	{
		AssetsLoadedWithLastPackage.Add(Package->GetFName());
	}
}

void FChunkManifestGenerator::OnLastPackageLoaded( const FName& PackageName )
{
	if ( !AssetsLoadedWithLastPackage.Contains(PackageName))
	{
		AssetsLoadedWithLastPackage.Add(PackageName);
	}
}

bool FChunkManifestGenerator::CleanTempPackagingDirectory(const FString& Platform) const
{
	FString TmpPackagingDir = GetTempPackagingDirectoryForPlatform(Platform);
	if (IFileManager::Get().DirectoryExists(*TmpPackagingDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*TmpPackagingDir, false, true))
		{
			UE_LOG(LogChunkManifestGenerator, Error, TEXT("Failed to delete directory: %s"), *TmpPackagingDir);
			return false;
		}
	}

	FString ChunkListDir = FPaths::Combine(*FPaths::GameLogDir(), TEXT("ChunkLists"));
	if (IFileManager::Get().DirectoryExists(*ChunkListDir))
	{
		if (!IFileManager::Get().DeleteDirectory(*ChunkListDir, false, true))
		{
			UE_LOG(LogChunkManifestGenerator, Error, TEXT("Failed to delete directory: %s"), *ChunkListDir);
			return false;
		}
	}
	return true;
}

bool FChunkManifestGenerator::GenerateStreamingInstallManifest(const FString& Platform)
{
	FString GameNameLower = FString(FApp::GetGameName()).ToLower();

	// empty out the current paklist directory
	FString TmpPackagingDir = GetTempPackagingDirectoryForPlatform(Platform);
	if (!IFileManager::Get().MakeDirectory(*TmpPackagingDir, true))
	{
		UE_LOG(LogChunkManifestGenerator, Error, TEXT("Failed to create directory: %s"), *TmpPackagingDir);
		return false;
	}
	
	// open a file for writing the list of pak file lists that we've generated
	FString PakChunkListFilename = TmpPackagingDir / TEXT("pakchunklist.txt");
	TAutoPtr<FArchive> PakChunkListFile(IFileManager::Get().CreateFileWriter(*PakChunkListFilename));

	if (!PakChunkListFile.IsValid())
	{
		UE_LOG(LogChunkManifestGenerator, Error, TEXT("Failed to open output pakchunklist file %s"), *PakChunkListFilename);
		return false;
	}

	FString PakChunkLayerInfoFilename = FString::Printf(TEXT("%s/pakchunklayers.txt"), *TmpPackagingDir);
	TAutoPtr<FArchive> ChunkLayerFile(IFileManager::Get().CreateFileWriter(*PakChunkLayerInfoFilename));

	// generate per-chunk pak list files
	for (int32 Index = 0; Index < FinalChunkManifests.Num(); ++Index)
	{
		// Is this chunk empty?
		if (!FinalChunkManifests[Index] || FinalChunkManifests[Index]->Num() == 0)
		{
			continue;
		}
		FString PakListFilename = FString::Printf(TEXT("%s/pakchunk%d.txt"), *TmpPackagingDir, Index);
		TAutoPtr<FArchive> PakListFile(IFileManager::Get().CreateFileWriter(*PakListFilename));

		if (!PakListFile.IsValid())
		{
			UE_LOG(LogChunkManifestGenerator, Error, TEXT("Failed to open output paklist file %s"), *PakListFilename);
			return false;
		}

		for (auto& Filename : *FinalChunkManifests[Index])
		{
			FString PakListLine = FPaths::ConvertRelativePathToFull(Filename.Value.Replace(TEXT("[Platform]"), *Platform));
			PakListLine.ReplaceInline(TEXT("/"), TEXT("\\"));
			PakListLine += TEXT("\r\n");
			PakListFile->Serialize(TCHAR_TO_ANSI(*PakListLine), PakListLine.Len());
		}

		PakListFile->Close();

		// add this pakfilelist to our master list of pakfilelists
		FString PakChunkListLine = FString::Printf(TEXT("pakchunk%d.txt\r\n"), Index);
		PakChunkListFile->Serialize(TCHAR_TO_ANSI(*PakChunkListLine), PakChunkListLine.Len());

		int32 TargetLayer = 0;
		FGameDelegates::Get().GetAssignLayerChunkDelegate().ExecuteIfBound(FinalChunkManifests[Index], Platform, Index, TargetLayer);

		FString LayerString = FString::Printf(TEXT("%d\r\n"), TargetLayer);

		ChunkLayerFile->Serialize(TCHAR_TO_ANSI(*LayerString), LayerString.Len());
	}

	ChunkLayerFile->Close();
	PakChunkListFile->Close();

	return true;
}

void FChunkManifestGenerator::Initialize(bool InGenerateChunks)
{
	bGenerateChunks = InGenerateChunks;

	// Calculate the largest chunk id used by the registry to get the indices for the default chunks
	AssetRegistry.GetAllAssets(AssetRegistryData);
	int32 LargestChunkID = -1;

	for (int32 Index = 0; Index < AssetRegistryData.Num(); ++Index)
	{
		auto& AssetData = AssetRegistryData[Index];
		auto& RegistryChunkIDs = RegistryChunkIDsMap.FindOrAdd(AssetData.PackageName);
		for (auto ChunkIt = AssetData.ChunkIDs.CreateConstIterator(); ChunkIt; ++ChunkIt)
		{
			int32 ChunkID = *ChunkIt;
			if (ChunkID < 0)
			{
				UE_LOG(LogChunkManifestGenerator, Warning, TEXT("Out of range ChunkID: %d"), ChunkID);
				ChunkID = 0;
			}
			else if (ChunkID > LargestChunkID)
			{
				LargestChunkID = ChunkID;
			}
			if (!RegistryChunkIDs.Contains(ChunkID))
			{
				RegistryChunkIDs.Add(ChunkID);
			}
		}
		// Now clear the original chunk id list. We will fill it with real IDs when cooking.
		AssetData.ChunkIDs.Empty();
		// Map asset data to its package (there can be more than one asset data per package).
		auto& PackageData = PackageToRegistryDataMap.FindOrAdd(AssetData.PackageName);
		PackageData.Add(Index);
	}

	// Hook up game delegate
	if (bGenerateChunks)
	{
		FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FChunkManifestGenerator::OnAssetLoaded);
	}
}

void FChunkManifestGenerator::AddPackageToChunkManifest(const FName& PackageFName, const FString& PackagePathName, const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile* InSandboxFile)
{
	TArray<int32> TargetChunks;
	TArray<int32> ExistingChunkIDs;

	if (!bGenerateChunks)
	{
		TargetChunks.AddUnique(0);
		ExistingChunkIDs.AddUnique(0);
	}

	if (bGenerateChunks)
	{
		// Try to determine if this package has been loaded as a result of loading a map package.
		FString MapThisAssetWasLoadedWith;
		if (!LastLoadedMapName.IsEmpty())
		{
			if (AssetsLoadedWithLastPackage.Contains(PackageFName))
			{
				MapThisAssetWasLoadedWith = LastLoadedMapName;
			}
		}

		// Collect all chunk IDs associated with this package from the asset registry
		TArray<int32> RegistryChunkIDs = GetAssetRegistryChunkAssignments(PackageFName);
		ExistingChunkIDs = GetExistingPackageChunkAssignments(PackageFName);

		// Try to call game-specific delegate to determine the target chunk ID
		// FString Name = Package->GetPathName();
		if (FGameDelegates::Get().GetAssignStreamingChunkDelegate().IsBound())
		{
			FGameDelegates::Get().GetAssignStreamingChunkDelegate().ExecuteIfBound(PackagePathName, MapThisAssetWasLoadedWith, RegistryChunkIDs, ExistingChunkIDs, TargetChunks);
		}
		else
		{
			//Take asset registry assignments and existing assignments
			TargetChunks.Append(RegistryChunkIDs);
			TargetChunks.Append(ExistingChunkIDs);
		}
	}

	NotifyPackageWasCooked(SandboxFilename, PackageFName);

	bool bAssignedToChunk = false;
	// if the delegate requested a specific chunk assignment, add them package to it now.
	for (const auto& PackageChunk : TargetChunks)
	{
		AddPackageToManifest(SandboxFilename, PackageFName, PackageChunk);
		bAssignedToChunk = true;
	}
	// If the delegate requested to remove the package from any chunk, remove it now
	for (const auto& PackageChunk : ExistingChunkIDs)
	{
		if (!TargetChunks.Contains(PackageChunk))
		{
			RemovePackageFromManifest(PackageFName, PackageChunk);
		}
	}

	if (!bAssignedToChunk)
	{
		NotifyPackageWasNotAssigned(SandboxFilename, PackageFName);
	}
}

void FChunkManifestGenerator::AddPackageToChunkManifest(UPackage* Package, const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile* InSandboxFile)
{
#if 0
	AddPackageToChunkManifest(Package->GetFName(), Package->GetPathName(), SandboxFilename, LastLoadedMapName, InSandboxFile);
#else
	TArray<int32> TargetChunks;
	TArray<int32> ExistingChunkIDs;
	
	if (!bGenerateChunks)
	{
		TargetChunks.AddUnique(0);
		ExistingChunkIDs.AddUnique(0);
	}
	
	auto PackageFName = Package->GetFName();
	if (bGenerateChunks)
	{
		// Try to determine if this package has been loaded as a result of loading a map package.
		FString MapThisAssetWasLoadedWith;
		if (!LastLoadedMapName.IsEmpty())
		{
			if (AssetsLoadedWithLastPackage.Contains(PackageFName))
			{
				MapThisAssetWasLoadedWith = LastLoadedMapName;
			}
		}

		// Collect all chunk IDs associated with this package from the asset registry
		TArray<int32> RegistryChunkIDs = GetAssetRegistryChunkAssignments(Package);
		ExistingChunkIDs = GetExistingPackageChunkAssignments(PackageFName);

		// Try to call game-specific delegate to determine the target chunk ID
		FString Name = Package->GetPathName();
		if (FGameDelegates::Get().GetAssignStreamingChunkDelegate().IsBound())
		{
			FGameDelegates::Get().GetAssignStreamingChunkDelegate().ExecuteIfBound(Name, MapThisAssetWasLoadedWith, RegistryChunkIDs, ExistingChunkIDs, TargetChunks);
		}
		else
		{
			//Take asset registry assignments and existing assignments
			TargetChunks.Append(RegistryChunkIDs);
			TargetChunks.Append(ExistingChunkIDs);
		}
	}

	NotifyPackageWasCooked(SandboxFilename, PackageFName);

	bool bAssignedToChunk = false;
	// if the delegate requested a specific chunk assignment, add them package to it now.
	for (const auto& PackageChunk : TargetChunks)
	{
		AddPackageToManifest(SandboxFilename, PackageFName, PackageChunk);
		bAssignedToChunk = true;
	}
	// If the delegate requested to remove the package from any chunk, remove it now
	for (const auto& PackageChunk : ExistingChunkIDs)
	{
		if (!TargetChunks.Contains(PackageChunk))
		{
			RemovePackageFromManifest(PackageFName, PackageChunk);
		}
	}

	if (!bAssignedToChunk)
	{
		NotifyPackageWasNotAssigned(SandboxFilename, PackageFName);
	}
#endif
}

void FChunkManifestGenerator::PrepareToLoadNewPackage(const FString& Filename)
{
	AssetsLoadedWithLastPackage.Empty();
}

void FChunkManifestGenerator::CleanManifestDirectories()
{
	for (auto Platform : Platforms)
	{
		CleanTempPackagingDirectory(Platform->PlatformName());
	}
}

bool FChunkManifestGenerator::SaveManifests(FSandboxPlatformFile* InSandboxFile)
{
	// Always do package dependency work, is required to modify asset registry
	FixupPackageDependenciesForChunks(InSandboxFile);

	if (bGenerateChunks)
	{
		for (auto Platform : Platforms)
		{
			if (!GenerateStreamingInstallManifest(Platform->PlatformName()))
			{
				return false;
			}

			// Generate map for the platform abstraction
			TMultiMap<FString, int32> ChunkMap;	// asset -> ChunkIDs map
			TSet<int32> ChunkIDsInUse;
			const FString PlatformName = Platform->PlatformName();

			// Collect all unique chunk indices and map all files to their chunks
			for (int32 ChunkIndex = 0; ChunkIndex < FinalChunkManifests.Num(); ++ChunkIndex)
			{
				if (FinalChunkManifests[ChunkIndex] && FinalChunkManifests[ChunkIndex]->Num())
				{
					ChunkIDsInUse.Add(ChunkIndex);
					for (auto& Filename : *FinalChunkManifests[ChunkIndex])
					{
						FString PlatFilename = Filename.Value.Replace(TEXT("[Platform]"), *PlatformName);
						ChunkMap.Add(PlatFilename, ChunkIndex);
					}
				}
			}

			// Sort our chunk IDs and file paths
			ChunkMap.KeySort(TLess<FString>());
			ChunkIDsInUse.Sort(TLess<int32>());

			// Platform abstraction will generate any required platform-specific files for the chunks
			if (!Platform->GenerateStreamingInstallManifest(ChunkMap, ChunkIDsInUse))
			{
				return false;
			}
		}


		GenerateAssetChunkInformationCSV(FPaths::Combine(*FPaths::GameLogDir(), TEXT("ChunkLists")));
	}

	return true;
}

bool FChunkManifestGenerator::LoadAssetRegistry(const FString& SandboxPath, const TSet<FName>* PackagesToKeep)
{
	UE_LOG(LogChunkManifestGenerator, Display, TEXT("Saving asset registry."));

	// Load generated registry for each platform
	check(Platforms.Num() == 1);

	for (auto Platform : Platforms)
	{
		/*FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform->PlatformName());
		FArchive* AssetRegistryReader = IFileManager::Get().CreateFileReader(*PlatformSandboxPath);*/

		FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform->PlatformName());
		FArrayReader FileContents;
		if (FFileHelper::LoadFileToArray(FileContents, *PlatformSandboxPath) == false)
		{
			continue;
		}
		FArchive* AssetRegistryReader = &FileContents;

		TMap<FName, FAssetData*> SavedAssetRegistryData;
		AssetRegistry.LoadRegistryData(*AssetRegistryReader, SavedAssetRegistryData);

		for (auto& LoadedAssetData : AssetRegistryData)
		{
			FName ShortPackageName = FName(*FPaths::GetBaseFilename(LoadedAssetData.PackageName.ToString()));
			if (PackagesToKeep &&
				PackagesToKeep->Contains(ShortPackageName) == false)
			{
				continue;
			}

			FAssetData* FoundAssetData = SavedAssetRegistryData.FindRef(LoadedAssetData.ObjectPath);
			if ( FoundAssetData )
			{
				LoadedAssetData.ChunkIDs.Append(FoundAssetData->ChunkIDs);
				
				SavedAssetRegistryData.Remove(LoadedAssetData.ObjectPath);
				delete FoundAssetData;
			}
		}

		for (const auto& SavedAsset : SavedAssetRegistryData)
		{
			FName ShortPackageName = FName(*FPaths::GetBaseFilename(SavedAsset.Value->PackageName.ToString()));

			if (PackagesToKeep && PackagesToKeep->Contains(ShortPackageName))
			{ 
				AssetRegistryData.Add(*SavedAsset.Value);
			}
			
			delete SavedAsset.Value;
		}
		SavedAssetRegistryData.Empty();
	}
	return true;
}

bool FChunkManifestGenerator::SaveAssetRegistry(const FString& SandboxPath)
{
	UE_LOG(LogChunkManifestGenerator, Display, TEXT("Saving asset registry."));

	// Create asset registry data
	FArrayWriter SerializedAssetRegistry;
	SerializedAssetRegistry.SetFilterEditorOnly(true);
	TMap<FName, FAssetData*> GeneratedAssetRegistryData;
	for (auto& AssetData : AssetRegistryData)
	{
		// Add only assets that have actually been cooked and belong to any chunk
		if (AssetData.ChunkIDs.Num() > 0)
		{
			GeneratedAssetRegistryData.Add(AssetData.ObjectPath, &AssetData);
		}
	}
	AssetRegistry.SaveRegistryData(SerializedAssetRegistry, GeneratedAssetRegistryData);
	UE_LOG(LogChunkManifestGenerator, Display, TEXT("Generated asset registry num assets %d, size is %5.2fkb"), GeneratedAssetRegistryData.Num(), (float)SerializedAssetRegistry.Num() / 1024.f);

	// Save the generated registry for each platform
	for (auto Platform : Platforms)
	{
		FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform->PlatformName());
		FFileHelper::SaveArrayToFile(SerializedAssetRegistry, *PlatformSandboxPath);
	}

	UE_LOG(LogChunkManifestGenerator, Display, TEXT("Done saving asset registry."));

	return true;
}

typedef TSharedRef< TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR> > > JsonWriter;
typedef TSharedRef< TJsonReader<TCHAR> > JsonReader;

void CopyJsonValueToWriter( JsonWriter &Json, const FString& ValueName, const TSharedPtr<FJsonValue>& JsonValue )
{
	if ( JsonValue->Type == EJson::String )
	{
		Json->WriteValue( ValueName, JsonValue->AsString() );
	}
	else if ( JsonValue->Type == EJson::Array )
	{
		if (ValueName.IsEmpty())
		{
			Json->WriteArrayStart();
		}
		else
		{
			Json->WriteArrayStart(ValueName);
		}
		
		const TArray<TSharedPtr<FJsonValue>>& Array = JsonValue->AsArray();
		for ( const auto& ArrayValue : Array )
		{
			CopyJsonValueToWriter(Json, FString(), ArrayValue);
		}

		Json->WriteArrayEnd();
	}
	else if ( JsonValue->Type == EJson::Object )
	{
		if (ValueName.IsEmpty())
		{
			Json->WriteObjectStart();
		}
		else
		{
			Json->WriteObjectStart(ValueName);
		}

		const TSharedPtr<FJsonObject>& Object = JsonValue->AsObject();
		for ( const auto& ObjectProperty : Object->Values)
		{
			CopyJsonValueToWriter(Json, ObjectProperty.Key, ObjectProperty.Value );
		}

		Json->WriteObjectEnd();
	}
	else
	{
		
		UE_LOG(LogChunkManifestGenerator, Warning, TEXT("Unrecognized json value type %d in object %s"), *UEnum::GetValueAsString(TEXT("Json.EJson"), JsonValue->Type), *ValueName)
	}
}

// cooked package asset registry saves information about all the cooked packages and assets contained within for stats purposes
// in json format
bool FChunkManifestGenerator::SaveCookedPackageAssetRegistry( const FString& SandboxCookedRegistryFilename, const bool Append )
{
	bool bSuccess = false;
	for ( const auto& Platform : Platforms )
	{
		TSet<FName> CookedPackages;

		// save the file 
		const FString CookedAssetRegistryFilename = SandboxCookedRegistryFilename.Replace(TEXT("[Platform]"), *Platform->PlatformName());

		FString JsonOutString;
		JsonWriter Json = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >::Create(&JsonOutString);

		Json->WriteObjectStart();
		Json->WriteArrayStart(TEXT("Packages"));

		for ( const auto& Package : AllCookedPackages )
		{
			Json->WriteObjectStart(); // unnamed package start
			const FName& PackageName = Package.Key;
			const FString& SandboxPath = Package.Value;

			CookedPackages.Add( PackageName );

			FString PlatformSandboxPath = SandboxPath.Replace(TEXT("[Platform]"), *Platform->PlatformName());
			
			FDateTime TimeStamp = IFileManager::Get().GetTimeStamp( *PlatformSandboxPath );

			Json->WriteValue( "SourcePackageName", PackageName.ToString() );
			Json->WriteValue( "CookedPackageName", PlatformSandboxPath );
			Json->WriteValue( "CookedPackageTimeStamp", TimeStamp.ToString() );
			
			Json->WriteArrayStart("AssetData");
			for (const auto& AssetData : AssetRegistryData)
			{	// Add only assets that have actually been cooked and belong to any chunk
				if (AssetData.ChunkIDs.Num() > 0 && (AssetData.PackageName == PackageName))
				{
					Json->WriteObjectStart();
					// save all their infos 
					Json->WriteValue(TEXT("ObjectPath"), AssetData.ObjectPath.ToString() );
					Json->WriteValue(TEXT("PackageName"), AssetData.PackageName.ToString() );
					Json->WriteValue(TEXT("PackagePath"), AssetData.PackagePath.ToString() );
					Json->WriteValue(TEXT("GroupNames"), AssetData.GroupNames.ToString() );
					Json->WriteValue(TEXT("AssetName"), AssetData.AssetName.ToString() );
					Json->WriteValue(TEXT("AssetClass"), AssetData.AssetClass.ToString() );
					Json->WriteObjectStart("TagsAndValues");
					for ( const auto& Tag : AssetData.TagsAndValues )
					{
						Json->WriteValue( Tag.Key.ToString(), Tag.Value );
					}
					Json->WriteObjectEnd(); // end tags and values object
					Json->WriteObjectEnd(); // end unnamed array object
				}
			}
			Json->WriteArrayEnd();
			Json->WriteObjectEnd(); // unnamed package
		}

		if ( Append )
		{
			FString JsonInString;
			if ( FFileHelper::LoadFileToString(JsonInString, *CookedAssetRegistryFilename) )
			{
				// load up previous package asset registry and fill in any packages which weren't recooked on this run
				JsonReader Reader = TJsonReaderFactory<TCHAR>::Create(JsonInString);
				TSharedPtr<FJsonObject> JsonObject;
				bool shouldRead = FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid() && JsonObject->HasTypedField<EJson::Array>(TEXT("Packages"));
				if ( shouldRead )
				{
					TArray<TSharedPtr<FJsonValue>> PackageList = JsonObject->GetArrayField(TEXT("Packages"));
					for (auto PackageListIt = PackageList.CreateConstIterator(); PackageListIt && shouldRead; ++PackageListIt)
					{
						const TSharedPtr<FJsonValue>& JsonValue = *PackageListIt;
						shouldRead = JsonValue->Type == EJson::Object;
						if ( shouldRead )
						{
							const TSharedPtr<FJsonObject>& JsonPackage = JsonValue->AsObject();

							// get the package name and see if we have already written it out this run
							
							FString CookedPackageName;
							verify( JsonPackage->TryGetStringField(TEXT("SourcePackageName"), CookedPackageName) );

							const FName CookedPackageFName(*CookedPackageName);
							if ( CookedPackages.Contains(CookedPackageFName))
							{
								// don't need to process this package
								continue;
							}


							// check that the on disk version is still valid
							FString SourcePackageName;
							check( JsonPackage->TryGetStringField( TEXT("SourcePackageName"), SourcePackageName) );

							// if our timestamp is different then don't copy the information over
							FDateTime CurrentTimeStamp = IFileManager::Get().GetTimeStamp( *CookedPackageName );

							FString SavedTimeString;
							check( JsonPackage->TryGetStringField(TEXT("CookedPackageTimeStamp"), SavedTimeString) );
							FDateTime SavedTimeStamp;
							FDateTime::Parse(SavedTimeString, SavedTimeStamp);

							if ( SavedTimeStamp != CurrentTimeStamp )
							{
								continue;
							}



							CopyJsonValueToWriter(Json, FString(), JsonValue);
							// read in all the other stuff and copy it over to the new registry
							/*Json->WriteObjectStart(); // open package

							// copy all the values over
							for ( const auto& JsonPackageValue : JsonPackage->Values)
							{
								CopyJsonValueToWriter(Json, JsonPackageValue.Key, JsonPackageValue.Value);
							}

							Json->WriteObjectEnd();*/
						}
						
					}
				}
				else
				{
					UE_LOG(LogChunkManifestGenerator, Warning, TEXT("Unable to read or json is invalid format %s"), *CookedAssetRegistryFilename);
				}
			}
		}


		Json->WriteArrayEnd();
		Json->WriteObjectEnd();

		if (Json->Close())
		{
			FArchive* ItemTemplatesFile = IFileManager::Get().CreateFileWriter(*CookedAssetRegistryFilename);
			if (ItemTemplatesFile)
			{
				// serialize the file contents
				TStringConversion<FTCHARToUTF8_Convert> Convert(*JsonOutString);
				ItemTemplatesFile->Serialize(const_cast<ANSICHAR*>(Convert.Get()), Convert.Length());
				ItemTemplatesFile->Close();
				if ( !ItemTemplatesFile->IsError() )
				{
					bSuccess = true;
				}
				else
				{
					UE_LOG(LogChunkManifestGenerator, Error, TEXT("Unable to write to %s"), *CookedAssetRegistryFilename);
				}
				delete ItemTemplatesFile;
			}
			else
			{
				UE_LOG(LogChunkManifestGenerator, Error, TEXT("Unable to open %s for writing."), *CookedAssetRegistryFilename);
			}
		}
		else
		{
			UE_LOG(LogChunkManifestGenerator, Error, TEXT("Error closing Json Writer"));
		}
	}
	return bSuccess;
}

bool FChunkManifestGenerator::GetPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames)
{
	return AssetRegistry.GetDependencies(PackageName, DependentPackageNames);
}

bool FChunkManifestGenerator::GatherAllPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames)
{
	TArray<FName> LocalDependentPackages;
	if (!GetPackageDependencies(PackageName, LocalDependentPackages))
	{
		return false;
	}

	DependentPackageNames.Append(LocalDependentPackages);
	for (const auto& DependentPackage : LocalDependentPackages)
	{
		if (!GetPackageDependencies(DependentPackage, DependentPackageNames))
		{
			return false;
		}
	}

	return true;
}

bool FChunkManifestGenerator::GenerateAssetChunkInformationCSV(const FString& OutputPath)
{
	FString TmpString;
	FString CSVString;
	FString HeaderText(TEXT("ChunkID, Package Name, Class Type, Hard or Soft Chunk, File Size, Other Chunks\n"));
	FString EndLine(TEXT("\n"));
	FString NoneText(TEXT("None\n"));
	CSVString = HeaderText;

	for (int32 ChunkID = 0, ChunkNum = FinalChunkManifests.Num(); ChunkID < ChunkNum; ++ChunkID)
	{
		FString PerChunkManifestCSV = HeaderText;
		TMap<FName, FAssetData*> GeneratedAssetRegistryData;
		for (auto& AssetData : AssetRegistryData)
		{
			// Add only assets that have actually been cooked and belong to any chunk
			if (AssetData.ChunkIDs.Num() > 0)
			{
				FString Fullname;
				if (AssetData.ChunkIDs.Contains(ChunkID) && FPackageName::DoesPackageExist(*AssetData.PackageName.ToString(), nullptr, &Fullname))
				{
					auto FileSize = IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(*AssetData.PackageName.ToString(), FPackageName::GetAssetPackageExtension()));
					if (FileSize == INDEX_NONE)
					{
						FileSize = IFileManager::Get().FileSize(*FPackageName::LongPackageNameToFilename(*AssetData.PackageName.ToString(), FPackageName::GetMapPackageExtension()));
					}

					if (FileSize == INDEX_NONE)
					{
						FileSize = 0;
					}

					FString SoftChain;
					bool bHardChunk = false;
					if (ChunkID < ChunkManifests.Num())
					{
						bHardChunk = ChunkManifests[ChunkID] && ChunkManifests[ChunkID]->Contains(AssetData.PackageName);
					
						if (!bHardChunk)
						{
							//
							SoftChain = GetShortestReferenceChain(AssetData.PackageName, ChunkID);
						}
					}
					if (SoftChain.IsEmpty())
					{
						SoftChain = TEXT("Soft: Possibly Unassigned Asset");
					}

					TmpString = FString::Printf(TEXT("%d,%s,%s,%s,%lld,"), ChunkID, *AssetData.PackageName.ToString(), *AssetData.AssetClass.ToString(), bHardChunk ? TEXT("Hard") : *SoftChain, FileSize);
					CSVString += TmpString;
					PerChunkManifestCSV += TmpString;
					if (AssetData.ChunkIDs.Num() == 1)
					{
						CSVString += NoneText;
						PerChunkManifestCSV += NoneText;
					}
					else
					{
						for (const auto& OtherChunk : AssetData.ChunkIDs)
						{
							if (OtherChunk != ChunkID)
							{
								TmpString = FString::Printf(TEXT("%d "), OtherChunk);
								CSVString += TmpString;
								PerChunkManifestCSV += TmpString;
							}
						}
						CSVString += EndLine;
						PerChunkManifestCSV += EndLine;
					}
				}
			}
		}

		FFileHelper::SaveStringToFile(PerChunkManifestCSV, *FPaths::Combine(*OutputPath, *FString::Printf(TEXT("Chunks%dInfo.csv"), ChunkID)));
	}

	return FFileHelper::SaveStringToFile(CSVString, *FPaths::Combine(*OutputPath, TEXT("AllChunksInfo.csv")));
}

void FChunkManifestGenerator::AddPackageToManifest(const FString& PackageSandboxPath, FName PackageName, int32 ChunkId)
{
	while (ChunkId >= ChunkManifests.Num())
	{
		ChunkManifests.Add(nullptr);
	}
	if (!ChunkManifests[ChunkId])
	{
		ChunkManifests[ChunkId] = new FChunkPackageSet();
	}
	ChunkManifests[ChunkId]->Add(PackageName, PackageSandboxPath);
	//Safety check, it the package happens to exist in the unassigned list remove it now.
	UnassignedPackageSet.Remove(PackageName);
}

void FChunkManifestGenerator::AddUnassignedPackageToManifest(UPackage* Package, const FString& PackageSandboxPath )
{
	NotifyPackageWasNotAssigned(PackageSandboxPath, Package->GetFName() );
}

void FChunkManifestGenerator::RemovePackageFromManifest(FName PackageName, int32 ChunkId)
{
	if (ChunkManifests[ChunkId])
	{
		ChunkManifests[ChunkId]->Remove(PackageName);
	}
}

void FChunkManifestGenerator::NotifyPackageWasNotAssigned(const FString& PackageSandboxPath, FName PackageName)
{
	UnassignedPackageSet.Add(PackageName, PackageSandboxPath);
}

void FChunkManifestGenerator::NotifyPackageWasCooked(const FString& PackageSandboxPath, FName PackageName)
{
	AllCookedPackages.Add(PackageName, PackageSandboxPath);
}

void FChunkManifestGenerator::ResolveChunkDependencyGraph(const FChunkDependencyTreeNode& Node, FChunkPackageSet BaseAssetSet) 
{
	if (FinalChunkManifests.Num() > Node.ChunkID && FinalChunkManifests[Node.ChunkID])
	{
		for (auto It = BaseAssetSet.CreateConstIterator(); It; ++It)
		{
			// Remove any assets belonging to our parents.
			FinalChunkManifests[Node.ChunkID]->Remove(It.Key());
		}
		// Add the current Chunk's assets
		for (auto It = FinalChunkManifests[Node.ChunkID]->CreateConstIterator(); It; ++It)//for (const auto It : *(FinalChunkManifests[Node.ChunkID]))
		{
			BaseAssetSet.Add(It.Key(), It.Value());
		}
		for (const auto It : Node.ChildNodes)
		{
			ResolveChunkDependencyGraph(It, BaseAssetSet);
		}
	}
}

bool FChunkManifestGenerator::CheckChunkAssetsAreNotInChild(const FChunkDependencyTreeNode& Node)
{
	for (const auto It : Node.ChildNodes)
	{
		if (!CheckChunkAssetsAreNotInChild(It))
		{
			return false;
		}
	}

	if (!(FinalChunkManifests.Num() > Node.ChunkID && FinalChunkManifests[Node.ChunkID]))
	{
		return true;
	}

	for (const auto ChildIt : Node.ChildNodes)
	{
		if(FinalChunkManifests.Num() > ChildIt.ChunkID && FinalChunkManifests[ChildIt.ChunkID])
		{
			for (auto It = FinalChunkManifests[Node.ChunkID]->CreateConstIterator(); It; ++It)
			{
				if (FinalChunkManifests[ChildIt.ChunkID]->Find(It.Key()))
				{
					return false;
				}
			}
		}
	}

	return true;
}

void FChunkManifestGenerator::FixupPackageDependenciesForChunks(FSandboxPlatformFile* InSandboxFile)
{
	for (int32 ChunkID = 0, MaxChunk = ChunkManifests.Num(); ChunkID < MaxChunk; ++ChunkID)
	{
		FinalChunkManifests.Add(nullptr);
		if (!ChunkManifests[ChunkID])
		{
			continue;
		}
		FinalChunkManifests[ChunkID] = new FChunkPackageSet();
		for (auto It = ChunkManifests[ChunkID]->CreateConstIterator(); It; ++It)
		{
			AddPackageAndDependenciesToChunk(FinalChunkManifests[ChunkID], It.Key(), It.Value(), ChunkID, InSandboxFile);
		}
	}

	auto* ChunkDepGraph = DependencyInfo->GetChunkDependencyGraph(ChunkManifests.Num());
	//Once complete, Add any remaining assets (that are not assigned to a chunk) to the first chunk.
	if (FinalChunkManifests.Num() == 0)
	{
		FinalChunkManifests.Add(nullptr);
	}
	if (!FinalChunkManifests[0])
	{
		FinalChunkManifests[0] = new FChunkPackageSet();
	}
	// Copy the remaining assets
	auto RemainingAssets = UnassignedPackageSet;
	for (auto It = RemainingAssets.CreateConstIterator(); It; ++It)
	{
		AddPackageAndDependenciesToChunk(FinalChunkManifests[0], It.Key(), It.Value(), 0, InSandboxFile);
	}

	if (!CheckChunkAssetsAreNotInChild(*ChunkDepGraph))
	{
		UE_LOG(LogChunkManifestGenerator, Log, TEXT("Initial scan of chunks found duplicate assets in graph children"));
	}

	//Finally, if the previous step may added any extra packages to the 0 chunk. Pull them out of other chunks and save space
	ResolveChunkDependencyGraph(*ChunkDepGraph, FChunkPackageSet());

	if (!CheckChunkAssetsAreNotInChild(*ChunkDepGraph))
	{
		UE_LOG(LogChunkManifestGenerator, Error, TEXT("Second Scan of chunks found duplicate asset entries in children."));
	}

	// Fix up the asset registry to reflect this chunk layout
	for (int32 ChunkID = 0, MaxChunk = FinalChunkManifests.Num(); ChunkID < MaxChunk; ++ChunkID)
	{
		if (!FinalChunkManifests[ChunkID])
		{
			continue;
		}
		for (const auto& Asset : *FinalChunkManifests[ChunkID])
		{
			auto* AssetIndexArray = PackageToRegistryDataMap.Find(Asset.Key);
			if (AssetIndexArray)
			{
				for (auto AssetIndex : *AssetIndexArray)
				{
					AssetRegistryData[AssetIndex].ChunkIDs.AddUnique(ChunkID);
				}
			}
		}
	}
}

void FChunkManifestGenerator::AddPackageAndDependenciesToChunk(FChunkPackageSet* ThisPackageSet, FName InPkgName, const FString& InSandboxFile, int32 ChunkID, FSandboxPlatformFile* SandboxPlatformFile)
{
	//Add this asset
	ThisPackageSet->Add(InPkgName, InSandboxFile);

	//Only gather dependencies if we're chunking
	if (!bGenerateChunks)
	{
		return;
	}

	//now add any dependencies
	TArray<FName> DependentPackageNames;
	if (GatherAllPackageDependencies(InPkgName, DependentPackageNames))
	{
		for (const auto& PkgName : DependentPackageNames)
		{
			bool bSkip = false;
			if (ChunkID != 0 && FinalChunkManifests[0])
			{
				// Do not add if this asset was assigned to the 0 chunk. These assets always exist on disk
				bSkip = FinalChunkManifests[0]->Contains(PkgName);
			}
			if (!bSkip)
			{
				auto DependentPackageLongName = PkgName.ToString();
				if (FPackageName::IsShortPackageName(PkgName))
				{
					DependentPackageLongName = FPackageName::ConvertToLongScriptPackageName(*PkgName.ToString());
				}
				FString DependentSandboxFile = SandboxPlatformFile->ConvertToAbsolutePathForExternalAppForWrite(*FPackageName::LongPackageNameToFilename(DependentPackageLongName));
				ThisPackageSet->Add(PkgName, DependentSandboxFile);
				UnassignedPackageSet.Remove(PkgName);
			}
		}
	}
}


void FChunkManifestGenerator::FindShortestReferenceChain(TArray<FReferencePair> PackageNames, int32 ChunkID, uint32& OutParentIndex, FString& OutChainPath)
{
	TArray<FReferencePair> ReferencesToCheck;
	uint32 Index = 0;
	for (const auto& Pkg : PackageNames)
	{
		if (ChunkManifests[ChunkID] && ChunkManifests[ChunkID]->Contains(Pkg.PackageName))
		{
			OutChainPath += TEXT("Soft: ");
			OutChainPath += Pkg.PackageName.ToString();
			OutParentIndex = Pkg.ParentNodeIndex;
			return;
		}
		TArray<FName> AssetReferences;
		AssetRegistry.GetReferencers(Pkg.PackageName, AssetReferences);
		for (const auto& Ref : AssetReferences)
		{
			if (!InspectedNames.Contains(Ref))
			{
				ReferencesToCheck.Add(FReferencePair(Ref, Index));
				InspectedNames.Add(Ref);
			}
		}

		++Index;
	}

	if (ReferencesToCheck.Num() > 0)
	{
		uint32 ParentIndex = INDEX_NONE;
		FindShortestReferenceChain(ReferencesToCheck, ChunkID, ParentIndex, OutChainPath);

		if (ParentIndex < (uint32)PackageNames.Num())
		{
			OutChainPath += TEXT("->");
			OutChainPath += PackageNames[ParentIndex].PackageName.ToString();
			OutParentIndex = PackageNames[ParentIndex].ParentNodeIndex;
		}
	}
	else if (PackageNames.Num() > 0)
	{
		//best guess
		OutChainPath += TEXT("Soft From Unassigned Package? Best Guess: ");
		OutChainPath += PackageNames[0].PackageName.ToString();
		OutParentIndex = PackageNames[0].ParentNodeIndex;
	}
}

FString FChunkManifestGenerator::GetShortestReferenceChain(FName PackageName, int32 ChunkID)
{
	FString StringChain;
	TArray<FReferencePair> ReferencesToCheck;
	uint32 ParentIndex;
	ReferencesToCheck.Add(FReferencePair(PackageName, 0));
	InspectedNames.Empty();
	InspectedNames.Add(PackageName);
	FindShortestReferenceChain(ReferencesToCheck, ChunkID, ParentIndex, StringChain);

	return StringChain;
}
