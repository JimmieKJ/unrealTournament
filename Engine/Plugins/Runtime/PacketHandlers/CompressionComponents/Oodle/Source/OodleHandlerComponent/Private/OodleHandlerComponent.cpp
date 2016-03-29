// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OodleHandlerComponentPCH.h"

#include "OodleTrainerCommandlet.h"


DEFINE_LOG_CATEGORY(OodleHandlerComponentLog);


// @todo #JohnB: Deprecate the 'training' mode name from this handler eventually, as it is misnamed/confusing

// @todo #JohnB: You need context-sensitive data capturing; e.g. you do NOT want to be capturing voice channel data


// @todo #JohnB: Get rid of 'Mode' variable, and enable 'Capturing' and 'Release' mode separately (so they can operate at same time);
//					rename 'Release' mode too, to something a bit more descriptive.


// @todo #JohnB: You're not taking into account, the overhead of sending 'DecompressedLength', in the stats


#if HAS_OODLE_SDK
#define OODLE_INI_SECTION TEXT("OodleHandlerComponent")


// @todo #JohnB: Find a better solution than this, for handling the maximum packet size. This is far higher than it needs to be.
// Reduced from 65535 to 16383, to minimize Oodle packet overhead from 3 bytes to 2 bytes
#define MAX_OODLE_PACKET_SIZE 16383


#if STATS

#if OODLE_STAT_NET
DEFINE_STAT(STAT_Net_Oodle_OutRaw);
DEFINE_STAT(STAT_Net_Oodle_OutCompressed);
DEFINE_STAT(STAT_Net_Oodle_InRaw);
DEFINE_STAT(STAT_Net_Oodle_InCompressed);
#endif

DEFINE_STAT(STAT_Oodle_OutRaw);
DEFINE_STAT(STAT_Oodle_OutCompressed);
DEFINE_STAT(STAT_Oodle_OutSavings);
DEFINE_STAT(STAT_Oodle_OutTotalSavings);
DEFINE_STAT(STAT_Oodle_InRaw);
DEFINE_STAT(STAT_Oodle_InCompressed);
DEFINE_STAT(STAT_Oodle_InSavings);
DEFINE_STAT(STAT_Oodle_InTotalSavings);
DEFINE_STAT(STAT_Oodle_PacketOverhead);

#if !UE_BUILD_SHIPPING
DEFINE_STAT(STAT_Oodle_InDecompressTime);
DEFINE_STAT(STAT_Oodle_OutCompressTime);
#endif

DEFINE_STAT(STAT_Oodle_DictionaryCount);
DEFINE_STAT(STAT_Oodle_DictionaryBytes);
DEFINE_STAT(STAT_Oodle_SharedBytes);
DEFINE_STAT(STAT_Oodle_StateBytes);
#endif


FString GOodleSaveDir = TEXT("");
FString GOodleContentDir = TEXT("");


typedef TMap<FString, TSharedPtr<FOodleDictionary>> FDictionaryMap;

/** Persistent map of loaded dictionaries */
static FDictionaryMap DictionaryMap;


/** Whether or not Oodle is presently force-enabled */
static bool bOodleForceEnable = false;


#if STATS
/** The global net stats tracker for Oodle */
static FOodleNetStats GOodleNetStats;


/**
 * FOodleNetStats
 */

void FOodleNetStats::UpdateStats(float DeltaTime)
{
	// Input
	const uint32 InRaw = FMath::TruncToInt(InDecompressedLength / DeltaTime);
	const uint32 InCompressed = FMath::TruncToInt(InCompressedLength / DeltaTime);

#if OODLE_STAT_NET
	SET_DWORD_STAT(STAT_Net_Oodle_InRaw, InRaw);
	SET_DWORD_STAT(STAT_Net_Oodle_InCompressed, InCompressed);
#endif

	SET_DWORD_STAT(STAT_Oodle_InRaw, InRaw);
	SET_DWORD_STAT(STAT_Oodle_InCompressed, InCompressed);

	double InSavings = InCompressedLength > 0 ? ((1.0 - ((double)InCompressedLength/(double)InDecompressedLength)) * 100.0) : 0.0;

	SET_FLOAT_STAT(STAT_Oodle_InSavings, InSavings);


	// Output
	uint32 OutRaw = FMath::TruncToInt(OutUncompressedLength / DeltaTime);
	uint32 OutCompressed = FMath::TruncToInt(OutCompressedLength / DeltaTime);

#if OODLE_STAT_NET
	SET_DWORD_STAT(STAT_Net_Oodle_OutRaw, OutRaw);
	SET_DWORD_STAT(STAT_Net_Oodle_OutCompressed, OutCompressed);
#endif

	SET_DWORD_STAT(STAT_Oodle_OutRaw, OutRaw);
	SET_DWORD_STAT(STAT_Oodle_OutCompressed, OutCompressed);

	double OutSavings = OutCompressedLength > 0 ? ((1.0 - ((double)OutCompressedLength/(double)OutUncompressedLength)) * 100.0) : 0.0;

	SET_FLOAT_STAT(STAT_Oodle_OutSavings, OutSavings);


	// Crude process-lifetime accumulation of all stat savings
	if (TotalInCompressedLength > 0)
	{
		SET_FLOAT_STAT(STAT_Oodle_InTotalSavings, (1.0 - ((double)TotalInCompressedLength/(double)TotalInDecompressedLength)) * 100.0);
	}

	if (TotalOutCompressedLength > 0)
	{
		SET_FLOAT_STAT(STAT_Oodle_OutTotalSavings,
						(1.0 - ((double)TotalOutCompressedLength/(double)TotalOutUncompressedLength)) * 100.0);
	}


	// Reset stats accumulated since last update
	InCompressedLength = 0;
	InDecompressedLength = 0;
	OutCompressedLength = 0;
	OutUncompressedLength = 0;
}
#endif


