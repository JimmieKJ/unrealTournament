// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchGeneration.cpp: Implements the classes that control build
	installation, and the generation of chunks and manifests from a build image.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

#include "Generation/DataScanner.h"
#include "Generation/BuildStreamer.h"
#include "Generation/CloudEnumeration.h"
#include "Generation/ManifestBuilder.h"
#include "Generation/FileAttributesParser.h"

using namespace BuildPatchServices;

#if WITH_BUILDPATCHGENERATION

FFileAttributes::FFileAttributes()
	: bReadOnly(false)
	, bCompressed(false)
	, bUnixExecutable(false)
{}

/**
 * Creates a 32bit hash value from a FSHAHashData. This is so that a TMap can be keyed
 * using a FSHAHashData
 * @param Hash	The SHA1 hash
 * @return The crc for the data
 */
FORCEINLINE uint32 GetTypeHash(const FSHAHashData& Hash)
{
	return FCrc::MemCrc32(Hash.Hash, FSHA1::DigestSize);
}

static bool IsUnixExecutable(const TCHAR* Filename)
{
#if PLATFORM_MAC
	struct stat FileInfo;
	if (stat(TCHAR_TO_UTF8(Filename), &FileInfo) == 0)
	{
		return (FileInfo.st_mode & S_IXUSR) != 0;
	}
#endif
	return false;
}

static FString GetSymlinkTarget(const TCHAR* Filename)
{
#if PLATFORM_MAC
	ANSICHAR SymlinkTarget[MAX_PATH] = { 0 };
	if (readlink(TCHAR_TO_UTF8(Filename), SymlinkTarget, MAX_PATH) != -1)
	{
		return UTF8_TO_TCHAR(SymlinkTarget);
	}
#endif
	return TEXT("");
}

/* FBuildDataFileProcessor implementation
*****************************************************************************/
FBuildDataFileProcessor::FBuildDataFileProcessor(FBuildPatchAppManifestRef InBuildManifest, const FString& InBuildRoot, const FDateTime& InDataThresholdTime)
	: NumNewFiles( 0 )
	, NumKnownFiles( 0 )
	, BuildManifest( InBuildManifest )
	, BuildRoot( InBuildRoot )
	, CurrentFile(nullptr)
	, FileHash()
	, FileSize( 0 )
	, IsProcessingFile( false )
	, DataThresholdTime( InDataThresholdTime )
{
}

void FBuildDataFileProcessor::BeginFile( const FString& InFileName )
{
	check( !IsProcessingFile );
	IsProcessingFile = true;

	// Create the new file
	BuildManifest->Data->FileManifestList.Add(FFileManifestData());
	CurrentFile = &BuildManifest->Data->FileManifestList.Last();
	CurrentFile->Filename = InFileName;
	CurrentFile->bIsUnixExecutable = IsUnixExecutable(*(BuildRoot / InFileName));
	CurrentFile->SymlinkTarget = GetSymlinkTarget(*(BuildRoot / InFileName));

	// Add the 'chunk part' which will just be the whole file
	CurrentFile->FileChunkParts.Add(FChunkPartData());

	// Reset the hash and size counter
	FileHash.Reset();
	FileSize = 0;
}

void FBuildDataFileProcessor::ProcessFileData( const uint8* Data, const uint32& DataLen )
{
	check( IsProcessingFile );

	// Update the hash
	FileHash.Update( Data, DataLen );

	// Count size
	FileSize += DataLen;
}

void FBuildDataFileProcessor::EndFile()
{
	check( IsProcessingFile );
	check(CurrentFile);
	check( CurrentFile->FileChunkParts.Num() == 1 );
	IsProcessingFile = false;

	// Finalize the hash
	FileHash.Final();
	FileHash.GetHash( CurrentFile->FileHash.Hash );

	// Use hash and full file name to find out if we have a file match
	FString FullPath = FPaths::Combine( *BuildRoot, *CurrentFile->Filename );
	FGuid FileGuid = FGuid::NewGuid();
	bool bFoundSameFile = FBuildDataGenerator::FindExistingFileData( FullPath, CurrentFile->FileHash, DataThresholdTime, FileGuid );

	// Fill the 'chunk part' info
	FChunkPartData& FilePart = CurrentFile->FileChunkParts[0];
	FilePart.Guid = FileGuid;
	FilePart.Offset = 0;
	FilePart.Size = FileSize;

	// Init and check size
	CurrentFile->Init();
	check(CurrentFile->GetFileSize() == FileSize);

	// Always call save to ensure new versions exist when recognising old ones, the save will be skipped if required copies already exist.
	FBuildDataGenerator::SaveOutFileData( FullPath, CurrentFile->FileHash, FileGuid );

	// Also add the info to the data
	if (!BuildManifest->ChunkInfoLookup.Contains(FileGuid))
	{
		BuildManifest->Data->ChunkList.Add(FChunkInfoData());
		FChunkInfoData& DataInfo = BuildManifest->Data->ChunkList.Last();
		DataInfo.Guid = FileGuid;
		DataInfo.Hash = INDEX_NONE;
		DataInfo.FileSize = FileSize;
		DataInfo.GroupNumber = FCrc::MemCrc32(&FileGuid, sizeof(FGuid)) % 100;
		// Lookup is used just to ensure one copy of each chunk added, we must not use the ptr as array will realloc
		BuildManifest->ChunkInfoLookup.Add(FileGuid, nullptr);
	}

	// Count new/known stats
	if( bFoundSameFile == false )
	{
		++NumNewFiles;
	}
	else
	{
		++NumKnownFiles;
	}

	// Output to log for builder info
	GLog->Logf( TEXT( "%s %s [%d:%d]" ), *BuildManifest->GetAppName(), *BuildManifest->GetVersionString(), NumNewFiles, NumKnownFiles );
}

