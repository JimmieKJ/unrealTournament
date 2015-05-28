// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformProcess.h"
#include "Windows/WindowsSystemIncludes.h"


/** Windows implementation of the process handle. */
struct FProcHandle : public TProcHandle<HANDLE, nullptr>
{
public:
	/** Default constructor. */
	FORCEINLINE FProcHandle()
		: TProcHandle()
	{}

	/** Initialization constructor. */
	FORCEINLINE explicit FProcHandle( HandleType Other )
		: TProcHandle( Other )
	{}

	DEPRECATED(4.8, "FProcHandle::Close() is redundant - handles created with FPlatformProcess::CreateProc() should be closed with FPlatformProcess::CloseProc().")
	FORCEINLINE bool Close();
};


/**
* Windows implementation of the Process OS functions.
**/
struct CORE_API FWindowsPlatformProcess
	: public FGenericPlatformProcess
{
	/**
	 * Windows representation of a interprocess semaphore
	 */
	struct FWindowsSemaphore : public FSemaphore
	{
		virtual void	Lock();
		virtual bool	TryLock(uint64 NanosecondsToWait);
		virtual void	Unlock();

		/** Returns the OS handle */
		HANDLE			GetSemaphore() { return Semaphore; }

		/** Constructor */
		FWindowsSemaphore(const FString& InName, HANDLE InSemaphore);

		/** Destructor */
		virtual ~FWindowsSemaphore();

	protected:

		/** OS handle */
		HANDLE			Semaphore;
	};

public:

	// FGenericPlatformProcess interface

	static void* GetDllHandle( const TCHAR* Filename );
	static void FreeDllHandle( void* DllHandle );
	static void* GetDllExport( void* DllHandle, const TCHAR* ProcName );
	static int32 GetDllApiVersion( const TCHAR* Filename );
	static void AddDllDirectory(const TCHAR* Directory);
	static void PushDllDirectory(const TCHAR* Directory);
	static void PopDllDirectory(const TCHAR* Directory);
	static void CleanFileCache();
	static uint32 GetCurrentProcessId();
	static void SetThreadAffinityMask( uint64 AffinityMask );
	static const TCHAR* BaseDir();
	static const TCHAR* UserDir();
	static const TCHAR* UserTempDir();
	static const TCHAR* UserSettingsDir();
	static const TCHAR* ApplicationSettingsDir();
	static const TCHAR* ComputerName();
	static const TCHAR* UserName(bool bOnlyAlphaNumeric = true);
	static void SetCurrentWorkingDirectoryToBaseDir();
	static const TCHAR* ExecutableName(bool bRemoveExtension = true);
	static FString GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration);
	static const TCHAR* GetModuleExtension();
	static const TCHAR* GetBinariesSubdirectory();
	static void LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error );
	static FProcHandle CreateProc( const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWrite );
	static bool IsProcRunning( FProcHandle & ProcessHandle );
	static void WaitForProc( FProcHandle & ProcessHandle );
	static void CloseProc( FProcHandle & ProcessHandle );
	static void TerminateProc( FProcHandle & ProcessHandle, bool KillTree = false );
	static bool GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode );
	static bool GetApplicationMemoryUsage(uint32 ProcessId, SIZE_T* OutMemoryUsage);
	static bool IsApplicationRunning( uint32 ProcessId );
	static bool IsApplicationRunning( const TCHAR* ProcName );
	static FString GetApplicationName( uint32 ProcessId );	
	static bool IsThisApplicationForeground();
	static bool ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr );
	static bool ExecElevatedProcess(const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode);
	static void LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms = NULL, ELaunchVerb::Type Verb = ELaunchVerb::Open );
	static void ExploreFolder( const TCHAR* FilePath );
	static bool ResolveNetworkPath( FString InUNCPath, FString& OutPath ); 
	static void Sleep(float Seconds);
	static void SleepNoStats(float Seconds);
	static void SleepInfinite();
	static class FEvent* CreateSynchEvent(bool bIsManualReset = false);
	static class FRunnableThread* CreateRunnableThread();
	static void ClosePipe( void* ReadPipe, void* WritePipe );
	static bool CreatePipe( void*& ReadPipe, void*& WritePipe );
	static FString ReadPipe( void* ReadPipe );
	static bool ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output);
	static FSemaphore* NewInterprocessSynchObject(const FString& Name, bool bCreate, uint32 MaxLocks = 1);
	static bool DeleteInterprocessSynchObject(FSemaphore * Object);
	static bool Daemonize();
	static FProcHandle OpenProcess(uint32 ProcessID);
protected:

	/**
	 * Reads from a collection of anonymous pipes.
	 *
	 * @param OutStrings Will hold the read data.
	 * @param InPipes The pipes to read from.
	 * @param PipeCount The number of pipes.
	 */
	static void ReadFromPipes(FString* OutStrings[], HANDLE InPipes[], int32 PipeCount);

private:

	/**
	 * Since Windows can only have one directory at a time,
	 * this stack is used to reset the previous DllDirectory.
	 */
	static TArray<FString> DllDirectoryStack;
};


typedef FWindowsPlatformProcess FPlatformProcess;

inline bool FProcHandle::Close()
{
	if (IsValid())
	{
		FPlatformProcess::CloseProc(*this);
		return true;
	}
	return false;
}

#include "WindowsCriticalSection.h"

typedef FWindowsCriticalSection FCriticalSection;
