// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "GenericPlatform/GenericPlatformContext.h"
#include "Misc/App.h"
#include "Runtime/Launch/Resources/Version.h"

const ANSICHAR* FGenericCrashContext::CrashContextRuntimeXMLNameA = "CrashContext.runtime-xml";
const TCHAR* FGenericCrashContext::CrashContextRuntimeXMLNameW = TEXT( "CrashContext.runtime-xml" );
bool FGenericCrashContext::bIsInitialized = false;

namespace NCachedCrashContextProperties
{
	static FString CrashDescriptionCommonBuffer;
	static FString BaseDir;
	static FString EpicAccountId;
	static FString MachineIdStr;
	static FString OsVersion;
	static FString OsSubVersion;
	static int32 NumberOfCores;
	static int32 NumberOfCoresIncludingHyperthreads;
	static FString CPUVendor;
	static FString CPUBrand;
	static FString PrimaryGPUBrand;
	static FString UserName;
}
using namespace NCachedCrashContextProperties;

void FGenericCrashContext::Initialize()
{
	BaseDir = FPlatformProcess::BaseDir();
	EpicAccountId = FPlatformMisc::GetEpicAccountId();
	MachineIdStr = FPlatformMisc::GetMachineId().ToString( EGuidFormats::Digits );
	FPlatformMisc::GetOSVersions(OsVersion,OsSubVersion);
	NumberOfCores = FPlatformMisc::NumberOfCores();
	NumberOfCoresIncludingHyperthreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();

	CPUVendor = FPlatformMisc::GetCPUVendor();
	CPUBrand = FPlatformMisc::GetCPUBrand();
	PrimaryGPUBrand = FPlatformMisc::GetPrimaryGPUBrand();
	UserName = FPlatformProcess::UserName();

	bIsInitialized = true;
}


FGenericCrashContext::FGenericCrashContext() :
CommonBuffer( CrashDescriptionCommonBuffer )
{
	CommonBuffer.Reserve( 32768 );
}

