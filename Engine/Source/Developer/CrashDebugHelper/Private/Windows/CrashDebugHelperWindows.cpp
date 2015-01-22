// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugHelperPrivatePCH.h"
#include "Database.h"


#include "ISourceControlModule.h"

#include "AllowWindowsPlatformTypes.h"
#include <DbgHelp.h>

FCrashDebugHelperWindows::FCrashDebugHelperWindows()
{
}

FCrashDebugHelperWindows::~FCrashDebugHelperWindows()
{
}

//
//	Helper functions for processing windows crash dumps
//

/**
 *	Verify the given crash dump is Windows based
 *
 *	@param	InMiniDumpHeader		The header from the crash dump
 *
 *	@return	bool					true if it is a valid crash dump, false if not
 */
bool VerifyCrashDump_Windows(MINIDUMP_HEADER& InMiniDumpHeader)
{
	// See if the signature is "MDMP"
	if( InMiniDumpHeader.Signature == 0x504d444d )
	{
		return true;
	}

	return false;
}

/**
 *	Extract the UE engine version from the crash dump
 *
 *	@param	InFileHandle		The handle to the opened crash dump file.
 *	@param	InMiniDumpHeader	The header from the crash dump
 *	@param	OutEngineVersion	The engine version if successfully extracted
 *
 *	@return	bool				true if successfully extracted, false if not
 */
