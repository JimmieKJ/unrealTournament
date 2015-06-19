// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealFrontendPrivatePCH.h"
#include "StatsConvertCommand.h"


void FStatsConvertCommand::WriteString( FArchive& Writer, const ANSICHAR* Format, ... )
{
	ANSICHAR Array[1024];
	va_list ArgPtr;
	va_start(ArgPtr,Format);
	// Build the string
	int32 Result = FCStringAnsi::GetVarArgs(Array,ARRAY_COUNT(Array),ARRAY_COUNT(Array)-1,Format,ArgPtr);
	// Now write that to the file
	Writer.Serialize((void*)Array,Result);
}


void FStatsConvertCommand::InternalRun()
{
#if	STATS
	// get the target file
	FString TargetFile;
	FParse::Value(FCommandLine::Get(), TEXT("-INFILE="), TargetFile);
	FString OutFile;
	FParse::Value(FCommandLine::Get(), TEXT("-OUTFILE="), OutFile);
	FString StatListString;
	FParse::Value(FCommandLine::Get(), TEXT("-STATLIST="), StatListString);

	// get the list of stats
	TArray<FString> StatArrayString;
	if (StatListString.ParseIntoArray(StatArrayString, TEXT("+"), true) == 0)
	{
		StatArrayString.Add(TEXT("STAT_FrameTime"));
	}

	// convert to FNames for faster compare
	for( const FString& It : StatArrayString )
	{
		StatList.Add(*It);
	}

	// open a csv file for write
	TAutoPtr<FArchive> FileWriter( IFileManager::Get().CreateFileWriter( *OutFile ) );
	if (!FileWriter)
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open output file: %s" ), *OutFile );
		return;
	}

	// @TODO yrx 2014-03-24 move to function
	// attempt to read the data and convert to csv
	const int64 Size = IFileManager::Get().FileSize( *TargetFile );
	if( Size < 4 )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open input file: %s" ), *TargetFile );
		return;
	}

	TAutoPtr<FArchive> FileReader( IFileManager::Get().CreateFileReader( *TargetFile ) );
	if( !FileReader )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open input file: %s" ), *TargetFile );
		return;
	}

	if( !Stream.ReadHeader( *FileReader ) )
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open input file, bad magic: %s" ), *TargetFile );
		return;
	}

	// This is not supported yet.
	if (Stream.Header.bRawStatsFile)
	{
		UE_LOG( LogStats, Error, TEXT( "Could not open input file, not supported type (raw): %s" ), *TargetFile );
		return;
	}

	const bool bIsFinalized = Stream.Header.IsFinalized();
	if( bIsFinalized )
	{
		// Read metadata.
		TArray<FStatMessage> MetadataMessages;
		Stream.ReadFNamesAndMetadataMessages( *FileReader, MetadataMessages );
		ThreadState.ProcessMetaDataOnly( MetadataMessages );

		// Read frames offsets.
		Stream.ReadFramesOffsets( *FileReader );
		FileReader->Seek( Stream.FramesInfo[0].FrameFileOffset );
	}

	if( Stream.Header.HasCompressedData() )
	{
		UE_CLOG( !bIsFinalized, LogStats, Fatal, TEXT( "Compressed stats file has to be finalized" ) );
	}

	ReadAndConvertStatMessages( *FileReader, *FileWriter );

#endif // STATS
}


