// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#if	STATS

class FAsyncWriteWorker;


/**
* Magic numbers for stats streams, this is for the first version.
*/
namespace EStatMagicNoHeader
{
	enum Type
	{
		MAGIC = 0x7E1B83C1,
		MAGIC_SWAPPED = 0xC1831B7E,
		NO_VERSION = 0,
	};
}

/**
* Magic numbers for stats streams, this is for the second and later versions.
* Allows more advanced options.
*/
namespace EStatMagicWithHeader
{
	enum Type
	{
		MAGIC = 0x10293847,
		MAGIC_SWAPPED = 0x47382910,
		VERSION_2 = 2,
		VERSION_3 = 3,

		/**
		 *	Added support for compressing the stats data, now each frame is compressed.
		 */
		VERSION_4 = 4,
		HAS_COMPRESSED_DATA_VER = VERSION_4
	};
}

struct EStatsFileConstants
{
	enum
	{
		/**
		 *	Maximum size of the data that can be compressed.
		 *	Should be ok for almost all cases, except the boot stats.
		 */
		MAX_COMPRESSED_SIZE = 1024 * 1024,

		/** Header reserved for the compression internals. */
		DUMMY_HEADER_SIZE = 1024,

		/** Indicates the end of the compressed data. */
		END_OF_COMPRESSED_DATA = 0xE0F0DA4A,

		/** Indicates that the compression is disabled for the data. */
		NO_COMPRESSION = 0,
	};
};

/*-----------------------------------------------------------------------------
	FCompressedStatsData
-----------------------------------------------------------------------------*/

/** Helper struct used to operate on the compressed data. */
struct FCompressedStatsData
{
	/**
	 * Initialization constructor 
	 *	
	 * @param SrcData - uncompressed data if saving, compressed data if loading
	 * @param DestData - compressed data if saving, uncompressed data if loading
	 *
	 */
	FCompressedStatsData( TArray<uint8>& InSrcData, TArray<uint8>& InDestData )
		: SrcData( InSrcData )
		, DestData( InDestData )
		, bEndOfCompressedData( false )
	{}

	/**
	 * Writes a special data to mark the end of the compressed data.
	 */
	static void WriteEndOfCompressedData( FArchive& Writer )
	{
		int32 Marker = EStatsFileConstants::END_OF_COMPRESSED_DATA;
		check( Writer.IsSaving() );
		Writer << Marker << Marker;
	}

protected:
	/** Serialization operator. */
	friend FArchive& operator << (FArchive& Ar, FCompressedStatsData& Data)
	{
		if( Ar.IsSaving() )
		{
			Data.WriteCompressed( Ar );
		}
		else if( Ar.IsLoading() )
		{
			Data.ReadCompressed( Ar );
		}
		else
		{
			check( 0 );
		}
		return Ar;
	}

	/** Compress the data and writes to the archive. */
	void WriteCompressed( FArchive& Writer )
	{
		int32 UncompressedSize = SrcData.Num();
		if( UncompressedSize > EStatsFileConstants::MAX_COMPRESSED_SIZE - EStatsFileConstants::DUMMY_HEADER_SIZE )
		{
			int32 DisabledCompressionSize = EStatsFileConstants::NO_COMPRESSION;
			Writer << DisabledCompressionSize << UncompressedSize;
			Writer.Serialize( SrcData.GetData(), UncompressedSize );
		}
		else
		{
			DestData.Reserve( EStatsFileConstants::MAX_COMPRESSED_SIZE );
			int32 CompressedSize = DestData.GetAllocatedSize();

			const bool bResult = FCompression::CompressMemory( COMPRESS_ZLIB, DestData.GetData(), CompressedSize, SrcData.GetData(), UncompressedSize );
			check( bResult );
			Writer << CompressedSize << UncompressedSize;
			Writer.Serialize( DestData.GetData(), CompressedSize );
		}
	}

