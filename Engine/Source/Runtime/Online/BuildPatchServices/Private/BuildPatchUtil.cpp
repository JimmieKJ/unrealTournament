// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BuildPatchUtil.cpp: Implements miscellaneous utility functions.
=============================================================================*/

#include "BuildPatchServicesPrivatePCH.h"

/* FBuildPatchUtils implementation
*****************************************************************************/
FString FBuildPatchUtils::GetChunkNewFilename( const EBuildPatchAppManifestVersion::Type ManifestVersion, const FString& RootDirectory, const FGuid& ChunkGUID, const uint64& ChunkHash )
{
	check( ChunkGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("%s/%02d/%016llX_%s.chunk"), *EBuildPatchAppManifestVersion::GetChunkSubdir( ManifestVersion ),FCrc::MemCrc32( &ChunkGUID, sizeof( FGuid ) ) % 100, ChunkHash, *ChunkGUID.ToString() ) );
}

FString FBuildPatchUtils::GetFileNewFilename(const EBuildPatchAppManifestVersion::Type ManifestVersion, const FString& RootDirectory, const FGuid& FileGUID, const FSHAHashData& FileHash)
{
	check( FileGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("%s/%02d/%s_%s.file"), *EBuildPatchAppManifestVersion::GetFileSubdir( ManifestVersion ), FCrc::MemCrc32( &FileGUID, sizeof( FGuid ) ) % 100, *FileHash.ToString(), *FileGUID.ToString() ) );
}

FString FBuildPatchUtils::GetFileNewFilename(const EBuildPatchAppManifestVersion::Type ManifestVersion, const FString& RootDirectory, const FGuid& FileGUID, const uint64& FileHash)
{
	check( FileGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("%s/%02d/%016llX_%s.file"), *EBuildPatchAppManifestVersion::GetFileSubdir( ManifestVersion ), FCrc::MemCrc32( &FileGUID, sizeof( FGuid ) ) % 100, FileHash, *FileGUID.ToString() ) );
}

void FBuildPatchUtils::GetChunkDetailFromNewFilename( const FString& ChunkNewFilename, FGuid& ChunkGUID, uint64& ChunkHash )
{
	const FString ChunkFilename = FPaths::GetBaseFilename( ChunkNewFilename );
	FString GuidString;
	FString HashString;
	ChunkFilename.Split( TEXT( "_" ), &HashString, &GuidString );
	// Check that the strings are exactly as we expect otherwise this is not being used properly
	check( GuidString.Len() == 32 );
	check( HashString.Len() == 16 );
	ChunkHash = FCString::Strtoui64( *HashString, NULL, 16 );
	FGuid::Parse( GuidString, ChunkGUID );
}

void FBuildPatchUtils::GetFileDetailFromNewFilename(const FString& FileNewFilename, FGuid& FileGUID, FSHAHashData& FileHash)
{
	const FString FileFilename = FPaths::GetBaseFilename( FileNewFilename );
	FString GuidString;
	FString HashString;
	FileFilename.Split( TEXT( "_" ), &HashString, &GuidString );
	// Check that the strings are exactly as we expect otherwise this is not being used properly
	check( GuidString.Len() == 32 );
	check( HashString.Len() == 40 );
	HexToBytes( HashString, FileHash.Hash );
	FGuid::Parse( GuidString, FileGUID );
}

FString FBuildPatchUtils::GetChunkOldFilename( const FString& RootDirectory, const FGuid& ChunkGUID )
{
	check( ChunkGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("Chunks/%02d/%s.chunk"), FCrc::MemCrc_DEPRECATED( &ChunkGUID, sizeof( FGuid ) ) % 100, *ChunkGUID.ToString() ) );
}

FString FBuildPatchUtils::GetFileOldFilename( const FString& RootDirectory, const FGuid& FileGUID )
{
	check( FileGUID.IsValid() );
	return FPaths::Combine( *RootDirectory, *FString::Printf( TEXT("Files/%02d/%s.file"), FCrc::MemCrc_DEPRECATED( &FileGUID, sizeof( FGuid ) ) % 100, *FileGUID.ToString() ) );
}

FString FBuildPatchUtils::GetDataTypeOldFilename( const FBuildPatchData::Type DataType, const FString& RootDirectory, const FGuid& Guid )
{
	check( Guid.IsValid() );

	switch ( DataType )
	{
	case FBuildPatchData::ChunkData:
		return GetChunkOldFilename( RootDirectory, Guid );
	case FBuildPatchData::FileData:
		return GetFileOldFilename( RootDirectory, Guid );
	}

	// Error, didn't case type
	check( false );
	return TEXT( "" );
}

