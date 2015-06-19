// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

#include "AllowWindowsPlatformTypes.h"
	#include <DbgHelp.h>				
	#include <TlHelp32.h>		
	#include <psapi.h>
#include "HideWindowsPlatformTypes.h"

#include "ModuleManager.h"


/*-----------------------------------------------------------------------------
	Stack walking.
	@TODO To be removed
-----------------------------------------------------------------------------*/

/** Whether appInitStackWalking() has been called successfully or not. */
static bool GStackWalkingInitialized = false;
static bool GNeedToRefreshSymbols = false;

// NOTE: Make sure to enable Stack Frame pointers: bOmitFramePointers = false, or /Oy-
// If GStackWalkingInitialized is true, traces will work anyway but will be much slower.
#define USE_FAST_STACKTRACE 1

typedef bool  (WINAPI *TFEnumProcesses)( uint32* lpidProcess, uint32 cb, uint32* cbNeeded);
typedef bool  (WINAPI *TFEnumProcessModules)(HANDLE hProcess, HMODULE *lphModule, uint32 cb, LPDWORD lpcbNeeded);
typedef uint32 (WINAPI *TFGetModuleBaseName)(HANDLE hProcess, HMODULE hModule, LPSTR lpBaseName, uint32 nSize);
typedef uint32 (WINAPI *TFGetModuleFileNameEx)(HANDLE hProcess, HMODULE hModule, LPSTR lpFilename, uint32 nSize);
typedef bool  (WINAPI *TFGetModuleInformation)(HANDLE hProcess, HMODULE hModule, LPMODULEINFO lpmodinfo, uint32 cb);

static TFEnumProcesses			FEnumProcesses;
static TFEnumProcessModules		FEnumProcessModules;
static TFGetModuleBaseName		FGetModuleBaseName;
static TFGetModuleFileNameEx	FGetModuleFileNameEx;
static TFGetModuleInformation	FGetModuleInformation;

/**
 * Helper function performing the actual stack walk. This code relies on the symbols being loaded for best results
 * walking the stack albeit at a significant performance penalty.
 *
 * This helper function is designed to be called within a structured exception handler.
 *
 * @param	BackTrace			Array to write backtrace to
 * @param	MaxDepth			Maximum depth to walk - needs to be less than or equal to array size
 * @param	Context				Thread context information
 * @return	EXCEPTION_EXECUTE_HANDLER
 */