/**
 * FOodleDictionary
 */

FOodleDictionary::FOodleDictionary(uint32 InHashTableSize, uint8* InDictionaryData, uint32 InDictionarySize,
									OodleNetwork1_Shared* InSharedDictionary, uint32 InSharedDictionarySize,
									OodleNetwork1UDP_State* InInitialCompressorState, uint32 InCompressorStateSize)
	: HashTableSize(InHashTableSize)
	, DictionaryData(InDictionaryData)
	, DictionarySize(InDictionarySize)
	, SharedDictionary(InSharedDictionary)
	, SharedDictionarySize(InSharedDictionarySize)
	, CompressorState(InInitialCompressorState)
	, CompressorStateSize(InCompressorStateSize)
{
#if STATS
	INC_DWORD_STAT(STAT_Oodle_DictionaryCount);
	INC_MEMORY_STAT_BY(STAT_Oodle_DictionaryBytes, DictionarySize);
	INC_MEMORY_STAT_BY(STAT_Oodle_SharedBytes, SharedDictionarySize);
	INC_MEMORY_STAT_BY(STAT_Oodle_StateBytes, CompressorStateSize);
#endif
}

FOodleDictionary::~FOodleDictionary()
{
#if STATS
	DEC_DWORD_STAT(STAT_Oodle_DictionaryCount);
	DEC_MEMORY_STAT_BY(STAT_Oodle_DictionaryBytes, DictionarySize);
	DEC_MEMORY_STAT_BY(STAT_Oodle_SharedBytes, SharedDictionarySize);
	DEC_MEMORY_STAT_BY(STAT_Oodle_StateBytes, CompressorStateSize);
#endif

	if (DictionaryData != nullptr)
	{
		delete [] DictionaryData;
	}

	if (SharedDictionary != nullptr)
	{
		FMemory::Free(SharedDictionary);
	}

	if (CompressorState != nullptr)
	{
		FMemory::Free(CompressorState);
	}
}


/**
 * OodleHandlerComponent
 */

OodleHandlerComponent::OodleHandlerComponent()
	: bEnableOodle(false)
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	, InPacketLog(nullptr)
	, OutPacketLog(nullptr)
	, bUseDictionaryIfPresent(false)
#endif
	, Mode(EOodleHandlerMode::Release)
	, ServerDictionary()
	, ClientDictionary()
{
	SetActive(true);
}

OodleHandlerComponent::~OodleHandlerComponent()
{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	if (OutPacketLog != nullptr)
	{
		OutPacketLog->Close();
		OutPacketLog->DeleteInnerArchive();

		delete OutPacketLog;

		OutPacketLog = nullptr;
	}

	if (InPacketLog != nullptr)
	{
		InPacketLog->Close();
		InPacketLog->DeleteInnerArchive();

		delete InPacketLog;

		InPacketLog = nullptr;
	}
#endif


	FreeDictionary(ServerDictionary);
	FreeDictionary(ClientDictionary);


	// @todo #JohnB: Deprecate all of this code sooner rather than later - this is broken, as there will be multiple instances
#if UE4_OODLE_VER < 200
	Oodle_Shutdown_NoThreads();
#endif
}