FString FBuildPatchUtils::GetDataFilename(const FBuildPatchAppManifestRef& Manifest, const FString& RootDirectory, const FGuid& DataGUID)
{
	const FBuildPatchData::Type DataType = Manifest->IsFileDataManifest() ? FBuildPatchData::FileData : FBuildPatchData::ChunkData;
	if (Manifest->GetManifestVersion() < EBuildPatchAppManifestVersion::DataFileRenames)
	{
		return FBuildPatchUtils::GetDataTypeOldFilename(DataType, RootDirectory, DataGUID);
	}
	else if (!Manifest->IsFileDataManifest())
	{
		uint64 ChunkHash;
		const bool bFound = Manifest->GetChunkHash(DataGUID, ChunkHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetChunkNewFilename(Manifest->GetManifestVersion(), RootDirectory, DataGUID, ChunkHash);
	}
	else if (Manifest->GetManifestVersion() <= EBuildPatchAppManifestVersion::StoredAsCompressedUClass)
	{
		FSHAHashData FileHash;
		const bool bFound = Manifest->GetFileHash(DataGUID, FileHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetFileNewFilename(Manifest->GetManifestVersion(), RootDirectory, DataGUID, FileHash);
	}
	else
	{
		uint64 FileHash;
		const bool bFound = Manifest->GetFilePartHash(DataGUID, FileHash);
		// Should be impossible to not exist
		check(bFound);
		return FBuildPatchUtils::GetFileNewFilename(Manifest->GetManifestVersion(), RootDirectory, DataGUID, FileHash);
	}
	return TEXT("");
}



bool FBuildPatchUtils::GetGUIDFromFilename( const FString& DataFilename, FGuid& DataGUID )
{
	const FString DataBaseFilename = FPaths::GetBaseFilename( DataFilename );
	FString GuidString;
	if( DataBaseFilename.Contains( TEXT( "_" ) ) )
	{
		DataBaseFilename.Split( TEXT( "_" ), NULL, &GuidString );
	}
	else
	{
		GuidString = DataBaseFilename;
	}
	check( GuidString.Len() == 32 );
	return FGuid::Parse( GuidString, DataGUID );
}

uint8 FBuildPatchUtils::VerifyFile(const FString& FileToVerify, const FSHAHashData& Hash1, const FSHAHashData& Hash2)
{
	FBuildPatchFloatDelegate NoProgressDelegate;
	FBuildPatchBoolRetDelegate NoPauseDelegate;
	double NoDouble;
	return VerifyFile(FileToVerify, Hash1, Hash2, NoProgressDelegate, NoPauseDelegate, NoDouble);
}

uint8 FBuildPatchUtils::VerifyFile(const FString& FileToVerify, const FSHAHashData& Hash1, const FSHAHashData& Hash2, FBuildPatchFloatDelegate ProgressDelegate, FBuildPatchBoolRetDelegate ShouldPauseDelegate, double& TimeSpentPaused)
{
	uint8 ReturnValue = 0;
	FArchive* FileReader = IFileManager::Get().CreateFileReader(*FileToVerify);
	ProgressDelegate.ExecuteIfBound(0.0f);
	if (FileReader != NULL)
	{
		FSHA1 HashState;
		FSHAHashData HashValue;
		const int64 FileSize = FileReader->TotalSize();
		uint8* FileReadBuffer = new uint8[FileBufferSize];
		while (!FileReader->AtEnd() && !FBuildPatchInstallError::HasFatalError())
		{
			// Pause if necessary
			const double PrePauseTime = FPlatformTime::Seconds();
			double PostPauseTime = PrePauseTime;
			bool bShouldPause = ShouldPauseDelegate.IsBound() && ShouldPauseDelegate.Execute();
			while (bShouldPause && !FBuildPatchInstallError::HasFatalError())
			{
				FPlatformProcess::Sleep(0.1f);
				bShouldPause = ShouldPauseDelegate.Execute();
				PostPauseTime = FPlatformTime::Seconds();
			}
			// Count up pause time
			TimeSpentPaused += PostPauseTime - PrePauseTime;
			// Read file and update hash state
			const int64 SizeLeft = FileSize - FileReader->Tell();
			const uint32 ReadLen = FMath::Min< int64 >(FileBufferSize, SizeLeft);
			FileReader->Serialize(FileReadBuffer, ReadLen);
			HashState.Update(FileReadBuffer, ReadLen);
			const double FileSizeTemp = FileSize;
			const float Progress = 1.0f - ((SizeLeft - ReadLen) / FileSizeTemp);
			ProgressDelegate.ExecuteIfBound(Progress);
		}
		delete[] FileReadBuffer;
		HashState.Final();
		HashState.GetHash(HashValue.Hash);
		ReturnValue = (HashValue == Hash1) ? 1 : (HashValue == Hash2) ? 2 : 0;
		if (ReturnValue == 0)
		{
			GLog->Logf(TEXT("BuildDataGenerator: Verify failed on %s"), *FPaths::GetCleanFilename(FileToVerify));
		}
		FileReader->Close();
		delete FileReader;
	}
	else
	{
		GLog->Logf(TEXT("BuildDataGenerator: ERROR VerifyFile cannot open %s"), *FileToVerify);
	}
	ProgressDelegate.ExecuteIfBound(1.0f);
	return ReturnValue;
}

bool FBuildPatchUtils::VerifyChunkFile( const TArray< uint8 >& ChunkFileArray, bool bQuickCheck )
{
	FMemoryReader ChunkArrayReader( ChunkFileArray );
	return VerifyChunkFile( ChunkArrayReader, bQuickCheck );
}

bool FBuildPatchUtils::VerifyChunkFile( FArchive& ChunkFileData, bool bQuickCheck )
{
	const int64 FileSize = ChunkFileData.TotalSize();
	bool bSuccess = ChunkFileData.IsLoading();
	if ( !bSuccess )
	{
		GLog->Logf( TEXT( "BuildPatchServices: ERROR: VerifyChunkFile expected readonly archive" ) );
	}
	else
	{
		// Read the header
		FChunkHeader Header;
		ChunkFileData << Header;
		// Check header magic
		if ( !Header.IsValidMagic() )
		{
			bSuccess = false;
			GLog->Logf( TEXT( "BuildPatchServices: ERROR: VerifyChunkFile corrupt header" ) );
		}
		// Check Header and data size
		if ( bSuccess && ( Header.HeaderSize + Header.DataSize ) != FileSize )
		{
			bSuccess = false;
			GLog->Logf( TEXT( "BuildPatchServices: ERROR: VerifyChunkFile header info does not match file size" ) );
		}
		if( bSuccess && !bQuickCheck )
		{
			// Hashes for checking data
			FSHA1 SHAHasher;
			FSHAHashData SHAHash;
			uint64 CycPoly64Hash = 0;
			// Load the data to check
			uint8* FileReadBuffer = new uint8[ FileBufferSize ];
			int64 DataOffset = 0;
			switch ( Header.StoredAs )
			{
			case FChunkHeader::STORED_RAW:
				while( !ChunkFileData.AtEnd() )
				{
					const int64 SizeLeft = FileSize - ChunkFileData.Tell();
					const uint32 ReadLen = FMath::Min< int64 >( FileBufferSize, SizeLeft );
					ChunkFileData.Serialize( FileReadBuffer, ReadLen );
					switch ( Header.HashType )
					{
					case FChunkHeader::HASH_ROLLING:
						CycPoly64Hash = FCycPoly64Hash::GetHashForDataSet(FileReadBuffer, ReadLen, CycPoly64Hash);
						break;
					case  FChunkHeader::HASH_SHA1:
						SHAHasher.Update( FileReadBuffer, ReadLen );
						break;
					default:
						check( false ); // @TODO LSwift: Implement other storage methods!
						bSuccess = false;
						break;
					}
					DataOffset += ReadLen;
				}
				if( bSuccess )
				{
					switch ( Header.HashType )
					{
					case FChunkHeader::HASH_ROLLING:
						bSuccess = Header.RollingHash == CycPoly64Hash;
						break;
					case  FChunkHeader::HASH_SHA1:
						SHAHasher.Final();
						SHAHasher.GetHash( SHAHash.Hash );
						bSuccess = SHAHash == Header.SHAHash;
						break;
					}
					if (!bSuccess)
					{
						GLog->Logf(TEXT("BuildPatchServices: ERROR: VerifyChunkFile file hashcheck failed"));
					}
				}
				break;
			default:
				GLog->Logf( TEXT( "BuildPatchServices: ERROR: VerifyChunkFile failed, unknown storage type" ) );
				bSuccess = false;
				break;
			}
			delete[] FileReadBuffer;
		}
	}

	return bSuccess;
}

bool FBuildPatchUtils::UncompressChunkFile( TArray< uint8 >& ChunkFileArray )
{
	FMemoryReader ChunkArrayReader( ChunkFileArray );
	// Read the header
	FChunkHeader Header;
	ChunkArrayReader << Header;
	// Check header
	const bool bValidHeader = Header.IsValidMagic();
	const bool bSupportedFormat = Header.HashType == FChunkHeader::HASH_ROLLING && !( Header.StoredAs & FChunkHeader::STORED_ENCRYPTED );
	if( bValidHeader && bSupportedFormat )
	{
		bool bSuccess = true;
		// Uncompress if we need to
		if( Header.StoredAs == FChunkHeader::STORED_COMPRESSED )
		{
			// Load the compressed chunk data
			TArray< uint8 > CompressedData;
			TArray< uint8 > UncompressedData;
			CompressedData.Empty( Header.DataSize );
			CompressedData.AddUninitialized( Header.DataSize );
			UncompressedData.Empty( FBuildPatchData::ChunkDataSize );
			UncompressedData.AddUninitialized( FBuildPatchData::ChunkDataSize );
			ChunkArrayReader.Serialize( CompressedData.GetData(), Header.DataSize );
			ChunkArrayReader.Close();
			// Uncompress
			bSuccess = FCompression::UncompressMemory(
				static_cast< ECompressionFlags >( COMPRESS_ZLIB | COMPRESS_BiasMemory ),
				UncompressedData.GetData(),
				UncompressedData.Num(),
				CompressedData.GetData(),
				CompressedData.Num() );
			// If successful, write back over the original array
			if( bSuccess )
			{
				ChunkFileArray.Empty();
				FMemoryWriter ChunkArrayWriter( ChunkFileArray );
				Header.StoredAs = FChunkHeader::STORED_RAW;
				Header.DataSize = FBuildPatchData::ChunkDataSize;
				ChunkArrayWriter << Header;
				ChunkArrayWriter.Serialize( UncompressedData.GetData(), UncompressedData.Num() );
				ChunkArrayWriter.Close();
			}
		}
		return bSuccess;
	}
	return false;
}

bool FBuildPatchUtils::UncompressFileDataFile(TArray< uint8 >& DataFileArray, FChunkHeader* OutHeader/* = nullptr*/)
{
	FChunkHeader Header;
	if (OutHeader == nullptr)
	{
		OutHeader = &Header;
	}
	FMemoryReader FileArrayReader(DataFileArray);
	// Read the header
	FileArrayReader << *OutHeader;
	// Check header
	const bool bValidHeader = OutHeader->IsValidMagic();
	const bool bSupportedFormat = !(OutHeader->StoredAs & FChunkHeader::STORED_ENCRYPTED);
	if (bValidHeader && bSupportedFormat)
	{
		bool bSuccess = true;
		// Uncompress if we need to
		if (OutHeader->StoredAs == FChunkHeader::STORED_COMPRESSED)
		{
			// Load the compressed data
			const int32 CompressedSize = DataFileArray.Num() - OutHeader->HeaderSize;
			TArray< uint8 > CompressedData;
			TArray< uint8 > UncompressedData;
			CompressedData.Empty(CompressedSize);
			CompressedData.AddUninitialized(CompressedSize);
			UncompressedData.Empty(OutHeader->DataSize);
			UncompressedData.AddUninitialized(OutHeader->DataSize);
			FileArrayReader.Serialize(CompressedData.GetData(), CompressedSize);
			FileArrayReader.Close();
			// Uncompress
			bSuccess = FCompression::UncompressMemory(
				static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
				UncompressedData.GetData(),
				UncompressedData.Num(),
				CompressedData.GetData(),
				CompressedData.Num());
			// If successful, write back over the original array
			if (bSuccess)
			{
				DataFileArray.Empty();
				FMemoryWriter FileArrayWriter(DataFileArray);
				OutHeader->StoredAs = FChunkHeader::STORED_RAW;
				FileArrayWriter << *OutHeader;
				FileArrayWriter.Serialize(UncompressedData.GetData(), UncompressedData.Num());
				FileArrayWriter.Close();
			}
		}
		return bSuccess;
	}
	return false;
}