void FBuildDataFileProcessor::GetFileStats( uint32& OutNewFiles, uint32& OutKnownFiles )
{
	OutNewFiles = NumNewFiles;
	OutKnownFiles = NumKnownFiles;
}

/* FBuildSimpleChunkCache::FChunkReader implementation
*****************************************************************************/
FBuildGenerationChunkCache::FChunkReader::FChunkReader(const FString& InChunkFilePath, TSharedRef< FChunkFile > InChunkFile, uint32* InBytesRead)
	: ChunkFilePath( InChunkFilePath )
	, ChunkFileReader( NULL )
	, ChunkFile( InChunkFile )
	, FileBytesRead( InBytesRead )
	, MemoryBytesRead( 0 )
{
	ChunkFile->GetDataLock( &ChunkData, &ChunkHeader );
}

FBuildGenerationChunkCache::FChunkReader::~FChunkReader()
{
	ChunkFile->ReleaseDataLock();

	// Close file handle
	if( ChunkFileReader != NULL )
	{
		ChunkFileReader->Close();
		delete ChunkFileReader;
		ChunkFileReader = NULL;
	}
}

FArchive* FBuildGenerationChunkCache::FChunkReader::GetArchive()
{
	// Open file handle?
	if( ChunkFileReader == NULL )
	{
		ChunkFileReader = IFileManager::Get().CreateFileReader( *ChunkFilePath );
		if( ChunkFileReader == NULL )
		{
			// Break the magic to mark as invalid
			ChunkHeader->Magic = 0;
			GLog->Logf( TEXT( "WARNING: Skipped missing chunk file %s" ), *ChunkFilePath );
			return NULL;
		}
		// Read Header?
		if( ChunkHeader->Guid.IsValid() == false )
		{
			*ChunkFileReader << *ChunkHeader;
		}
		// Check we can seek otherwise bad chunk
		const int64 ExpectedFileSize = ChunkHeader->DataSize + ChunkHeader->HeaderSize;
		const int64 ChunkFileSize = ChunkFileReader->TotalSize();
		const int64 NextByte = ChunkHeader->HeaderSize + *FileBytesRead;
		if( ChunkFileSize == ExpectedFileSize
		 && NextByte < ChunkFileSize )
		{
			// Seek to next byte
			ChunkFileReader->Seek( NextByte );
			// Break the magic to mark as invalid if archive errored, this chunk will get ignored
			if( ChunkFileReader->GetError() )
			{
				ChunkHeader->Magic = 0;
			}
		}
		else
		{
			// Break the magic to mark as invalid
			ChunkHeader->Magic = 0;
		}
		// If this chunk is valid and compressed, we must read the entire file and decompress to memory now if we have not already
		// as we cannot compare to compressed data
		if( *FileBytesRead == 0 && ChunkHeader->StoredAs & FChunkHeader::STORED_COMPRESSED )
		{
			// Load the compressed chunk data
			TArray< uint8 > CompressedData;
			CompressedData.Empty( ChunkHeader->DataSize );
			CompressedData.AddUninitialized( ChunkHeader->DataSize );
			ChunkFileReader->Serialize( CompressedData.GetData(), ChunkHeader->DataSize );
			// Uncompress
			bool bSuceess = FCompression::UncompressMemory(
				static_cast< ECompressionFlags >( COMPRESS_ZLIB | COMPRESS_BiasMemory ),
				ChunkData,
				FBuildPatchData::ChunkDataSize,
				CompressedData.GetData(),
				ChunkHeader->DataSize );
			// Mark that we have fully read decompressed data and update the chunkfile's data size as we are expanding it
			*FileBytesRead = FBuildPatchData::ChunkDataSize;
			ChunkHeader->DataSize = FBuildPatchData::ChunkDataSize;
			// Check uncompression was OK
			if( !bSuceess )
			{
				ChunkHeader->Magic = 0;
			}
		}
	}
	return ChunkFileReader;
}

const bool FBuildGenerationChunkCache::FChunkReader::IsValidChunk()
{
	if( ChunkHeader->Guid.IsValid() == false )
	{
		GetArchive();
	}
	// Check magic, and current support etc.
	const bool bValidHeader = ChunkHeader->IsValidMagic();
	const bool bValidChunkGuid = ChunkHeader->Guid.IsValid();
	const bool bSupportedFormat = ChunkHeader->StoredAs == FChunkHeader::STORED_RAW || ChunkHeader->StoredAs == FChunkHeader::STORED_COMPRESSED;
	return bValidHeader && bValidChunkGuid && bSupportedFormat;
}

const FGuid& FBuildGenerationChunkCache::FChunkReader::GetChunkGuid()
{
	if( ChunkHeader->Guid.IsValid() == false )
	{
		GetArchive();
	}
	return ChunkHeader->Guid;
}

void FBuildGenerationChunkCache::FChunkReader::ReadNextBytes( uint8** OutDataBuffer, const uint32& ReadLength )
{
	uint8* BufferNextByte = &ChunkData[ MemoryBytesRead ];
	// Do we need to load from disk?
	if( ( MemoryBytesRead + ReadLength ) > *FileBytesRead )
	{
		FArchive* Reader = GetArchive();
		// Do not allow incorrect usage
		check( Reader );
		check( ReadLength <= BytesLeft() );
		// Read the number of bytes extra we need
		const int32 NumFileBytesRead = *FileBytesRead;
		const int32 NextMemoryBytesRead = MemoryBytesRead + ReadLength;
		const uint32 FileReadLen = FMath::Max<int32>( 0, NextMemoryBytesRead - NumFileBytesRead );
		*FileBytesRead += FileReadLen;
		Reader->Serialize( BufferNextByte, FileReadLen );
		// Assert if read error, if theres some problem accessing chunks then continuing would cause bad patch
		// ratios, so it's better to hard fault.
		const bool bChunkReadOK = !Reader->GetError();
		if( !bChunkReadOK )
		{
			// Print something helpful
			GLog->Logf( TEXT( "FATAL ERROR: Could not read from chunk FArchive %s" ), *ChunkFilePath );
			// Check with bool variable so that output will be readable
			check( bChunkReadOK );
		}
	}
	MemoryBytesRead += ReadLength;
	(*OutDataBuffer) = BufferNextByte;
}