	/** Reads the data and decompresses it. */
	void ReadCompressed( FArchive& Reader )
	{
		int32 CompressedSize = 0;
		int32 UncompressedSize = 0;
		Reader << CompressedSize << UncompressedSize;

		if( CompressedSize == EStatsFileConstants::END_OF_COMPRESSED_DATA && UncompressedSize == EStatsFileConstants::END_OF_COMPRESSED_DATA )
		{
			bEndOfCompressedData = true;
		}
		// This chunk is not compressed.
		else if( CompressedSize == 0 )
		{
			DestData.Reset( UncompressedSize );
			DestData.AddUninitialized( UncompressedSize );
			Reader.Serialize( DestData.GetData(), UncompressedSize );
		}
		else
		{
			SrcData.Reset( CompressedSize );
			DestData.Reset( UncompressedSize );
			SrcData.AddUninitialized( CompressedSize );
			DestData.AddUninitialized( UncompressedSize );

			Reader.Serialize( SrcData.GetData(), CompressedSize );
			const bool bResult = FCompression::UncompressMemory( COMPRESS_ZLIB, DestData.GetData(), UncompressedSize, SrcData.GetData(), CompressedSize );
			check( bResult );
		}
	}

public:
	/**
	 * @return true if we reached the end of the compressed data.
	 */
	const bool HasReachedEndOfCompressedData() const
	{
		return bEndOfCompressedData;
	}
	
protected:
	/** Reference to the source data. */
	TArray<uint8>& SrcData;

	/** Reference to the destination data. */
	TArray<uint8>& DestData;

	/** Set to true if we reached the end of the compressed data. */
	bool bEndOfCompressedData;
};

/*-----------------------------------------------------------------------------
	Stats file writing functionality
-----------------------------------------------------------------------------*/

/** Header for a stats file. */
struct FStatsStreamHeader
{
	/** Default constructor. */
	FStatsStreamHeader()
		: Version( 0 )
		, FrameTableOffset( 0 )
		, FNameTableOffset( 0 )
		, NumFNames( 0 )
		, MetadataMessagesOffset( 0 )
		, NumMetadataMessages( 0 )
		, bRawStatsFile( false )
	{}

	/**
	*	Version number to detect version mismatches
	*	1 - Initial version for supporting raw ue4 stats files
	*/
	uint32	Version;

	/** Platform that this file was captured on. */
	FString	PlatformName;

	/** Offset in the file for the frame table. Serialized as TArray<int64>. */
	uint64	FrameTableOffset;

	/** Offset in the file for the FName table. Serialized with WriteFName/ReadFName. */
	uint64	FNameTableOffset;

	/** Number of the FNames. */
	uint64	NumFNames;

	/** Offset in the file for the metadata messages. Serialized with WriteMessage/ReadMessage. */
	uint64	MetadataMessagesOffset;

	/** Number of the metadata messages. */
	uint64	NumMetadataMessages;

	/** Whether this stats file uses raw data, required for thread view. */
	bool bRawStatsFile;

	/** Whether this stats file has been finalized, so we have the whole data and can load file a unordered access. */
	bool IsFinalized() const
	{
		return NumMetadataMessages > 0 && MetadataMessagesOffset > 0 && FrameTableOffset > 0;
	}

	/** Whether this stats file contains compressed data and needs the special serializer to read that data. */
	bool HasCompressedData() const
	{
		return Version >= EStatMagicWithHeader::HAS_COMPRESSED_DATA_VER;
	}

	/** Serialization operator. */
	friend FArchive& operator << (FArchive& Ar, FStatsStreamHeader& Header)
	{
		Ar << Header.Version;

		if( Ar.IsSaving() )
		{
			Header.PlatformName.SerializeAsANSICharArray( Ar, 255 );
		}
		else if( Ar.IsLoading() )
		{
			Ar << Header.PlatformName;
		}

		Ar << Header.FrameTableOffset;

		Ar << Header.FNameTableOffset
			<< Header.NumFNames;

		Ar << Header.MetadataMessagesOffset
			<< Header.NumMetadataMessages;

		Ar << Header.bRawStatsFile;

		return Ar;
	}
};

/**
 * Contains basic information about one frame of the stats.
 * This data is used to generate ultra-fast preview of the stats history without the requirement of reading the whole file.
 */
struct FStatsFrameInfo
{
	/** Empty constructor. */
	FStatsFrameInfo()
		: FrameFileOffset(0)
	{}

	/** Initialization constructor. */
	FStatsFrameInfo( int64 InFrameFileOffset, TMap<uint32, int64>& InThreadCycles )
		: FrameFileOffset(InFrameFileOffset)
		, ThreadCycles(InThreadCycles)
	{}