void FGenericCrashContext::SerializeContentToBuffer()
{
	AddHeader();

	// ProcessId will be used by the crash report client to get more information about the crashed process.
	// It should be enough to open the process, get the memory stats, a list of loaded modules etc.
	// It means that the crashed process needs to be alive as long as the crash report client is working on the process.
	AddCrashProperty( TEXT( "ProcessId" ), FPlatformProcess::GetCurrentProcessId() );

	// Add common crash properties.
	AddCrashProperty( TEXT( "GameName" ), *FString::Printf( TEXT( "UE4-%s" ), FApp::GetGameName() ) );
	AddCrashProperty( TEXT( "PlatformName" ), *FString( FPlatformProperties::PlatformName() ) );
	AddCrashProperty( TEXT( "EngineMode" ), FPlatformMisc::GetEngineMode() );
	AddCrashProperty( TEXT( "EngineVersion" ), ENGINE_VERSION_STRING );
	AddCrashProperty( TEXT( "CommandLine" ), FCommandLine::IsInitialized() ? FCommandLine::Get() : TEXT("") );
	AddCrashProperty( TEXT( "LanguageLCID" ), FInternationalization::Get().GetCurrentCulture()->GetLCID() );

	AddCrashProperty( TEXT( "IsUE4Release" ), (int32)FRocketSupport::IsRocket() );

	// Get the user name only for non-UE4 releases.
	if( !FRocketSupport::IsRocket() )
	{
		// Remove periods from internal user names to match AutoReporter user names
		// The name prefix is read by CrashRepository.AddNewCrash in the website code

		AddCrashProperty( TEXT( "UserName" ), *UserName.Replace( TEXT( "." ), TEXT( "" ) ) );
	}


	AddCrashProperty( TEXT( "BaseDir" ), *BaseDir );
	AddCrashProperty( TEXT( "MachineId" ), *MachineIdStr );
	AddCrashProperty( TEXT( "EpicAccountId" ), *EpicAccountId );

	AddCrashProperty( TEXT( "CallStack" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "SourceContext" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "UserDescription" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "ErrorMessage" ), (const TCHAR*)GErrorMessage );

	// @TODO yrx 2014-10-08 Move to the crash report client.
	/*if( CanUseUnsafeAPI() )
	{
		AddCrashProperty( TEXT( "TimeOfCrash" ), FDateTime::UtcNow().GetTicks() );
	}
	*/

	// Add misc stats.
	AddCrashProperty( TEXT( "Misc.NumberOfCores" ), NumberOfCores );
	AddCrashProperty( TEXT( "Misc.NumberOfCoresIncludingHyperthreads" ), NumberOfCoresIncludingHyperthreads );
	AddCrashProperty( TEXT( "Misc.Is64bitOperatingSystem" ), (int32)FPlatformMisc::Is64bitOperatingSystem() );

	AddCrashProperty( TEXT( "Misc.CPUVendor" ), *CPUVendor );
	AddCrashProperty( TEXT( "Misc.CPUBrand" ), *CPUBrand );
	AddCrashProperty( TEXT( "Misc.PrimaryGPUBrand" ), *PrimaryGPUBrand );
	AddCrashProperty( TEXT( "Misc.OSVersion" ), *FString::Printf( TEXT( "%s, %s" ), *OsVersion, *OsSubVersion ) );

	// @TODO yrx 2014-10-08 Move to the crash report client.
	/*if( CanUseUnsafeAPI() )
	{
		uint64 AppDiskTotalNumberOfBytes = 0;
		uint64 AppDiskNumberOfFreeBytes = 0;
		FPlatformMisc::GetDiskTotalAndFreeSpace( FPlatformProcess::BaseDir(), AppDiskTotalNumberOfBytes, AppDiskNumberOfFreeBytes );
		AddCrashProperty( TEXT( "Misc.AppDiskTotalNumberOfBytes" ), AppDiskTotalNumberOfBytes );
		AddCrashProperty( TEXT( "Misc.AppDiskNumberOfFreeBytes" ), AppDiskNumberOfFreeBytes );
	}*/

	// FPlatformMemory::GetConstants is called in the GCreateMalloc, so we can assume it is always valid.
	{
		// Add memory stats.
		const FPlatformMemoryConstants& MemConstants = FPlatformMemory::GetConstants();

		AddCrashProperty( TEXT( "MemoryStats.TotalPhysical" ), (uint64)MemConstants.TotalPhysical );
		AddCrashProperty( TEXT( "MemoryStats.TotalVirtual" ), (uint64)MemConstants.TotalVirtual );
		AddCrashProperty( TEXT( "MemoryStats.PageSize" ), (uint64)MemConstants.PageSize );
		AddCrashProperty( TEXT( "MemoryStats.TotalPhysicalGB" ), MemConstants.TotalPhysicalGB );
	}

	// @TODO yrx 2014-10-08 Move to the crash report client.
	/*if( CanUseUnsafeAPI() )
	{
		const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
		AddCrashProperty( TEXT( "MemoryStats.AvailablePhysical" ), (uint64)MemStats.AvailablePhysical );
		AddCrashProperty( TEXT( "MemoryStats.AvailableVirtual" ), (uint64)MemStats.AvailableVirtual );
		AddCrashProperty( TEXT( "MemoryStats.UsedPhysical" ), (uint64)MemStats.UsedPhysical );
		AddCrashProperty( TEXT( "MemoryStats.PeakUsedPhysical" ), (uint64)MemStats.PeakUsedPhysical );
		AddCrashProperty( TEXT( "MemoryStats.UsedVirtual" ), (uint64)MemStats.UsedVirtual );
		AddCrashProperty( TEXT( "MemoryStats.PeakUsedVirtual" ), (uint64)MemStats.PeakUsedVirtual );
		AddCrashProperty( TEXT( "MemoryStats.bIsOOM" ), (int32)FPlatformMemory::bIsOOM );
	}*/

	// @TODO yrx 2014-09-10 
	//Architecture
	//CrashedModuleName
	//LoadedModules
	AddFooter();

	// Add platform specific properties.
	CommonBuffer += TEXT( "<PlatformSpecificProperties>" ) LINE_TERMINATOR;
	AddPlatformSpecificProperties();
	CommonBuffer += TEXT( "</PlatformSpecificProperties>" ) LINE_TERMINATOR;
}

void FGenericCrashContext::SerializeAsXML( const TCHAR* Filename )
{
	SerializeContentToBuffer();
	// @TODO yrx 2014-10-08 We need mechanism to store the file that is aware of crashes and OOMs.
	// Probably will be replaced with some kind of Inter-Process Communication
	FFileHelper::SaveStringToFile( CommonBuffer, Filename, FFileHelper::EEncodingOptions::ForceUTF8 );
}

void FGenericCrashContext::AddPlatformSpecificProperties()
{
	// Nothing really to do here. Can be overridden by the platform code.
	// @see FWindowsPlatformCrashContext::AddPlatformSpecificProperties
}

void FGenericCrashContext::AddHeader()
{
	CommonBuffer += TEXT( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" ) LINE_TERMINATOR;
	CommonBuffer += TEXT( "<RuntimeCrashDescription>" ) LINE_TERMINATOR;

	AddCrashProperty( TEXT( "Version" ), (int32)ECrashDescVersions::VER_2_AddedNewProperties );
}

void FGenericCrashContext::AddFooter()
{
	CommonBuffer += TEXT( "</RuntimeCrashDescription>" ) LINE_TERMINATOR;
}

void FGenericCrashContext::AddCrashProperty( const TCHAR* PropertyName, const TCHAR* PropertyValue )
{
	CommonBuffer += TEXT( "<" );
	CommonBuffer += PropertyName;
	CommonBuffer += TEXT( ">" );

	CommonBuffer += PropertyValue;

	CommonBuffer += TEXT( "</" );
	CommonBuffer += PropertyName;
	CommonBuffer += TEXT( ">" );
	CommonBuffer += LINE_TERMINATOR;
}