const uint32 FBuildGenerationChunkCache::FChunkReader::BytesLeft()
{
	if( ChunkHeader->Guid.IsValid() == false )
	{
		GetArchive();
	}
	return FBuildPatchData::ChunkDataSize - MemoryBytesRead;
}

/* FBuildGenerationChunkCache implementation
*****************************************************************************/
FBuildGenerationChunkCache::FBuildGenerationChunkCache()
	: StatChunksInDataCache(nullptr)
	, StatNumCacheLoads(nullptr)
	, StatNumCacheBoots(nullptr)
{
}

void FBuildGenerationChunkCache::SetStatsCollector(FStatsCollectorRef InStatsCollector)
{
	StatsCollector = InStatsCollector;
	StatChunksInDataCache = StatsCollector->CreateStat(TEXT("Chunk Cache: Num Chunks"), EStatFormat::Value);
	StatNumCacheLoads = StatsCollector->CreateStat(TEXT("Chunk Cache: Num Chunks Loaded"), EStatFormat::Value);
	StatNumCacheBoots = StatsCollector->CreateStat(TEXT("Chunk Cache: Num Chunks Booted"), EStatFormat::Value);
}

TSharedRef< FBuildGenerationChunkCache::FChunkReader > FBuildGenerationChunkCache::GetChunkReader( const FString& ChunkFilePath )
{
	if( ChunkCache.Contains( ChunkFilePath ) == false )
	{
		// Remove oldest access from cache?
		if( ChunkCache.Num() >= NumChunksToCache )
		{
			FString OldestAccessChunk = TEXT( "" );
			double OldestAccessTime = FPlatformTime::Seconds();
			for( auto ChunkCacheIt = ChunkCache.CreateConstIterator(); ChunkCacheIt; ++ChunkCacheIt )
			{
				const FString& ChunkCacheFilePath = ChunkCacheIt.Key();
				const FChunkFile& ChunkFile = ChunkCacheIt.Value().Get();
				if( ChunkFile.GetLastAccessTime() < OldestAccessTime )
				{
					OldestAccessTime = ChunkFile.GetLastAccessTime();
					OldestAccessChunk = ChunkCacheFilePath;
				}
			}
			ChunkCache.Remove( OldestAccessChunk );
			delete BytesReadPerChunk[ OldestAccessChunk ];
			BytesReadPerChunk.Remove( OldestAccessChunk );
			if(StatChunksInDataCache != nullptr)
			{
				FStatsCollector::Accumulate(StatChunksInDataCache, -1);
			}
			if(StatNumCacheBoots != nullptr)
			{
				FStatsCollector::Accumulate(StatNumCacheBoots, 1);
			}
		}
		// Add the chunk to cache
		ChunkCache.Add( ChunkFilePath, MakeShareable( new FChunkFile( 1, true ) ) );
		BytesReadPerChunk.Add( ChunkFilePath, new uint32( 0 ) );
		if(StatChunksInDataCache != nullptr)
		{
			FStatsCollector::Accumulate(StatChunksInDataCache, 1);
		}
		if(StatNumCacheLoads != nullptr)
		{
			FStatsCollector::Accumulate(StatNumCacheLoads, 1);
		}
	}
	return MakeShareable( new FChunkReader(ChunkFilePath, ChunkCache[ ChunkFilePath ], BytesReadPerChunk[ ChunkFilePath ]) );
}

void FBuildGenerationChunkCache::Cleanup()
{
	ChunkCache.Empty( 0 );
	for( auto BytesReadPerChunkIt = BytesReadPerChunk.CreateConstIterator(); BytesReadPerChunkIt; ++BytesReadPerChunkIt )
	{
		delete BytesReadPerChunkIt.Value();
	}
	BytesReadPerChunk.Empty( 0 );
	if(StatChunksInDataCache != nullptr)
	{
		FStatsCollector::Set(StatChunksInDataCache, 0);
	}
}

/* FBuildGenerationChunkCache system singleton setup
*****************************************************************************/
TSharedPtr< FBuildGenerationChunkCache > FBuildGenerationChunkCache::SingletonInstance = NULL;

void FBuildGenerationChunkCache::Init()
{
	// We won't allow misuse of these functions
	check( !SingletonInstance.IsValid() );
	SingletonInstance = MakeShareable(new FBuildGenerationChunkCache());
}

FBuildGenerationChunkCache& FBuildGenerationChunkCache::Get()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	return *SingletonInstance.Get();
}

void FBuildGenerationChunkCache::Shutdown()
{
	// We won't allow misuse of these functions
	check( SingletonInstance.IsValid() );
	SingletonInstance->Cleanup();
	SingletonInstance.Reset();
}

/* FBuildDataGenerator static variables
*****************************************************************************/
TMap< FGuid, FString > FBuildDataGenerator::ExistingChunkGuidInventory;
TMap< uint64, TArray< FGuid > > FBuildDataGenerator::ExistingChunkHashInventory;
bool FBuildDataGenerator::ExistingChunksEnumerated = false;
TMap< FSHAHashData, TArray< FString > > FBuildDataGenerator::ExistingFileInventory;
bool FBuildDataGenerator::ExistingFilesEnumerated = false;
FCriticalSection FBuildDataGenerator::SingleConcurrentBuildCS;