	/** Serialization operator. */
	friend FArchive& operator << (FArchive& Ar, FStatsFrameInfo& Data)
	{
		Ar << Data.ThreadCycles << Data.FrameFileOffset;
		return Ar;
	}

	/** Frame offset in the stats file. */
	int64 FrameFileOffset;

	/** Thread cycles for this frame. */
	TMap<uint32, int64> ThreadCycles;
};


struct CORE_API FStatsWriteFile : public TSharedFromThis<FStatsWriteFile, ESPMode::ThreadSafe>
{
	friend class FAsyncWriteWorker;

protected:
	/** Stats stream header. */
	FStatsStreamHeader Header;

	/** Set of names already sent **/
	TSet<int32> FNamesSent;

	/** Array of stats frames info already captured. */
	TArray<FStatsFrameInfo> FramesInfo;

	/** Data to write through the async task. **/
	TArray<uint8> OutData;

	/** Thread cycles for the last frame. */
	TMap<uint32, int64> ThreadCycles;

	/** Filename of the archive that we are writing to. */
	FString ArchiveFilename;

	/** Stats file archive. */
	FArchive* File;

	/** Async task used to offload saving the capture data. */
	FAsyncTask<FAsyncWriteWorker>* AsyncTask;

	/** Buffer to store the compressed data, used by the FAsyncWriteWorker. */
	TArray<uint8> CompressedData;

	/** NewFrame delegate handle  */
	FDelegateHandle NewFrameDelegateHandle;

public:
	/** Constructor. **/
	FStatsWriteFile();

	/** Virtual destructor. */
	virtual ~FStatsWriteFile()
	{}

	/** Starts writing the stats data into the specified file. */
	void Start( FString const& InFilename, bool bIsRawStatsFile );

	/** Stops writing the stats data. */
	void Stop();

	bool IsValid() const;

	const TArray<uint8>& GetOutData() const
	{
		return OutData;
	}

	void ResetData()
	{
		OutData.Reset();
	}

	/**
	*	Writes magic value, dummy header and initial metadata.
	*	Called from the stats thread.
	*/
	void WriteHeader( bool bIsRawStatsFile );

	/**
	 *	Grabs a frame from the local FStatsThreadState and adds it to the output.
	 *	Called from the stats thread, but the data is saved using the the FAsyncWriteWorker. 
	 */
	virtual void WriteFrame( int64 TargetFrame, bool bNeedFullMetadata = false );

protected:
	/**
	 *	Finalizes writing to the file.
	 *	Called from the stats thread.
	 */
	void Finalize();

	void NewFrame( int64 TargetFrame );

	/** Sends the data to the file via async task. */
	void SendTask();

	void AddNewFrameDelegate();
	void RemoveNewFrameDelegate();

	/**
	 *	Writes metadata messages into the stream. 
	 *	Called from the stats thread.
	 */
	void WriteMetadata( FArchive& Ar )
	{
		const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
		for( const auto& It : Stats.ShortNameToLongName )
		{
			const FStatMessage& StatMessage = It.Value;
			WriteMessage( Ar, StatMessage );
		}
	}

	/** Sends an FName, and the string it represents if we have not sent that string before. **/
	FORCEINLINE_STATS void WriteFName( FArchive& Ar, FStatNameAndInfo NameAndInfo )
	{
		FName RawName = NameAndInfo.GetRawName();
		bool bSendFName = !FNamesSent.Contains( RawName.GetComparisonIndex() );
		int32 Index = RawName.GetComparisonIndex();
		Ar << Index;
		int32 Number = NameAndInfo.GetRawNumber();
		if( bSendFName )
		{
			FNamesSent.Add( RawName.GetComparisonIndex() );
			Number |= EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift);
		}
		Ar << Number;
		if( bSendFName )
		{
			FString Name = RawName.ToString();
			Ar << Name;
		}
	}

	/** Write a stat message. **/
	FORCEINLINE_STATS void WriteMessage( FArchive& Ar, FStatMessage const& Item )
	{
		WriteFName( Ar, Item.NameAndInfo );
		switch( Item.NameAndInfo.GetField<EStatDataType>() )
		{
			case EStatDataType::ST_int64:
			{
				int64 Payload = Item.GetValue_int64();
				Ar << Payload;
			}
				break;
			case EStatDataType::ST_double:
			{
				double Payload = Item.GetValue_double();
				Ar << Payload;
			}
				break;
			case EStatDataType::ST_FName:
				WriteFName( Ar, FStatNameAndInfo( Item.GetValue_FName(), false ) );
				break;
		}
	}
};

