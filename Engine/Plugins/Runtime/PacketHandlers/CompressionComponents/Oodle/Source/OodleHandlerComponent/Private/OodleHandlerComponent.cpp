// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OodleHandlerComponentPCH.h"


DEFINE_LOG_CATEGORY(OodleHandlerComponentLog);


// @todo #JohnB: Deprecated the 'training' mode from this handler eventually, as it is misnamed/confusing

// @todo #JohnB: Right now, there is no caching of loaded dictionary files, so this is very inefficient with multiple clients

#if HAS_OODLE_SDK
#define OODLE_INI_SECTION TEXT("OodleHandlerComponent")


// @todo #JohnB: Remove this when replacing the current dictionary serialization code
#define STOPGAP_DICT_SERIALIZE 1

// @todo #JohnB: Find a better solution than this, for handling the maximum packet size. This is far higher than it needs to be.
#define MAX_OODLE_PACKET_SIZE 65535


DEFINE_STAT(STAT_Oodle_BytesRaw);
DEFINE_STAT(STAT_Oodle_BytesCompressed);


FString GOodleSaveDir = TEXT("");


// @todo #JohnB: You need to integrate the dictionary generation into a commandlet, to support FArchive-format dictionaries
//					(alternative - and do it this way first! - adjust OodleTrainer to read/write the FArchive version files)

// @todo #JohnB: You need context-sensitive data capturing; e.g. you do NOT want to be capturing voice channel data


// Saved compressor model file :
// must match example_packet - or whatever you use to write the state file
struct OodleNetwork1_SavedStates_Header
{
#define ON1_MAGIC	0x11235801
	U32 magic;
	U32 compressor;
	U32 ht_bits;
	U32 dic_size;
	U32 oodle_header_version;
	U32 dic_complen;
	U32 statecompacted_size;
	U32 statecompacted_complen;

	OodleNetwork1_SavedStates_Header()
		: magic(0)
		, compressor(0)
		, ht_bits(0)
		, dic_size(0)
		, oodle_header_version(0)
		, dic_complen(0)
		, statecompacted_size(0)
		, statecompacted_complen(0)
	{
	}

	// Temporary serializer, prior to replacement of this code
	void SerializeSavedStates(FArchive& InArc)
	{
		InArc << (uint32&)magic;
		InArc << (uint32&)compressor;
		InArc << (uint32&)ht_bits;
		InArc << (uint32&)dic_size;
		InArc << (uint32&)oodle_header_version;
		InArc << (uint32&)dic_complen;
		InArc << (uint32&)statecompacted_size;
		InArc << (uint32&)statecompacted_complen;
	}
};

// OODLE
OodleHandlerComponent::OodleHandlerComponent()
	: BytesSaved(0)
#if EXAMPLE_CAPTURE_FORMAT
	, PacketLogFile(NULL)
	, bPacketLogAppend(false)
#else
	, InPacketLog(NULL)
	, OutPacketLog(NULL)
#endif
	, bEnableOodle(false)
#if !UE_BUILD_SHIPPING
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
#if EXAMPLE_CAPTURE_FORMAT
	// @todo #JohnB: PacketLogfile is not deleted. Doesn't matter though, as this is being deprecated.
#else
	if (OutPacketLog != NULL)
	{
		OutPacketLog->Close();
		OutPacketLog->DeleteInnerArchive();

		delete OutPacketLog;

		OutPacketLog = NULL;
	}

	if (InPacketLog != NULL)
	{
		InPacketLog->Close();
		InPacketLog->DeleteInnerArchive();

		delete InPacketLog;

		InPacketLog = NULL;
	}
#endif

	if (ServerDictionary.Dictionary != NULL)
	{
		free(ServerDictionary.Dictionary);
	}

	if (ServerDictionary.SharedDictionaryData != NULL)
	{
		free(ServerDictionary.SharedDictionaryData);
	}

	if (ServerDictionary.CompressorState != NULL)
	{
		free(ServerDictionary.CompressorState);
	}


	if (ClientDictionary.Dictionary != NULL)
	{
		free(ClientDictionary.Dictionary);
	}

	if (ClientDictionary.SharedDictionaryData != NULL)
	{
		free(ClientDictionary.SharedDictionaryData);
	}

	if (ClientDictionary.CompressorState != NULL)
	{
		free(ClientDictionary.CompressorState);
	}

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