void OodleHandlerComponent::InitFirstRunConfig()
{
	// Check that the OodleHandlerComponent section exists, and if not, init with defaults
	if (!GConfig->DoesSectionExist(OODLE_INI_SECTION, GEngineIni))
	{
		GConfig->SetBool(OODLE_INI_SECTION, TEXT("bEnableOodle"), true, GEngineIni);

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		GConfig->SetBool(OODLE_INI_SECTION, TEXT("bUseDictionaryIfPresent"), false, GEngineIni);
		GConfig->SetString(OODLE_INI_SECTION, TEXT("PacketLogFile"), TEXT("PacketDump"), GEngineIni);
#endif

		GConfig->SetString(OODLE_INI_SECTION, TEXT("Mode"), TEXT("Capturing"), GEngineIni);

		GConfig->SetString(OODLE_INI_SECTION, TEXT("ServerDictionary"), TEXT(""), GEngineIni);
		GConfig->SetString(OODLE_INI_SECTION, TEXT("ClientDictionary"), TEXT(""), GEngineIni);

		GConfig->Flush(false);
	}
}

void OodleHandlerComponent::Initialize()
{
	InitFirstRunConfig();

	// Class config variables
	GConfig->GetBool(OODLE_INI_SECTION, TEXT("bEnableOodle"), bEnableOodle, GEngineIni);

	if (!bEnableOodle && bOodleForceEnable)
	{
		UE_LOG(OodleHandlerComponentLog, Log, TEXT("Force-enabling Oodle from commandline."));
		bEnableOodle = true;
	}

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
	GConfig->GetBool(OODLE_INI_SECTION, TEXT("bUseDictionaryIfPresent"), bUseDictionaryIfPresent, GEngineIni);

	if (!bUseDictionaryIfPresent && bOodleForceEnable)
	{
		UE_LOG(OodleHandlerComponentLog, Log, TEXT("Force-enabling 'bUseDictionaryIfPresent', due to -Oodle on commandline."));
		bUseDictionaryIfPresent = true;
	}


	FString ReadMode;
	GConfig->GetString(OODLE_INI_SECTION, TEXT("Mode"), ReadMode, GEngineIni);

	if (ReadMode == TEXT("Training") || ReadMode == TEXT("Capturing"))
	{
		Mode = EOodleHandlerMode::Capturing;
	}
#endif
	else //if (ReadMode == TEXT("Release"))
	{
		Mode = EOodleHandlerMode::Release;
	}

	if (bEnableOodle)
	{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		bool bForceCapturing = FParse::Param(FCommandLine::Get(), TEXT("OodleTraining")) ||
								FParse::Param(FCommandLine::Get(), TEXT("OodleCapturing"));

		if (bForceCapturing && Mode != EOodleHandlerMode::Capturing)
		{
			UE_LOG(OodleHandlerComponentLog, Log, TEXT("Switching Oodle to Capturing mode."));

			Mode = EOodleHandlerMode::Capturing;
		}
		// Forces release mode, if configured to search for (and finds) fallback dictionaries
		else if (!bForceCapturing && bUseDictionaryIfPresent && Mode != EOodleHandlerMode::Release)
		{
			FString ServerDicPath;
			FString ClientDicPath;

			if (FindFallbackDictionaries(ServerDicPath, ClientDicPath, true))
			{
				UE_LOG(OodleHandlerComponentLog, Log,
						TEXT("Switching Oodle to Release mode - found server dictionary '%s' and client dictionary '%s'."),
						*ServerDicPath, *ClientDicPath);

				Mode = EOodleHandlerMode::Release;
			}
		}
#endif

		switch(Mode)
		{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
			// @todo #JohnB: Convert this code so that just one capture file is used for all connections, per session
			//					(could set it up much like the dictionary sharing code)
		case EOodleHandlerMode::Capturing:
			{
				if (Handler->Mode == Handler::Mode::Server)
				{
					IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
					FString ReadOutputLogDirectory = FPaths::Combine(*GOodleSaveDir, TEXT("Server"));
					FString BaseFilename;

					PlatformFile.CreateDirectoryTree(*ReadOutputLogDirectory);
					PlatformFile.CreateDirectoryTree(*FPaths::Combine(*ReadOutputLogDirectory, TEXT("Input")));
					PlatformFile.CreateDirectoryTree(*FPaths::Combine(*ReadOutputLogDirectory, TEXT("Output")));
					GConfig->GetString(OODLE_INI_SECTION, TEXT("PacketLogFile"), BaseFilename, GEngineIni);

					BaseFilename = FPaths::GetBaseFilename(BaseFilename);

					BaseFilename = BaseFilename + TEXT("_") + FDateTime::Now().ToString();

					FString PreExtInFilePath = FPaths::Combine(*ReadOutputLogDirectory, TEXT("Input"), *(BaseFilename + TEXT("_Input")));
					FString PreExtOutFilePath = FPaths::Combine(*ReadOutputLogDirectory, TEXT("Output"), *(BaseFilename + TEXT("_Output")));
					FString InPath = PreExtInFilePath + CAPTURE_EXT;
					FString OutPath = PreExtOutFilePath + CAPTURE_EXT;

					// Ensure the In/Out filenames are unique
					for (int32 i=1; PlatformFile.FileExists(*InPath) || PlatformFile.FileExists(*OutPath); i++)
					{
						InPath = PreExtInFilePath + FString::Printf(TEXT("_%i"), i) + CAPTURE_EXT;
						OutPath = PreExtOutFilePath + FString::Printf(TEXT("_%i"), i) + CAPTURE_EXT;
					}

					FArchive* InArc = IFileManager::Get().CreateFileWriter(*InPath);
					FArchive* OutArc = (InArc != nullptr ? IFileManager::Get().CreateFileWriter(*OutPath) : nullptr);

					InPacketLog = (InArc != nullptr ? new FPacketCaptureArchive(*InArc) : nullptr);
					OutPacketLog = (OutArc != nullptr ? new FPacketCaptureArchive(*OutArc) : nullptr);


					if (InPacketLog == nullptr || OutPacketLog == nullptr)
					{
						LowLevelFatalError(TEXT("Failed to create files '%s' and '%s'"), *InPath, *OutPath);
						return;
					}


					InPacketLog->SerializeCaptureHeader();
					OutPacketLog->SerializeCaptureHeader();
				}

				break;
			}
#endif

		case EOodleHandlerMode::Release:
			{
				FString ServerDictionaryPath;
				FString ClientDictionaryPath;
				bool bGotDictionaryPath = false;

#if ( !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING ) && !PLATFORM_PS4
				if (bUseDictionaryIfPresent)
				{
					bGotDictionaryPath = FindFallbackDictionaries(ServerDictionaryPath, ClientDictionaryPath);
				}
#endif

				if (!bGotDictionaryPath)
				{
					bGotDictionaryPath = GetDictionaryPaths(ServerDictionaryPath, ClientDictionaryPath);
				}

				if (bGotDictionaryPath)
				{
					InitializeDictionary(ServerDictionaryPath, ServerDictionary);
					InitializeDictionary(ClientDictionaryPath, ClientDictionary);
				}
				else
				{
					LowLevelFatalError(TEXT("Failed to load Oodle dictionaries."));
				}

				break;
			}

		default:
			break;
		}
	}

	Initialized();
}

