// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CrashReportClientApp.h"
#include "GenericPlatformContext.h"

#include "EngineVersion.h"
#include "XmlFile.h"
#include "CrashDescription.h"
#include "CrashReportAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

FCrashDescription::FCrashDescription() :
	CrashDescriptionVersion( ECrashDescVersions::VER_1_NewCrashFormat ),
	BuiltFromCL( -1 ),
	TimeOfCrash( FDateTime::UtcNow() ),
	bHasMiniDumpFile( false ),
	bHasLogFile( false ),
	bHasVideoFile( false ),
	bHasCompleteData( false )
{
	InitializeIDs();
}

/** Unescapes a specified XML string, naive implementation. */
FString UnescapeXMLString( const FString& Text )
{
	
	return Text
		.Replace( TEXT( "&amp;" ), TEXT( "&" ) )
		.Replace( TEXT( "&quot;" ), TEXT( "\"" ) )
		.Replace( TEXT( "&lt;" ), TEXT( "<" ) )
		.Replace( TEXT( "&gt;" ), TEXT( ">" ) );
}

FCrashDescription::FCrashDescription( FString WERXMLFilepath ) :
	// WER XML files are forced to be in the first version
	CrashDescriptionVersion( ECrashDescVersions::VER_1_NewCrashFormat ),
	BuiltFromCL( -1 ),
	TimeOfCrash( FDateTime::UtcNow() ),
	bHasMiniDumpFile( false ),
	bHasLogFile( false ),
	bHasVideoFile( false ),
	bHasCompleteData( false )
{
	InitializeIDs();

	// This is for the current system that uses files from Windows Error Reporting.
	// Will be replaced with the unified version soon.
	FXmlFile XmlFile( WERXMLFilepath );

	ReportName = FPaths::GetPath( WERXMLFilepath );
	FPaths::NormalizeDirectoryName( ReportName );
	// Grab the last component...
	ReportName = FPaths::GetCleanFilename( ReportName );

	const bool bIsValid = XmlFile.IsValid();
	if( bIsValid )
	{
		const FXmlNode* OSVersionInformationNode = XmlFile.GetRootNode()->FindChildNode( TEXT( "OSVersionInformation" ) );
		const FXmlNode* ProblemSignaturesNode = XmlFile.GetRootNode()->FindChildNode( TEXT( "ProblemSignatures" ) );
		const FXmlNode* DynamicSignaturesNode = XmlFile.GetRootNode()->FindChildNode( TEXT( "DynamicSignatures" ) );

		FString Product;
		int EngineVersionComponents = 0;

		if( OSVersionInformationNode )
		{
			const FXmlNode* ProductNode = OSVersionInformationNode->FindChildNode( TEXT( "Product" ) );
			if( ProductNode )
			{
				Product = ProductNode->GetContent();
			}
		}

		if( ProblemSignaturesNode )
		{
			const FXmlNode* GameNameNode = ProblemSignaturesNode->FindChildNode( TEXT( "Parameter0" ) );
			if( GameNameNode )
			{
				GameName = GameNameNode->GetContent();
			}

			const FXmlNode* BuildVersionNode = ProblemSignaturesNode->FindChildNode( TEXT( "Parameter1" ) );
			if( BuildVersionNode )
			{
				BuildVersion = BuildVersionNode->GetContent();
				EngineVersionComponents++;
			}

			const FXmlNode* Parameter8Node = ProblemSignaturesNode->FindChildNode( TEXT( "Parameter8" ) );
			if( Parameter8Node )
			{
				const FString Parameter8Value = Parameter8Node->GetContent();

				TArray<FString> ParsedParameters8;
				Parameter8Value.ParseIntoArray( ParsedParameters8, TEXT( "!" ), false );

				if( ParsedParameters8.Num() > 1 )
				{
					CommandLine = UnescapeXMLString( ParsedParameters8[1] );
				}

				if( ParsedParameters8.Num() > 2 )
				{
					ErrorMessage.Add( ParsedParameters8[2] );
				}
			}

			const FXmlNode* Parameter9Node = ProblemSignaturesNode->FindChildNode( TEXT( "Parameter9" ) );
			if( Parameter9Node )
			{
				const FString Parameter9Value = Parameter9Node->GetContent();

				TArray<FString> ParsedParameters9;
				Parameter9Value.ParseIntoArray( ParsedParameters9, TEXT( "!" ), false );

				if( ParsedParameters9.Num() > 0 )
				{
					BranchName = ParsedParameters9[0].Replace( TEXT( "+" ), TEXT( "/" ) );

					const FString DepotRoot = TEXT( "//depot/" );
					if( BranchName.StartsWith( DepotRoot ) )
					{
						BranchName = BranchName.Mid( DepotRoot.Len() );
					}
					EngineVersionComponents++;
				}

				if( ParsedParameters9.Num() > 1 )
				{
					const FString BaseDirectory = ParsedParameters9[1];

					TArray<FString> SubDirs;
					BaseDirectory.ParseIntoArray( SubDirs, TEXT( "/" ), true );
					const int SubDirsNum = SubDirs.Num();
					const FString PlatformName = SubDirsNum > 0 ? SubDirs[SubDirsNum - 1] : TEXT("");
					if( Product.Len() > 0 )
					{
						Platform = FString::Printf( TEXT( "%s [%s]" ), *PlatformName, *Product );
					}
					else
					{
						Platform = PlatformName;
					}
				}

				if( ParsedParameters9.Num() > 2 )
				{
					EngineMode = ParsedParameters9[2];
				}

				if( ParsedParameters9.Num() > 3 )
				{
					TTypeFromString<uint32>::FromString( BuiltFromCL, *ParsedParameters9[3] );
					EngineVersionComponents++;
				}
			}
		}

		if( DynamicSignaturesNode )
		{
			const FXmlNode* LanguageNode = DynamicSignaturesNode->FindChildNode( TEXT( "Parameter2" ) );
			if( LanguageNode )
			{
				LanguageLCID = LanguageNode->GetContent();
			}
		}

		// We have all three components of the engine version, so initialize it.
		if( EngineVersionComponents == 3 )
		{
			InitializeEngineVersion();
		}

		bHasCompleteData = true;
	}
	else
	{
		bHasCompleteData = false;
	}
}

