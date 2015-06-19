// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "GenericPlatform/GenericPlatformContext.h"
#include "Misc/App.h"
#include "Runtime/Launch/Resources/Version.h"
#include "EngineBuildSettings.h"

const ANSICHAR* FGenericCrashContext::CrashContextRuntimeXMLNameA = "CrashContext.runtime-xml";
const TCHAR* FGenericCrashContext::CrashContextRuntimeXMLNameW = TEXT( "CrashContext.runtime-xml" );
bool FGenericCrashContext::bIsInitialized = false;

namespace NCachedCrashContextProperties
{
	static FString ExecutableName;
	static FString PlatformName;
	static FString PlatformNameIni;
	static FString BaseDir;
	static FString RootDir;
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
	static FString DefaultLocale;

	static FString CrashGUID;
}
using namespace NCachedCrashContextProperties;

void FGenericCrashContext::Initialize()
{
	ExecutableName = FPlatformProcess::ExecutableName();
	PlatformName = FPlatformProperties::PlatformName();
	PlatformNameIni = FPlatformProperties::IniPlatformName();
	BaseDir = FPlatformProcess::BaseDir();
	RootDir = FPlatformMisc::RootDir();
	EpicAccountId = FPlatformMisc::GetEpicAccountId();
	MachineIdStr = FPlatformMisc::GetMachineId().ToString( EGuidFormats::Digits );
	FPlatformMisc::GetOSVersions(OsVersion,OsSubVersion);
	NumberOfCores = FPlatformMisc::NumberOfCores();
	NumberOfCoresIncludingHyperthreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();

	CPUVendor = FPlatformMisc::GetCPUVendor();
	CPUBrand = FPlatformMisc::GetCPUBrand();
	PrimaryGPUBrand = FPlatformMisc::GetPrimaryGPUBrand();
	UserName = FPlatformProcess::UserName();
	DefaultLocale = FPlatformMisc::GetDefaultLocale();

	const FGuid Guid = FGuid::NewGuid();
	CrashGUID = FString::Printf( TEXT( "UE4CC-%s-%s" ), *PlatformNameIni, *Guid.ToString( EGuidFormats::Digits ) );

	bIsInitialized = true;
}

FGenericCrashContext::FGenericCrashContext()
{
	CommonBuffer.Reserve( 32768 );
}