static int32 CaptureStackTraceHelper( uint64 *BackTrace, uint32 MaxDepth, CONTEXT* Context )
{
	STACKFRAME64		StackFrame64;
	HANDLE				ProcessHandle;
	HANDLE				ThreadHandle;
	unsigned long		LastError;
	bool				bStackWalkSucceeded	= true;
	uint32				CurrentDepth		= 0;
	uint32				MachineType			= IMAGE_FILE_MACHINE_I386;
	CONTEXT				ContextCopy = *Context;

#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__try
#endif
	{
		// Get context, process and thread information.
		ProcessHandle	= GetCurrentProcess();
		ThreadHandle	= GetCurrentThread();

		// Zero out stack frame.
		FMemory::Memzero( StackFrame64 );

		// Initialize the STACKFRAME structure.
		StackFrame64.AddrPC.Mode         = AddrModeFlat;
		StackFrame64.AddrStack.Mode      = AddrModeFlat;
		StackFrame64.AddrFrame.Mode      = AddrModeFlat;
#if PLATFORM_64BITS
		StackFrame64.AddrPC.Offset       = Context->Rip;
		StackFrame64.AddrStack.Offset    = Context->Rsp;
		StackFrame64.AddrFrame.Offset    = Context->Rbp;
		MachineType                      = IMAGE_FILE_MACHINE_AMD64;
#else	//PLATFORM_64BITS
		StackFrame64.AddrPC.Offset       = Context->Eip;
		StackFrame64.AddrStack.Offset    = Context->Esp;
		StackFrame64.AddrFrame.Offset    = Context->Ebp;
#endif	//PLATFORM_64BITS

		// Walk the stack one frame at a time.
		while( bStackWalkSucceeded && (CurrentDepth < MaxDepth) )
		{
			bStackWalkSucceeded = !!StackWalk64(  MachineType, 
												ProcessHandle, 
												ThreadHandle, 
												&StackFrame64,
												&ContextCopy,
												NULL,
												SymFunctionTableAccess64,
												SymGetModuleBase64,
												NULL );

			BackTrace[CurrentDepth++] = StackFrame64.AddrPC.Offset;

			if( !bStackWalkSucceeded  )
			{
				// StackWalk failed! give up.
				LastError = GetLastError( );
				break;
			}

			// Stop if the frame pointer or address is NULL.
			if( StackFrame64.AddrFrame.Offset == 0 || StackFrame64.AddrPC.Offset == 0 )
			{
				break;
			}
		}
	} 
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__except( EXCEPTION_EXECUTE_HANDLER )
	{
		// We need to catch any exceptions within this function so they don't get sent to 
		// the engine's error handler, hence causing an infinite loop.
		return EXCEPTION_EXECUTE_HANDLER;
	} 
#endif

	// NULL out remaining entries.
	for ( ; CurrentDepth<MaxDepth; CurrentDepth++ )
	{
		BackTrace[CurrentDepth] = 0;
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

PRAGMA_DISABLE_OPTIMIZATION // Work around "flow in or out of inline asm code suppresses global optimization" warning C4740.

#if USE_FAST_STACKTRACE
NTSYSAPI uint16 NTAPI RtlCaptureStackBackTrace(
	__in uint32 FramesToSkip,
	__in uint32 FramesToCapture,
	__out_ecount(FramesToCapture) PVOID *BackTrace,
	__out_opt PDWORD BackTraceHash
	);

/** Maximum callstack depth that is supported by the current OS. */
static ULONG GMaxCallstackDepth = 62;

/** Whether DetermineMaxCallstackDepth() has been called or not. */
static bool GMaxCallstackDepthInitialized = false;

/** Maximum callstack depth we support, no matter what OS we're running on. */
#define MAX_CALLSTACK_DEPTH 128

/** Checks the current OS version and sets up the GMaxCallstackDepth variable. */
void DetermineMaxCallstackDepth()
{
	// Check that we're running on Vista or newer (version 6.0+).
	if ( FWindowsPlatformMisc::VerifyWindowsVersion(6, 0) )
	{
		GMaxCallstackDepth = MAX_CALLSTACK_DEPTH;
	}
	else
	{
		GMaxCallstackDepth = FMath::Min<ULONG>(62,MAX_CALLSTACK_DEPTH);
	}
	GMaxCallstackDepthInitialized = true;
}

#endif

void FWindowsPlatformStackWalk::StackWalkAndDump( ANSICHAR* HumanReadableString, SIZE_T HumanReadableStringSize, int32 IgnoreCount, void* Context )
{
	InitStackWalking();
	FGenericPlatformStackWalk::StackWalkAndDump(HumanReadableString, HumanReadableStringSize, IgnoreCount, Context);
}
// @TODO yrx 2014-09-05 Switch to TArray<uint64,TFixedAllocator<100>>>
/**
 * Capture a stack backtrace and optionally use the passed in exception pointers.
 *
 * @param	BackTrace			[out] Pointer to array to take backtrace
 * @param	MaxDepth			Entries in BackTrace array
 * @param	Context				Optional thread context information
 */
void FWindowsPlatformStackWalk::CaptureStackBackTrace( uint64* BackTrace, uint32 MaxDepth, void* Context )
{
	// Make sure we have place to store the information before we go through the process of raising
	// an exception and handling it.
	if( BackTrace == NULL || MaxDepth == 0 )
	{
		return;
	}

	if( Context )
	{
		CaptureStackTraceHelper( BackTrace, MaxDepth, (CONTEXT*)Context );
	}
	else
	{
#if USE_FAST_STACKTRACE
		// NOTE: Make sure to enable Stack Frame pointers: bOmitFramePointers = false, or /Oy-
		// If GStackWalkingInitialized is true, traces will work anyway but will be much slower.
		if ( GStackWalkingInitialized )
		{
			CONTEXT HelperContext;
			RtlCaptureContext( &HelperContext );

			// Capture the back trace.
			CaptureStackTraceHelper( BackTrace, MaxDepth, &HelperContext );
		}
		else
		{
			if ( !GMaxCallstackDepthInitialized )
			{
				DetermineMaxCallstackDepth();
			}
			PVOID WinBackTrace[MAX_CALLSTACK_DEPTH];
			uint16 NumFrames = RtlCaptureStackBackTrace( 0, FMath::Min<ULONG>(GMaxCallstackDepth,MaxDepth), WinBackTrace, NULL );
			for ( uint16 FrameIndex=0; FrameIndex < NumFrames; ++FrameIndex )
			{
				BackTrace[ FrameIndex ] = (uint64) WinBackTrace[ FrameIndex ];
			}
			while ( NumFrames < MaxDepth )
			{
				BackTrace[ NumFrames++ ] = 0;
			}
		}
#elif PLATFORM_64BITS
		// Raise an exception so CaptureStackBackTraceHelper has access to context record.
		__try
		{
			RaiseException(	0,			// Application-defined exception code.
							0,			// Zero indicates continuable exception.
							0,			// Number of arguments in args array (ignored if args is NULL)
							NULL );		// Array of arguments
			}
		// Capture the back trace.
		__except( CaptureStackTraceHelper( BackTrace, MaxDepth, (GetExceptionInformation())->ContextRecord ) )
		{
		}
#elif 1
		// Use a bit of inline assembly to capture the information relevant to stack walking which is
		// basically EIP and EBP.
		CONTEXT HelperContext;
		memset( &HelperContext, 0, sizeof(CONTEXT) );
		HelperContext.ContextFlags = CONTEXT_FULL;

		// Use a fake function call to pop the return address and retrieve EIP.
		__asm
		{
			call FakeFunctionCall
		FakeFunctionCall: 
			pop eax
			mov HelperContext.Eip, eax
			mov HelperContext.Ebp, ebp
			mov HelperContext.Esp, esp
		}

		// Capture the back trace.
		CaptureStackTraceHelper( BackTrace, MaxDepth, &HelperContext );
#else
		CONTEXT HelperContext;
		// RtlCaptureContext relies on EBP being untouched so if the below crashes it is because frame pointer
		// omission is enabled. It is implied by /Ox or /O2 and needs to be manually disabled via /Oy-
		RtlCaptureContext( HelperContext );

		// Capture the back trace.
		CaptureStackTraceHelper( BackTrace, MaxDepth, &HelperContext );
#endif
	}
}

PRAGMA_ENABLE_OPTIMIZATION

void FWindowsPlatformStackWalk::ProgramCounterToSymbolInfo( uint64 ProgramCounter, FProgramCounterSymbolInfo& out_SymbolInfo )
{
	// Initialize stack walking as it loads up symbol information which we require.
	InitStackWalking();

	// Set the program counter.
	out_SymbolInfo.ProgramCounter = ProgramCounter;

	uint32 LastError = 0;
	HANDLE ProcessHandle = GetCurrentProcess();

	// Initialize symbol.
	ANSICHAR SymbolBuffer[sizeof( IMAGEHLP_SYMBOL64 ) + FProgramCounterSymbolInfo::MAX_NAME_LENGHT] = {0};
	IMAGEHLP_SYMBOL64* Symbol = (IMAGEHLP_SYMBOL64*)SymbolBuffer;
	Symbol->SizeOfStruct = sizeof(SymbolBuffer);
	Symbol->MaxNameLength = FProgramCounterSymbolInfo::MAX_NAME_LENGHT;

	// Get function name.
	if( SymGetSymFromAddr64( ProcessHandle, ProgramCounter, nullptr, Symbol ) )
	{
		// Skip any funky chars in the beginning of a function name.
		int32 Offset = 0;
		while( Symbol->Name[Offset] < 32 || Symbol->Name[Offset] > 127 )
		{
			Offset++;
		}

		// Write out function name.
		FCStringAnsi::Strncpy( out_SymbolInfo.FunctionName, Symbol->Name + Offset, FProgramCounterSymbolInfo::MAX_NAME_LENGHT ); 
		FCStringAnsi::Strncat( out_SymbolInfo.FunctionName, "()", FProgramCounterSymbolInfo::MAX_NAME_LENGHT );
	}
	else
	{
		// No symbol found for this address.
		LastError = GetLastError();
	}

	// Get filename and line number.
	IMAGEHLP_LINE64	ImageHelpLine = {0};
	ImageHelpLine.SizeOfStruct = sizeof( ImageHelpLine );
	if( SymGetLineFromAddr64( ProcessHandle, ProgramCounter, (::DWORD *)&out_SymbolInfo.SymbolDisplacement, &ImageHelpLine ) )
	{
		FCStringAnsi::Strncpy( out_SymbolInfo.Filename, ImageHelpLine.FileName, FProgramCounterSymbolInfo::MAX_NAME_LENGHT );
		out_SymbolInfo.LineNumber = ImageHelpLine.LineNumber;
	}
	else
	{
		LastError = GetLastError();
	}

	// Get module name.
	IMAGEHLP_MODULE64 ImageHelpModule = {0};
	ImageHelpModule.SizeOfStruct = sizeof( ImageHelpModule );
	if( SymGetModuleInfo64( ProcessHandle, ProgramCounter, &ImageHelpModule) )
	{
		// Write out module information.
		FCStringAnsi::Strncpy( out_SymbolInfo.ModuleName, ImageHelpModule.ImageName, FProgramCounterSymbolInfo::MAX_NAME_LENGHT );
	}
	else
	{
		LastError = GetLastError();
	}
}

/**
 * Loads modules for current process.
 */ 
static void LoadProcessModules()
{
	int32 ErrorCode = 0;
	HANDLE ProcessHandle = GetCurrentProcess();
	const int32	MAX_MOD_HANDLES = 1024;
	HMODULE ModuleHandleArray[MAX_MOD_HANDLES] = {0};
	HMODULE* ModuleHandlePointer = ModuleHandleArray;
	uint32 BytesRequired = 0;

	// Enumerate process modules.
	bool bEnumProcessModulesSucceeded = FEnumProcessModules( ProcessHandle, ModuleHandleArray, sizeof(ModuleHandleArray), (::DWORD *)&BytesRequired );
	if( !bEnumProcessModulesSucceeded )
	{
		ErrorCode = GetLastError();
		return;
	}

	// Static array isn't sufficient so we dynamically allocate one.
	bool bNeedToFreeModuleHandlePointer = false;
	if( BytesRequired > sizeof( ModuleHandleArray ) )
	{
		// Keep track of the fact that we need to free it again.
		bNeedToFreeModuleHandlePointer = true;
		ModuleHandlePointer = (HMODULE*) GMalloc->Malloc( BytesRequired );
		FEnumProcessModules( ProcessHandle, ModuleHandlePointer, sizeof(ModuleHandleArray), (::DWORD *)&BytesRequired );
	}

	// Find out how many modules we need to load modules for.
	const int32	ModuleCount = BytesRequired / sizeof( HMODULE );

	// Load the modules.
	for( int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex++ )
	{
		MODULEINFO ModuleInfo = {0};
		ANSICHAR ModuleName[FProgramCounterSymbolInfo::MAX_NAME_LENGHT] = {0};
		ANSICHAR ImageName[FProgramCounterSymbolInfo::MAX_NAME_LENGHT] = {0};
#if PLATFORM_64BITS
		static_assert(sizeof( MODULEINFO ) == 24, "Broken alignment for 64bit Windows include.");
#else
		static_assert(sizeof( MODULEINFO ) == 12, "Broken alignment for 32bit Windows include.");
#endif
		FGetModuleInformation( ProcessHandle, ModuleHandleArray[ModuleIndex], &ModuleInfo, sizeof( ModuleInfo ) );
		FGetModuleFileNameEx( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, FProgramCounterSymbolInfo::MAX_NAME_LENGHT );
		FGetModuleBaseName( ProcessHandle, ModuleHandleArray[ModuleIndex], ModuleName, FProgramCounterSymbolInfo::MAX_NAME_LENGHT );

		// Set the search path to find PDBs in the same folder as the DLL.
		ANSICHAR SearchPath[MAX_PATH] = {0};
		ANSICHAR* FileName = NULL;
		const auto Result = GetFullPathNameA( ImageName, MAX_PATH, SearchPath, &FileName );

		if( Result != 0 && Result < MAX_PATH )
		{
			*FileName = 0;
			SymSetSearchPath(GetCurrentProcess(), SearchPath);
		}

		// Load module.
		const DWORD64 BaseAddress = SymLoadModule64( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, ModuleName, (DWORD64) ModuleInfo.lpBaseOfDll, (uint32) ModuleInfo.SizeOfImage );
		if( !BaseAddress )
		{
			ErrorCode = GetLastError();
		}
	} 

	// Free the module handle pointer allocated in case the static array was insufficient.
	if( bNeedToFreeModuleHandlePointer )
	{
		GMalloc->Free( ModuleHandlePointer );
	}
}

int32 FWindowsPlatformStackWalk::GetProcessModuleCount()
{
	FPlatformStackWalk::InitStackWalking();

	HANDLE ProcessHandle = GetCurrentProcess(); 
	const int32	MAX_MOD_HANDLES = 1024;
	HMODULE	ModuleHandleArray[MAX_MOD_HANDLES] = {0};
	HMODULE* ModuleHandlePointer = ModuleHandleArray;
	uint32 BytesRequired = 0;

	// Enumerate process modules.
	bool bEnumProcessModulesSucceeded = FEnumProcessModules( ProcessHandle, ModuleHandleArray, sizeof(ModuleHandleArray), (::DWORD *)&BytesRequired );
	if( !bEnumProcessModulesSucceeded )
	{
		return 0;
	}

	// Find out how many modules we need to load modules for.
	const int32 ModuleCount = BytesRequired / sizeof( HMODULE );
	return ModuleCount;
}

int32 FWindowsPlatformStackWalk::GetProcessModuleSignatures(FStackWalkModuleInfo *ModuleSignatures, const int32 ModuleSignaturesSize)
{
	FPlatformStackWalk::InitStackWalking();

	HANDLE		ProcessHandle = GetCurrentProcess(); 
	const int32	MAX_MOD_HANDLES = 1024;
	HMODULE		ModuleHandleArray[MAX_MOD_HANDLES] = {0};
	HMODULE*	ModuleHandlePointer = ModuleHandleArray;
	uint32		BytesRequired;

	// Enumerate process modules.
	bool bEnumProcessModulesSucceeded = FEnumProcessModules( ProcessHandle, ModuleHandleArray, sizeof(ModuleHandleArray), (::DWORD *)&BytesRequired );
	if( !bEnumProcessModulesSucceeded )
	{
		return 0;
	}

	// Static array isn't sufficient so we dynamically allocate one.
	bool bNeedToFreeModuleHandlePointer = false;
	if( BytesRequired > sizeof( ModuleHandleArray ) )
	{
		// Keep track of the fact that we need to free it again.
		bNeedToFreeModuleHandlePointer = true;
		ModuleHandlePointer = (HMODULE*) FMemory::Malloc( BytesRequired );
		FEnumProcessModules( ProcessHandle, ModuleHandlePointer, sizeof(ModuleHandleArray), (::DWORD *)&BytesRequired );
	}

	// Find out how many modules we need to load modules for.
	const int32 ModuleCount = BytesRequired / sizeof( HMODULE );
	IMAGEHLP_MODULEW64 Img = {0};
	Img.SizeOfStruct = sizeof(Img);

	int32 SignatureIndex = 0;

	// Load the modules.
	for( int32 ModuleIndex = 0; ModuleIndex < ModuleCount && SignatureIndex < ModuleSignaturesSize; ModuleIndex++ )
	{
		MODULEINFO ModuleInfo = {0};
		ANSICHAR ModuleName[MAX_PATH] = {0};
		ANSICHAR ImageName[MAX_PATH] = {0};
#if PLATFORM_64BITS
		static_assert(sizeof( MODULEINFO ) == 24, "Broken alignment for 64bit Windows include.");
#else
		static_assert(sizeof( MODULEINFO ) == 12, "Broken alignment for 32bit Windows include.");
#endif
		FGetModuleInformation( ProcessHandle, ModuleHandleArray[ModuleIndex], &ModuleInfo,sizeof( ModuleInfo ) );
		FGetModuleFileNameEx( ProcessHandle, ModuleHandleArray[ModuleIndex], ImageName, MAX_PATH );
		FGetModuleBaseName( ProcessHandle, ModuleHandleArray[ModuleIndex], ModuleName, MAX_PATH );

		// Load module.
		if(SymGetModuleInfoW64(ProcessHandle, (DWORD64)ModuleInfo.lpBaseOfDll, &Img))
		{
			FStackWalkModuleInfo Info = {0};
			Info.BaseOfImage = Img.BaseOfImage;
			FCString::Strcpy(Info.ImageName, Img.ImageName);
			Info.ImageSize = Img.ImageSize;
			FCString::Strcpy(Info.LoadedImageName, Img.LoadedImageName);
			FCString::Strcpy(Info.ModuleName, Img.ModuleName);
			Info.PdbAge = Img.PdbAge;
			Info.PdbSig = Img.PdbSig;
			FMemory::Memcpy(&Info.PdbSig70, &Img.PdbSig70, sizeof(GUID));
			Info.TimeDateStamp = Img.TimeDateStamp;

			ModuleSignatures[SignatureIndex] = Info;
			++SignatureIndex;
		}
	}

	// Free the module handle pointer allocated in case the static array was insufficient.
	if( bNeedToFreeModuleHandlePointer )
	{
		FMemory::Free( ModuleHandlePointer );
	}

	return SignatureIndex;
}

/**
 * Callback from the modules system that the loaded modules have changed and we need to reload symbols.
 */ 
static void OnModulesChanged( FName ModuleThatChanged, EModuleChangeReason ReasonForChange )
{
	GNeedToRefreshSymbols = true;
}

/**
 * Initializes the symbol engine if needed.
 */ 
bool FWindowsPlatformStackWalk::InitStackWalking()
{
	// DbgHelp functions are not thread safe, but this function can potentially be called from different
	// threads in our engine, so we take a critical section
	static FCriticalSection CriticalSection;
	FScopeLock Lock( &CriticalSection );

	// Only initialize once.
	if( !GStackWalkingInitialized )
	{
		void* DllHandle = FPlatformProcess::GetDllHandle( TEXT("PSAPI.DLL") );
		if( DllHandle == NULL )
		{
			return false;
		}

		// Load dynamically linked PSAPI routines.
		FEnumProcesses			= (TFEnumProcesses)			FPlatformProcess::GetDllExport( DllHandle,TEXT("EnumProcesses"));
		FEnumProcessModules		= (TFEnumProcessModules)	FPlatformProcess::GetDllExport( DllHandle,TEXT("EnumProcessModules"));
		FGetModuleFileNameEx	= (TFGetModuleFileNameEx)	FPlatformProcess::GetDllExport( DllHandle,TEXT("GetModuleFileNameExA"));
		FGetModuleBaseName		= (TFGetModuleBaseName)		FPlatformProcess::GetDllExport( DllHandle,TEXT("GetModuleBaseNameA"));
		FGetModuleInformation	= (TFGetModuleInformation)	FPlatformProcess::GetDllExport( DllHandle,TEXT("GetModuleInformation"));

		// Abort if we can't look up the functions.
		if( !FEnumProcesses || !FEnumProcessModules || !FGetModuleFileNameEx || !FGetModuleBaseName || !FGetModuleInformation )
		{
			return false;
		}

		// Set up the symbol engine.
		uint32 SymOpts = SymGetOptions();

		SymOpts |= SYMOPT_LOAD_LINES;
		SymOpts |= SYMOPT_FAIL_CRITICAL_ERRORS;
		SymOpts |= SYMOPT_DEFERRED_LOADS;
		SymOpts |= SYMOPT_EXACT_SYMBOLS;

		// This option allows for undecorated names to be handled by the symbol engine.
		SymOpts |= SYMOPT_UNDNAME;

		// Disable by default as it can be very spammy/slow.  Turn it on if you are debugging symbol look-up!
		//		SymOpts |= SYMOPT_DEBUG;

		// Not sure these are important or desirable
		//		SymOpts |= SYMOPT_ALLOW_ABSOLUTE_SYMBOLS;
		//		SymOpts |= SYMOPT_CASE_INSENSITIVE;

		SymSetOptions( SymOpts );

		// Initialize the symbol engine.
		SymInitialize( GetCurrentProcess(), NULL, true );
		LoadProcessModules();
	
		GNeedToRefreshSymbols = false;
		GStackWalkingInitialized = true;
	}
#if WINVER > 0x502
	else if (GNeedToRefreshSymbols)
	{
		// Refresh and reload symbols
		SymRefreshModuleList( GetCurrentProcess() );
		LoadProcessModules();
		GNeedToRefreshSymbols = false;
	}
#endif

	return GStackWalkingInitialized;
}

void FWindowsPlatformStackWalk::RegisterOnModulesChanged()
{
	// Register for callback so we can reload symbols when new modules are loaded
	FModuleManager::Get().OnModulesChanged().AddStatic( &OnModulesChanged );
}