/** Struct used to save unprocessed stats data as raw messages. */
struct FRawStatsWriteFile : public FStatsWriteFile
{
public:
	/** Virtual destructor. */
	virtual ~FRawStatsWriteFile()
	{}

protected:

	/**
	*	Grabs a frame from the local FStatsThreadState and adds it to the output.
	*	Called from the stats thread, but the data is saved using the the FAsyncWriteWorker.
	*/
	virtual void WriteFrame( int64 TargetFrame, bool bNeedFullMetadata = false ) override;

	/** Write a stat packed into the specified archive. */
	void WriteStatPacket( FArchive& Ar, /*const*/ FStatPacket& StatPacket )
	{
		Ar << StatPacket.Frame;
		Ar << StatPacket.ThreadId;
		int32 MyThreadType = (int32)StatPacket.ThreadType;
		Ar << MyThreadType;

		Ar << StatPacket.bBrokenCallstacks;
		// We must handle stat messages in a different way.
		int32 NumMessages = StatPacket.StatMessages.Num();
		Ar << NumMessages;
		for( int32 MessageIndex = 0; MessageIndex < NumMessages; ++MessageIndex )
		{
			WriteMessage( Ar, StatPacket.StatMessages[MessageIndex] );
		}
	}
};

/*-----------------------------------------------------------------------------
	Stats file reading functionality
-----------------------------------------------------------------------------*/

/**
* Class for maintaining state of receiving a stream of stat messages
*/
struct CORE_API FStatsReadStream
{
public:
	/** Stats stream header. */
	FStatsStreamHeader Header;

	/** FNames have a different index on each machine, so we translate via this map. **/
	TMap<int32, int32> FNamesIndexMap;

	/** Array of stats frame info. */
	TArray<FStatsFrameInfo> FramesInfo;

	/** Reads a stats stream header, returns true if the header is valid and we can continue reading. */
	bool ReadHeader( FArchive& Ar )
	{
		bool bStatWithHeader = false;

		uint32 Magic = 0;
		Ar << Magic;
		if( Magic == EStatMagicNoHeader::MAGIC )
		{

		}
		else if( Magic == EStatMagicNoHeader::MAGIC_SWAPPED )
		{
			Ar.SetByteSwapping( true );
		}
		else if( Magic == EStatMagicWithHeader::MAGIC )
		{
			bStatWithHeader = true;
		}
		else if( Magic == EStatMagicWithHeader::MAGIC_SWAPPED )
		{
			bStatWithHeader = true;
			Ar.SetByteSwapping( true );
		}
		else
		{
			return false;
		}

		// We detected a header for a stats file, read it.
		if( bStatWithHeader )
		{
			Ar << Header;
		}

		return true;
	}

	/** Reads a stat packed from the specified archive. */
	void ReadStatPacket( FArchive& Ar, FStatPacket& StatPacked, bool bHasFNameMap )
	{
		Ar << StatPacked.Frame;
		Ar << StatPacked.ThreadId;
		int32 MyThreadType = 0;
		Ar << MyThreadType;
		StatPacked.ThreadType = (EThreadType::Type)MyThreadType;

		Ar << StatPacked.bBrokenCallstacks;
		// We must handle stat messages in a different way.
		int32 NumMessages = 0;
		Ar << NumMessages;
		StatPacked.StatMessages.Reserve( NumMessages );
		for( int32 MessageIndex = 0; MessageIndex < NumMessages; ++MessageIndex )
		{
			new(StatPacked.StatMessages) FStatMessage( ReadMessage( Ar, bHasFNameMap ) );
		}
	}

