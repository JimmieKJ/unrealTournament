// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

struct FPDBCacheEntry;
typedef TSharedRef<FPDBCacheEntry> FPDBCacheEntryRef;
typedef TSharedPtr<FPDBCacheEntry> FPDBCacheEntryPtr;

enum EProcessorArchitecture
{
	PA_UNKNOWN,
	PA_ARM,
	PA_X86,
	PA_X64,
};

/** 
 * Details of a module from a crash dump
 */
class FCrashModuleInfo
{
public:
	FString Report;

	FString Name;
	FString Extension;
	uint64 BaseOfImage;
	uint32 SizeOfImage;
	uint16 Major;
	uint16 Minor;
	uint16 Patch;
	uint16 Revision;

	FCrashModuleInfo()
		: BaseOfImage( 0 )
		, SizeOfImage( 0 )
		, Major( 0 )
		, Minor( 0 )
		, Patch( 0 )
		, Revision( 0 )
	{
	}
};

/** 
 * Details about a thread from a crash dump
 */
class FCrashThreadInfo
{
public:
	FString Report;

	uint32 ThreadId;
	uint32 SuspendCount;

	TArray<uint64> CallStack;

	FCrashThreadInfo()
		: ThreadId( 0 )
		, SuspendCount( 0 )
	{
	}

	~FCrashThreadInfo()
	{
	}
};

/** 
 * Details about the exception in the crash dump
 */
class FCrashExceptionInfo
{
public:
	FString Report;

	uint32 ProcessId;
	uint32 ThreadId;
	uint32 Code;
	FString ExceptionString;

	TArray<FString> CallStackString;

	FCrashExceptionInfo()
		: ProcessId( 0 )
		, ThreadId( 0 )
		, Code( 0 )
	{
	}

	~FCrashExceptionInfo()
	{
	}
};

/** 
 * Details about the system the crash dump occurred on
 */
class FCrashSystemInfo
{
public:
	FString Report;

	EProcessorArchitecture ProcessorArchitecture;
	uint32 ProcessorCount;

	uint16 OSMajor;
	uint16 OSMinor;
	uint16 OSBuild;
	uint16 OSRevision;

	FCrashSystemInfo()
		: ProcessorArchitecture( PA_UNKNOWN )
		, ProcessorCount( 0 )
		, OSMajor( 0 )
		, OSMinor( 0 )
		, OSBuild( 0 )
		, OSRevision( 0 )
	{
	}
};

/** A platform independent representation of a crash */
class CRASHDEBUGHELPER_API FCrashInfo
{
public:
	enum
	{
		/** An invalid changelist, something went wrong. */
		INVALID_CHANGELIST = -1,
	};

	/** Report log. */
	FString Report;

	/** The depot name, indicate where the executables and symbols are stored. */
	FString DepotName;

	/** Product version, based on FEngineVersion. */
	FString ProductVersion;

	/** CL built from. */
	int32 ChangelistBuiltFrom;

	/** The label the describes the executables and symbols. */
	FString LabelName;

	/** The network path where the executables are stored. */
	FString NetworkPath;

	FString SourceFile;
	uint64 SourceLineNumber;
	TArray<FString> SourceContext;

	/** Only modules names, retrieved from the minidump file. */
	TArray<FString> ModuleNames;

	FCrashSystemInfo SystemInfo;
	FCrashExceptionInfo Exception;
	TArray<FCrashThreadInfo> Threads;
	TArray<FCrashModuleInfo> Modules;

	/** Shared pointer to the PDB Cache entry, if valid contains all information about synced PDBs. */
	FPDBCacheEntryPtr PDBCacheEntry;

	FCrashInfo()
		: ChangelistBuiltFrom( INVALID_CHANGELIST )
		, SourceLineNumber( 0 )
	{
	}

	~FCrashInfo()
	{
	}

	/** 
	 * Generate a report for the crash in the requested path
	 */
	void GenerateReport( const FString& DiagnosticsPath );

	/** 
	 * Handle logging
	 */
	void Log( FString Line );

private:
	const TCHAR* GetProcessorArchitecture( EProcessorArchitecture PA );
	int64 StringSize( const ANSICHAR* Line );
	void WriteLine( FArchive* ReportFile, const ANSICHAR* Line = NULL );
};

