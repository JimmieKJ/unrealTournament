// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Misc/App.h"
#include "EngineVersion.h"
#include "EngineBuildSettings.h"

/*-----------------------------------------------------------------------------
	FGenericCrashContext
-----------------------------------------------------------------------------*/

const ANSICHAR* FGenericCrashContext::CrashContextRuntimeXMLNameA = "CrashContext.runtime-xml";
const TCHAR* FGenericCrashContext::CrashContextRuntimeXMLNameW = TEXT( "CrashContext.runtime-xml" );

const FString FGenericCrashContext::CrashContextExtension = TEXT( ".runtime-xml" );
const FString FGenericCrashContext::RuntimePropertiesTag = TEXT( "RuntimeProperties" );
const FString FGenericCrashContext::PlatformPropertiesTag = TEXT( "PlatformProperties" );
const FString FGenericCrashContext::UE4MinidumpName = TEXT( "UE4Minidump.dmp" );
const FString FGenericCrashContext::NewLineTag = TEXT( "&nl;" );

bool FGenericCrashContext::bIsInitialized = false;
FPlatformMemoryStats FGenericCrashContext::CrashMemoryStats = FPlatformMemoryStats();

namespace NCachedCrashContextProperties
{
	static bool bIsInternalBuild;
	static bool bIsPerforceBuild;
	static bool bIsSourceDistribution;
	static bool bIsUE4Release;
	static FString GameName;
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
	static int32 CrashDumpMode;
	static int32 SecondsSinceStart;
	static FString CrashGUID;
}

