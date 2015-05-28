// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Forward declarations
class UClientUnitTest;

/**
 * A class for enabling verbose log message categories, within a particular code scope (disabled when going out of scope).
 * NOTE: If you are logging any kind of net-related log messages, specify a unit test (even if you aren't doing remote logging)
 *
 * Also supports remote (server) logging, for net functions executed within the current code scope
 * (causes net packets to be flushed both upon entering the current scope, and when exiting it - required for correct log timing).
 *
 * NOTE: If you are trying to catch remote log messages deep within the internal game netcode, 
 *			then this class may not be appropriate, as remote logging passes through the netcode (YMMV)
 */
class FScopedLog
{
protected:
	FScopedLog()
	{
	}

public:
	/**
	 * Constructor used for setting up the type of logging that is done.
	 *
	 * @param InLogCategories	The list of log categories to be enabled
	 * @param InUnitTest		When tracking netcode-related logs, or doing remote logging, specify the client unit test here
	 * @param bInRemoteLogging	Whether or not to enable logging on the remote server
	 */
	FScopedLog(const TArray<FString>& InLogCategories, UClientUnitTest* InUnitTest=NULL, bool bInRemoteLogging=false)
	{
		InternalConstruct(InLogCategories, InUnitTest, bInRemoteLogging);
	}

	// As above, but for a single log category
	FScopedLog(const FString InLogCategory, UClientUnitTest* InUnitTest=NULL, bool bInRemoteLogging=false)
	{
		TArray<FString> TempLogCategories;
		TempLogCategories.Add(InLogCategory);

		InternalConstruct(TempLogCategories, InUnitTest, bInRemoteLogging);
	}

protected:
	void InternalConstruct(const TArray<FString>& InLogCategories, UClientUnitTest* InUnitTest, bool bInRemoteLogging);

public:
	~FScopedLog();


protected:
	/** The list of unsuppressed log messages */
	TArray<FString> LogCategories;

	/** Stores a reference to the unit test doing the logging, if specified */
	UClientUnitTest* UnitTest;

	/** Whether or not this is also controlling remote logging as well */
	bool bRemoteLogging;
};

/**
 * Version of the above, for scoped logging of all netcode-related logs
 */
class FScopedLogNet : public FScopedLog
{
public:
	FScopedLogNet(UClientUnitTest* InUnitTest, bool bInRemoteLogging=false)
	{
		TArray<FString> TempLogCategories;

		// @todo JohnB: See if there are any other good net categories to add here
		TempLogCategories.Add(TEXT("LogNet"));
		TempLogCategories.Add(TEXT("LogNetTraffic"));
		TempLogCategories.Add(TEXT("LogNetSerialization"));
		TempLogCategories.Add(TEXT("LogNetPackageMap"));
		TempLogCategories.Add(TEXT("LogNetPlayerMovement"));
		TempLogCategories.Add(TEXT("LogNetDormancy"));
		TempLogCategories.Add(TEXT("LogProperty"));

		InternalConstruct(TempLogCategories, InUnitTest, bInRemoteLogging);
	}
};


#if !UE_BUILD_SHIPPING
/**
 * A class for hooking and logging all ProcessEvent calls, within a particular code scope, similar to the above code
 */
class FScopedProcessEventLog
{
public:
	FScopedProcessEventLog();

	~FScopedProcessEventLog();

	bool ProcessEventHook(AActor* Actor, UFunction* Function, void* Parameters);

protected:
	/** If a 'Actor::ProcessEventDelegate' value was already set, this caches it so it can be transparently hooked and restored later */
	FOnProcessEvent		OrigEventHook;
};
#endif


/**
 * General debug functions
 */
struct NUTDebug
{
	// @todo JohnB: Might be useful to find a place for the hexdump functions within the base engine code itself;
	//				they are very useful

	// NOTE: If outputting hex dump info to the unit test log windows, make use of bMonospace, to retain the hex formatting