/** Helper structure for tracking crash debug information */
struct CRASHDEBUGHELPER_API FCrashDebugInfo
{
	/** The name of the crash dump file */
	FString CrashDumpName;
	/** The engine version of the crash dump build */
	int32 EngineVersion;
	/** The platform of the crash dump build */
	FString PlatformName;
	/** The source control label of the crash dump build */
	FString SourceControlLabel;
};

/** Helper struct that holds various information about one PDB Cache entry. */
struct FPDBCacheEntry
{
	/** Default constructor. */
	FPDBCacheEntry( const FDateTime InLastAccessTime )
		: LastAccessTime( InLastAccessTime )
		, SizeGB( 0 )
	{}

	/** Initialization constructor. */
	FPDBCacheEntry( const TArray<FString>& InFiles, const FString& InDirectory, const FDateTime InLastAccessTime, const int32 InSizeGB )
		: Files( InFiles )
		, Directory( InDirectory )
		, LastAccessTime( InLastAccessTime )
		, SizeGB( InSizeGB )
	{}

	void SetLastAccessTimeToNow()
	{
		LastAccessTime = FDateTime::UtcNow();
	}

	/** Paths to files associated with this PDB Cache entry. */
	TArray<FString> Files;

	/** The path associated with this PDB Cache entry. */
	FString Directory;
	
	/** Last access time, changed every time this PDB cache entry is used. */
	FDateTime LastAccessTime;

	/** Size of the cache entry, in GBs. Rounded-up. */
	const int32 SizeGB;

	/**
	 * Serializer.
	 */
	friend FArchive& operator<<(FArchive& Ar, FPDBCacheEntry& Entry)
	{
		return Ar << Entry.Files << (FString&)Entry.Directory << (int32&)Entry.SizeGB;
	}
};

struct FPDBCacheEntryByAccessTime
{
	FORCEINLINE bool operator()( const FPDBCacheEntryRef& A, const FPDBCacheEntryRef& B ) const
	{
		return A->LastAccessTime.GetTicks() < B->LastAccessTime.GetTicks();
	}
};

/** Implements PDB cache. */
struct FPDBCache
{
protected:

	// Defaults for the PDB cache.
	enum
	{
		/** Size of the PDB cache, in GBs. */
		PDB_CACHE_SIZE_GB = 128,
		MIN_FREESPACE_GB = 64,
		/** Age of file when it should be deleted from the PDB cache. */
		DAYS_TO_DELETE_UNUSED_FILES = 14,

		/**
		*	Number of iterations inside the CleanPDBCache method.
		*	Mostly to verify that MinDiskFreeSpaceGB requirement is met.
		*/
		CLEAN_PDBCACHE_NUM_ITERATIONS = 2,

		/** Number of bytes per one gigabyte. */
		NUM_BYTES_PER_GB = 1024 * 1024 * 1024
	};

	/** Dummy file used to read/set the file timestamp. */
	static const TCHAR* PDBTimeStampFileNoMeta;

	/** Data file used to read/set the file timestamp, contains all metadata. */
	static const TCHAR* PDBTimeStampFile;

	/** Map of the PDB Cache entries. */
	TMap<FString, FPDBCacheEntryRef> PDBCacheEntries;

	/** Path to the folder where the PDB cache is stored. */
	FString PDBCachePath;

	/** Age of file when it should be deleted from the PDB cache. */
	int32 DaysToDeleteUnusedFilesFromPDBCache;

	/** Size of the PDB cache, in GBs. */
	int32 PDBCacheSizeGB;

	/**
	*	Minimum disk free space that should be available on the disk where the PDB cache is stored, in GBs.
	*	Minidump diagnostics runs usually on the same drive as the crash reports drive, so we need to leave some space for the crash receiver.
	*	If there is not enough disk free space, we will run the clean-up process.
	*/
	int32 MinDiskFreeSpaceGB;

	/** Whether to use the PDB cache. */
	bool bUsePDBCache;

public:
	/** Default constructor. */
	FPDBCache()
		: DaysToDeleteUnusedFilesFromPDBCache( DAYS_TO_DELETE_UNUSED_FILES )
		, PDBCacheSizeGB( PDB_CACHE_SIZE_GB )
		, MinDiskFreeSpaceGB( MIN_FREESPACE_GB )
		, bUsePDBCache( false )
	{}

	/** Basic initialization, reading config etc.. */
	void Init();