void OodleHandlerComponent::InitializeDictionary(FString FilePath, TSharedPtr<FOodleDictionary>& OutDictionary)
{
	TSharedPtr<FOodleDictionary>* DictionaryRef = DictionaryMap.Find(FilePath);

	// Load the dictionary, if it's not yet loaded
	if (DictionaryRef == nullptr)
	{
		FArchive* ReadArc = IFileManager::Get().CreateFileReader(*FilePath);

		if (ReadArc != nullptr)
		{
			FOodleDictionaryArchive BoundArc(*ReadArc);

			uint8* DictionaryData = nullptr;
			uint32 DictionaryBytes = 0;
			uint8* CompactCompressorState = nullptr;
			uint32 CompactCompressorStateBytes = 0;

			BoundArc.SerializeHeader();
			BoundArc.SerializeDictionaryAndState(DictionaryData, DictionaryBytes, CompactCompressorState, CompactCompressorStateBytes);

			if (!BoundArc.IsError())
			{
				UE_LOG(OodleHandlerComponentLog, Log, TEXT("Loading dictionary file: %s"), *FilePath);

				// Uncompact the compressor state
				uint32 CompressorStateSize = OodleNetwork1UDP_State_Size();
				OodleNetwork1UDP_State* CompressorState = (OodleNetwork1UDP_State*)FMemory::Malloc(CompressorStateSize);

				OodleNetwork1UDP_State_Uncompact(CompressorState, (OodleNetwork1UDP_StateCompacted*)CompactCompressorState);


				// Create the shared dictionary state
				int32 HashTableSize = BoundArc.Header.HashTableSize.Get();
				uint32 SharedDictionarySize = OodleNetwork1_Shared_Size(HashTableSize);
				OodleNetwork1_Shared* SharedDictionary = (OodleNetwork1_Shared*)FMemory::Malloc(SharedDictionarySize);

				OodleNetwork1_Shared_SetWindow(SharedDictionary, HashTableSize, (void*)DictionaryData, DictionaryBytes);


				// Now add the dictionary data to the map
				FOodleDictionary* NewDictionary = new FOodleDictionary(HashTableSize, DictionaryData, DictionaryBytes, SharedDictionary,
																		SharedDictionarySize, CompressorState, CompressorStateSize);

				DictionaryRef = &DictionaryMap.Add(FilePath, MakeShareable(NewDictionary));


				if (CompactCompressorState != nullptr)
				{
					delete [] CompactCompressorState;
				}
			}
			else
			{
				UE_LOG(OodleHandlerComponentLog, Warning, TEXT("Error loading dictionary file: %s"), *FilePath);

				if (DictionaryData != nullptr)
				{
					delete [] DictionaryData;
				}

				if (CompactCompressorState != nullptr)
				{
					delete [] CompactCompressorState;
				}
			}


			ReadArc->Close();

			delete ReadArc;
			ReadArc = nullptr;
		}
		else
		{
			LowLevelFatalError(TEXT("Incorrect DictionaryFile Provided"));
		}
	}


	if (DictionaryRef != nullptr)
	{
		OutDictionary = *DictionaryRef;
	}
}