bool ExtractEngineVersion_Windows(HANDLE InFileHandle, MINIDUMP_HEADER& InMiniDumpHeader, int32& OutEngineVersion, FString& OutPlatform)
{
	bool bResult = false;

	// Read the directories
	MINIDUMP_DIRECTORY ModuleListDirectory;
	FMemory::Memzero(&ModuleListDirectory, sizeof(MINIDUMP_DIRECTORY));
	int32 ModuleListStreamIdx = -1;

	bool bFailed = false;
	LONG StreamOffset = InMiniDumpHeader.StreamDirectoryRva;
	for (ULONG StreamIdx = 0; (StreamIdx < InMiniDumpHeader.NumberOfStreams) && !bFailed; StreamIdx++)
	{
		SetFilePointer(InFileHandle, StreamOffset, NULL, FILE_BEGIN);

		MINIDUMP_DIRECTORY MDDirectory;
		DWORD NumByteToRead = sizeof(MINIDUMP_DIRECTORY);
		DWORD NumBytesRead = 0;
		if (ReadFile(InFileHandle, &MDDirectory, NumByteToRead, &NumBytesRead, NULL) == TRUE)
		{
			switch (MDDirectory.StreamType)
			{
			// Reserved. Do not use this enumeration value.
			case UnusedStream:
			case ReservedStream0:
			case ReservedStream1:
				break;

			// * The stream contains thread information and stack. For more information, see MINIDUMP_THREAD_LIST.
			case ThreadListStream:
				break;

			// * The stream contains module information with the base, size and version info. For more information, see MINIDUMP_MODULE_LIST.
			case ModuleListStream:
				ModuleListStreamIdx = StreamIdx;
				FMemory::Memcpy(&ModuleListDirectory, &MDDirectory, sizeof(MINIDUMP_DIRECTORY));
				break;

			// The stream contains memory allocation information. For more information, see MINIDUMP_MEMORY_LIST.
			case MemoryListStream:
				break;

			// * The stream contains exception information with the code. For more information, see MINIDUMP_EXCEPTION_STREAM.
			case ExceptionStream:
				break;

			// * The stream contains general system information. For more information, see MINIDUMP_SYSTEM_INFO.
			case SystemInfoStream:
				break;

			// The stream contains extended thread information. For more information, see MINIDUMP_THREAD_EX_LIST.
			case ThreadExListStream:
				break;

			// The stream contains memory allocation information. For more information, see MINIDUMP_MEMORY64_LIST.
			case Memory64ListStream:
				break;

			// The stream contains an ANSI string used for documentation purposes.
			case CommentStreamA:
				break;

			// The stream contains a Unicode string used for documentation purposes.
			case CommentStreamW:
				break;

			// The stream contains high-level information about the active operating system handles. For more information, see MINIDUMP_HANDLE_DATA_STREAM.
			case HandleDataStream:
				break;

			// The stream contains function table information. For more information, see MINIDUMP_FUNCTION_TABLE_STREAM.
			case FunctionTableStream:
				break;

			// The stream contains module information for the unloaded modules. For more information, see MINIDUMP_UNLOADED_MODULE_LIST.
			case UnloadedModuleListStream:
				break;

			// The stream contains miscellaneous information. For more information, see MINIDUMP_MISC_INFO or MINIDUMP_MISC_INFO_2.
			case MiscInfoStream:
				break;

			// The stream contains memory region description information. It corresponds to the information that would be returned for the process from the 
			// VirtualQuery function. For more information, see MINIDUMP_MEMORY_INFO_LIST.
			case MemoryInfoListStream:
				break;

			// * The stream contains thread state information. For more information, see MINIDUMP_THREAD_INFO_LIST.
			case ThreadInfoListStream:
				break;

			// This stream contains operation list information. For more information, see MINIDUMP_HANDLE_OPERATION_LIST.
			case HandleOperationListStream:
				break;

			// Any value greater than this value will not be used by the system and can be used to represent application-defined data streams. For more information, see MINIDUMP_USER_STREAM.
			case LastReservedStream:
			default:
				break;
			}
		}
		else
		{
			bFailed = true;
		}

		StreamOffset += sizeof(MINIDUMP_DIRECTORY);
	}

	if (ModuleListStreamIdx != -1)
	{
		// We found the module list stream. Parse that...
		// Prep the file pointer to the right location
		SetFilePointer(InFileHandle, ModuleListDirectory.Location.Rva, NULL, FILE_BEGIN);

		ULONG32 NumberOfModules = 0;
		DWORD NumByteToRead = sizeof(ULONG32);
		DWORD NumBytesRead = 0;

		if (ReadFile(InFileHandle, &NumberOfModules, NumByteToRead, &NumBytesRead, NULL) == TRUE)
		{
			//@todo. This is excess as we really only care about the executable module... clean this up.
			TMap<FString, MINIDUMP_MODULE> MDModulesMap;
			TArray<MINIDUMP_MODULE> MDModules;
			MDModules.Empty(NumberOfModules);
			MDModules.AddZeroed(NumberOfModules);

			bool bFailed = false;
			for (uint32 ModuleIdx = 0; (ModuleIdx < NumberOfModules) && !bFailed; ModuleIdx++)
			{
				NumByteToRead = sizeof(MINIDUMP_MODULE);
				NumBytesRead = 0;
				if (ReadFile(InFileHandle, &(MDModules[ModuleIdx]), NumByteToRead, &NumBytesRead, NULL) == FALSE)
				{
					bFailed = true;
				}
			}

			if (bFailed == false)
			{
				for (uint32 NameIdx = 0; (NameIdx < (uint32)MDModules.Num()) && !bFailed; NameIdx++)
				{
					MINIDUMP_MODULE& Module = MDModules[NameIdx];
					SetFilePointer(InFileHandle, Module.ModuleNameRva, NULL, FILE_BEGIN);

					WCHAR TempBuffer[8192];
					ULONG32 NameLength;
					NumByteToRead = sizeof(ULONG32);
					NumBytesRead = 0;
					if (ReadFile(InFileHandle, &NameLength, NumByteToRead, &NumBytesRead, NULL) == TRUE)
					{
						if ((NameLength / 2) < 8192)
						{
							NumByteToRead = NameLength;
							NumBytesRead = 0;
							if (ReadFile(InFileHandle, &TempBuffer, NumByteToRead, &NumBytesRead, NULL) == TRUE)
							{
								TempBuffer[NameLength/2] = 0;
								FString ModuleName = TempBuffer;
								MDModulesMap.Add(ModuleName, Module);
							}
							else
							{
								bFailed = true;
							}
						}
						else
						{
							bFailed = true;
						}
					}
					else
					{
						bFailed = true;
					}
				}
			}

			if (bFailed == false)
			{
				// Find the executable module...
				for (TMap<FString,MINIDUMP_MODULE>::TIterator DumpIt(MDModulesMap); DumpIt; ++DumpIt)
				{
					FString& ModuleName = DumpIt.Key();
					MINIDUMP_MODULE& Module = DumpIt.Value();

					//@todo. We need to be able to determine other UE4 executables like SlateViewer...
					//@todo rocket Game suffix! We need another way to discover game executables

					FString GameExeName = TEXT("Game.exe");

					FString Win64GameDirName(TEXT("Game\\Binaries\\Win64\\"));
					FString Win64UE4ExeName(TEXT("Binaries\\Win64\\UE4"));

					FString Win32GameDirName(TEXT("Game\\Binaries\\Win32\\"));
					FString Win32UE4ExeName(TEXT("Binaries\\Win32\\UE4"));
					FString UE4ExeName = TEXT(".exe");

					bool bIsOurExecutable = false;
					if ((ModuleName.Contains(Win64GameDirName) && ModuleName.EndsWith(GameExeName)) || 
						(ModuleName.Contains(Win64UE4ExeName) && ModuleName.EndsWith(UE4ExeName)))
					{
						bIsOurExecutable = true;
						OutPlatform = TEXT("Win64");
					}

					if ((ModuleName.Contains(Win32GameDirName) && ModuleName.EndsWith(GameExeName)) ||
						(ModuleName.Contains(Win32UE4ExeName) && ModuleName.EndsWith(UE4ExeName)))
					{
						bIsOurExecutable = true;
						OutPlatform = TEXT("Win32");
					}

					if (bIsOurExecutable == true)
					{
						int32 VerA = (Module.VersionInfo.dwProductVersionMS & 0xFFFF0000) >> 16;
						int32 VerB = (Module.VersionInfo.dwProductVersionMS & 0x0000FFFF);
						int32 VerC = (Module.VersionInfo.dwProductVersionLS & 0xFFFF0000) >> 16;
						int32 VerD = (Module.VersionInfo.dwProductVersionLS & 0x0000FFFF);

						if (VerD == 0)
						{
							// Format is #.#.ENGINEVERSION.#
							OutEngineVersion = VerC;
						}
						else
						{
							// Format is #.#.HIChangeList.LOWChangeList
							OutEngineVersion = (VerC << 16) | VerD;
						}

						bResult = true;
					}
				}
			}
		}
		else
		{
			UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseMiniDumpFile: Failed to read module list"));
		}
	}
	else
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("Failed to find module list stream in crash dump..."));
	}

	return bResult;
}

