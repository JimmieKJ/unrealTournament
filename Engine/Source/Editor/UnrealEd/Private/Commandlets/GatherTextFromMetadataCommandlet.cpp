// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "SourceCodeNavigation.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromMetaDataCommandlet, Log, All);

//////////////////////////////////////////////////////////////////////////
//GatherTextFromMetaDataCommandlet

UGatherTextFromMetaDataCommandlet::UGatherTextFromMetaDataCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGatherTextFromMetaDataCommandlet::Main( const FString& Params )
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	//Set config file
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;
	
	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	//Set config section
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	//Modules to Preload
	TArray<FString> ModulesToPreload;
	GetConfigArray(*SectionName, TEXT("ModulesToPreload"), ModulesToPreload, GatherTextConfigPath);

	for (const FString& ModuleName : ModulesToPreload)
	{
		FModuleManager::Get().LoadModule(*ModuleName);
	}

	//Include paths
	TArray<FString> IncludePaths;
	GetConfigArray(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);

	if (IncludePaths.Num() == 0)
	{
		UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("No include paths in section %s"), *SectionName);
		return -1;
	}

	for (FString& IncludePath : IncludePaths)
	{
		FPaths::NormalizeDirectoryName(IncludePath);
		if (FPaths::IsRelative(IncludePath))
		{
			if (!FPaths::GameDir().IsEmpty())
			{
				IncludePath = FPaths::Combine( *( FPaths::GameDir() ), *IncludePath );
			}
			else
			{
				IncludePath = FPaths::Combine( *( FPaths::EngineDir() ), *IncludePath );
			}
		}

		IncludePath = FPaths::ConvertRelativePathToFull(IncludePath);

		// All paths must ends with "/*"
		if ( !IncludePath.EndsWith(TEXT("/*")) )
		{
			// If it ends in a slash, add the star.
			if ( IncludePath.EndsWith(TEXT("/")) )
			{
				IncludePath.AppendChar(TEXT('*'));
			}
			// If it doesn't end in a slash or slash star, just add slash star.
			else
			{
				IncludePath.Append(TEXT("/*"));
			}
		}
	}

	//Exclude paths
	TArray<FString> ExcludePaths;
	GetConfigArray(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);

	//Required module names
	TArray<FString> RequiredModuleNames;
	GetConfigArray(*SectionName, TEXT("RequiredModuleNames"), RequiredModuleNames, GatherTextConfigPath);

	// Pre-load all required modules so that UFields from those modules can be guaranteed a chance to have their metadata gathered.
	for(const FString& RequiredModuleName : RequiredModuleNames)
	{
		FModuleManager::Get().LoadModule(*RequiredModuleName);
	}

	FGatherParameters Arguments;
	GetConfigArray(*SectionName, TEXT("InputKeys"), Arguments.InputKeys, GatherTextConfigPath);
	GetConfigArray(*SectionName, TEXT("OutputNamespaces"), Arguments.OutputNamespaces, GatherTextConfigPath);
	TArray<FString> OutputKeys;
	GetConfigArray(*SectionName, TEXT("OutputKeys"), OutputKeys, GatherTextConfigPath);
	for(const auto& OutputKey : OutputKeys)
	{
		Arguments.OutputKeys.Add(FText::FromString(OutputKey));
	}

	// Execute gather.
	GatherTextFromUObjects(IncludePaths, ExcludePaths, Arguments);

	// Add any manifest dependencies if they were provided
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

	if( !ManifestInfo->AddManifestDependencies( ManifestDependenciesList ) )
	{
		UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("The GatherTextFromMetaData commandlet couldn't find all the specified manifest dependencies."));
		return -1;
	}

	return 0;
}

