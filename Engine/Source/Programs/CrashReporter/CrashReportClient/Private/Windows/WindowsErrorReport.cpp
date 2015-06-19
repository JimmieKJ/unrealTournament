// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

#include "WindowsErrorReport.h"
#include "XmlFile.h"
#include "CrashDebugHelperModule.h"
#include "../CrashReportUtil.h"

#include "AllowWindowsPlatformTypes.h"
#include <ShlObj.h>
#include "HideWindowsPlatformTypes.h"

#define LOCTEXT_NAMESPACE "CrashReportClient"

namespace
{
	/** Pointer to dynamically loaded crash diagnosis module */
	FCrashDebugHelperModule* CrashHelperModule;
}

/** Helper class used to parse specified string value based on the marker. */
struct FWindowsReportParser
{
	static FString Find( const FString& ReportDirectory, const TCHAR* Marker )
	{
		FString Result;

		TArray<uint8> FileData;
		FFileHelper::LoadFileToArray( FileData, *(ReportDirectory / TEXT( "Report.wer" )) );
		FileData.Add( 0 );
		FileData.Add( 0 );

		const FString FileAsString = reinterpret_cast<TCHAR*>(FileData.GetData());

		TArray<FString> String;
		FileAsString.ParseIntoArray( String, TEXT( "\r\n" ), true );

		for( const auto& StringLine : String )
		{
			if( StringLine.Contains( Marker ) )
			{
				TArray<FString> SeparatedParameters;
				StringLine.ParseIntoArray( SeparatedParameters, Marker, true );

				Result = SeparatedParameters[SeparatedParameters.Num()-1];
				break;
			}
		}

		return Result;
	}
};

FWindowsErrorReport::FWindowsErrorReport(const FString& Directory)
	: FGenericErrorReport(Directory)
{
}

void FWindowsErrorReport::Init()
{
	CrashHelperModule = &FModuleManager::LoadModuleChecked<FCrashDebugHelperModule>(FName("CrashDebugHelper"));
}

void FWindowsErrorReport::ShutDown()
{
	CrashHelperModule->ShutdownModule();
}

FString FWindowsErrorReport::FindCrashedAppPath() const
{
	return FWindowsReportParser::Find(ReportDirectory, TEXT("AppPath="));
}

FText FWindowsErrorReport::DiagnoseReport() const
{
	// Should check if there are local PDBs before doing anything
	auto CrashDebugHelper = CrashHelperModule ? CrashHelperModule->Get() : nullptr;
	if (!CrashDebugHelper)
	{
		// Not localized: should never be seen
		return FText::FromString(TEXT("Failed to load CrashDebugHelper."));
	}

	FString DumpFilename;
	if (!FindFirstReportFileWithExtension(DumpFilename, TEXT(".dmp")))
	{
		if (!FindFirstReportFileWithExtension(DumpFilename, TEXT(".mdmp")))
		{
			return LOCTEXT("MinidumpNotFound", "No minidump found for this crash.");
		}
	}

	if (!CrashDebugHelper->CreateMinidumpDiagnosticReport(ReportDirectory / DumpFilename))
	{
		return LOCTEXT("NoDebuggingSymbols", "You do not have any debugging symbols required to display the callstack for this crash.");
	}

	// There's a callstack, so write it out to save the server trying to do it
	CrashDebugHelper->CrashInfo.GenerateReport(ReportDirectory / FCrashReportClientConfig::Get().GetDiagnosticsFilename());

	const auto& Exception = CrashDebugHelper->CrashInfo.Exception;
	const FString Assertion = FWindowsReportParser::Find( ReportDirectory, TEXT( "AssertLog=" ) );

	return FCrashReportUtil::FormatReportDescription( Exception.ExceptionString, Assertion, Exception.CallStackString );
}

FString FWindowsErrorReport::FindMostRecentErrorReport()
{
	auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	auto DirectoryModifiedTime = FDateTime::MinValue();
	FString ReportDirectory;
	auto ReportFinder = MakeDirectoryVisitor([&](const TCHAR* FilenameOrDirectory, bool bIsDirectory) 
	{
		if (bIsDirectory)
		{
			auto TimeStamp = PlatformFile.GetTimeStamp(FilenameOrDirectory);
			if (TimeStamp > DirectoryModifiedTime && FCString::Strstr( FilenameOrDirectory, TEXT("UE4-") ) )
			{
				ReportDirectory = FilenameOrDirectory;
				DirectoryModifiedTime = TimeStamp;
			}
		}
		return true;
	});

	TCHAR LocalAppDataPath[MAX_PATH];
	SHGetFolderPath(0, CSIDL_LOCAL_APPDATA, NULL, 0, LocalAppDataPath);
	PlatformFile.IterateDirectory( *(FString(LocalAppDataPath) / TEXT("Microsoft/Windows/WER/ReportQueue")), ReportFinder);

	return ReportDirectory;
}

#undef LOCTEXT_NAMESPACE