static void AddCustomFieldsToBuildManifest(const TMap<FString, FVariant>& CustomFields, IBuildManifestPtr BuildManifest)
{
	for (const auto& CustomField : CustomFields)
	{
		int32 VarType = CustomField.Value.GetType();
		if (VarType == EVariantTypes::Float || VarType == EVariantTypes::Double)
		{
			BuildManifest->SetCustomField(CustomField.Key, (double)CustomField.Value);
		}
		else if (VarType == EVariantTypes::Int8 || VarType == EVariantTypes::Int16 || VarType == EVariantTypes::Int32 || VarType == EVariantTypes::Int64 ||
			VarType == EVariantTypes::UInt8 || VarType == EVariantTypes::UInt16 || VarType == EVariantTypes::UInt32 || VarType == EVariantTypes::UInt64)
		{
			BuildManifest->SetCustomField(CustomField.Key, (int64)CustomField.Value);
		}
		else if (VarType == EVariantTypes::String)
		{
			BuildManifest->SetCustomField(CustomField.Key, CustomField.Value.GetValue<FString>());
		}
	}
}

/* FBuildDataGenerator implementation
*****************************************************************************/
bool FBuildDataGenerator::GenerateChunksManifestFromDirectory( const FBuildPatchSettings& Settings )
{
	uint64 StartTime = FStatsCollector::GetCycles();

	// Take the build CS
	FScopeLock SingleConcurrentBuild( &SingleConcurrentBuildCS );

	// Create a manifest details
	FManifestDetails ManifestDetails;
	ManifestDetails.bIsFileData = false;
	ManifestDetails.AppId = Settings.AppID;
	ManifestDetails.AppName = Settings.AppName;
	ManifestDetails.BuildVersion = Settings.BuildVersion;
	ManifestDetails.LaunchExe = Settings.LaunchExe;
	ManifestDetails.LaunchCommand = Settings.LaunchCommand;
	ManifestDetails.PrereqName = Settings.PrereqName;
	ManifestDetails.PrereqPath = Settings.PrereqPath;
	ManifestDetails.PrereqArgs = Settings.PrereqArgs;
	ManifestDetails.CustomFields = Settings.CustomFields;

	// Load the required file attributes
	if(!Settings.AttributeListFile.IsEmpty())
	{
		FFileAttributesParserRef FileAttributesParser = FFileAttributesParserFactory::Create();
		if(!FileAttributesParser->ParseFileAttributes(Settings.AttributeListFile, ManifestDetails.FileAttributesMap))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Attributes list file did not parse %s"), *Settings.AttributeListFile);
			return false;
		}
	}

	// Create stat collector
	FStatsCollectorRef StatsCollector = FStatsCollectorFactory::Create();

	// Enumerate Chunks
	const FDateTime Cutoff = Settings.bShouldHonorReuseThreshold ? FDateTime::UtcNow() - FTimespan::FromDays(Settings.DataAgeThreshold) : FDateTime::MinValue();
	FCloudEnumerationRef CloudEnumeration = FCloudEnumerationFactory::Create(FBuildPatchServicesModule::GetCloudDirectory(), Cutoff);

	// Force waiting on cloud enumeration for more accurate stats, this line can be removed when stats are not required.
	CloudEnumeration->GetChunkInventory();

	// Chunks matching
	FDataMatcherRef DataMatcher = FDataMatcherFactory::Create(FBuildPatchServicesModule::GetCloudDirectory());

	// Create a build streamer
	FBuildStreamerRef BuildStream = FBuildStreamerFactory::Create(Settings.RootDirectory, Settings.IgnoreListFile, StatsCollector);

	// Output to log for builder info
	GLog->Logf(TEXT("Running Chunks Patch Generation for: %u:%s %s"), Settings.AppID, *Settings.AppName, *Settings.BuildVersion);

	// Create our chunk cache
	FBuildGenerationChunkCache::Init();
	FBuildGenerationChunkCache::Get().SetStatsCollector(StatsCollector);

	// Create the manifest builder
	FManifestBuilderRef ManifestBuilder = FManifestBuilderFactory::Create(ManifestDetails, BuildStream);

	// Used to store data read lengths
	uint32 ReadLen = 0;

	// The last time we logged out data processed
	double LastProgressLog = FPlatformTime::Seconds();
	const double TimeGenStarted = LastProgressLog;

	// 50MB Data buffer
	const int32 DataBufferSize = 1024*1024*15;
	const int32 OverlapSize = FBuildPatchData::ChunkDataSize - 1;
	TArray<uint8> DataBuffer;
	uint64 DataOffset = 0;

	// Setup Generation stats
	auto* StatTotalTime = StatsCollector->CreateStat(TEXT("Generation: Total Time"), EStatFormat::Timer);

	// Loop through all data
	while (!BuildStream->IsEndOfData())
	{
		// Keep the overlap data from previous scanner
		int32 PreviousSize = DataBuffer.Num();
		if(PreviousSize > 0)
		{
			check(PreviousSize > OverlapSize);
			uint8* CopyTo = DataBuffer.GetData();
			uint8* CopyFrom = CopyTo + (PreviousSize - OverlapSize);
			FMemory::Memcpy(CopyTo, CopyFrom, OverlapSize);
			DataBuffer.SetNum(OverlapSize, false);
			DataOffset += DataBufferSize - OverlapSize;
		}

		// Grab some data from the build stream
		PreviousSize = DataBuffer.Num();
		DataBuffer.SetNumUninitialized(DataBufferSize);
		ReadLen = BuildStream->DequeueData(DataBuffer.GetData() + PreviousSize, DataBufferSize - PreviousSize);
		DataBuffer.SetNum(PreviousSize + ReadLen, false);

		// Create data processor, waiting for available slot first
		while(FDataScannerCounter::GetNumIncompleteScanners() > FDataScannerCounter::GetNumRunningScanners())
		{
			FPlatformProcess::Sleep(0.01f);
			StatsCollector->LogStats(10.0f);
			GLog->FlushThreadedLogs();
		}

		// Pass a data scanner for this piece of data to the manifest builder
		ManifestBuilder->AddDataScanner(FDataScannerFactory::Create(DataOffset, DataBuffer, CloudEnumeration, DataMatcher, StatsCollector));

		// Log collected stats
		FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
		StatsCollector->LogStats(10.0f);
		GLog->FlushThreadedLogs();
	}

	// Save manifest into the cloud directory
	FString JsonFilename = FBuildPatchServicesModule::GetCloudDirectory() / FDefaultValueHelper::RemoveWhitespaces(Settings.AppName + Settings.BuildVersion) + TEXT(".manifest");
	ManifestBuilder->SaveToFile(JsonFilename);

	// Final value log
	FStatsCollector::Set(StatTotalTime, FStatsCollector::GetCycles() - StartTime);
	StatsCollector->LogStats();

	// Output to log for builder info
	GLog->Logf(TEXT("Saved manifest to %s"), *JsonFilename);


	uint64 EndTime = FStatsCollector::GetCycles();
	GLog->Logf(TEXT("Completed in %s"), *FPlatformTime::PrettyTime(FStatsCollector::CyclesToSeconds(EndTime - StartTime)));

	// Clean up memory
	FBuildGenerationChunkCache::Shutdown();

	return true;
}