bool FCrashDebugHelperWindows::LaunchDebugger(const FString& InCrashDumpFilename, FCrashDebugInfo& OutCrashDebugInfo)
{
	// Ensure we are in a valid state to launch the debugger
	return false;
}

bool FCrashDebugHelperWindows::ParseCrashDump_Windows(const TCHAR* InCrashDumpFilename, FCrashDebugInfo& OutCrashDebugInfo)
{
	bool bResult = false;

	HANDLE FileHandle = CreateFileW(InCrashDumpFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		// Read the header
		MINIDUMP_HEADER	MDHeader;
		FMemory::Memzero(&MDHeader, sizeof(MINIDUMP_HEADER));
		DWORD NumByteToRead = sizeof(MINIDUMP_HEADER);
		DWORD NumBytesRead;
		if (ReadFile(FileHandle, &MDHeader, NumByteToRead, &NumBytesRead, NULL) == TRUE)
		{
			UE_LOG(LogCrashDebugHelper, Display, TEXT("ParseCrashDump_Windows: Read header from %s"), InCrashDumpFilename);

			// Verify it is a legitimate crash dump
			if (VerifyCrashDump_Windows(MDHeader) == true)
			{
				UE_LOG(LogCrashDebugHelper, Display, TEXT("ParseCrashDump_Windows: Verified windows crash dump: %s"), InCrashDumpFilename);

				// Extract the engine version
				if (ExtractEngineVersion_Windows(FileHandle, MDHeader, OutCrashDebugInfo.EngineVersion, OutCrashDebugInfo.PlatformName) == true)
				{
					UE_LOG(LogCrashDebugHelper, Display, TEXT("ParseCrashDump_Windows: Extracted engine version from crash dump: %s"), InCrashDumpFilename);
					UE_LOG(LogCrashDebugHelper, Display, TEXT("\tEngine Version = %d"), OutCrashDebugInfo.EngineVersion);
					UE_LOG(LogCrashDebugHelper, Display, TEXT("\tPlatform       = %s"), *(OutCrashDebugInfo.PlatformName));

					// Obsolete?
					//OutCrashDebugInfo.SourceControlLabel = GetLabelFromEngineVersion(OutCrashDebugInfo.EngineVersion);
					if (OutCrashDebugInfo.SourceControlLabel.Len() > 0)
					{
						
						UE_LOG(LogCrashDebugHelper, Display, TEXT("ParseCrashDump_Windows: Retrieve changelist label: %s"), *(OutCrashDebugInfo.SourceControlLabel));
						bResult = true;
					}
					else
					{
						UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseCrashDump_Windows: Failed to retrieve changelist label for engine version: %d"), OutCrashDebugInfo.EngineVersion);
					}
				}
				else
				{
					UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseCrashDump_Windows: Failed to extract engine version from crash dump: %s"), InCrashDumpFilename);
				}
			}
			else
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseCrashDump_Windows: Invalid windows crash dump: %s"), InCrashDumpFilename);
			}

			CloseHandle(FileHandle);
		}
		else
		{
			UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseCrashDump_Windows: Failed to read header from %s"), InCrashDumpFilename);
		}
	}
	else
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseCrashDump_Windows: Failed to open crash dump file: %s"), InCrashDumpFilename);
	}
	return bResult;
}


