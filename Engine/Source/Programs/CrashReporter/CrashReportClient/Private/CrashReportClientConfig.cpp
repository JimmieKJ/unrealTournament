// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"

FCrashReportClientConfig::FCrashReportClientConfig()
	: DiagnosticsFilename( TEXT( "Diagnostics.txt" ) )
	, SectionName( TEXT( "CrashReportClient" ) )
{
	const bool bUnattended =
#if CRASH_REPORT_UNATTENDED_ONLY
		true;
#else
		FApp::IsUnattended();
#endif // CRASH_REPORT_UNATTENDED_ONLY

	if (!GConfig->GetString( *SectionName, TEXT( "CrashReportReceiverIP" ), CrashReportReceiverIP, GEngineIni ))
	{
		// Use the default value.
		CrashReportReceiverIP = TEXT( "http://crashreporter.epicgames.com:57005" );
	}

	if (!GConfig->GetBool( TEXT( "CrashReportClient" ), TEXT( "bAllowToBeContacted" ), bAllowToBeContacted, GEngineIni ))
	{
		// Default to true when unattended when config is missing. This is mostly for dedicated servers that do not have config files for CRC.
		if (bUnattended)
		{
			bAllowToBeContacted = true;
		}
	}

	if (!GConfig->GetBool( TEXT( "CrashReportClient" ), TEXT( "bSendLogFile" ), bSendLogFile, GEngineIni ))
	{
		// Default to true when unattended when config is missing. This is mostly for dedicated servers that do not have config files for CRC.
		if (bUnattended)
		{
			bSendLogFile = true;
		}
	}
	
	ReadFullCrashDumpConfigurations();

	UE_LOG( CrashReportClientLog, Log, TEXT( "CrashReportReceiverIP: %s" ), *CrashReportReceiverIP );
}

void FCrashReportClientConfig::SetAllowToBeContacted( bool bNewValue )
{
	bAllowToBeContacted = bNewValue;
	GConfig->SetBool( *SectionName, TEXT( "bAllowToBeContacted" ), bAllowToBeContacted, GEngineIni );
}

void FCrashReportClientConfig::SetSendLogFile( bool bNewValue )
{
	bSendLogFile = bNewValue;
	GConfig->SetBool( *SectionName, TEXT( "bSendLogFile" ), bSendLogFile, GEngineIni );
}

const FString FCrashReportClientConfig::GetFullCrashDumpLocationForBranch( const FString& BranchName ) const
{
	for (const auto& It : FullCrashDumpConfigurations)
	{
		const bool bExactMatch = It.bExactMatch;
		if (bExactMatch && BranchName == It.BranchName)
		{
			return It.Location;
		}
		else if (!bExactMatch && BranchName.Contains( It.BranchName ))
		{
			return It.Location;
		}
	}

	return TEXT( "" );
}

FString FCrashReportClientConfig::GetKey( const FString& KeyName )
{
	FString Result;
	if (!GConfig->GetString( *SectionName, *KeyName, Result, GEngineIni ))
	{
		return TEXT( "" );
	}
	return Result;
}

void FCrashReportClientConfig::ReadFullCrashDumpConfigurations()
{
	for (int32 NumEntries = 0;; ++NumEntries)
	{
		FString Branch = GetKey( FString::Printf( TEXT( "FullCrashDump_%i_Branch" ), NumEntries ) );
		if (Branch.IsEmpty())
		{
			break;
		}

		const FString NetworkLocation = GetKey( FString::Printf( TEXT( "FullCrashDump_%i_Location" ), NumEntries ) );
		const bool bExactMatch = !Branch.EndsWith( TEXT( "*" ) );
		Branch.ReplaceInline( TEXT( "*" ), TEXT( "" ) );

		FullCrashDumpConfigurations.Add( FFullCrashDumpEntry( Branch, NetworkLocation, bExactMatch ) );

		UE_LOG( CrashReportClientLog, Log, TEXT( "FullCrashDump: %s, NetworkLocation: %s, bExactMatch:%i" ), *Branch, *NetworkLocation, bExactMatch );
	}
}