bool FBuildDataGenerator::GenerateFilesManifestFromDirectory( const FBuildPatchSettings& Settings )
{
	// Output to log for builder info
	GLog->Logf(TEXT("Running Files Patch Generation for: %u:%s %s"), Settings.AppID, *Settings.AppName, *Settings.BuildVersion);

	// Take the build CS
	FScopeLock SingleConcurrentBuild( &SingleConcurrentBuildCS );

	// Create a manifest
	FBuildPatchAppManifestRef BuildManifest = MakeShareable( new FBuildPatchAppManifest() );

	// Setup custom fields
	AddCustomFieldsToBuildManifest(Settings.CustomFields, BuildManifest);

	// Get the file attributes
	FString AttributesList;
	TMap<FString, FFileAttributes> FileAttributesMap;
	if(!Settings.AttributeListFile.IsEmpty())
	{
		FFileAttributesParserRef FileAttributesParser = FFileAttributesParserFactory::Create();
		if(!FileAttributesParser->ParseFileAttributes(Settings.AttributeListFile, FileAttributesMap))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Attributes list file did not parse %s"), *Settings.AttributeListFile);
			return false;
		}
	}

	// Reset file inventory
	ExistingFilesEnumerated = false;
	ExistingFileInventory.Empty();

	// Declare a build processor
	const FDateTime Cutoff = Settings.bShouldHonorReuseThreshold ? FDateTime::UtcNow() - FTimespan::FromDays(Settings.DataAgeThreshold) : FDateTime::MinValue();
	FBuildDataFileProcessor DataProcessor(BuildManifest, Settings.RootDirectory, Cutoff);

	// Set the basic details
	BuildManifest->Data->bIsFileData = true;
	BuildManifest->Data->AppID = Settings.AppID;
	BuildManifest->Data->AppName = Settings.AppName;
	BuildManifest->Data->BuildVersion = Settings.BuildVersion;
	BuildManifest->Data->LaunchExe = Settings.LaunchExe;
	BuildManifest->Data->LaunchCommand = Settings.LaunchCommand;
	BuildManifest->Data->PrereqName = Settings.PrereqName;
	BuildManifest->Data->PrereqPath = Settings.PrereqPath;
	BuildManifest->Data->PrereqArgs = Settings.PrereqArgs;

	// Create a data buffer
	uint8* FileReadBuffer = new uint8[ FileBufferSize ];
	
	// Refers to how much data has been processed (into the FBuildDataFileProcessor)
	uint64 ProcessPos = 0;

	// Find all files
	TArray< FString > AllFiles;
	IFileManager::Get().FindFilesRecursive(AllFiles, *Settings.RootDirectory, TEXT("*.*"), true, false);
	AllFiles.Sort();

	// Remove the files that appear in an ignore list
	FBuildDataGenerator::StripIgnoredFiles(AllFiles, Settings.RootDirectory, Settings.IgnoreListFile);

	// Loop through all files
	for( auto FileIt = AllFiles.CreateConstIterator(); FileIt; ++FileIt )
	{
		const FString& FileName = *FileIt;
		// Read the file
		FArchive* FileReader = IFileManager::Get().CreateFileReader( *FileName );
		checkf(FileReader != nullptr, TEXT("Fatal Error: Could not open build file %s"), *FileName);
		if( FileReader != NULL )
		{
			// Make SourceFile the format we want it in and start a new file
			FString SourceFile = FileName;
			FPaths::MakePathRelativeTo(SourceFile, *(Settings.RootDirectory + TEXT("/")));
			int64 FileSize = FileReader->TotalSize();
			if( FileSize < 0 )
			{
				// Skip potential error ( INDEX_NONE == -1 )
				continue;
			}
			DataProcessor.BeginFile( SourceFile );
			while( !FileReader->AtEnd() )
			{
				const int64 SizeLeft = FileSize - FileReader->Tell();
				const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
				ProcessPos += ReadLen;
				FileReader->Serialize( FileReadBuffer, ReadLen );
				// Copy into data stream
				DataProcessor.ProcessFileData( FileReadBuffer, ReadLen );
			}
			FileReader->Close();
			delete FileReader;
			DataProcessor.EndFile();
		}
	}

	// Fill out lookups
	BuildManifest->InitLookups();

	// Fill out the file attributes
	for (const auto& Entry : FileAttributesMap)
	{
		const FString& Filename = Entry.Key;
		const FFileAttributes& Attributes = Entry.Value;
		if (BuildManifest->FileManifestLookup.Contains(Filename))
		{
			FFileManifestData& FileManifest = *BuildManifest->FileManifestLookup[Filename];
			FileManifest.bIsReadOnly = Attributes.bReadOnly;
			FileManifest.bIsCompressed = Attributes.bCompressed;
			FileManifest.InstallTags = Attributes.InstallTags.Array();
			// Only overwrite unix exe if true
			if (Attributes.bUnixExecutable)
			{
				FileManifest.bIsUnixExecutable = Attributes.bUnixExecutable;
			}
		}
		else
		{
			GLog->Logf(TEXT("File Attributes: File not in build %s"), *Filename);
		}
	}

	// Save manifest into the cloud directory
	FString JsonFilename = FBuildPatchServicesModule::GetCloudDirectory() / FDefaultValueHelper::RemoveWhitespaces(BuildManifest->Data->AppName + BuildManifest->Data->BuildVersion) + TEXT(".manifest");
	BuildManifest->Data->ManifestFileVersion = EBuildPatchAppManifestVersion::GetLatestFileDataVersion();
	BuildManifest->SaveToFile(JsonFilename, false);

	// Output to log for builder info
	GLog->Logf(TEXT("Saved manifest to %s"), *JsonFilename);

	// Clean up memory
	delete[] FileReadBuffer;

	// @TODO LSwift: Detect errors and return false on failure
	return true;
}