void UGatherTextFromMetaDataCommandlet::GatherTextFromUObjects(const TArray<FString>& IncludePaths, const TArray<FString>& ExcludePaths, const FGatherParameters& Arguments)
{
	for(TObjectIterator<UField> It; It; ++It)
	{
		FString SourceFilePath;
		FSourceCodeNavigation::FindClassHeaderPath(*It, SourceFilePath);
		SourceFilePath = FPaths::ConvertRelativePathToFull(SourceFilePath);

		// Returns true if in an include path. False otherwise.
		auto IncludePathLogic = [&]() -> bool
		{
			for(int32 i = 0; i < IncludePaths.Num(); ++i)
			{
				if(SourceFilePath.MatchesWildcard(IncludePaths[i]))
				{
					return true;
				}
			}
			return false;
		};
		if(!IncludePathLogic())
		{
			continue;
		}

		// Returns true if in an exclude path. False otherwise.
		auto ExcludePathLogic = [&]() -> bool
		{
			for(int32 i = 0; i < ExcludePaths.Num(); ++i)
			{
				if(SourceFilePath.MatchesWildcard(ExcludePaths[i]))
				{
					return true;
				}
			}
			return false;
		};
		if(ExcludePathLogic())
		{
			continue;
		}

		GatherTextFromUObject(*It, Arguments);
	}
}

void UGatherTextFromMetaDataCommandlet::GatherTextFromUObject(UField* const Field, const FGatherParameters& Arguments)
{
	// Gather for object.
	{
		if( !Field->HasMetaData( TEXT("DisplayName") ) )
		{
			Field->SetMetaData( TEXT("DisplayName"), *FName::NameToDisplayString( Field->GetName(), Field->IsA( UBoolProperty::StaticClass() ) ) );
		}

		for(int32 i = 0; i < Arguments.InputKeys.Num(); ++i)
		{
			FFormatNamedArguments PatternArguments;
			PatternArguments.Add( TEXT("FieldPath"), FText::FromString( Field->GetFullGroupName(true) + TEXT(".") + Field->GetName() ) );

			if( Field->HasMetaData( *Arguments.InputKeys[i] ) )
			{
				const FString& MetaDataValue = Field->GetMetaData(*Arguments.InputKeys[i]);
				if( !MetaDataValue.IsEmpty() )
				{
					PatternArguments.Add( TEXT("MetaDataValue"), FText::FromString(MetaDataValue) );

					const FString Namespace = Arguments.OutputNamespaces[i];
					FLocItem LocItem(MetaDataValue);
					FContext Context;
					Context.Key = FText::Format(Arguments.OutputKeys[i], PatternArguments).ToString();
					Context.SourceLocation = FString::Printf(TEXT("From metadata for key %s of member %s in %s"), *Arguments.InputKeys[i], *Field->GetName(), *Field->GetFullGroupName(true));
					ManifestInfo->AddEntry(TEXT("EntryDescription"), Namespace, LocItem, Context);
				}
			}
		}
	}

	// For enums, also gather for enum values.
	{
		UEnum* Enum = Cast<UEnum>(Field);
		if(Enum)
		{
			const int32 ValueCount = Enum->NumEnums();
			for(int32 i = 0; i < ValueCount; ++i)
			{
				if( !Enum->HasMetaData(TEXT("DisplayName"), i) )
				{
					Enum->SetMetaData(TEXT("DisplayName"), *FName::NameToDisplayString(Enum->GetEnumName(i), false), i);
				}

				for(int32 j = 0; j < Arguments.InputKeys.Num(); ++j)
				{
					FFormatNamedArguments PatternArguments;
					PatternArguments.Add( TEXT("FieldPath"), FText::FromString( Enum->GetFullGroupName(true) + TEXT(".") + Enum->GetName() + TEXT(".") + Enum->GetEnumName(i) ) );

					if( Enum->HasMetaData(*Arguments.InputKeys[j], i) )
					{
						const FString& MetaDataValue = Enum->GetMetaData(*Arguments.InputKeys[j], i);
						if( !MetaDataValue.IsEmpty() )
						{
							PatternArguments.Add( TEXT("MetaDataValue"), FText::FromString(MetaDataValue) );

							const FString Namespace = Arguments.OutputNamespaces[j];
							FLocItem LocItem(MetaDataValue);
							FContext Context;
							Context.Key = FText::Format(Arguments.OutputKeys[j], PatternArguments).ToString();
							Context.SourceLocation = FString::Printf(TEXT("From metadata for key %s of enum value %s of enum %s in %s"), *Arguments.InputKeys[j], *Enum->GetEnumName(i), *Enum->GetName(), *Enum->GetFullGroupName(true));
							ManifestInfo->AddEntry(TEXT("EntryDescription"), Namespace, LocItem, Context);
						}
					}
				}
			}
		}
	}
}