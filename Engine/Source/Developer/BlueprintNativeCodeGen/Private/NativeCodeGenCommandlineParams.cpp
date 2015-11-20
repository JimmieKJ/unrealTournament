// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNativeCodeGenPCH.h"
#include "NativeCodeGenCommandlineParams.h"
#include "BlueprintNativeCodeGenManifest.h"

DEFINE_LOG_CATEGORY_STATIC(LogNativeCodeGenCommandline, Log, All);

/**  */
const FString FNativeCodeGenCommandlineParams::HelpMessage = TEXT("\n\
\n\
-------------------------------------------------------------------------------\n\
::  GenerateNativePluginFromBlueprint :: Converts Blueprint assets into C++    \n\
-------------------------------------------------------------------------------\n\
\n\
::                                                                             \n\
:: Usage                                                                       \n\
::                                                                             \n\
\n\
    UE4Editor.exe <project> -run=GenerateNativePluginFromBlueprint [parameters]\n\
\n\
::                                                                             \n\
:: Parameters                                                                  \n\
::                                                                             \n\
\n\
    -whitelist=<Assets> Identifies assets that you wish to convert. <Assets>   \n\
                        can be a comma seperated list, specifying multiple     \n\
                        packages (either directories, or explicit assets) in   \n\
                        the form: /Game/MyContentDir,/Engine/EngineAssetName.  \n\
\n\
    -blacklist=<Assets> Explicitly specifies assets that you don't want        \n\
                        converted (listed in the same manner as the -whitelist \n\
                        param). This takes priority over the whitelist (even if\n\
                        it results in uncompilable code).                      \n\
\n\
    -output=<TargetDir> Specifies the path where you want converted assets     \n\
                        saved to. If left unset, this will default to the      \n\
                        project's intermeadiate folder (within a sub-folder of \n\
                        its own). If specified as a relative path, it will be  \n\
                        interpreted as relative to the project directory.      \n\
\n\
    -pluginName=<Name>  Defines the generated plugin's name. If the output     \n\
                        directory doesn't match this in name, then a sub-folder\n\
                        will be created there to house the plugin.             \n\
\n\
    -noWipe             By default, we'll clear out the target directory from  \n\
                        previous conversions. However, with this, if the       \n\
						target directory contains source from a previous run,  \n\
                        we'll build atop the existing module and appends to its\n\
						existing manifest.                                     \n\
\n\
    -manifest=<Path>    Specifies where to save the resultant manifest file. If\n\
                        <Path> is not an existing directory or file, then it is\n\
                        assumed that this is a file path. If a manifest already\n\
                        exists here, then it'll be used to define the target   \n\
                        output directory (which will be read from the existing \n\
                        manifest file). If left unset then the manifest will   \n\
                        default to the plugin's directory.                     \n\
\n\
    -preview            When specified, the process will NOT wipe any existing \n\
                        files, and the target assets will NOT be compiled and  \n\
						converted. Instead, just a manifest file will be       \n\
						generated, detailing the command's predicted output.   \n\
\n\
    -help, -h, -?       Display this message and then exit.                    \n\
\n");

/*******************************************************************************
 * NativeCodeGenCommandlineParamsImpl
 ******************************************************************************/

namespace NativeCodeGenCommandlineParamsImpl
{
	static const FString DefaultPluginName(TEXT("GeneratedBpCode"));
	static const FString ParamListDelim(TEXT(","));

	/**
	 * Cleans up  the passed in path from the commandline (removes quotes, 
	 * corrects slashes, etc.), so that it is ready to be utilized by the 
	 * conversion process.
	 * 
	 * @param  PathInOut    The path you want standardized.
	 */
	static void SanitizePathParam(FString& PathInOut);
}
 
//------------------------------------------------------------------------------
static void NativeCodeGenCommandlineParamsImpl::SanitizePathParam(FString& PathInOut)
{
	PathInOut.RemoveFromStart(TEXT("\""));
	PathInOut.RemoveFromEnd(TEXT("\""));
	FPaths::NormalizeDirectoryName(PathInOut);
}

/*******************************************************************************
 * FNativeCodeGenCommandlineParams
 ******************************************************************************/

