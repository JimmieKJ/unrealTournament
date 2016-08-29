// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IntPoint.h"
#include "Map.h"
#include "ThreadingBase.h"
#include "Containers/ArrayView.h"

/**
 * Exec handler that registers itself and is being routed via StaticExec.
 * Note: Not intended for use with UObjects!
 */
class CORE_API FSelfRegisteringExec : public FExec
{
public:
	/** Constructor, registering this instance. */
	FSelfRegisteringExec();
	/** Destructor, unregistering this instance. */
	virtual ~FSelfRegisteringExec();

	/** Routes a command to the self-registered execs. */
	static bool StaticExec( UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar );

private:
	
	/** Array of registered exec's routed via StaticExec. */
	static TArray<FSelfRegisteringExec*>& GetRegisteredExecs();
};

/** Registers a static Exec function using FSelfRegisteringExec. */
class CORE_API FStaticSelfRegisteringExec : public FSelfRegisteringExec
{
public:

	/** Initialization constructor. */
	FStaticSelfRegisteringExec(bool (*InStaticExecFunc)(UWorld* Inworld, const TCHAR* Cmd,FOutputDevice& Ar));

	//~ Begin Exec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar );
	//~ End Exec Interface

private:

	bool (*StaticExecFunc)(UWorld* Inworld, const TCHAR* Cmd,FOutputDevice& Ar);
};

// Interface for returning a context string.
class FContextSupplier
{
public:
	virtual FString GetContext()=0;
};


struct CORE_API FMaintenance
{
	/** deletes log files older than a number of days specified in the Engine ini file */
	static void DeleteOldLogs();
};

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

struct CORE_API FCommandLine
{
	/** maximum size of the command line */
	static const uint32 MaxCommandLineSize = 16384;

	/** 
	 * Returns an edited version of the executable's command line with the game name and certain other parameters removed. 
	 */
	static const TCHAR* Get();
	
	/**
	 * Returns an edited version of the executable's command line. 
	 */
	static const TCHAR* GetForLogging();

	/**
	 * Returns the command line originally passed to the executable.
	 */
	static const TCHAR* GetOriginal();

	/**
	 * Returns an edited version of the command line originally passed to the executable.
	 */
	static const TCHAR* GetOriginalForLogging();

	/** 
	 * Checks if the command line has been initialized. 
	 */
	static bool IsInitialized();

	/**
	 * Sets CmdLine to the string given
	 */
	static bool Set(const TCHAR* NewCommandLine);

	/**
	 * Appends the passed string to the command line as it is (no space is added).
	 * @param AppendString String that should be appended to the commandline.
	 */
	static void Append(const TCHAR* AppendString);

	/**
	 * Adds a new parameter to subprocess command line. If Param
	 * does not start with a space, one is added.
	 *
	 * @param Param Command line param string.
	 */
	static void AddToSubprocessCommandline( const TCHAR* Param );

	/** 
	 * Returns the subprocess command line string 
	 */
	static const FString& GetSubprocessCommandline();

	/** 
	* Removes the executable name from the passed CmdLine, denoted by parentheses.
	* Returns the CmdLine string without the executable name.
	*/
	static const TCHAR* RemoveExeName(const TCHAR* CmdLine);

	/**
	 * Parses a string into tokens, separating switches (beginning with - or /) from
	 * other parameters
	 *
	 * @param	CmdLine		the string to parse
	 * @param	Tokens		[out] filled with all parameters found in the string
	 * @param	Switches	[out] filled with all switches found in the string
	 */
	static void Parse(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches);
private:
#if WANTS_COMMANDLINE_WHITELIST
	/** Filters both the original and current command line list for approved only args */
	static void WhitelistCommandLines();
	/** Filters any command line args that aren't on the approved list */
	static TArray<FString> FilterCommandLine(TCHAR* CommandLine);
	/** Filters any command line args that are on the to-strip list */
	static TArray<FString> FilterCommandLineForLogging(TCHAR* CommandLine);
	/** Rebuilds the command line using the filtered args */
	static void BuildWhitelistCommandLine(TCHAR* CommandLine, uint32 Length, const TArray<FString>& FilteredArgs);
	static TArray<FString> ApprovedArgs;
	static TArray<FString> FilterArgsForLogging;
#else
#define WhitelistCommandLines()
#endif

	/** Flag to check if the commandline has been initialized or not. */
	static bool bIsInitialized;
	/** character buffer containing the command line */
	static TCHAR CmdLine[MaxCommandLineSize];
	/** character buffer containing the original command line */
	static TCHAR OriginalCmdLine[MaxCommandLineSize];
	/** character buffer containing the command line filtered for logging purposes */
	static TCHAR LoggingCmdLine[MaxCommandLineSize];
	/** character buffer containing the original command line filtered for logging purposes */
	static TCHAR LoggingOriginalCmdLine[MaxCommandLineSize];
	/** subprocess command line */
	static FString SubprocessCommandLine;
};