void FCrashDescription::InitializeEngineVersion()
{
	uint16 Major = 0;
	uint16 Minor = 0;
	uint16 Patch = 0;

	TArray<FString> ParsedBuildVersion;
	BuildVersion.ParseIntoArray( ParsedBuildVersion, TEXT( "." ), false );

	if( ParsedBuildVersion.Num() >= 3 )
	{
		TTypeFromString<uint16>::FromString( Patch, *ParsedBuildVersion[2] );
	}

	if( ParsedBuildVersion.Num() >= 2 )
	{
		TTypeFromString<uint16>::FromString( Minor, *ParsedBuildVersion[1] );
	}

	if( ParsedBuildVersion.Num() >= 1 )
	{
		TTypeFromString<uint16>::FromString( Major, *ParsedBuildVersion[0] );
	}

	EngineVersion = FEngineVersion( Major, Minor, Patch, BuiltFromCL, BranchName );
}

void FCrashDescription::InitializeIDs()
{
	MachineId = FPlatformMisc::GetMachineId().ToString( EGuidFormats::Digits );

	// The Epic ID can be looked up from this ID.
	EpicAccountId = FPlatformMisc::GetEpicAccountId();

	// Remove periods from user names to match AutoReporter user names
	// The name prefix is read by CrashRepository.AddNewCrash in the website code
	UserName = FString( FPlatformProcess::UserName() ).Replace( TEXT( "." ), TEXT( "" ) );
}

void FCrashDescription::SendAnalytics()
{
	// Connect the crash report client analytics provider.
	FCrashReportAnalytics::Initialize();

	IAnalyticsProvider& Analytics = FCrashReportAnalytics::GetProvider();
	//Analytics.SetUserID( MachineId );

	TArray<FAnalyticsEventAttribute> CrashAttributes;

	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "bHasCompleteData" ), bHasCompleteData ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "CrashDescriptionVersion" ), (int32)CrashDescriptionVersion ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "ReportName" ), ReportName ) );

	//	AppID = GameName
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "GameName" ), GameName ) );

	//	AppVersion = EngineVersion
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "EngineVersion" ), EngineVersion.ToString() ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "BuildVersion" ), BuildVersion ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "BuiltFromCL" ), BuiltFromCL ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "BranchName" ), BranchName ) );

	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "MachineID" ), MachineId ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "UserName" ), UserName ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "EpicAccountId" ), EpicAccountId ) );

	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "Platform" ), Platform ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "TimeOfCrash" ), TimeOfCrash.GetTicks() ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "EngineMode" ), EngineMode ) );
	CrashAttributes.Add( FAnalyticsEventAttribute( TEXT( "LanguageLCID" ), LanguageLCID ) );

	Analytics.RecordEvent( TEXT( "CrashReportClient.ReportCrash" ), CrashAttributes );

	// Shutdown analytics.
	FCrashReportAnalytics::Shutdown();
}
