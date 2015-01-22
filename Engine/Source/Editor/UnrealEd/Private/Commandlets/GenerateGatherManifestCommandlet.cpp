// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "ISourceControlModule.h"
#include "Internationalization/InternationalizationManifest.h"
#include "Json.h"
#include "JsonInternationalizationManifestSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateManifestCommandlet, Log, All);

/**
 *	UGenerateGatherManifestCommandlet
 */
UGenerateGatherManifestCommandlet::UGenerateGatherManifestCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGenerateGatherManifestCommandlet::Main( const FString& Params )
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine( *Params, Tokens, Switches, ParamVals );

	// Set config file.
	const FString* ParamVal = ParamVals.Find( FString( TEXT("Config") ) );
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No config specified.") );
		return -1;
	}

	// Set config section.
	ParamVal = ParamVals.Find( FString( TEXT("Section") ) );
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No config section specified.") );
		return -1;
	}

	// Get destination path.
	FString DestinationPath;
	if( !GetConfigString( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) )
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No destination path specified.") );
		return -1;
	}

	if (FPaths::IsRelative(DestinationPath))
	{
		if (!FPaths::GameDir().IsEmpty())
		{
			DestinationPath = FPaths::Combine( *( FPaths::GameDir() ), *DestinationPath );
		}
		else
		{
			DestinationPath = FPaths::Combine( *( FPaths::EngineDir() ), *DestinationPath );
		}
	}

	// Get manifest name.
	FString ManifestName;
	if( !GetConfigString( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) )
	{
		UE_LOG( LogGenerateManifestCommandlet, Error, TEXT("No manifest name specified.") );
		return -1;
	}

	//Grab any manifest dependencies
	TArray<FString> ManifestDependenciesList;
	GetConfigArray(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);

	for (FString& ManifestDependency : ManifestDependenciesList)
	{
		if (FPaths::IsRelative(ManifestDependency))
		{
			if (!FPaths::GameDir().IsEmpty())
			{
				ManifestDependency = FPaths::Combine( *( FPaths::GameDir() ), *ManifestDependency );
			}
			else
			{
				ManifestDependency = FPaths::Combine( *( FPaths::EngineDir() ), *ManifestDependency );
			}
		}
	}

	if( ManifestDependenciesList.Num() > 0 )
	{
		if( !ManifestInfo->AddManifestDependencies( ManifestDependenciesList ) )
		{
			UE_LOG(LogGenerateManifestCommandlet, Error, TEXT("The GenerateGatherManifest commandlet couldn't find all the specified manifest dependencies."));
			return -1;
		}
	
		ManifestInfo->ApplyManifestDependencies();
	}
	

	if( !WriteManifest( ManifestInfo->GetManifest(), DestinationPath / ManifestName ) )
	{
		UE_LOG( LogGenerateManifestCommandlet, Error,TEXT("Failed to write manifest to %s."), *DestinationPath );				
		return -1;
	}
	return 0;
}

bool UGenerateGatherManifestCommandlet::WriteManifest( const TSharedPtr<FInternationalizationManifest>& InManifest, const FString& OutputFilePath )
{
	// We can not continue if the provided manifest is not valid
	if( !InManifest.IsValid() )
	{
		return false;
	}
	FJsonInternationalizationManifestSerializer ManifestSerializer;
	TSharedRef<FJsonObject> JsonManifestObj = MakeShareable( new FJsonObject );
	
	bool bSuccess = ManifestSerializer.SerializeManifest( InManifest.ToSharedRef(), JsonManifestObj );
	
	if( bSuccess )
	{
		bSuccess = WriteJSONToTextFile( JsonManifestObj, OutputFilePath, SourceControlInfo );
	}
	return bSuccess;	
}