void FGenericCrashContext::SerializeContentToBuffer()
{
	// Must conform against:
	// https://www.securecoding.cert.org/confluence/display/seccode/SIG30-C.+Call+only+asynchronous-safe+functions+within+signal+handlers
	AddHeader();

	BeginSection( TEXT( "RuntimeProperties" ) );
	AddCrashProperty( TEXT( "Version" ), (int32)ECrashDescVersions::VER_2_AddedNewProperties );
	// ProcessId will be used by the crash report client to get more information about the crashed process.
	// It should be enough to open the process, get the memory stats, a list of loaded modules etc.
	// It means that the crashed process needs to be alive as long as the crash report client is working on the process.
	// Proposal, not implemented yet.
	AddCrashProperty( TEXT( "ProcessId" ), FPlatformProcess::GetCurrentProcessId() );

	AddCrashProperty( TEXT( "IsInternalBuild" ), (int32)FEngineBuildSettings::IsInternalBuild() );
	AddCrashProperty( TEXT( "IsPerforceBuild" ), (int32)FEngineBuildSettings::IsPerforceBuild() );
	AddCrashProperty( TEXT( "IsSourceDistribution" ), (int32)FEngineBuildSettings::IsSourceDistribution() );

	// Add common crash properties.
	AddCrashProperty( TEXT( "GameName" ), FApp::GetGameName() );
	AddCrashProperty( TEXT( "ExecutableName" ), *ExecutableName );
	AddCrashProperty( TEXT( "BuildConfiguration" ), EBuildConfigurations::ToString( FApp::GetBuildConfiguration() ) );

	AddCrashProperty( TEXT( "PlatformName" ), *PlatformName );
	AddCrashProperty( TEXT( "PlatformNameIni" ), *PlatformNameIni );
	AddCrashProperty( TEXT( "EngineMode" ), FPlatformMisc::GetEngineMode() );
	AddCrashProperty( TEXT( "EngineVersion" ), ENGINE_VERSION_STRING );
	AddCrashProperty( TEXT( "CommandLine" ), FCommandLine::IsInitialized() ? FCommandLine::Get() : TEXT("") );
	AddCrashProperty( TEXT( "LanguageLCID" ), FInternationalization::Get().GetCurrentCulture()->GetLCID() );
	AddCrashProperty( TEXT( "DefaultLocale" ), *DefaultLocale );

	AddCrashProperty( TEXT( "IsUE4Release" ), (int32)FApp::IsEngineInstalled() );

	// Remove periods from user names to match AutoReporter user names
	// The name prefix is read by CrashRepository.AddNewCrash in the website code
	AddCrashProperty( TEXT( "UserName" ), *UserName.Replace( TEXT( "." ), TEXT( "" ) ) );

	AddCrashProperty( TEXT( "BaseDir" ), *BaseDir );
	AddCrashProperty( TEXT( "RootDir" ), *RootDir );
	AddCrashProperty( TEXT( "MachineId" ), *MachineIdStr );
	AddCrashProperty( TEXT( "EpicAccountId" ), *EpicAccountId );

	AddCrashProperty( TEXT( "CallStack" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "SourceContext" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "UserDescription" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "ErrorMessage" ), (const TCHAR*)GErrorMessage ); // GErrorMessage may be broken.

	// @TODO yrx 2014-10-08 Move to the crash report client.
	/*if( CanUseUnsafeAPI() )
	{
		AddCrashProperty( TEXT( "TimeOfCrash" ), FDateTime::UtcNow().GetTicks() );
	}
	*/

	// Add misc stats.
	AddCrashProperty( TEXT( "MiscNumberOfCores" ), NumberOfCores );
	AddCrashProperty( TEXT( "MiscNumberOfCoresIncludingHyperthreads" ), NumberOfCoresIncludingHyperthreads );
	AddCrashProperty( TEXT( "MiscIs64bitOperatingSystem" ), (int32)FPlatformMisc::Is64bitOperatingSystem() );

	AddCrashProperty( TEXT( "MiscCPUVendor" ), *CPUVendor );
	AddCrashProperty( TEXT( "MiscCPUBrand" ), *CPUBrand );
	AddCrashProperty( TEXT( "MiscPrimaryGPUBrand" ), *PrimaryGPUBrand );
	AddCrashProperty( TEXT( "MiscOSVersionMajor" ), *OsVersion );
	AddCrashProperty( TEXT( "MiscOSVersionMinor" ), *OsSubVersion );


	// @TODO yrx 2014-10-08 Move to the crash report client.
	/*if( CanUseUnsafeAPI() )
	{
		uint64 AppDiskTotalNumberOfBytes = 0;
		uint64 AppDiskNumberOfFreeBytes = 0;
		FPlatformMisc::GetDiskTotalAndFreeSpace( FPlatformProcess::BaseDir(), AppDiskTotalNumberOfBytes, AppDiskNumberOfFreeBytes );
		AddCrashProperty( TEXT( "MiscAppDiskTotalNumberOfBytes" ), AppDiskTotalNumberOfBytes );
		AddCrashProperty( TEXT( "MiscAppDiskNumberOfFreeBytes" ), AppDiskNumberOfFreeBytes );
	}*/

	// FPlatformMemory::GetConstants is called in the GCreateMalloc, so we can assume it is always valid.
	{
		// Add memory stats.
		const FPlatformMemoryConstants& MemConstants = FPlatformMemory::GetConstants();

		AddCrashProperty( TEXT( "MemoryStatsTotalPhysical" ), (uint64)MemConstants.TotalPhysical );
		AddCrashProperty( TEXT( "MemoryStatsTotalVirtual" ), (uint64)MemConstants.TotalVirtual );
		AddCrashProperty( TEXT( "MemoryStatsPageSize" ), (uint64)MemConstants.PageSize );
		AddCrashProperty( TEXT( "MemoryStatsTotalPhysicalGB" ), MemConstants.TotalPhysicalGB );
	}

	// @TODO yrx 2014-10-08 Move to the crash report client.
	/*if( CanUseUnsafeAPI() )
	{
		const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
		AddCrashProperty( TEXT( "MemoryStatsAvailablePhysical" ), (uint64)MemStats.AvailablePhysical );
		AddCrashProperty( TEXT( "MemoryStatsAvailableVirtual" ), (uint64)MemStats.AvailableVirtual );
		AddCrashProperty( TEXT( "MemoryStatsUsedPhysical" ), (uint64)MemStats.UsedPhysical );
		AddCrashProperty( TEXT( "MemoryStatsPeakUsedPhysical" ), (uint64)MemStats.PeakUsedPhysical );
		AddCrashProperty( TEXT( "MemoryStatsUsedVirtual" ), (uint64)MemStats.UsedVirtual );
		AddCrashProperty( TEXT( "MemoryStatsPeakUsedVirtual" ), (uint64)MemStats.PeakUsedVirtual );
		AddCrashProperty( TEXT( "MemoryStatsbIsOOM" ), (int32)FPlatformMemory::bIsOOM );
	}*/

	//Architecture
	//CrashedModuleName
	//LoadedModules
	EndSection( TEXT( "RuntimeProperties" ) );
	
	// Add platform specific properties.
	BeginSection( TEXT("PlatformProperties") );
	AddPlatformSpecificProperties();
	EndSection( TEXT( "PlatformProperties" ) );

	AddFooter();
}

