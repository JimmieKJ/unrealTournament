// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "TextLocalizationResourceGenerator.h"
#include "Json.h"
#include "InternationalizationManifest.h"
#include "InternationalizationArchive.h"
#include "JsonInternationalizationManifestSerializer.h"
#include "JsonInternationalizationArchiveSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateTextLocalizationResourceCommandlet, Log, All);

UGenerateTextLocalizationResourceCommandlet::UGenerateTextLocalizationResourceCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGenerateTextLocalizationResourceCommandlet::Main(const FString& Params)
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Set config file.
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	// Set config section.
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// Get source path.
	FString SourcePath;
	if( !( GetPathFromConfig( *SectionName, TEXT("SourcePath"), SourcePath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Get manifest name.
	FString ManifestName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No manifest name specified."));
		return -1;
	}

	// Get cultures to generate.
	FString NativeCultureName;
	if( !( GetStringFromConfig( *SectionName, TEXT("NativeCulture"), NativeCultureName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No native culture specified."));
		return -1;
	}

	// Get cultures to generate.
	TArray<FString> CulturesToGenerate;
	GetStringArrayFromConfig( *SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, GatherTextConfigPath );

	if( CulturesToGenerate.Num() == 0 )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No cultures specified for generation."));
		return -1;
	}

	for(int32 i = 0; i < CulturesToGenerate.Num(); ++i)
	{
		if( FInternationalization::Get().GetCulture( CulturesToGenerate[i] ).IsValid() )
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(CulturesToGenerate[i]));
		}
	}

	// Get destination path.
	FString DestinationPath;
	if( !( GetPathFromConfig( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Get resource name.
	FString ResourceName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ResourceName"), ResourceName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No resource name specified."));
		return -1;
	}

	// Read the manifest file from the source path.
	FString ManifestFilePath = (SourcePath / ManifestName);
	ManifestFilePath = FPaths::ConvertRelativePathToFull(ManifestFilePath);
	TSharedPtr<FJsonObject> ManifestJSONObject = ReadJSONTextFile(ManifestFilePath);
	if( !(ManifestJSONObject.IsValid()) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No manifest found at %s."), *ManifestFilePath);
		return -1;
	}
	TSharedRef<FInternationalizationManifest> InternationalizationManifest = MakeShareable( new FInternationalizationManifest );
	{
		FJsonInternationalizationManifestSerializer ManifestSerializer;
		ManifestSerializer.DeserializeManifest(ManifestJSONObject.ToSharedRef(), InternationalizationManifest);
	}

	// For each culture:
	for(int32 Culture = 0; Culture < CulturesToGenerate.Num(); Culture++)
	{
		const FString CultureName = *(CulturesToGenerate[Culture]);
		const FString CulturePath = SourcePath / CultureName;

		// Write resource.
		const FString TextLocalizationResourcePath = DestinationPath / CultureName / ResourceName;

		if( SourceControlInfo.IsValid() )
		{
			FText SCCErrorText;
			if (!SourceControlInfo->CheckOutFile(TextLocalizationResourcePath, SCCErrorText))
			{
				UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("Check out of file %s failed: %s"), *TextLocalizationResourcePath, *SCCErrorText.ToString());
				return -1;
			}
		}

		TAutoPtr<FArchive> TextLocalizationResourceArchive(IFileManager::Get().CreateFileWriter(*TextLocalizationResourcePath));
		if (TextLocalizationResourceArchive)
		{
			FJsonInternationalizationArchiveSerializer ArchiveSerializer;

			if( !(FTextLocalizationResourceGenerator::Generate(SourcePath, InternationalizationManifest, NativeCultureName, CultureName, TextLocalizationResourceArchive, ArchiveSerializer)) )
			{
				IFileManager::Get().Delete( *TextLocalizationResourcePath );
			}
			TextLocalizationResourceArchive->Close();
		}
	}

	return 0;
}