void FStatsConvertCommand::ReadAndConvertStatMessages( FArchive& Reader, FArchive& Writer )
{
	// output the csv header
	WriteString( Writer, "Frame,Name,Value\r\n" );

	uint64 ReadMessages = 0;
	TArray<FStatMessage> Messages;

	// Buffer used to store the compressed and decompressed data.
	TArray<uint8> SrcData;
	TArray<uint8> DestData;

	const bool bHasCompressedData = Stream.Header.HasCompressedData();
	const bool bIsFinalized = Stream.Header.IsFinalized();
	float DataLoadingProgress = 0.0f;

	if( bHasCompressedData )
	{
		while( Reader.Tell() < Reader.TotalSize() )
		{
			// Read the compressed data.
			FCompressedStatsData UncompressedData( SrcData, DestData );
			Reader << UncompressedData;
			if( UncompressedData.HasReachedEndOfCompressedData() )
			{
				return;
			}

			FMemoryReader MemoryReader( DestData, true );

			while( MemoryReader.Tell() < MemoryReader.TotalSize() )
			{
				// read the message
				FStatMessage Message( Stream.ReadMessage( MemoryReader, bIsFinalized ) );
				ReadMessages++;
				if( ReadMessages % 32768 == 0 )
				{
					UE_LOG( LogStats, Log, TEXT( "StatsConvertCommand progress: %.1f%%" ), DataLoadingProgress );
				}

				if( Message.NameAndInfo.GetShortName() != TEXT( "Unknown FName" ) )
				{
					if( Message.NameAndInfo.GetField<EStatOperation>() == EStatOperation::AdvanceFrameEventGameThread && ReadMessages > 2 )
					{
						new (Messages) FStatMessage( Message );
						ThreadState.AddMessages( Messages );
						Messages.Reset();

						CollectAndWriteStatsValues( Writer );
						DataLoadingProgress = (double)Reader.Tell() / (double)Reader.TotalSize() * 100.0f;
					}

					new (Messages) FStatMessage( Message );
				}
				else
				{
					break;
				}
			}
		}
	}
	else
	{
		while( Reader.Tell() < Reader.TotalSize() )
		{
			// read the message
			FStatMessage Message( Stream.ReadMessage( Reader, bIsFinalized ) );
			ReadMessages++;
			if( ReadMessages % 32768 == 0 )
			{
				UE_LOG( LogStats, Log, TEXT( "StatsConvertCommand progress: %.1f%%" ), DataLoadingProgress );
			}

			if( Message.NameAndInfo.GetShortName() != TEXT( "Unknown FName" ) )
			{
				if( Message.NameAndInfo.GetField<EStatOperation>() == EStatOperation::SpecialMessageMarker )
				{
					// Simply break the loop.
					// The profiler supports more advanced handling of this message.
					return;
				}
				else if( Message.NameAndInfo.GetField<EStatOperation>() == EStatOperation::AdvanceFrameEventGameThread && ReadMessages > 2 )
				{
					new (Messages) FStatMessage( Message );
					ThreadState.AddMessages( Messages );
					Messages.Reset();

					CollectAndWriteStatsValues( Writer );
					DataLoadingProgress = (double)Reader.Tell() / (double)Reader.TotalSize() * 100.0f;
				}

				new (Messages) FStatMessage( Message );
			}
			else
			{
				break;
			}
		}
	}
}


void FStatsConvertCommand::CollectAndWriteStatsValues( FArchive& Writer )
{
	// get the thread stats
	TArray<FStatMessage> Stats;
	ThreadState.GetInclusiveAggregateStackStats(ThreadState.CurrentGameFrame, Stats);
	for (int32 Index = 0; Index < Stats.Num(); ++Index)
	{
		FStatMessage const& Meta = Stats[Index];
		//UE_LOG(LogTemp, Display, TEXT("Stat: %s"), *Meta.NameAndInfo.GetShortName().ToString());

		for (int32 Jndex = 0; Jndex < StatList.Num(); ++Jndex)
		{
			if (Meta.NameAndInfo.GetShortName() == StatList[Jndex])
			{
				double StatValue = 0.0f;
				if (Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
				{
					StatValue = FPlatformTime::ToMilliseconds( FromPackedCallCountDuration_Duration(Meta.GetValue_int64()) );
				}
				else if (Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
				{
					StatValue = FPlatformTime::ToMilliseconds( Meta.GetValue_int64() );
				}
				else
				{
					StatValue = Meta.GetValue_int64();
				}

				// write out to the csv file
				WriteString( Writer, "%d,%S,%f\r\n", ThreadState.CurrentGameFrame, *StatList[Jndex].ToString(), StatValue );
			}
		}
	}
}