void OodleHandlerComponent::FreeDictionary(TSharedPtr<FOodleDictionary>& InDictionary)
{
	if (InDictionary.IsValid())
	{
		// The dictionary is always referenced within DictionaryMap, so 2 represents last ref within an OodleHandlerComponent
		bool bLastDictionaryRef = InDictionary.GetSharedReferenceCount() == 2;

		if (bLastDictionaryRef)
		{
			for (FDictionaryMap::TIterator It(DictionaryMap); It; ++It)
			{
				if (It.Value() == InDictionary)
				{
					It.RemoveCurrent();
					break;
				}
			}
		}

		InDictionary.Reset();
	}
}

bool OodleHandlerComponent::GetDictionaryPaths(FString& OutServerDictionary, FString& OutClientDictionary, bool bFailFatal/*=true*/)
{
	bool bSuccess = false;

	FString ServerDictionaryPath;
	FString ClientDictionaryPath;

	bSuccess = GConfig->GetString(OODLE_INI_SECTION, TEXT("ServerDictionary"), ServerDictionaryPath, GEngineIni);
	bSuccess = bSuccess && GConfig->GetString(OODLE_INI_SECTION, TEXT("ClientDictionary"), ClientDictionaryPath, GEngineIni);

	if (bSuccess && (ServerDictionaryPath.Len() < 0 || ClientDictionaryPath.Len() < 0))
	{
		const TCHAR* Msg = TEXT("Specify both Server/Client dictionaries for Oodle compressor in DefaultEngine.ini");

		if (bFailFatal)
		{
			LowLevelFatalError(TEXT("%s"), Msg);
		}
		else
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("%s"), Msg);
		}

		bSuccess = false;
	}

	if (bSuccess)
	{
		// Path must be within game directory, e.g: "Content/Oodle/Output.udic" becomes "ShooterGam/Content/Oodle/Output.udic"
		ServerDictionaryPath = FPaths::Combine(*FPaths::GameDir(), *ServerDictionaryPath);
		ClientDictionaryPath = FPaths::Combine(*FPaths::GameDir(), *ClientDictionaryPath);

		FPaths::CollapseRelativeDirectories(ServerDictionaryPath);
		FPaths::CollapseRelativeDirectories(ClientDictionaryPath);

		FPaths::NormalizeDirectoryName(ServerDictionaryPath);
		FPaths::NormalizeDirectoryName(ClientDictionaryPath);

		// Don't allow directory traversal to escape the game directory
		if (!ServerDictionaryPath.StartsWith(FPaths::GameDir()) || !ClientDictionaryPath.StartsWith(FPaths::GameDir()))
		{
			const TCHAR* Msg = TEXT("DictionaryFile not allowed to use ../ paths to escape game directory.");

			if (bFailFatal)
			{
				LowLevelFatalError(TEXT("%s"), Msg);
			}
			else
			{
				UE_LOG(OodleHandlerComponentLog, Warning, TEXT("%s"), Msg);
			}

			bSuccess = false;
		}
	}

	if (bSuccess)
	{
		OutServerDictionary = ServerDictionaryPath;
		OutClientDictionary = ClientDictionaryPath;
	}
	else
	{
		OutServerDictionary = TEXT("");
		OutClientDictionary = TEXT("");
	}


	return bSuccess;
}