bool FBuildDataGenerator::FindExistingFileData(const FString& InSourceFile, const FSHAHashData& InFileHash, const FDateTime& DataThresholdTime, FGuid& OutFileGuid)
{
	bool bFoundMatchingFile = false;

	// Perform an inventory on Cloud files if not already done
	if( ExistingFilesEnumerated == false )
	{
		IFileManager& FileManager = IFileManager::Get();
		FString JSONOutput;
		TSharedRef< TJsonWriter< TCHAR, TPrettyJsonPrintPolicy< TCHAR > > > DebugWriter = TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy< TCHAR > >::Create( &JSONOutput );
		DebugWriter->WriteObjectStart();

		TArray< FGuid > FoundFiles;

		// The directory containing old filename version files
		const FString CloudFileDir = FBuildPatchServicesModule::GetCloudDirectory() / TEXT( "Files" );
		// The directory containing new filename version files
		const FString CloudFile2Dir = FBuildPatchServicesModule::GetCloudDirectory() / TEXT( "FilesV2" );

		GLog->Logf( TEXT( "BuildDataGenerator: Enumerating Files from %s" ), *CloudFile2Dir );
		if( FileManager.DirectoryExists( *CloudFile2Dir ) )
		{
			const double StartEnumerate = FPlatformTime::Seconds();

			// Find all files
			TArray<FString> AllFiles;
			FileManager.FindFilesRecursive( AllFiles, *CloudFile2Dir, TEXT("*.file"), true, false );

			for( auto FileIt = AllFiles.CreateConstIterator(); FileIt; ++FileIt )
			{
				FString SourceFile = *FileIt;
				FSHAHashData FoundHash;
				FGuid FoundGuid;
				FBuildPatchUtils::GetFileDetailFromNewFilename( SourceFile, FoundGuid, FoundHash );

				// Add to inventory
				FoundFiles.Add( FoundGuid );
				TArray< FString >& FileList = ExistingFileInventory.FindOrAdd( FoundHash );
				FileList.Add( SourceFile );

				const void* HashBuffer = &FoundHash;
				DebugWriter->WriteValue( FoundGuid.ToString(), FString::FromBlob( static_cast<const uint8*>( HashBuffer ), sizeof( FoundHash ) ) );
			}

			const double EnumerateTime = FPlatformTime::Seconds() - StartEnumerate;
			GLog->Logf( TEXT( "BuildDataGenerator: Found %d new name files in %.1f seconds" ), AllFiles.Num(), EnumerateTime );
		}
		else
		{
			GLog->Logf( TEXT( "BuildDataGenerator: Cloud directory does not exist: %s" ), *CloudFile2Dir );
		}

		GLog->Logf( TEXT( "BuildDataGenerator: Enumerating Files From %s" ), *CloudFileDir );
		if( IFileManager::Get().DirectoryExists( *CloudFileDir ) )
		{
			const double StartEnumerate = FPlatformTime::Seconds();
			const int32 PreviousNumFiles = FoundFiles.Num();

			// Find all files
			TArray<FString> AllFiles;
			IFileManager::Get().FindFilesRecursive( AllFiles, *CloudFileDir, TEXT("*.file"), true, false );

			for( auto FileIt = AllFiles.CreateConstIterator(); FileIt; ++FileIt )
			{
				FString SourceFile = *FileIt;

				// Skip any for GUID we already found
				FGuid FoundGuid;
				FBuildPatchUtils::GetGUIDFromFilename( SourceFile, FoundGuid );
				if( FoundFiles.Contains( FoundGuid ) )
				{
					continue;
				}

				// Read the file
				FArchive* FileReader = IFileManager::Get().CreateFileReader( *SourceFile );
				if( FileReader != NULL )
				{
					// Read the header
					FChunkHeader Header;
					*FileReader << Header;

					// Check magic
					if( Header.IsValidMagic() && Header.HashType == FChunkHeader::HASH_SHA1 )
					{
						TArray< FString >& FileList = ExistingFileInventory.FindOrAdd( Header.SHAHash );
						FileList.Add( SourceFile );
						FoundFiles.Add( Header.Guid );

						const void* HashBuffer = &Header.SHAHash;
						DebugWriter->WriteValue( Header.Guid.ToString(), FString::FromBlob( static_cast<const uint8*>( HashBuffer ), sizeof( Header.SHAHash ) ) );
					}
					else
					{
						GLog->Logf( TEXT( "BuildDataGenerator: Failed magic/hashtype check on file [%d:%d] %s" ), Header.Magic, Header.HashType, *SourceFile );
					}

					FileReader->Close();
					delete FileReader;
				}
				else
				{
					GLog->Logf( TEXT( "BuildDataGenerator: Failed to read chunk %s" ), *SourceFile );
				}
			}

			const double EnumerateTime = FPlatformTime::Seconds() - StartEnumerate;
			const int32 NewNumFiles = FoundFiles.Num();
			GLog->Logf( TEXT( "BuildDataGenerator: Found %d extra old files in %.1f seconds" ), NewNumFiles - PreviousNumFiles, EnumerateTime );
		}
		else
		{
			GLog->Logf( TEXT( "BuildDataGenerator: Cloud directory does not exist: %s" ), *CloudFileDir );
		}

		DebugWriter->WriteObjectEnd();
		DebugWriter->Close();

		FArchive* FileOut = IFileManager::Get().CreateFileWriter( *( CloudFileDir + TEXT( "DebugFileList.txt" ) ) );
		if( FileOut != NULL )
		{
			FileOut->Serialize(TCHAR_TO_ANSI(*JSONOutput), JSONOutput.Len());
			FileOut->Close();
			delete FileOut;
		}

		ExistingFilesEnumerated = true;
	}

	// Do we have a file matching this data?
	if( ExistingFileInventory.Num() > 0 )
	{
		TArray< FString >* FileList = ExistingFileInventory.Find( InFileHash );
		if( FileList != NULL )
		{
			// We need to load each chunk in this list and compare data
			for( auto FileIt = FileList->CreateConstIterator(); FileIt && !bFoundMatchingFile ; ++FileIt)
			{
				FString CloudFilename = *FileIt;
				// Check the file date
				FDateTime ModifiedDate = IFileManager::Get().GetTimeStamp( *CloudFilename );
				if (ModifiedDate < DataThresholdTime)
				{
					// We don't want to reuse this file's Guid, as it's older than any existing files we want to consider
					continue;
				}
				// Compare the files
				FArchive* SourceFile = IFileManager::Get().CreateFileReader( *InSourceFile );
				FArchive* FoundFile = IFileManager::Get().CreateFileReader( *CloudFilename );
				if( SourceFile != NULL && FoundFile != NULL)
				{
					FChunkHeader FoundHeader;
					*FoundFile << FoundHeader;
					const int64 SourceFileSize = SourceFile->TotalSize();
					const int64 FoundFileSize = FoundFile->TotalSize();
					if( SourceFileSize == FoundHeader.DataSize )
					{
						// Currently only support stored raw!
						check( FoundHeader.StoredAs == FChunkHeader::STORED_RAW );
						// Makes no sense here for the sizes to not add up
						check( FoundFileSize == ( FoundHeader.DataSize + FoundHeader.HeaderSize ) );
						// Move FoundFile to start of file data
						FoundFile->Seek( FoundHeader.HeaderSize );
						// Compare
						bool bSameData = true;
						uint8* TempSourceBuffer = new uint8[ FileBufferSize ];
						uint8* TempFoundBuffer = new uint8[ FileBufferSize ];
						while ( bSameData && ( ( SourceFile->AtEnd() || FoundFile->AtEnd() ) == false ) )
						{
							const int64 SizeLeft = SourceFileSize - SourceFile->Tell();
							const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
							SourceFile->Serialize( TempSourceBuffer, ReadLen );
							FoundFile->Serialize( TempFoundBuffer, ReadLen );
							bSameData = FMemory::Memcmp( TempSourceBuffer, TempFoundBuffer, ReadLen ) == 0;
						}
						delete[] TempSourceBuffer;
						delete[] TempFoundBuffer;
						// Did we match?
						if( bSameData && SourceFile->AtEnd() && FoundFile->AtEnd() )
						{
							// Yes we did!
							bFoundMatchingFile = true;
							OutFileGuid = FoundHeader.Guid;
						}
					}
				}
				if( SourceFile != NULL )
				{
					SourceFile->Close();
					delete SourceFile;
				}
				if( FoundFile != NULL )
				{
					FoundFile->Close();
					delete FoundFile;
				}
			}
		}
	}

	return bFoundMatchingFile;
}