/*-----------------------------------------------------------------------------
	FFileHelper
-----------------------------------------------------------------------------*/
struct CORE_API FFileHelper
{
	struct EHashOptions
	{
		enum Type
		{
			/** Enable the async task for verifying the hash for the file being loaded */
			EnableVerify		=1<<0,
			/** A missing hash entry should trigger an error */
			ErrorMissingHash	=1<<1
		};
	};

	struct EEncodingOptions
	{
		enum Type
		{
			AutoDetect,
			ForceAnsi,
			ForceUnicode,
			ForceUTF8,
			ForceUTF8WithoutBOM
		};
	};

	/**
	 * Load a text file to an FString.
	 * Supports all combination of ANSI/Unicode files and platforms.
	*/
	static void BufferToString( FString& Result, const uint8* Buffer, int32 Size );

	/**
	 * Load a binary file to a dynamic array.
	*/
	static bool LoadFileToArray( TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags = 0 );

	/**
	 * Load a text file to an FString.
	 * Supports all combination of ANSI/Unicode files and platforms.
	 * @param Result string representation of the loaded file
	 * @param Filename name of the file to load
	 * @param VerifyFlags flags controlling the hash verification behavior ( see EHashOptions )
	 */
	static bool LoadFileToString( FString& Result, const TCHAR* Filename, uint32 VerifyFlags=0 );

	/**
	 * Save a binary array to a file.
	 */
	static bool SaveArrayToFile(TArrayView<const uint8> Array, const TCHAR* Filename, IFileManager* FileManager=&IFileManager::Get(), uint32 WriteFlags = 0);

	/**
	 * Write the FString to a file.
	 * Supports all combination of ANSI/Unicode files and platforms.
	 */
	static bool SaveStringToFile( const FString& String, const TCHAR* Filename, EEncodingOptions::Type EncodingOptions=EEncodingOptions::AutoDetect, IFileManager* FileManager=&IFileManager::Get(), uint32 WriteFlags = 0 );

	/**
	 * Saves a 24/32Bit BMP file to disk
	 * 
	 * @param Pattern filename with path, must not be 0, if with "bmp" extension (e.g. "out.bmp") the filename stays like this, if without (e.g. "out") automatic index numbers are addended (e.g. "out00002.bmp")
	 * @param DataWidth - Width of the bitmap supplied in Data >0
	 * @param DataHeight - Height of the bitmap supplied in Data >0
	 * @param Data must not be 0
	 * @param SubRectangle optional, specifies a sub-rectangle of the source image to save out. If NULL, the whole bitmap is saved
	 * @param FileManager must not be 0
	 * @param OutFilename optional, if specified filename will be output
	 * @param bInWriteAlpha opional, specifies whether to write out the alpha channel. Will force BMP V4 format.
	 *
	 * @return true if success
	 */
	static bool CreateBitmap( const TCHAR* Pattern, int32 DataWidth, int32 DataHeight, const struct FColor* Data, struct FIntRect* SubRectangle = NULL, IFileManager* FileManager = &IFileManager::Get(), FString* OutFilename = NULL, bool bInWriteAlpha = false );

	/**
	 * Generates the next unique bitmap filename with a specified extension
	 *
	 * @param Pattern		Filename with path, but without extension.
	 * @oaran Extension		File extension to be appended
	 * @param OutFilename	Reference to an FString where the newly generated filename will be placed
	 * @param FileManager	Reference to a IFileManager (or the global instance by default)
	 *
	 * @return true if success
	 */
	static bool GenerateNextBitmapFilename(const FString& Pattern, const FString& Extension, FString& OutFilename, IFileManager* FileManager = &IFileManager::Get());
	
	/**
	 *	Load the given ANSI text file to an array of strings - one FString per line of the file.
	 *	Intended for use in simple text parsing actions
	 *
	 *	@param	InFilename			The text file to read, full path
	 *	@param	InFileManager		The filemanager to use - NULL will use &IFileManager::Get()
	 *	@param	OutStrings			The array of FStrings to fill in
	 *
	 *	@return	bool				true if successful, false if not
	 */
	static bool LoadANSITextFileToStrings(const TCHAR* InFilename, IFileManager* InFileManager, TArray<FString>& OutStrings);
};

/*-----------------------------------------------------------------------------
	Module singletons.
-----------------------------------------------------------------------------*/

/** Return the DDC interface, if it is available, otherwise return NULL **/
CORE_API class FDerivedDataCacheInterface* GetDerivedDataCache();