const FString& FGenericCrashContext::GetUniqueCrashName()
{
	return CrashGUID;
}

void FGenericCrashContext::SerializeAsXML( const TCHAR* Filename )
{
	SerializeContentToBuffer();
	// Use OS build-in functionality instead.
	FFileHelper::SaveStringToFile( CommonBuffer, Filename, FFileHelper::EEncodingOptions::AutoDetect );
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

void FGenericCrashContext::AddPlatformSpecificProperties()
{
	// Nothing really to do here. Can be overridden by the platform code.
	// @see FWindowsPlatformCrashContext::AddPlatformSpecificProperties
}

void FGenericCrashContext::AddHeader()
{
	CommonBuffer += TEXT( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" ) LINE_TERMINATOR;
	BeginSection( TEXT("FGenericCrashContext") );
}

void FGenericCrashContext::AddFooter()
{
	EndSection( TEXT( "FGenericCrashContext" ) );
}

void FGenericCrashContext::BeginSection( const TCHAR* SectionName )
{
	CommonBuffer += TEXT( "<" );
	CommonBuffer += SectionName;
	CommonBuffer += TEXT( ">" );
	CommonBuffer += LINE_TERMINATOR;
}

void FGenericCrashContext::EndSection( const TCHAR* SectionName )
{
	CommonBuffer += TEXT( "</" );
	CommonBuffer += SectionName;
	CommonBuffer += TEXT( ">" );
	CommonBuffer += LINE_TERMINATOR;
}


FProgramCounterSymbolInfoEx::FProgramCounterSymbolInfoEx( FString InModuleName, FString InFunctionName, FString InFilename, uint32 InLineNumber, uint64 InSymbolDisplacement, uint64 InOffsetInModule, uint64 InProgramCounter ) :
	ModuleName( InModuleName ),
	FunctionName( InFunctionName ),
	Filename( InFilename ),
	LineNumber( InLineNumber ),
	SymbolDisplacement( InSymbolDisplacement ),
	OffsetInModule( InOffsetInModule ),
	ProgramCounter( InProgramCounter )
{

}