	/** Read and translate or create an FName. **/
	FORCEINLINE_STATS FStatNameAndInfo ReadFName( FArchive& Ar, bool bHasFNameMap )
	{
		// If we read the whole FNames translation map, we don't want to add the FName again.
		// This is a bit tricky, even if we have the FName translation map, we still need to read the FString.
		// CAUTION!! This is considered to be thread safe in this case.
		int32 Index = 0;
		Ar << Index;
		int32 Number = 0;
		Ar << Number;
		FName TheFName;
		
		if( !bHasFNameMap )
		{
			if( Number & (EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift)) )
			{
				FString Name;
				Ar << Name;

				TheFName = FName( *Name );
				FNamesIndexMap.Add( Index, TheFName.GetComparisonIndex() );
				Number &= ~(EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift));
			}
			else
			{
				if( FNamesIndexMap.Contains( Index ) )
				{
					int32 MyIndex = FNamesIndexMap.FindChecked( Index );
					TheFName = FName( MyIndex, MyIndex, 0 );
				}
				else
				{
					TheFName = FName( TEXT( "Unknown FName" ) );
					Number = 0;
					UE_LOG( LogTemp, Warning, TEXT( "Missing FName Indexed: %d, %d" ), Index, Number );
				}
			}
		}
		else
		{
			if( Number & (EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift)) )
			{
				FString Name;
				Ar << Name;
				Number &= ~(EStatMetaFlags::SendingFName << (EStatMetaFlags::Shift + EStatAllFields::StartShift));
			}
			if( FNamesIndexMap.Contains( Index ) )
			{
				int32 MyIndex = FNamesIndexMap.FindChecked( Index );
				TheFName = FName( MyIndex, MyIndex, 0 );
			}
			else
			{
				TheFName = FName( TEXT( "Unknown FName" ) );
				Number = 0;
				UE_LOG( LogTemp, Warning, TEXT( "Missing FName Indexed: %d, %d" ), Index, Number );
			}
		}

		FStatNameAndInfo Result( TheFName, false );
		Result.SetNumberDirect( Number );
		return Result;
	}

	/** Read a stat message. **/
	FORCEINLINE_STATS FStatMessage ReadMessage( FArchive& Ar, bool bHasFNameMap = false )
	{
		FStatMessage Result( ReadFName( Ar, bHasFNameMap ) );
		Result.Clear();
		switch( Result.NameAndInfo.GetField<EStatDataType>() )
		{
			case EStatDataType::ST_int64:
			{
				int64 Payload = 0;
				Ar << Payload;
				Result.GetValue_int64() = Payload;
			}
				break;
			case EStatDataType::ST_double:
			{
				double Payload = 0;
				Ar << Payload;
				Result.GetValue_double() = Payload;
			}
				break;
			case EStatDataType::ST_FName:
				FStatNameAndInfo Payload( ReadFName( Ar, bHasFNameMap ) );
				Result.GetValue_FName() = Payload.GetRawName();
				break;
		}
		return Result;
	}

	/**
	 *	Reads stats frames info from the specified archive, only valid for finalized stats files.
	 *	Allows unordered file access and whole data mini-view.
	 */
	void ReadFramesOffsets( FArchive& Ar )
	{
		Ar.Seek( Header.FrameTableOffset );
		Ar << FramesInfo;
	}

	/**
	 *	Reads FNames and metadata messages from the specified archive, only valid for finalized stats files.
	 *	Allow unordered file access.
	 */
	void ReadFNamesAndMetadataMessages( FArchive& Ar, TArray<FStatMessage>& out_MetadataMessages )
	{
		// Read FNames.
		Ar.Seek( Header.FNameTableOffset );
		out_MetadataMessages.Reserve( Header.NumFNames );
		for( int32 Index = 0; Index < Header.NumFNames; Index++ )
		{
			ReadFName( Ar, false );
		}

		// Read metadata messages.
		Ar.Seek( Header.MetadataMessagesOffset );
		out_MetadataMessages.Reserve( Header.NumMetadataMessages );
		for( int32 Index = 0; Index < Header.NumMetadataMessages; Index++ )
		{
			new(out_MetadataMessages)FStatMessage( ReadMessage( Ar, false ) );
		}
	}
};

/*-----------------------------------------------------------------------------
	Commands functionality
-----------------------------------------------------------------------------*/

/** Implements 'Stat Start/StopFile' functionality. */
struct FCommandStatsFile
{
	/** Filename of the last saved stats file. */
	static FString LastFileSaved;

	/** First frame. */
	static int64 FirstFrame;

	/** Stats file that is currently being saved. */
	static TSharedPtr<FStatsWriteFile, ESPMode::ThreadSafe> CurrentStatsFile;

	/** Stat StartFile. */
	static void Start( const TCHAR* Cmd );

	/** Stat StartFileRaw. */
	static void StartRaw( const TCHAR* Cmd );

	/** Stat StopFile. */
	static void Stop();
};


#endif // STATS