	/**
	 * @return whether to use the PDB cache.
	 */
	bool UsePDBCache() const
	{
		return bUsePDBCache;
	}
	
	/**
	 * @return true, if the PDB Cache contains the specified label.
	 */
	bool ContainsPDBCacheEntry( const FString& PathOrLabel ) const
	{
		return PDBCacheEntries.Contains( EscapePath( PathOrLabel ) );
	}

	/**
	 *	Touches a PDB Cache entry by updating the timestamp.
	 */
	void TouchPDBCacheEntry( const FString& Directory );

	/**
	 * @return a PDB Cache entry for the specified label and touches it at the same time
	 */
	FPDBCacheEntryRef FindAndTouchPDBCacheEntry( const FString& PathOrLabel );

	/**
	 *	Creates a new PDB Cache entry, initializes it and adds to the database.
	 */
	FPDBCacheEntryRef CreateAndAddPDBCacheEntry( const FString& OriginalLabelName, const FString& DepotRoot, const FString& DepotName, const TArray<FString>& FilesToBeCached );

	/**
	 *	Creates a new PDB Cache entry, initializes it and adds to the database.
	 */
	FPDBCacheEntryRef CreateAndAddPDBCacheEntryMixed( const FString& ProductVersion, const TMap<FString,FString>& FilesToBeCached );

protected:
	/** Initializes the PDB Cache. */
	void InitializePDBCache();

	/**
	 * @return the size of the PDB cache entry, in GBs.
	 */
	int32 GetPDBCacheEntrySizeGB( const FString& PathOrLabel ) const
	{
		return PDBCacheEntries.FindChecked( PathOrLabel )->SizeGB;
	}

	/**
	 * @return the size of the PDB cache directory, in GBs.
	 */
	int32 GetPDBCacheSizeGB() const
	{
		int32 Result = 0;
		if( bUsePDBCache )
		{
			for( const auto& It : PDBCacheEntries )
			{
				Result += It.Value->SizeGB;
			}
		}
		return Result;
	}

	/**
	 * Cleans the PDB Cache.
	 *
	 * @param DaysToKeep - removes all PDB Cache entries that are older than this value
	 * @param NumberOfGBsToBeCleaned - if specifies, will try to remove as many PDB Cache entries as needed
	 * to free disk space specified by this value
	 *
	 */
	void CleanPDBCache( int32 DaysToKeep, int32 NumberOfGBsToBeCleaned = 0 );


	/**
	 *	Reads an existing PDB Cache entry.
	 */
	FPDBCacheEntryRef ReadPDBCacheEntry( const FString& Directory );

	/**
	 *	Sort PDB Cache entries by last access time.
	 */
	void SortPDBCache()
	{
		PDBCacheEntries.ValueSort( FPDBCacheEntryByAccessTime() );
	}

	/**
	 *	Removes a PDB Cache entry from the database.
	 *	Also removes all external files associated with this PDB Cache entry.
	 */
	void RemovePDBCacheEntry( const FString& Directory );

	/** Replaces all invalid chars with + for the specified name. */
	FString EscapePath( const FString& PathOrLabel ) const
	{
		// @see AutomationTool.CommandUtils.EscapePath 
		return PathOrLabel.Replace( TEXT( ":" ), TEXT( "" ) ).Replace( TEXT( "/" ), TEXT( "+" ) ).Replace( TEXT( "\\" ), TEXT( "+" ) ).Replace( TEXT( " " ), TEXT( "+" ) );
	}
};

/** The public interface for the crash dump handler singleton. */
class CRASHDEBUGHELPER_API ICrashDebugHelper
{
protected:
	/** The depot name that the handler is using to sync from */
	FString DepotName;

	/** Depot root, only used in conjunction with the PDB Cache. */
	FString DepotRoot;

	/** The local folder where symbol files should be stored */
	FString LocalSymbolStore;

	/**
	 *	Pattern to search in source control for the label.
	 *	This somehow works for older crashes, before 4.2 and for the newest one,
	 *	bur for the newest we also need to look for the executables on the network drive.
	 *	This may change in future.
	 */
	FString SourceControlBuildLabelPattern;
	
	/**
	 * Pattern to search in the network driver for the executables.
	 * Valid from 4.2 UE builds. (CL-2068994)
	 * This may change in future.
	 */
	FString ExecutablePathPattern;
	