	/**
	 * Quick conversion of a string to a hex-dumpable byte array
	 *
	 * @param InString		The string to convert
	 * @param OutBytes		The byte array to output to
	 */
	static inline void StringToBytes(const FString& InString, TArray<uint8>& OutBytes)
	{
		TArray<TCHAR> InStrCharArray = InString.GetCharArray();

		for (int i=0; i<InString.Len(); i++)
		{
			uint8 CharBytes[sizeof(TCHAR)];
			FMemory::Memcpy(&CharBytes[0], &InStrCharArray[i], sizeof(TCHAR));

			for (int CharIdx=0; CharIdx<sizeof(TCHAR); CharIdx++)
			{
				OutBytes.Add(CharBytes[CharIdx]);
			}
		}
	}

	/**
	 * Takes an array of bytes, and generates a hex dump string out of them, optionally including
	 * an ASCII dump and dumping byte offsets also (intended for debugging in the log window)
	 *
	 * @param InBytes		The array of bytes to be dumped
	 * @param bDumpASCII	Whether or not to dump ASCII along with the hex
	 * @param bDumpOffset	Whether or not to dump offset margins for hex rows/columns
	 * @return				Returns the hex dump, including line terminators
	 */
	static FString HexDump(const TArray<uint8>& InBytes, bool bDumpASCII=true, bool bDumpOffset=true);

	/**
	 * Version of the above hex-dump function, which takes a byte pointer and length as input
	 */
	static inline FString HexDump(const uint8* InBytes, const uint32 InBytesLen, bool bDumpASCII=true, bool bDumpOffset=true)
	{
		TArray<uint8> InBytesArray;

		InBytesArray.AddUninitialized(InBytesLen);
		FMemory::Memcpy(InBytesArray.GetData(), InBytes, InBytesLen);

		return HexDump(InBytesArray, bDumpASCII, bDumpOffset);
	}

	/**
	 * Version of the above hex-dump function, which takes a string as input
	 */
	static inline FString HexDump(const FString& InString, bool bDumpASCII=true, bool bDumpOffset=false)
	{
		TArray<uint8> InStrBytes;

		StringToBytes(InString, InStrBytes);

		return HexDump(InStrBytes, bDumpASCII, bDumpOffset);
	}

	/**
	 * Version of the above hex-dump function, which dumps in a format more friendly/readable
	 * in log text files
	 */
	static inline void LogHexDump(const TArray<uint8>& InBytes, bool bDumpASCII=true, bool bDumpOffset=false, FOutputDevice* OutLog=GLog)
	{
		FString HexDumpStr = HexDump(InBytes, bDumpASCII, bDumpOffset);
		TArray<FString> LogLines;

#if TARGET_UE4_CL < CL_STRINGPARSEARRAY
		HexDumpStr.ParseIntoArray(&LogLines, LINE_TERMINATOR, false);
#else
		HexDumpStr.ParseIntoArray(LogLines, LINE_TERMINATOR, false);
#endif

		for (int32 i=0; i<LogLines.Num(); i++)
		{
			// NOTE: It's important to pass it in as a parameter, otherwise there is a crash if the line contains '%s'
			OutLog->Logf(TEXT("%s"), *LogLines[i]);
		}
	}

	/**
	 * Version of the above hex-dump logging function, which takes a byte pointer and length as input
	 */
	static inline void LogHexDump(const uint8* InBytes, const uint32 InBytesLen, bool bDumpASCII=true, bool bDumpOffset=false,
							FOutputDevice* OutLog=GLog)
	{
		TArray<uint8> InBytesArray;

		InBytesArray.AddUninitialized(InBytesLen);
		FMemory::Memcpy(InBytesArray.GetData(), InBytes, InBytesLen);

		LogHexDump(InBytesArray, bDumpASCII, bDumpOffset, OutLog);
	}

	/**
	 * Version of the above, which takes a string as input
	 */
	static inline void LogHexDump(const FString& InString, bool bDumpASCII=true, bool bDumpOffset=false, FOutputDevice* OutLog=GLog)
	{
		TArray<uint8> InStrBytes;

		StringToBytes(InString, InStrBytes);
		LogHexDump(InStrBytes, bDumpASCII, bDumpOffset, OutLog);
	}
};