bool FBuildDataGenerator::SaveOutFileData(const FString& SourceFile, const FSHAHashData& FileHash, const FGuid& FileGuid)
{
	bool bAlreadySaved = false;
	bool bSuccess = false;
	IFileManager& FileManager = IFileManager::Get();

	const FString NewFilename = FBuildPatchUtils::GetFileNewFilename( EBuildPatchAppManifestVersion::GetLatestFileDataVersion(), FBuildPatchServicesModule::GetCloudDirectory(), FileGuid, FileHash );
	bAlreadySaved = FPlatformFileManager::Get().GetPlatformFile().FileExists(*NewFilename);

#if SAVE_OLD_FILEDATA_FILENAMES
	const FString OldFilename = FBuildPatchUtils::GetFileOldFilename( FBuildPatchServicesModule::GetCloudDirectory(), FileGuid );
	bAlreadySaved = bAlreadySaved && FPlatformFileManager::Get().GetPlatformFile().FileExists(*OldFilename);
#endif

	if( !bAlreadySaved )
	{
		FArchive* FileOut = FileManager.CreateFileWriter( *NewFilename );
		FArchive* FileIn = FileManager.CreateFileReader( *SourceFile );

		bSuccess = FileOut != NULL && FileIn != NULL;

#if SAVE_OLD_FILEDATA_FILENAMES
		FArchive* OldFileOut = FileManager.CreateFileWriter( *OldFilename );
		bSuccess = bSuccess && OldFileOut != NULL;
#endif

		if( bSuccess )
		{
			const int64 FileSize = FileIn->TotalSize();

			// LSwift: No support for too large files currently
			check( FileSize <= 0xFFFFFFFF );

			// Setup Header
			FChunkHeader Header;
			*FileOut << Header;
			Header.HeaderSize = FileOut->Tell();
			Header.DataSize = FileSize;
			Header.Guid = FileGuid;
			Header.HashType = FChunkHeader::HASH_SHA1;
			Header.SHAHash = FileHash;
			Header.StoredAs = FChunkHeader::STORED_RAW;

			// Write out file
			FileOut->Seek( 0 );
			*FileOut << Header;

#if SAVE_OLD_FILEDATA_FILENAMES
			*OldFileOut << Header;
#endif

			uint8* FileDataBuffer = new uint8[ FileBufferSize ];
			while( !FileIn->AtEnd() )
			{
				const int64 SizeLeft = FileSize - FileIn->Tell();
				const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
				FileIn->Serialize( FileDataBuffer, ReadLen );
				FileOut->Serialize( FileDataBuffer, ReadLen );

#if SAVE_OLD_FILEDATA_FILENAMES
				OldFileOut->Serialize( FileDataBuffer, ReadLen );
#endif

			}
			delete[] FileDataBuffer;
		}

		// Close files
		if( FileIn != NULL )
		{
			FileIn->Close();
			delete FileIn;
		}
		if( FileOut != NULL )
		{
			FileOut->Close();
			delete FileOut;
		}

#if SAVE_OLD_FILEDATA_FILENAMES
		if( OldFileOut != NULL )
		{
			OldFileOut->Close();
			delete OldFileOut;
		}
#endif

	}

	return bSuccess;
}