#if !UE_BUILD_SHIPPING
		GConfig->SetBool(OODLE_INI_SECTION, TEXT("bUseDictionaryIfPresent"), false, GEngineIni);
#endif

		GConfig->SetString(OODLE_INI_SECTION, TEXT("Mode"), TEXT("Capturing"), GEngineIni);
		GConfig->SetString(OODLE_INI_SECTION, TEXT("PacketLogFile"), TEXT("PacketDump"), GEngineIni);

#if EXAMPLE_CAPTURE_FORMAT
		GConfig->SetBool(OODLE_INI_SECTION, TEXT("bPacketLogAppend"), false, GEngineIni);
#endif

		GConfig->SetString(OODLE_INI_SECTION, TEXT("ServerDictionary"), TEXT(""), GEngineIni);
		GConfig->SetString(OODLE_INI_SECTION, TEXT("ClientDictionary"), TEXT(""), GEngineIni);
	}
}

void OodleHandlerComponent::Initialize()
{
	InitFirstRunConfig();

	// Class config variables
	GConfig->GetBool(OODLE_INI_SECTION, TEXT("bEnableOodle"), bEnableOodle, GEngineIni);

	if (!bEnableOodle && FParse::Param(FCommandLine::Get(), TEXT("Oodle")))
	{
		UE_LOG(OodleHandlerComponentLog, Log, TEXT("Force-enabling Oodle from commandline."));
		bEnableOodle = true;
	}

#if !UE_BUILD_SHIPPING
	GConfig->GetBool(OODLE_INI_SECTION, TEXT("bUseDictionaryIfPresent"), bUseDictionaryIfPresent, GEngineIni);
#endif

#if EXAMPLE_CAPTURE_FORMAT
	GConfig->GetBool(OODLE_INI_SECTION, TEXT("bPacketLogAppend"), bPacketLogAppend, GEngineIni);
#endif

	// Mode
	FString ReadMode;
	GConfig->GetString(OODLE_INI_SECTION, TEXT("Mode"), ReadMode, GEngineIni);

	if (ReadMode == TEXT("Training") || ReadMode == TEXT("Capturing"))
	{
		Mode = EOodleHandlerMode::Capturing;
	}
	else if(ReadMode == TEXT("Release"))
	{
		Mode = EOodleHandlerMode::Release;
	}
	// Default
	else
	{
		Mode = EOodleHandlerMode::Release;
	}

	if (bEnableOodle)
	{
#if !UE_BUILD_SHIPPING
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


#if EXAMPLE_CAPTURE_FORMAT
					FString FinalFilePath;

					// When bPacketLogAppend is used, a static log filename is used
					if (bPacketLogAppend)
					{
						FString PreExtFilePath = FPaths::Combine(*ReadOutputLogDirectory, *BaseFilename);

						// Try0: PacketDump.ucap, Try1: PacketDump_1.ucap, Try#:PacketDump_#.ucap
						FinalFilePath = PreExtFilePath + CAPTURE_EXT;

						for (int32 i=1; ; i++)
						{
							// If not appending to a file, make sure to disable this bool - otherwise the file header won't be written
							bPacketLogAppend = PlatformFile.FileExists(*FinalFilePath);
							PacketLogFile = PlatformFile.OpenWrite(*FinalFilePath, true);

							if (PacketLogFile != NULL)
							{
								break;
							}


							FinalFilePath = PreExtFilePath + FString::Printf(TEXT("_%i"), i) + CAPTURE_EXT;
						}
					}
					// When non-append logging is used, the log filename uses a timestamp
					else
					{
						BaseFilename = BaseFilename + TEXT("_") + FDateTime::Now().ToString();

						FString PreExtFilePath = FPaths::Combine(*ReadOutputLogDirectory, *BaseFilename);
						FinalFilePath = PreExtFilePath + CAPTURE_EXT;

						for (int32 i=1; PlatformFile.FileExists(*FinalFilePath); i++)
						{
							FinalFilePath = PreExtFilePath + FString::Printf(TEXT("_%i"), i) + CAPTURE_EXT;
						}

						// Try0: PacketDump_2015.09.23-20.12.54.ucap, Try1: PacketDump_2015.09.23-20.12.54_1.ucap, ...
						PacketLogFile = PlatformFile.OpenWrite(*FinalFilePath);
					}

					if (PacketLogFile == NULL)
					{
						LowLevelFatalError(TEXT("Failed to create file %s"), *FinalFilePath);
						return;
					}
#else
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
					FArchive* OutArc = (InArc != NULL ? IFileManager::Get().CreateFileWriter(*OutPath) : NULL);

					InPacketLog = (InArc != NULL ? new FPacketCaptureArchive(*InArc) : NULL);
					OutPacketLog = (OutArc != NULL ? new FPacketCaptureArchive(*OutArc) : NULL);


					if (InPacketLog == NULL || OutPacketLog == NULL)
					{
						LowLevelFatalError(TEXT("Failed to create files '%s' and '%s'"), *InPath, *OutPath);
						return;
					}
#endif


#if EXAMPLE_CAPTURE_FORMAT
					// Only write the initial header, if not appending
					if (!bPacketLogAppend)
					{
						int32 NumberOfChannels = 1;
			
						PacketLogFile->Write(reinterpret_cast<const uint8*>(&NumberOfChannels), sizeof(NumberOfChannels));
					}
#else
					InPacketLog->SerializeCaptureHeader();
					OutPacketLog->SerializeCaptureHeader();
#endif
				}

				break;
			}

		case EOodleHandlerMode::Release:
			{
				FString ServerDictionaryPath;
				FString ClientDictionaryPath;
				bool bGotDictionaryPath = false;

#if !UE_BUILD_SHIPPING
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

void OodleHandlerComponent::InitializeDictionary(FString FilePath, FOodleDictionary& OutDictionary)
{
	// @todo #JohnB: When rewriting, make sure to have proper error checking, for cases such as e.g. trying to load dictionary,
	//					from past/incompatible Oodle version

#if STOPGAP_DICT_SERIALIZE
	TArray<uint8> FileInMemory;
	FMemoryReader FileArc(FileInMemory);

	// @todo #JohnB: Need a way to verify the state file, is actually a state file (do this once converting to FArchive)
	if (FFileHelper::LoadFileToArray(FileInMemory, *FilePath))
	{
		// parse header :
		OodleNetwork1_SavedStates_Header SavedDictionaryFileData;

		SavedDictionaryFileData.SerializeSavedStates(FileArc);

		if (SavedDictionaryFileData.magic == ON1_MAGIC && SavedDictionaryFileData.oodle_header_version == OODLE_HEADER_VERSION)
		{
			// this code only loads compressed states currently
			check(SavedDictionaryFileData.compressor != -1);


			S32 StateHTBits = SavedDictionaryFileData.ht_bits;
			SINTa DictionarySize = SavedDictionaryFileData.dic_size;

			SINTa DictionaryCompLen = SavedDictionaryFileData.dic_complen;
			SINTa CompressorStateCompactedSize = SavedDictionaryFileData.statecompacted_size;
			SINTa CompressorStateCompactedLength = SavedDictionaryFileData.statecompacted_complen;

			SINTa SharedSize = OodleNetwork1_Shared_Size(StateHTBits);
			SINTa CompressorStateSize = OodleNetwork1UDP_State_Size();

			check(DictionarySize >= DictionaryCompLen);
			check(CompressorStateCompactedSize >= CompressorStateCompactedLength);

			check(CompressorStateCompactedSize > 0 && CompressorStateCompactedSize < OodleNetwork1UDP_StateCompacted_MaxSize());

			check(FileArc.TotalSize() == (S64)sizeof(OodleNetwork1_SavedStates_Header) + DictionaryCompLen +
					CompressorStateCompactedLength);

			//OodleLog_Printf_v1("OodleNetwork1UDP Loading; dic comp %d , state %d->%d\n",DictionaryCompLen,
			//						CompressorStateCompactedSize, CompressorStateCompactedLength);

			//-------------------------------------------
			// decompress Dictionary and CompressorStatecompacted

			// @todo #JohnB: Security issue
			OutDictionary.Dictionary = malloc(DictionarySize);

			void * DictionaryCompressed = &FileInMemory[FileArc.Tell()];

			SINTa DecompressedDictionarySize = OodleLZ_Decompress(DictionaryCompressed, DictionaryCompLen, OutDictionary.Dictionary,
																	DictionarySize);

			check(DecompressedDictionarySize == DictionarySize);

			OodleNetwork1UDP_StateCompacted* UDPNewCompacted = (OodleNetwork1UDP_StateCompacted *)malloc(CompressorStateCompactedSize);

			void * CompressorStatecompacted_comp_ptr = (U8 *)DictionaryCompressed + DictionaryCompLen;

			SINTa decomp_statecompacted_size = OodleLZ_Decompress(CompressorStatecompacted_comp_ptr, CompressorStateCompactedLength,
																	UDPNewCompacted, CompressorStateCompactedSize);

			check(decomp_statecompacted_size == CompressorStateCompactedSize);

			//----------------------------------------------
			// Uncompact the "Compacted" state into a usable state

			OutDictionary.CompressorState = (OodleNetwork1UDP_State *)malloc(CompressorStateSize);
			check(OutDictionary.CompressorState != NULL);

			OodleNetwork1UDP_State_Uncompact(OutDictionary.CompressorState, UDPNewCompacted);
			free(UDPNewCompacted);

			//----------------------------------------------
			// fill out SharedDictionaryData from the dictionary

			OutDictionary.SharedDictionaryData = (OodleNetwork1_Shared *)malloc(SharedSize);
			check(OutDictionary.SharedDictionaryData != NULL);
			OodleNetwork1_Shared_SetWindow(OutDictionary.SharedDictionaryData, StateHTBits, OutDictionary.Dictionary,
											(S32)DictionarySize);

			//-----------------------------------------------------
	
			//SET_DWORD_STAT(STAT_Oodle_DicSize, DictionarySize);
		}
		else if (SavedDictionaryFileData.magic != ON1_MAGIC)
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("OodleNetCompressor: state_file ON1_MAGIC mismatch"));
		}
		else //if (SavedDictionaryFileData->oodle_header_version != OODLE_HEADER_VERSION)
		{
			UE_LOG(OodleHandlerComponentLog, Warning,
					TEXT("OodleNetCompressor: state_file OODLE_HEADER_VERSION mismatch. Was: %u, Expected: %u"),
					(uint32)SavedDictionaryFileData.oodle_header_version, (uint32)OODLE_HEADER_VERSION);
		}
#else
	TArray<uint8> FileInMemory;

	// @todo #JohnB: Need a way to verify the state file, is actually a state file (do this once converting to FArchive)
	if (FFileHelper::LoadFileToArray(FileInMemory, *FilePath))
	{
		// parse header :
		OodleNetwork1_SavedStates_Header* SavedDictionaryFileData = (OodleNetwork1_SavedStates_Header*)FileInMemory.GetData();

		if (SavedDictionaryFileData->magic == ON1_MAGIC && SavedDictionaryFileData->oodle_header_version == OODLE_HEADER_VERSION)
		{
			// this code only loads compressed states currently
			check(SavedDictionaryFileData->compressor != -1);


			S32 StateHTBits = SavedDictionaryFileData->ht_bits;
			SINTa DictionarySize = SavedDictionaryFileData->dic_size;

			SINTa DictionaryCompLen = SavedDictionaryFileData->dic_complen;
			SINTa CompressorStateCompactedSize = SavedDictionaryFileData->statecompacted_size;
			SINTa CompressorStateCompactedLength = SavedDictionaryFileData->statecompacted_complen;

			SINTa SharedSize = OodleNetwork1_Shared_Size(StateHTBits);
			SINTa CompressorStateSize = OodleNetwork1UDP_State_Size();

			check(DictionarySize >= DictionaryCompLen);
			check(CompressorStateCompactedSize >= CompressorStateCompactedLength);

			check(CompressorStateCompactedSize > 0 && CompressorStateCompactedSize < OodleNetwork1UDP_StateCompacted_MaxSize());

			check(FileInMemory.Num() == (S64)sizeof(OodleNetwork1_SavedStates_Header) + DictionaryCompLen +
					CompressorStateCompactedLength);

			//OodleLog_Printf_v1("OodleNetwork1UDP Loading; dic comp %d , state %d->%d\n",DictionaryCompLen,
			//						CompressorStateCompactedSize, CompressorStateCompactedLength);

			//-------------------------------------------
			// decompress Dictionary and CompressorStatecompacted

			OutDictionary.Dictionary = malloc(DictionarySize);

			void * DictionaryCompressed = SavedDictionaryFileData + 1;

			SINTa DecompressedDictionarySize = OodleLZ_Decompress(DictionaryCompressed, DictionaryCompLen, OutDictionary.Dictionary,
																	DictionarySize);

			check(DecompressedDictionarySize == DictionarySize);

			OodleNetwork1UDP_StateCompacted* UDPNewCompacted = (OodleNetwork1UDP_StateCompacted *)malloc(CompressorStateCompactedSize);

			void * CompressorStatecompacted_comp_ptr = (U8 *)DictionaryCompressed + DictionaryCompLen;

			SINTa decomp_statecompacted_size = OodleLZ_Decompress(CompressorStatecompacted_comp_ptr, CompressorStateCompactedLength,
																	UDPNewCompacted, CompressorStateCompactedSize);

			check(decomp_statecompacted_size == CompressorStateCompactedSize);

			//----------------------------------------------
			// Uncompact the "Compacted" state into a usable state

			OutDictionary.CompressorState = (OodleNetwork1UDP_State *)malloc(CompressorStateSize);
			check(OutDictionary.CompressorState != NULL);

			OodleNetwork1UDP_State_Uncompact(OutDictionary.CompressorState, UDPNewCompacted);
			free(UDPNewCompacted);

			//----------------------------------------------
			// fill out SharedDictionaryData from the dictionary

			OutDictionary.SharedDictionaryData = (OodleNetwork1_Shared *)malloc(SharedSize);
			check(OutDictionary.SharedDictionaryData != NULL);
			OodleNetwork1_Shared_SetWindow(OutDictionary.SharedDictionaryData, StateHTBits, OutDictionary.Dictionary,
											(S32)DictionarySize);

			//-----------------------------------------------------
	
			//SET_DWORD_STAT(STAT_Oodle_DicSize, DictionarySize);
		}
		else if (SavedDictionaryFileData->magic != ON1_MAGIC)
		{
			UE_LOG(OodleHandlerComponentLog, Warning, TEXT("OodleNetCompressor: state_file ON1_MAGIC mismatch"));
		}
		else //if (SavedDictionaryFileData->oodle_header_version != OODLE_HEADER_VERSION)
		{
			UE_LOG(OodleHandlerComponentLog, Warning,
					TEXT("OodleNetCompressor: state_file OODLE_HEADER_VERSION mismatch. Was: %u, Expected: %u"),
					(uint32)SavedDictionaryFileData->oodle_header_version, (uint32)OODLE_HEADER_VERSION);
		}
#endif
	}
	else
	{
		LowLevelFatalError(TEXT("Incorrect DictionaryFile Provided"));
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

#if !UE_BUILD_SHIPPING
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
	if (bEnableOodle)
	{
		switch(Mode)
		{
		case EOodleHandlerMode::Capturing:
			{
				if(Handler->Mode == Handler::Mode::Server)
				{
					uint32 SizeOfPacket = Packet.GetNumBytes();

#if EXAMPLE_CAPTURE_FORMAT
					int32 channel = 0;

					PacketLogFile->Write(reinterpret_cast<const uint8*>(&channel), sizeof(channel));
					PacketLogFile->Write(reinterpret_cast<const uint8*>(&SizeOfPacket), sizeof(SizeOfPacket));
					PacketLogFile->Write(reinterpret_cast<const uint8*>(Packet.GetData()), SizeOfPacket);
#else
					void* PacketData = Packet.GetData();
					InPacketLog->SerializePacket(PacketData, SizeOfPacket);
#endif
				}
				break;
			}

		case EOodleHandlerMode::Release:
			{
				uint32 DecompressedLength;
				Packet.SerializeIntPacked(DecompressedLength);

				if (ensure(DecompressedLength < MAX_OODLE_PACKET_SIZE))
				{
					// @todo #JohnB: You want to tweak the size of 'MAX_OODLE_PACKET_SIZE'
					static uint8 CompressedData[MAX_OODLE_PACKET_SIZE];
					static uint8 DecompressedData[MAX_OODLE_PACKET_SIZE];
					const int32 CompressedLength = Packet.GetBytesLeft();
					FOodleDictionary* CurDict = ((Handler->Mode == Handler::Mode::Server) ? &ClientDictionary : &ServerDictionary);

					Packet.Serialize(CompressedData, CompressedLength);

					check(OodleNetwork1UDP_Decode(CurDict->CompressorState, CurDict->SharedDictionaryData, CompressedData,
													CompressedLength, DecompressedData, DecompressedLength));


					// @todo #JohnB: Decompress directly into FBitReader's buffer
					FBitReader UnCompressedPacket(DecompressedData, DecompressedLength * 8);
					Packet = UnCompressedPacket;
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
		case EOodleHandlerMode::Capturing:
			{
				if(Handler->Mode == Handler::Mode::Server)
				{
					uint32 SizeOfPacket = Packet.GetNumBytes();

#if EXAMPLE_CAPTURE_FORMAT
					int32 channel = 0;

					PacketLogFile->Write(reinterpret_cast<const uint8*>(&channel), sizeof(channel));
					PacketLogFile->Write(reinterpret_cast<const uint8*>(&SizeOfPacket), sizeof(SizeOfPacket));
					PacketLogFile->Write(reinterpret_cast<const uint8*>(Packet.GetData()), SizeOfPacket);
#else
					OutPacketLog->SerializePacket((void*)Packet.GetData(), SizeOfPacket);
#endif
				}
				break;
			}

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
					FOodleDictionary* CurDict = ((Handler->Mode == Handler::Mode::Server) ? &ServerDictionary : &ClientDictionary);

					check(OodleLZ_GetCompressedBufferSizeNeeded(UncompressedLength) < MAX_OODLE_PACKET_SIZE);

					memcpy(UncompressedData, Packet.GetData(), UncompressedLength);

					SINTa CompressedLengthSINT = OodleNetwork1UDP_Encode(CurDict->CompressorState, CurDict->SharedDictionaryData,
																			UncompressedData, UncompressedLength, CompressedData);

					uint32 CompressedLength = static_cast<uint32>(CompressedLengthSINT);

					Packet.Reset();

					// @todo #JohnB: Compress directly into a (deliberately oversized) FBitWriter buffer, which you can shrink afterwards
					Packet.SerializeIntPacked(UncompressedLength);
					Packet.Serialize(CompressedData, CompressedLength);


					BytesSaved += UncompressedLength - CompressedLength;

					//UE_LOG(OodleHandlerComponentLog, Log, TEXT("Oodle Compressed: UnCompressed: %i Compressed: %i Total: %i"),
					//			UncompressedLength, CompressedLength, BytesSaved);

					INC_DWORD_STAT_BY(STAT_Oodle_BytesRaw, UncompressedLength);
					INC_DWORD_STAT_BY(STAT_Oodle_BytesCompressed, CompressedLength);
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

// MODULE INTERFACE
TSharedPtr<HandlerComponent> FOodleComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new OodleHandlerComponent);
}

void FOodleComponentModuleInterface::StartupModule()
{
	// Use an absolute path for this, as we want all relative paths, to be relative to this folder
	GOodleSaveDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(*FPaths::GameSavedDir(), TEXT("Oodle")));


	// Load the Oodle library (NOTE: Path mirrored in Oodle.Build.cs)
	FString OodleBinaryPath = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/NotForLicensees/Oodle/");
	FString OodleBinaryFile;

#if PLATFORM_WINDOWS
	{
	#if UE4_OODLE_VER >= 200
		OodleBinaryFile = TEXT("oo2core_1");
	#else
		OodleBinaryFile = FString::Printf(TEXT("oodle_%i"), UE4_OODLE_VER);
	#endif

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
	}
#endif

#if !PLATFORM_LINUX
	check(OodleDllHandle != NULL);
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

	if (OodleDllHandle != NULL)
	{
		FPlatformProcess::FreeDllHandle(OodleDllHandle);
		OodleDllHandle = NULL;
	}
}
#else
TSharedPtr<HandlerComponent> FOodleComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	UE_LOG(OodleHandlerComponentLog, Error, TEXT("Can't create OodleHandlerComponent instance - HAS_OODLE_SDK is false."));

	return TSharedPtr<HandlerComponent>(NULL);
}

void FOodleComponentModuleInterface::StartupModule()
{
}

void FOodleComponentModuleInterface::ShutdownModule()
{
}
#endif

IMPLEMENT_MODULE( FOodleComponentModuleInterface, OodleHandlerComponent );