#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
bool OodleHandlerComponent::FindFallbackDictionaries(FString& OutServerDictionary, FString& OutClientDictionary, bool bTestOnly/*=false*/)
{
	bool bSuccess = false;

	OutServerDictionary = TEXT("");
	OutClientDictionary = TEXT("");

	// First test the normal dictionary config paths
	FString DefaultServerDicPath;
	FString DefaultClientDicPath;
	IFileManager& FileMan = IFileManager::Get();

	bSuccess = GetDictionaryPaths(DefaultServerDicPath, DefaultClientDicPath, false);

	bSuccess = bSuccess && FileMan.FileExists(*DefaultServerDicPath);
	bSuccess = bSuccess && FileMan.FileExists(*DefaultClientDicPath);


	if (bSuccess)
	{
		OutServerDictionary = DefaultServerDicPath;
		OutClientDictionary = DefaultClientDicPath;
	}
	// If either of the default dictionaries do not exist, do a more speculative search
	else
	{
		TArray<FString> DictionaryList;
		
		FileMan.FindFilesRecursive(DictionaryList, *FPaths::GameDir(), TEXT("*.udic"), true, false);

		if (DictionaryList.Num() > 0)
		{
			// Sort the list alphabetically (case-insensitive)
			DictionaryList.Sort();

			// Very simple matching - anything 'server/output' is a server dictionary, anything 'client/input' is a client dictionary
			int32 FoundServerIdx = DictionaryList.IndexOfByPredicate(
				[](const FString& CurEntry)
				{
					return CurEntry.Contains(TEXT("Server")) || CurEntry.Contains(TEXT("Output"));
				});

			int32 FoundClientIdx = DictionaryList.IndexOfByPredicate(
				[](const FString& CurEntry)
				{
					return CurEntry.Contains(TEXT("Client")) || CurEntry.Contains(TEXT("Input"));
				});

			bSuccess = FoundServerIdx != INDEX_NONE && FoundClientIdx != INDEX_NONE;

			if (!bTestOnly)
			{
				UE_LOG(OodleHandlerComponentLog, Log,
						TEXT("Searched for Oodle dictionary files, and selected the following non-default dictionaries:"));
			}

			if (bSuccess)
			{
				OutServerDictionary = DictionaryList[FoundServerIdx];
				OutClientDictionary = DictionaryList[FoundClientIdx];
			}
			else
			{
				// If all else fails, use any found dictionary, or just use the first listed dictionary, for both client/server
				int32 DicIdx = FMath::Max3(0, FoundServerIdx, FoundClientIdx);

				OutServerDictionary = DictionaryList[DicIdx];
				OutClientDictionary = DictionaryList[DicIdx];

				bSuccess = true;

				if (!bTestOnly)
				{
					UE_LOG(OodleHandlerComponentLog, Log, TEXT("WARNING: Using the same dictionary for both server/client!"));
				}
			}

			if (!bTestOnly)
			{
				UE_LOG(OodleHandlerComponentLog, Log, TEXT("   Server: %s"), *OutServerDictionary);
				UE_LOG(OodleHandlerComponentLog, Log, TEXT("   Client: %s"), *OutClientDictionary);
			}
		}
	}

	return bSuccess;
}
#endif

bool OodleHandlerComponent::IsValid() const
{
	return true;
}

void OodleHandlerComponent::Incoming(FBitReader& Packet)
{
	// Oodle must be the first HandlerComponent to process incoming packets, so does not support bit-shifted reads
	check(Packet.GetPosBits() == 0);

	if (bEnableOodle)
	{
		switch(Mode)
		{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		case EOodleHandlerMode::Capturing:
			{
				if (Handler->Mode == Handler::Mode::Server)
				{
					uint32 SizeOfPacket = Packet.GetNumBytes();

					if (SizeOfPacket > 0)
					{
						void* PacketData = Packet.GetData();

						InPacketLog->SerializePacket(PacketData, SizeOfPacket);
					}
				}
				break;
			}
#endif

		case EOodleHandlerMode::Release:
			{
				uint32 DecompressedLength;
				Packet.SerializeIntPacked(DecompressedLength);

				if (DecompressedLength < MAX_OODLE_PACKET_SIZE)
				{
					// @todo #JohnB: You want to tweak the size of 'MAX_OODLE_PACKET_SIZE'
					static uint8 CompressedData[MAX_OODLE_PACKET_SIZE];
					static uint8 DecompressedData[MAX_OODLE_PACKET_SIZE];

					const int32 CompressedLength = Packet.GetBytesLeft();
					FOodleDictionary* CurDict =
						((Handler->Mode == Handler::Mode::Server) ? ClientDictionary.Get() : ServerDictionary.Get());

					Packet.Serialize(CompressedData, CompressedLength);

					bool bSuccess = !Packet.IsError();

					if (bSuccess)
					{
						{
#if STATS && !UE_BUILD_SHIPPING
							SCOPE_CYCLE_COUNTER(STAT_Oodle_InDecompressTime);
#endif
							bSuccess = !!OodleNetwork1UDP_Decode(CurDict->CompressorState, CurDict->SharedDictionary, CompressedData,
															CompressedLength, DecompressedData, DecompressedLength);
						}



						if (!bSuccess)
						{
							// @todo #JohnB: Set packet archive error and allow in shipping - see note below

							// @todo #JohnB: I don't think this is always an error. This may occur when Oodle decides not to compress,
							//					in which case it should fall-through below (i.e. 'Packet' should remain unmodified)
#if !UE_BUILD_SHIPPING
							UE_LOG(OodleHandlerComponentLog, Error, TEXT("Error decoding Oodle network data."));
#endif
						}
					}
					else
					{
						// @todo #JohnB: Set packet archive error and allow in shipping - see note below
#if !UE_BUILD_SHIPPING
						UE_LOG(OodleHandlerComponentLog, Error, TEXT("Error serializing received packet data"));
#endif
					}


					if (bSuccess)
					{
						// @todo #JohnB: Decompress directly into FBitReader's buffer
						FBitReader UnCompressedPacket(DecompressedData, DecompressedLength * 8);
						Packet = UnCompressedPacket;


#if STATS
						GOodleNetStats.IncomingStats(CompressedLength, DecompressedLength);
#endif
					}
					else
					{
						// @todo #JohnB
					}
				}
				else
				{
					// @todo #JohnB: Add ArIsError handling to the PacketHandler code, then allow the log message in shipping,
					//					and set an archive error
#if !UE_BUILD_SHIPPING
					UE_LOG(OodleHandlerComponentLog, Error,
							TEXT("Received packet with DecompressedLength (%i) >= MAX_OODLE_PACKET_SIZE"), DecompressedLength);

					// @todo #JohnB: Set packet error
#endif
				}


				break;
			}
		}
	}
}