bool FCrashDebugHelperWindows::ParseCrashDump(const FString& InCrashDumpName, FCrashDebugInfo& OutCrashDebugInfo)
{
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("ParseCrashDump: CrashDebugHelper not initialized"));
		return false;
	}

	if (ParseCrashDump_Windows(*InCrashDumpName, OutCrashDebugInfo) == true)
	{
		return true;
	}

	return false;
}

bool FCrashDebugHelperWindows::SyncAndDebugCrashDump(const FString& InCrashDumpName)
{
	if (bInitialized == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncAndDebugCrashDump: CrashDebugHelper not initialized"));
		return false;
	}

	FCrashDebugInfo CrashDebugInfo;
	
	// Parse the crash dump if it hasn't already been...
	if (ParseCrashDump(InCrashDumpName, CrashDebugInfo) == false)
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncAndDebugCrashDump: Failed to parse crash dump %s"), *InCrashDumpName);
		return false;
	}

	// Now sync the required pdb files
	if (SyncRequiredFilesForDebuggingFromLabel(CrashDebugInfo.SourceControlLabel, CrashDebugInfo.PlatformName) == true)
	{
		// Launch the debugger
		if (LaunchDebugger(InCrashDumpName, CrashDebugInfo) == true)
		{
			return true;
		}
		else
		{
			UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncAndDebugCrashDump: Failed to launch debugger for crash dump %s"), *InCrashDumpName);
		}
	}
	else
	{
		UE_LOG(LogCrashDebugHelper, Warning, TEXT("SyncAndDebugCrashDump: Failed to sync required files for debugging crash dump %s"), *InCrashDumpName);
	}

	return false;
}

#include "HideWindowsPlatformTypes.h"