/** Return the DDC interface, fatal error if it is not available. **/
CORE_API class FDerivedDataCacheInterface& GetDerivedDataCacheRef();

/** Return the Target Platform Manager interface, if it is available, otherwise return NULL **/
CORE_API class ITargetPlatformManagerModule* GetTargetPlatformManager();

/** Return the Target Platform Manager interface, fatal error if it is not available. **/
CORE_API class ITargetPlatformManagerModule& GetTargetPlatformManagerRef();

/*-----------------------------------------------------------------------------
	Runtime.
-----------------------------------------------------------------------------*/

/**
 * Check to see if this executable is running as dedicated server
 * Editor can run as dedicated with -server
 */
FORCEINLINE bool IsRunningDedicatedServer()
{
	if (FPlatformProperties::IsServerOnly())
	{
		return true;
	}

	if (FPlatformProperties::IsGameOnly())
	{
		return false;
	}

#if UE_EDITOR
	extern CORE_API int32 StaticDedicatedServerCheck();
	return (StaticDedicatedServerCheck() == 1);
#else
	return false;
#endif
}

/**
 * Check to see if this executable is running as "the game"
 * - contains all net code (WITH_SERVER_CODE=1)
 * Editor can run as a game with -game
 */
FORCEINLINE bool IsRunningGame()
{
	if (FPlatformProperties::IsGameOnly())
	{
		return true;
	}

	if (FPlatformProperties::IsServerOnly())
	{
		return false;
	}

#if UE_EDITOR
	extern CORE_API int32 StaticGameCheck();
	return (StaticGameCheck() == 1);
#else
	return false;
#endif
}

/**
 * Check to see if this executable is running as "the client"
 * - removes all net code (WITH_SERVER_CODE=0)
 * Editor can run as a game with -clientonly
 */
FORCEINLINE bool IsRunningClientOnly()
{
	if (FPlatformProperties::IsClientOnly())
	{
		return true;
	}

#if UE_EDITOR
	extern CORE_API int32 StaticClientOnlyCheck();
	return (StaticClientOnlyCheck() == 1);
#else
	return false;
#endif
}

/**
 * Helper for obtaining the default Url configuration
 */
struct CORE_API FUrlConfig
{
	FString DefaultProtocol;
	FString DefaultName;
	FString DefaultHost;
	FString DefaultPortal;
	FString DefaultSaveExt;
	int32 DefaultPort;

	/**
	 * Initialize with defaults from ini
	 */
	void Init();

	/**
	 * Reset state
	 */
	void Reset();
};

bool CORE_API StringHasBadDashes(const TCHAR* Str);

/** Helper function to generate a set of windowed resolutions which are convenient for the current primary display size */
void CORE_API GenerateConvenientWindowedResolutions(const struct FDisplayMetrics& InDisplayMetrics, TArray<FIntPoint>& OutResolutions);

/**
 * Helper for script stack traces
 */
struct FScriptTraceStackNode
{
	FName	Scope;
	FName	FunctionName;

	FScriptTraceStackNode(FName InScope, FName InFunctionName)
		: Scope(InScope)
		, FunctionName(InFunctionName)
	{
	}

	FString GetStackDescription() const
	{
		return Scope.ToString() + TEXT(".") + FunctionName.ToString();
	}

private:
	FScriptTraceStackNode() {};
};

/** Helper structure for boolean values in config */
struct CORE_API FBoolConfigValueHelper
{
private:
	bool bValue;
public:
	FBoolConfigValueHelper(const TCHAR* Section, const TCHAR* Key, const FString& Filename = GEditorIni);

	operator bool() const
	{
		return bValue;
	}
};

#ifndef DO_BLUEPRINT_GUARD
	#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		#define DO_BLUEPRINT_GUARD 1
	#endif
#endif

#if DO_BLUEPRINT_GUARD
/** 
 * Helper struct for dealing with Blueprint exceptions 
 */
struct CORE_API FBlueprintExceptionTracker : TThreadSingleton<FBlueprintExceptionTracker>
{
	FBlueprintExceptionTracker()
		: Runaway(0)
		, Recurse(0)
		, bRanaway(false)
		, ScriptEntryTag(0)
	{}

	void ResetRunaway();

	static FBlueprintExceptionTracker& Get();
public:
	// map of currently displayed warnings in exception handler
	TMap<FName, int32> DisplayedWarningsMap;

	// runaway tracking
	int32 Runaway;
	int32 Recurse;
	bool bRanaway;

	// Script entry point tracking
	int32 ScriptEntryTag;

	// Stack names from the VM to be unrolled when we assert
	TArray<FScriptTraceStackNode> ScriptStack;
};

#endif // DO_BLUEPRINT_GUARD