void OodleHandlerComponent::Outgoing(FBitWriter& Packet)
{
	if (bEnableOodle)
	{
		switch(Mode)
		{
#if !UE_BUILD_SHIPPING || OODLE_DEV_SHIPPING
		case EOodleHandlerMode::Capturing:
			{
				if (Handler->Mode == Handler::Mode::Server)
				{
					uint32 SizeOfPacket = Packet.GetNumBytes();

					if (SizeOfPacket > 0)
					{
						OutPacketLog->SerializePacket((void*)Packet.GetData(), SizeOfPacket);
					}
				}
				break;
			}
#endif

		case EOodleHandlerMode::Release:
			{
				// Add size
				uint32 UncompressedLength = Packet.GetNumBytes();

				check(UncompressedLength < MAX_OODLE_PACKET_SIZE);

				if (UncompressedLength > 0)
				{
					// @todo #JohnB: You should be able to share the same two buffers between Incoming and Outgoing, as an optimization
					static uint8 UncompressedData[MAX_OODLE_PACKET_SIZE];
					static uint8 CompressedData[MAX_OODLE_PACKET_SIZE];
					FOodleDictionary* CurDict =
										((Handler->Mode == Handler::Mode::Server) ? ServerDictionary.Get() : ClientDictionary.Get());

					check(OodleLZ_GetCompressedBufferSizeNeeded(UncompressedLength) < MAX_OODLE_PACKET_SIZE);

					memcpy(UncompressedData, Packet.GetData(), UncompressedLength);

					SINTa CompressedLengthSINT = 0;
					
					{
#if STATS && !UE_BUILD_SHIPPING
						SCOPE_CYCLE_COUNTER(STAT_Oodle_OutCompressTime);
#endif

						CompressedLengthSINT = OodleNetwork1UDP_Encode(CurDict->CompressorState, CurDict->SharedDictionary,
																			UncompressedData, UncompressedLength, CompressedData);
					}

					uint32 CompressedLength = static_cast<uint32>(CompressedLengthSINT);

					Packet.Reset();

					// @todo #JohnB: Compress directly into a (deliberately oversized) FBitWriter buffer, which you can shrink after
					Packet.SerializeIntPacked(UncompressedLength);
					Packet.Serialize(CompressedData, CompressedLength);

#if STATS
					GOodleNetStats.OutgoingStats(CompressedLength, UncompressedLength);
#endif
				}
				else
				{
					Packet.SerializeIntPacked(UncompressedLength);
				}

				break;
			}
		}
	}
}

int32 OodleHandlerComponent::GetPacketOverheadBits()
{
	int32 ReturnVal = 0;

	if (bEnableOodle && Mode == EOodleHandlerMode::Release)
	{
		// Oodle writes the decompressed packet size, as its addition to the protocol - it writes this using SerializeIntPacked however,
		// so determine the worst case number of packed bytes that will be written, based on the MAX_OODLE_PACKET_SIZE packet limit.
		// Normally, this should be 2-bytes/16-bits.
		TArray<uint8> MeasureArray;
		FMemoryWriter MeasureAr(MeasureArray);
		uint32 MaxOodlePacket = MAX_OODLE_PACKET_SIZE;

		MeasureAr.SerializeIntPacked(MaxOodlePacket);

		if (!MeasureAr.IsError())
		{
			ReturnVal += MeasureAr.TotalSize() * 8;

			SET_DWORD_STAT(STAT_Oodle_PacketOverhead, ReturnVal);
		}
		else
		{
			LowLevelFatalError(TEXT("Failed to determine OodleHandlerComponent packet overhead."));
		}
	}

	return ReturnVal;
}