//------------------------------------------------------------------------------
FNativeCodeGenCommandlineParams::FNativeCodeGenCommandlineParams(const TArray<FString>& CommandlineSwitches)
	: bHelpRequested(false)
	, bWipeRequested(true)
	, bPreviewRequested(false)
{
	using namespace NativeCodeGenCommandlineParamsImpl;

	IFileManager& FileManager = IFileManager::Get();

	for (const FString& Param : CommandlineSwitches)
	{
		FString Switch = Param, Value;
		Param.Split(TEXT("="), &Switch, &Value);

		if (!Switch.Compare(TEXT("h"), ESearchCase::IgnoreCase) ||
			!Switch.Compare(TEXT("?"), ESearchCase::IgnoreCase) ||
			!Switch.Compare(TEXT("help"), ESearchCase::IgnoreCase))
		{
			bHelpRequested = true;
		}
		else if (!Switch.Compare(TEXT("whitelist"), ESearchCase::IgnoreCase)) 
		{
			Value.ParseIntoArray(WhiteListedAssetPaths, *NativeCodeGenCommandlineParamsImpl::ParamListDelim);
		}
		else if (!Switch.Compare(TEXT("blacklist"), ESearchCase::IgnoreCase))
		{
			Value.ParseIntoArray(BlackListedAssetPaths, *NativeCodeGenCommandlineParamsImpl::ParamListDelim);
		}
		else if (!Switch.Compare(TEXT("output"), ESearchCase::IgnoreCase))
		{
			SanitizePathParam(Value);
			// if it's a relative path, let's have it relative to the game 
			// directory (not the UE executable)
			if (FPaths::IsRelative(Value))
			{
				Value = FPaths::Combine(*FPaths::GameDir(), *Value);
			}

			if (FileManager.DirectoryExists(*Value) || FileManager.MakeDirectory(*Value))
			{
				OutputDir = Value;
			}
			else
			{
				UE_LOG(LogNativeCodeGenCommandline, Warning, TEXT("'%s' doesn't appear to be a valid output directory, reverting to the default."), *Value);
			}
		}
		else if (!Switch.Compare(TEXT("pluginName"), ESearchCase::IgnoreCase))
		{
			PluginName = Value;
		}
		else if (!Switch.Compare(TEXT("noWipe"), ESearchCase::IgnoreCase))
		{
			bWipeRequested = false;
		}
		else if (!Switch.Compare(TEXT("preview"), ESearchCase::IgnoreCase))
		{
			bPreviewRequested = true;
		}
		else if (!Switch.Compare(TEXT("manifest"), ESearchCase::IgnoreCase))
		{
			SanitizePathParam(Value);

			if (FileManager.FileExists(*Value))
			{
				ManifestFilePath = Value;
			}
			else if (FileManager.DirectoryExists(*Value))
			{
				ManifestFilePath = FPaths::Combine(*Value, *FBlueprintNativeCodeGenManifest::GetDefaultFilename());
			}
			else
			{
				UE_LOG(LogNativeCodeGenCommandline, Display, TEXT("Unsure if '%s' is supposed to denote a directory or a filename, assuming it's a file."), *Value);

				FString FilePath = FPaths::GetPath(*Value);
				if (FileManager.DirectoryExists(*FilePath) || FileManager.MakeDirectory(*FilePath))
				{
					ManifestFilePath = Value;
				}
				else
				{
					if (FPaths::IsRelative(Value))
					{
						Value = FPaths::ConvertRelativePathToFull(Value);
					}
					UE_LOG( LogNativeCodeGenCommandline, Warning, TEXT("'%s' doesn't appear to be a valid manifest file name/path, defaulting to: '%s'."), 
						*Value,	*FBlueprintNativeCodeGenManifest::GetDefaultFilename() );
				}
			}
		}
		else
		{
			//UE_LOG(LogNativeCodeGenCommandline, Warning, TEXT("Unrecognized commandline parameter: %s"), *Switch);
		}
	}

	bool const bUtilizeExistingManifest = !ManifestFilePath.IsEmpty() && !bWipeRequested && FileManager.FileExists(*ManifestFilePath);
	// an existing manifest would specify where to put the plugin
	if (!bUtilizeExistingManifest)
	{
		if (PluginName.IsEmpty())
		{
			PluginName = NativeCodeGenCommandlineParamsImpl::DefaultPluginName;
		}
		if (OutputDir.IsEmpty())
		{
			const FString DefaultOutputPath = FPaths::Combine(*FPaths::Combine(*FPaths::GameIntermediateDir(), TEXT("Plugins")), *PluginName);
			OutputDir = DefaultOutputPath;
		}

		if ( PluginName.Compare(FPaths::GetBaseFilename(OutputDir)) )
		{
			OutputDir = FPaths::Combine(*OutputDir, *PluginName);
		}
	}	
}