	/** Indicates that the crash handler is ready to do work */
	bool bInitialized;

	/** Instance of the PDB Cache. */
	FPDBCache PDBCache;

public:
	/** A platform independent representation of a crash */
	FCrashInfo CrashInfo;
	
	/** Virtual destructor */
	virtual ~ICrashDebugHelper()
	{}

	/**
	 *	Initialize the helper
	 *
	 *	@return	bool		true if successful, false if not
	 */
	virtual bool Init();

	/** Get the depot name */
	const FString& GetDepotName() const
	{
		return DepotName;
	}

	/** Set the depot name */
	void SetDepotName( const FString& InDepotName )
	{
		DepotName = InDepotName;
	}

	/**
	 *	Parse the given crash dump, determining EngineVersion of the build that produced it - if possible. 
	 *
	 *	@param	InCrashDumpName		The crash dump file to process
	 *	@param	OutCrashDebugInfo	The crash dump info extracted from the file
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool ParseCrashDump(const FString& InCrashDumpName, FCrashDebugInfo& OutCrashDebugInfo)
	{
		return false;
	}

	/**
	 *	Parse the given crash dump, and generate a report. 
	 *
	 *	@param	InCrashDumpName		The crash dump file to process
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool CreateMinidumpDiagnosticReport( const FString& InCrashDumpName )
	{
		return false;
	}

	/**
	 *	Get the source control label for the given changelist number
	 *
	 *	@param	InChangelistNumber	The changelist number to retrieve the label for
	 *
	 *	@return	FString				The label containing that changelist number build, empty if not found
	 */
	virtual FString GetLabelFromChangelistNumber(int32 InChangelistNumber)
	{
		return RetrieveBuildLabel(InChangelistNumber);
	}

	/**
	 *	Sync the required files from source control for debugging
	 *
	 *	@param	InLabel			The label to sync files from
	 *	@param	InPlatform		The platform to sync files for
	 *
	 *	@return bool			true if successful, false if not
	 */
	virtual bool SyncRequiredFilesForDebuggingFromLabel(const FString& InLabel, const FString& InPlatform);

	/**
	 *	Sync the required files from source control for debugging
	 *	NOTE: Currently this will only handle changelists of 'valid' builds from the build machine
	 *			Ie the changelist must map to a build label.
	 *
	 *	@param	InChangelist	The changelist to sync files from
	 *	@param	InPlatform		The platform to sync files for
	 *
	 *	@return bool			true if successful, false if not
	 */
	virtual bool SyncRequiredFilesForDebuggingFromChangelist(int32 InChangelistNumber, const FString& InPlatform);

	/**
	 *	Sync required files and launch the debugger for the crash dump.
	 *	Will call parse crash dump if it has not been called already.
	 *
	 *	@params	InCrashDumpName		The name of the crash dump file to debug
	 *
	 *	@return	bool				true if successful, false if not
	 */
	virtual bool SyncAndDebugCrashDump(const FString& InCrashDumpName)
	{
		return false;
	}

	/**
	 *	Sync the binaries and associated pdbs to the requested label.
	 *
	 *	@return	bool		true if successful, false if not
	 */
	virtual bool SyncModules();

	/**
	 *	Sync a single source file to the requested label.
	 *
	 *	@return	bool		true if successful, false if not
	 */
	virtual bool SyncSourceFile();

	/**
	 *	Extract lines from a source file, and add to the crash report.
	 */
	virtual void AddSourceToReport();

	/**
	 *	Extract annotated lines from a source file stored in Perforce, and add to the crash report.
	 */
	virtual bool AddAnnotatedSourceToReport();

protected:
	/**
	 *	Retrieve the build label for the given changelist number.
	 *
	 *	@param	InChangelistNumber	The changelist number to retrieve the label for
	 *	@return	FString				The label if successful, empty if it wasn't found or another error
	 */
	FString RetrieveBuildLabel(int32 InChangelistNumber);

	/**
	 *	Retrieve the build label and the network path for the given changelist number.
	 *
	 *	@param	InChangelistNumber	The changelist number to retrieve the label for
	 */
	void RetrieveBuildLabelAndNetworkPath( int32 InChangelistNumber );

	bool ReadSourceFile( const TCHAR* InFilename, TArray<FString>& OutStrings );

public:
	bool InitSourceControl(bool bShowLogin);
	void ShutdownSourceControl();
};