uint8 OodleHandlerComponent::GetBitAlignment()
{
	return (GetPacketOverheadBits() % 8);
}

bool OodleHandlerComponent::DoesResetBitAlignment()
{
	return true;
}


#if !UE_BUILD_SHIPPING
/**
 * Exec Interface
 */
static bool OodleExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bReturnVal = false;

	if (FParse::Command(&Cmd, TEXT("Oodle")))
	{
		// Used by unit testing code, to enable/disable Oodle during a unit test
		// NOTE: Do not use while a NetConnection is using Oodle, as this will cause it to break. Debugging/Testing only.
		if (FParse::Command(&Cmd, TEXT("ForceEnable")))
		{
			bool bTurnOn = false;

			if (FParse::Command(&Cmd, TEXT("On")))
			{
				bTurnOn = true;
			}
			else if (FParse::Command(&Cmd, TEXT("Off")))
			{
				bTurnOn = false;
			}
			else if (FParse::Command(&Cmd, TEXT("Default")))
			{
				bTurnOn = FParse::Param(FCommandLine::Get(), TEXT("Oodle"));
			}
			else
			{
				bTurnOn = !bOodleForceEnable;
			}

			if (bTurnOn != bOodleForceEnable)
			{
				bOodleForceEnable = bTurnOn;

				if (bOodleForceEnable)
				{
					UOodleTrainerCommandlet::HandleEnable();
				}
			}
		}
		else
		{
			Ar.Logf(TEXT("Unknown Oodle command 'Oodle %s'"), Cmd);
		}

		bReturnVal = true;
	}

	return bReturnVal;
}

FStaticSelfRegisteringExec OodleExecRegistration(&OodleExec);
#endif


/**
 * Module Interface
 */

TSharedPtr<HandlerComponent> FOodleComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new OodleHandlerComponent);
}

void FOodleComponentModuleInterface::StartupModule()
{
	// If Oodle is force-enabled on the commandline, execute the commandlet-enable command, which also adds to the PacketHandler list
	bOodleForceEnable = FParse::Param(FCommandLine::Get(), TEXT("Oodle"));

	if (bOodleForceEnable)
	{
		UOodleTrainerCommandlet::HandleEnable();
	}


	// Use an absolute path for this, as we want all relative paths, to be relative to this folder
	GOodleSaveDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Oodle")));
	GOodleContentDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::GameContentDir(), TEXT("Oodle")));

#if PLATFORM_WINDOWS
	{
		// Load the Oodle library (NOTE: Path and fallback path mirrored in Oodle.Build.cs)
		FString OodleBinaryPath = FPaths::GameDir() / TEXT( "Binaries/ThirdParty/Oodle/" );
		FString OodleBinaryFile = TEXT( "oo2core_1" );

	#if PLATFORM_64BITS
		OodleBinaryPath += TEXT("Win64/");
		OodleBinaryFile += TEXT("_win64.dll");
	#else
		OodleBinaryPath += TEXT("Win32/");
		OodleBinaryFile += TEXT("_win32.dll");
	#endif

		FPlatformProcess::PushDllDirectory(*OodleBinaryPath);

		OodleDllHandle = FPlatformProcess::GetDllHandle(*(OodleBinaryPath + OodleBinaryFile));

		FPlatformProcess::PopDllDirectory(*OodleBinaryPath);

		if ( OodleDllHandle == nullptr )
		{
			UE_LOG( OodleHandlerComponentLog, Fatal, TEXT( "Could not find Oodle .dll's in path: %s" ), *( OodleBinaryPath + OodleBinaryFile ) );
		}
	}
#endif

#if UE4_OODLE_VER < 200
	OodleInitOptions Options;

	Oodle_Init_GetDefaults_Minimal(OODLE_HEADER_VERSION, &Options);
	Oodle_Init_NoThreads(OODLE_HEADER_VERSION, &Options);
#endif
}

void FOodleComponentModuleInterface::ShutdownModule()
{
#if UE4_OODLE_VER < 200
	Oodle_Shutdown();
#endif

	if (OodleDllHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(OodleDllHandle);
		OodleDllHandle = nullptr;
	}
}
#else
TSharedPtr<HandlerComponent> FOodleComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	UE_LOG(OodleHandlerComponentLog, Error, TEXT("Can't create OodleHandlerComponent instance - HAS_OODLE_SDK is false."));

	return TSharedPtr<HandlerComponent>(nullptr);
}

void FOodleComponentModuleInterface::StartupModule()
{
}

void FOodleComponentModuleInterface::ShutdownModule()
{
}
#endif

IMPLEMENT_MODULE( FOodleComponentModuleInterface, OodleHandlerComponent );