void FGenericCrashContext::Initialize()
{
	NCachedCrashContextProperties::bIsInternalBuild = FEngineBuildSettings::IsInternalBuild();
	NCachedCrashContextProperties::bIsPerforceBuild = FEngineBuildSettings::IsPerforceBuild();
	NCachedCrashContextProperties::bIsSourceDistribution = FEngineBuildSettings::IsSourceDistribution();
	NCachedCrashContextProperties::bIsUE4Release = FApp::IsEngineInstalled();

	NCachedCrashContextProperties::GameName = FString::Printf( TEXT("UE4-%s"), FApp::GetGameName() );
	NCachedCrashContextProperties::ExecutableName = FPlatformProcess::ExecutableName();
	NCachedCrashContextProperties::PlatformName = FPlatformProperties::PlatformName();
	NCachedCrashContextProperties::PlatformNameIni = FPlatformProperties::IniPlatformName();
	NCachedCrashContextProperties::BaseDir = FPlatformProcess::BaseDir();
	NCachedCrashContextProperties::RootDir = FPlatformMisc::RootDir();
	NCachedCrashContextProperties::EpicAccountId = FPlatformMisc::GetEpicAccountId();
	NCachedCrashContextProperties::MachineIdStr = FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits);
	FPlatformMisc::GetOSVersions(NCachedCrashContextProperties::OsVersion, NCachedCrashContextProperties::OsSubVersion);
	NCachedCrashContextProperties::NumberOfCores = FPlatformMisc::NumberOfCores();
	NCachedCrashContextProperties::NumberOfCoresIncludingHyperthreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();

	NCachedCrashContextProperties::CPUVendor = FPlatformMisc::GetCPUVendor();
	NCachedCrashContextProperties::CPUBrand = FPlatformMisc::GetCPUBrand();
	NCachedCrashContextProperties::PrimaryGPUBrand = FPlatformMisc::GetPrimaryGPUBrand();
	NCachedCrashContextProperties::UserName = FPlatformProcess::UserName();
	NCachedCrashContextProperties::DefaultLocale = FPlatformMisc::GetDefaultLocale();

	// Using the -fullcrashdump parameter will cause full memory minidumps to be created for crashes
	NCachedCrashContextProperties::CrashDumpMode = (int32)ECrashDumpMode::Default;
	if (FCommandLine::IsInitialized())
	{
		const TCHAR* CmdLine = FCommandLine::Get();
		if (FParse::Param( CmdLine, TEXT("fullcrashdumpalways") ))
		{
			NCachedCrashContextProperties::CrashDumpMode = (int32)ECrashDumpMode::FullDumpAlways;
		}
		else if (FParse::Param( CmdLine, TEXT("fullcrashdump") ))
		{
			NCachedCrashContextProperties::CrashDumpMode = (int32)ECrashDumpMode::FullDump;
		}
	}

	const FGuid Guid = FGuid::NewGuid();
	NCachedCrashContextProperties::CrashGUID = FString::Printf(TEXT("UE4CC-%s-%s"), *NCachedCrashContextProperties::PlatformNameIni, *Guid.ToString(EGuidFormats::Digits));

	// Initialize delegate for updating SecondsSinceStart, because FPlatformTime::Seconds() is not POSIX safe.
	const float PollingInterval = 1.0f;
	FTicker::GetCoreTicker().AddTicker( FTickerDelegate::CreateLambda( []( float DeltaTime )
	{
		NCachedCrashContextProperties::SecondsSinceStart = int32(FPlatformTime::Seconds() - GStartTime);
		return true;
	} ), PollingInterval );

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

	BeginSection( *RuntimePropertiesTag );
	AddCrashProperty( TEXT( "CrashVersion" ), (int32)ECrashDescVersions::VER_3_CrashContext );
	AddCrashProperty( TEXT( "CrashGUID" ), *NCachedCrashContextProperties::CrashGUID );
	AddCrashProperty( TEXT( "ProcessId" ), FPlatformProcess::GetCurrentProcessId() );
	AddCrashProperty( TEXT( "IsInternalBuild" ), NCachedCrashContextProperties::bIsInternalBuild );
	AddCrashProperty( TEXT( "IsPerforceBuild" ), NCachedCrashContextProperties::bIsPerforceBuild );
	AddCrashProperty( TEXT( "IsSourceDistribution" ), NCachedCrashContextProperties::bIsSourceDistribution );

	AddCrashProperty( TEXT( "SecondsSinceStart" ), NCachedCrashContextProperties::SecondsSinceStart );

	// Add common crash properties.
	AddCrashProperty( TEXT( "GameName" ), *NCachedCrashContextProperties::GameName );
	AddCrashProperty( TEXT( "ExecutableName" ), *NCachedCrashContextProperties::ExecutableName );
	AddCrashProperty( TEXT( "BuildConfiguration" ), EBuildConfigurations::ToString( FApp::GetBuildConfiguration() ) );

	AddCrashProperty( TEXT( "PlatformName" ), *NCachedCrashContextProperties::PlatformName );
	AddCrashProperty( TEXT( "PlatformNameIni" ), *NCachedCrashContextProperties::PlatformNameIni );
	AddCrashProperty( TEXT( "EngineMode" ), FPlatformMisc::GetEngineMode() );
	AddCrashProperty( TEXT( "EngineVersion" ), *FEngineVersion::Current().ToString() );

	FString CommandLineWithoutPassword = TEXT("");
	if (FCommandLine::IsInitialized())
	{
		CommandLineWithoutPassword = FCommandLine::GetOriginal();
		int32 PasswordStart = CommandLineWithoutPassword.Find((TEXT("-password=")));
		if (PasswordStart != INDEX_NONE)
		{
			int32 PasswordEnd = CommandLineWithoutPassword.Find(TEXT(" "), ESearchCase::IgnoreCase, ESearchDir::FromStart, PasswordStart);
			if (PasswordEnd == INDEX_NONE)
			{
				PasswordEnd = CommandLineWithoutPassword.Len();
			}

			CommandLineWithoutPassword.RemoveAt(PasswordStart, PasswordEnd - PasswordStart);
		}
	}
	AddCrashProperty( TEXT( "CommandLine" ), *CommandLineWithoutPassword );

	AddCrashProperty( TEXT( "LanguageLCID" ), FInternationalization::Get().GetCurrentCulture()->GetLCID() );
	AddCrashProperty( TEXT( "AppDefaultLocale" ), *NCachedCrashContextProperties::DefaultLocale );

	AddCrashProperty( TEXT( "IsUE4Release" ), NCachedCrashContextProperties::bIsUE4Release );

	// Remove periods from user names to match AutoReporter user names
	// The name prefix is read by CrashRepository.AddNewCrash in the website code
	const bool bSendUserName = NCachedCrashContextProperties::bIsInternalBuild;
	AddCrashProperty( TEXT( "UserName" ), bSendUserName ? *NCachedCrashContextProperties::UserName.Replace( TEXT( "." ), TEXT( "" ) ) : TEXT( "" ) );

	AddCrashProperty( TEXT( "BaseDir" ), *NCachedCrashContextProperties::BaseDir );
	AddCrashProperty( TEXT( "RootDir" ), *NCachedCrashContextProperties::RootDir );
	AddCrashProperty( TEXT( "MachineId" ), *NCachedCrashContextProperties::MachineIdStr );
	AddCrashProperty( TEXT( "EpicAccountId" ), *NCachedCrashContextProperties::EpicAccountId );

	AddCrashProperty( TEXT( "CallStack" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "SourceContext" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "UserDescription" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "ErrorMessage" ), (const TCHAR*)GErrorMessage ); // GErrorMessage may be broken.
	AddCrashProperty( TEXT( "CrashDumpMode" ), NCachedCrashContextProperties::CrashDumpMode );

	// Add misc stats.
	AddCrashProperty( TEXT( "Misc.NumberOfCores" ), NCachedCrashContextProperties::NumberOfCores );
	AddCrashProperty( TEXT( "Misc.NumberOfCoresIncludingHyperthreads" ), NCachedCrashContextProperties::NumberOfCoresIncludingHyperthreads );
	AddCrashProperty( TEXT( "Misc.Is64bitOperatingSystem" ), (int32)FPlatformMisc::Is64bitOperatingSystem() );

	AddCrashProperty( TEXT( "Misc.CPUVendor" ), *NCachedCrashContextProperties::CPUVendor );
	AddCrashProperty( TEXT( "Misc.CPUBrand" ), *NCachedCrashContextProperties::CPUBrand );
	AddCrashProperty( TEXT( "Misc.PrimaryGPUBrand" ), *NCachedCrashContextProperties::PrimaryGPUBrand );
	AddCrashProperty( TEXT( "Misc.OSVersionMajor" ), *NCachedCrashContextProperties::OsVersion );
	AddCrashProperty( TEXT( "Misc.OSVersionMinor" ), *NCachedCrashContextProperties::OsSubVersion );


	// #YRX_Crash: 2015-07-21 Move to the crash report client.
	/*{
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

	AddCrashProperty( TEXT( "MemoryStats.AvailablePhysical" ), (uint64)CrashMemoryStats.AvailablePhysical );
	AddCrashProperty( TEXT( "MemoryStats.AvailableVirtual" ), (uint64)CrashMemoryStats.AvailableVirtual );
	AddCrashProperty( TEXT( "MemoryStats.UsedPhysical" ), (uint64)CrashMemoryStats.UsedPhysical );
	AddCrashProperty( TEXT( "MemoryStats.PeakUsedPhysical" ), (uint64)CrashMemoryStats.PeakUsedPhysical );
	AddCrashProperty( TEXT( "MemoryStats.UsedVirtual" ), (uint64)CrashMemoryStats.UsedVirtual );
	AddCrashProperty( TEXT( "MemoryStats.PeakUsedVirtual" ), (uint64)CrashMemoryStats.PeakUsedVirtual );
	AddCrashProperty( TEXT( "MemoryStats.bIsOOM" ), (int32)FPlatformMemory::bIsOOM );
	AddCrashProperty( TEXT( "MemoryStats.OOMAllocationSize"), (uint64)FPlatformMemory::OOMAllocationSize );
	AddCrashProperty( TEXT( "MemoryStats.OOMAllocationAlignment"), (int32)FPlatformMemory::OOMAllocationAlignment );

	//Architecture
	//CrashedModuleName
	//LoadedModules
	EndSection( *RuntimePropertiesTag );
	
	// Add platform specific properties.
	BeginSection( *PlatformPropertiesTag );
	AddPlatformSpecificProperties();
	EndSection( *PlatformPropertiesTag );

	AddFooter();
}

const FString& FGenericCrashContext::GetUniqueCrashName()
{
	return NCachedCrashContextProperties::CrashGUID;
}

const bool FGenericCrashContext::IsFullCrashDump()
{
	return (NCachedCrashContextProperties::CrashDumpMode == (int32)ECrashDumpMode::FullDump) ||
		(NCachedCrashContextProperties::CrashDumpMode == (int32)ECrashDumpMode::FullDumpAlways);
}

const bool FGenericCrashContext::IsFullCrashDumpOnEnsure()
{
	return (NCachedCrashContextProperties::CrashDumpMode == (int32)ECrashDumpMode::FullDumpAlways);
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


	CommonBuffer += EscapeXMLString( PropertyValue );

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


FString FGenericCrashContext::EscapeXMLString( const FString& Text )
{
	return Text
		.Replace( TEXT( "&" ), TEXT( "&amp;" ) )
		.Replace( TEXT( "\"" ), TEXT( "&quot;" ) )
		.Replace( TEXT( "'" ), TEXT( "&apos;" ) )
		.Replace( TEXT( "<" ), TEXT( "&lt;" ) )
		.Replace( TEXT( ">" ), TEXT( "&gt;" ) )
		// Replace newline for FXMLFile.
		.Replace( TEXT( "\n" ), *NewLineTag )
		// Ignore return carriage.
		.Replace( TEXT( "\r" ), TEXT("") );
}

FString FGenericCrashContext::UnescapeXMLString( const FString& Text )
{
	return Text
		.Replace( TEXT( "&amp;" ), TEXT( "&" ) )
		.Replace( TEXT( "&quot;" ), TEXT( "\"" ) )
		.Replace( TEXT( "&apos;" ), TEXT( "'" ) )
		.Replace( TEXT( "&lt;" ), TEXT( "<" ) )
		.Replace( TEXT( "&gt;" ), TEXT( ">" ) )
		.Replace( *NewLineTag, TEXT( "\n" ) );
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