bool FBuildDataGenerator::CompareDataToChunk( const FString& ChunkFilePath, uint8* ChunkData, FGuid& ChunkGuid, bool& OutSourceChunkIsValid )
{
	bool bMatching = false;

	// Read the file
	TSharedRef< FBuildGenerationChunkCache::FChunkReader > ChunkReader = FBuildGenerationChunkCache::Get().GetChunkReader( ChunkFilePath );
	if( ChunkReader->IsValidChunk() )
	{
		ChunkGuid = ChunkReader->GetChunkGuid();
		// Default true
		bMatching = true;
		// Compare per small block (for early outing!)
		const uint32 CompareSize = 64;
		uint8* ReadBuffer;
		uint32 NumCompared = 0;
		while( bMatching && ChunkReader->BytesLeft() > 0 && NumCompared < FBuildPatchData::ChunkDataSize )
		{
			const uint32 ReadLen = FMath::Min< uint32 >( CompareSize, ChunkReader->BytesLeft() );
			ChunkReader->ReadNextBytes( &ReadBuffer, ReadLen );
			bMatching = FMemory::Memcmp( &ChunkData[ NumCompared ], ReadBuffer, ReadLen ) == 0;
			NumCompared += ReadLen;
		}
	}

	// Set chunk valid state after loading in case reading discovered bad or ignorable data
	OutSourceChunkIsValid = ChunkReader->IsValidChunk();

	return bMatching;
}

void FBuildDataGenerator::StripIgnoredFiles( TArray< FString >& AllFiles, const FString& DepotDirectory, const FString& IgnoreListFile )
{
	struct FRemoveMatchingStrings
	{ 
		const TSet<FString>& IgnoreList;
		FRemoveMatchingStrings( const TSet<FString>& InIgnoreList )
			: IgnoreList(InIgnoreList) {}

		bool operator()(const FString& RemovalCandidate) const
		{
			const bool bRemove = IgnoreList.Contains(RemovalCandidate);
			if (bRemove)
			{
				GLog->Logf(TEXT("    - %s"), *RemovalCandidate);
			}
			return bRemove;
		}
	};

	GLog->Logf(TEXT("Stripping ignorable files"));
	const int32 OriginalNumFiles = AllFiles.Num();
	FString IgnoreFileList = TEXT( "" );
	FFileHelper::LoadFileToString( IgnoreFileList, *IgnoreListFile );
	TArray< FString > IgnoreFiles;
	IgnoreFileList.ParseIntoArray( IgnoreFiles, TEXT( "\r\n" ), true );

	// Normalize all paths first
	for (FString& Filename : AllFiles)
	{
		FPaths::NormalizeFilename(Filename);
	}
	for (FString& Filename : IgnoreFiles)
	{
		int32 TabLocation = Filename.Find(TEXT("\t"));
		if (TabLocation != INDEX_NONE)
		{
			// Strip tab deliminated timestamp if it exists
			Filename = Filename.Left(TabLocation);
		}
		Filename = DepotDirectory / Filename;
		FPaths::NormalizeFilename(Filename);
	}

	// Convert ignore list to set
	TSet<FString> IgnoreSet(MoveTemp(IgnoreFiles));

	// Filter file list
	FRemoveMatchingStrings FileFilter(IgnoreSet);
	AllFiles.RemoveAll(FileFilter);

	const int32 NewNumFiles = AllFiles.Num();
	GLog->Logf( TEXT( "Stripped %d ignorable file(s)" ), ( OriginalNumFiles - NewNumFiles ) );
}

#endif //WITH_BUILDPATCHGENERATION

