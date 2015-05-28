// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Misc/App.h"
#include "ConfigCacheIni.h"
#include "RemoteConfigIni.h"
#include "AES.h"
#include "SecureHash.h"
#include "DefaultValueHelper.h"
#include "EngineBuildSettings.h"
#include "Paths.h"
#include "ConfigManifest.h"

#if WITH_EDITOR
	#define INI_CACHE 1
#else
	#define INI_CACHE 0
#endif

DEFINE_LOG_CATEGORY(LogConfig);
namespace 
{
	FString GenerateHierarchyCacheKey(const FConfigFileHierarchy& IniHierarchy, const FString& IniPath, const FString& BaseIniName)
	{
#if !INI_CACHE
		return TEXT("");
#else
		// A Hierarchy Key is a combined list of all ini file paths that affect that inis data set.
		FString HierKey;
		//
		auto KeyLen = IniPath.Len();
		KeyLen += BaseIniName.Len();
		for (const auto& Ini : IniHierarchy)
		{
			KeyLen += Ini.Value.Filename.Len();
		}
		HierKey.Reserve(KeyLen);
		HierKey += BaseIniName;
		for (const auto& Ini : IniHierarchy)
		{
			HierKey += Ini.Value.Filename;
		}
		HierKey += IniPath;

		return HierKey;
#endif
	}
#if INI_CACHE
	TMap<FString, FConfigFile> HierarchyCache;
#endif
}

class FTextFriendHelper
{
	friend class FConfigFile;
	friend class FConfigCacheIni;

	static FString AsString( const FText& Text )
	{
		if( Text.IsTransient() )
		{
			UE_LOG( LogConfig, Warning, TEXT( "FTextFriendHelper::AsString() Transient FText") );
			return FString(TEXT("Error: Transient FText"));
		}

		FString Str;
		if( FTextInspector::GetSourceString(Text) )
		{
			FString SourceString( *FTextInspector::GetSourceString(Text) );
			for( auto Iter( SourceString.CreateConstIterator()); Iter; ++Iter )
			{
				const TCHAR Ch = *Iter;
				if( Ch == '\"' )
				{
					Str += TEXT("\\\"");
				}
				else if( Ch == '\t' )
				{
					Str += TEXT("\\t");
				}
				else if( Ch == '\r' )
				{
					Str += TEXT("\\r");
				}
				else if( Ch == '\n' )
				{
					Str += TEXT("\\n");
				}
				else if( Ch == '\\' )
				{
					Str += TEXT("\\\\");
				}
				else
				{
					Str += Ch;
				}
			}
		}

		//this prevents our source code text gatherer from trying to gather the following messages
#define LOC_DEFINE_REGION
		if( Text.IsCultureInvariant() )
		{
			return FString::Printf( TEXT( "NSLOCTEXT(\"\",\"\",\"%s\")" ), *Str );
		}
		else
		{
			return FString::Printf( TEXT( "NSLOCTEXT(\"%s\",\"%s\",\"%s\")" ),
				*FTextInspector::GetNamespace(Text).Get(TEXT("")), *FTextInspector::GetKey(Text).Get(TEXT("")), *Str );
		}
#undef LOC_DEFINE_REGION
	}
};

/*-----------------------------------------------------------------------------
	FConfigSection
-----------------------------------------------------------------------------*/

bool FConfigSection::HasQuotes( const FString& Test ) const
{
	return Test.Left(1) == TEXT("\"") && Test.Right(1) == TEXT("\"");
}

bool FConfigSection::operator==( const FConfigSection& Other ) const
{
	if ( Pairs.Num() != Other.Pairs.Num() )
		return 0;

	FConfigSectionMap::TConstIterator My(*this), Their(Other);
	while ( My && Their )
	{
		if (My.Key() != Their.Key())
			return 0;

		const FString& MyValue = My.Value(), &TheirValue = Their.Value();
		if ( FCString::Strcmp(*MyValue,*TheirValue) &&
			(!HasQuotes(MyValue) || FCString::Strcmp(*TheirValue,*MyValue.Mid(1,MyValue.Len()-2))) &&
			(!HasQuotes(TheirValue) || FCString::Strcmp(*MyValue,*TheirValue.Mid(1,TheirValue.Len()-2))) )
			return 0;

		++My, ++Their;
	}
	return 1;
}

bool FConfigSection::operator!=( const FConfigSection& Other ) const
{
	return ! (FConfigSection::operator==(Other));
}

/*-----------------------------------------------------------------------------
	FConfigFile
-----------------------------------------------------------------------------*/
FConfigFile::FConfigFile()
: Dirty( false )
, NoSave( false )
, Name( NAME_None )
, SourceConfigFile(NULL)
{}
	
FConfigFile::~FConfigFile()
{
	if( SourceConfigFile != NULL )
	{
		delete SourceConfigFile;
		SourceConfigFile = NULL;
	}
}

bool FConfigFile::operator==( const FConfigFile& Other ) const
{
	if ( Pairs.Num() != Other.Pairs.Num() )
		return 0;

	for ( TMap<FString,FConfigSection>::TConstIterator It(*this), OtherIt(Other); It && OtherIt; ++It, ++OtherIt)
	{
		if ( It.Key() != OtherIt.Key() )
			return 0;

		if ( It.Value() != OtherIt.Value() )
			return 0;
	}

	return 1;
}

bool FConfigFile::operator!=( const FConfigFile& Other ) const
{
	return ! (FConfigFile::operator==(Other));
}

bool FConfigFile::Combine(const FString& Filename)
{
	FString Text;
	// note: we don't check if FileOperations are disabled because downloadable content calls this directly (which
	// needs file ops), and the other caller of this is already checking for disabled file ops
	if( FFileHelper::LoadFileToString( Text, *Filename ) )
	{
		CombineFromBuffer(Text);
		return true;
	}

	return false;
}


void FConfigFile::CombineFromBuffer(const FString& Buffer)
{
	// Replace %GAME% with game name.
	FString Text = Buffer.Replace(TEXT("%GAME%"), FApp::GetGameName(), ESearchCase::CaseSensitive);

	// Replace %GAMEDIR% with the game directory.
	Text = Text.Replace( TEXT("%GAMEDIR%"), *FPaths::GameDir(), ESearchCase::CaseSensitive );

	// Replace %ENGINEUSERDIR% with the user's engine directory.
	Text = Text.Replace(TEXT("%ENGINEUSERDIR%"), *FPaths::EngineUserDir(), ESearchCase::CaseSensitive);

	// Replace %ENGINEVERSIONAGNOSTICUSERDIR% with the user's engine directory.
	Text = Text.Replace(TEXT("%ENGINEVERSIONAGNOSTICUSERDIR%"), *FPaths::EngineVersionAgnosticUserDir(), ESearchCase::CaseSensitive);

	// Replace %APPSETTINGSDIR% with the game directory.
	FString AppSettingsDir = FPlatformProcess::ApplicationSettingsDir();
	FPaths::NormalizeFilename(AppSettingsDir);
	Text = Text.Replace( TEXT("%APPSETTINGSDIR%"), *AppSettingsDir, ESearchCase::CaseSensitive );

	const TCHAR* Ptr = *Text;
	FConfigSection* CurrentSection = NULL;
	bool Done = false;
	while( !Done )
	{
		// Advance past new line characters
		while( *Ptr=='\r' || *Ptr=='\n' )
		{
			Ptr++;
		}

		// read the next line
		FString TheLine;
		int32 LinesConsumed = 0;
		FParse::LineExtended(&Ptr, TheLine, LinesConsumed, false);
		if (Ptr == NULL || *Ptr == 0)
		{
			Done = true;
		}
		TCHAR* Start = const_cast<TCHAR*>(*TheLine);

		// Strip trailing spaces from the current line
		while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
		{
			Start[FCString::Strlen(Start)-1] = 0;
		}

		// If the first character in the line is [ and last char is ], this line indicates a section name
		if( *Start=='[' && Start[FCString::Strlen(Start)-1]==']' )
		{
			// Remove the brackets
			Start++;
			Start[FCString::Strlen(Start)-1] = 0;

			// If we don't have an existing section by this name, add one
			CurrentSection = Find( Start );
			if( !CurrentSection )
			{
				CurrentSection = &Add( Start, FConfigSection() );
			}
		}

		// Otherwise, if we're currently inside a section, and we haven't reached the end of the stream
		else if( CurrentSection && *Start )
		{
			TCHAR* Value = 0;

			// ignore [comment] lines that start with ;
			if(*Start != (TCHAR)';')
			{
				Value = FCString::Strstr(Start,TEXT("="));
			}

			// Ignore any lines that don't contain a key-value pair
			if( Value )
			{
				// Terminate the property name, advancing past the =
				*Value++ = 0;

				// strip leading whitespace from the property name
				while ( *Start && FChar::IsWhitespace(*Start) )
				{						
					Start++;
				}

				// determine how this line will be merged
				TCHAR Cmd = Start[0];
				if ( Cmd=='+' || Cmd=='-' || Cmd=='.' || Cmd=='!' )
				{
					Start++;
				}
				else
				{
					Cmd=' ';
				}

				// Strip trailing spaces from the property name.
				while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
				{
					Start[FCString::Strlen(Start)-1] = 0;
				}

				FString ProcessedValue;

				// Strip leading whitespace from the property value
				while ( *Value && FChar::IsWhitespace(*Value) )
				{
					Value++;
				}

				// strip trailing whitespace from the property value
				while( *Value && FChar::IsWhitespace(Value[FCString::Strlen(Value)-1]) )
				{
					Value[FCString::Strlen(Value)-1] = 0;
				}

				// If this line is delimited by quotes
				if( *Value=='\"' )
				{
					Value++;
					//epic moelfke: fixed handling of escaped characters in quoted string
					while (*Value && *Value != '\"')
					{
						if (*Value != '\\') // unescaped character
						{
							ProcessedValue += *Value++;
						}
						else if (*++Value == '\\') // escaped forward slash "\\"
						{
							ProcessedValue += '\\';
							Value++;
						}
						else if (*Value == '\"') // escaped double quote "\""
						{
							ProcessedValue += '\"';
							Value++;
						}
						else if ( *Value == TEXT('n') )
						{
							ProcessedValue += TEXT('\n');
							Value++;
						}
						else if( *Value == TEXT('u') && Value[1] && Value[2] && Value[3] && Value[4] )	// \uXXXX - UNICODE code point
						{
							ProcessedValue += FParse::HexDigit(Value[1])*(1<<12) + FParse::HexDigit(Value[2])*(1<<8) + FParse::HexDigit(Value[3])*(1<<4) + FParse::HexDigit(Value[4]);
							Value += 5;
						}
						else if( Value[1] ) // some other escape sequence, assume it's a hex character value
						{
							ProcessedValue += FParse::HexDigit(Value[0])*16 + FParse::HexDigit(Value[1]);
							Value += 2;
						}
					}
				}
				else
				{
					ProcessedValue = Value;
				}

				if( Cmd=='+' ) 
				{
					// Add if not already present.
					CurrentSection->AddUnique( Start, *ProcessedValue );
				}
				else if( Cmd=='-' )	
				{
					// Remove if present.
					CurrentSection->RemoveSingle( Start, *ProcessedValue );
					CurrentSection->Compact();
				}
				else if ( Cmd=='.' )
				{
					CurrentSection->Add( Start, *ProcessedValue );
				}
				else if( Cmd=='!' )
				{
					CurrentSection->Remove( Start );
				}
				else
				{
					// Add if not present and replace if present.
					FString* Str = CurrentSection->Find( Start );
					if( !Str )
					{
						CurrentSection->Add( Start, *ProcessedValue );
					}
					else
					{
						*Str = ProcessedValue;
					}
				}

				// Mark as dirty so "Write" will actually save the changes.
				Dirty = 1;
			}
		}
	}

	// Avoid memory wasted in array slack.
	Shrink();
	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		It.Value().Shrink();
	}
}

/**
 * Process the contents of an .ini file that has been read into an FString
 * 
 * @param Contents Contents of the .ini file
 */
void FConfigFile::ProcessInputFileContents(const FString& Contents)
{
	// Replace %GAME% with game name.
	FString Text = Contents.Replace(TEXT("%GAME%"), FApp::GetGameName(), ESearchCase::CaseSensitive);

	// Replace %GAMEDIR% with the game directory.
	Text = Text.Replace( TEXT("%GAMEDIR%"), *FPaths::GameDir(), ESearchCase::CaseSensitive );

	// Replace %ENGINEUSERDIR% with the user's engine directory.
	Text = Text.Replace(TEXT("%ENGINEUSERDIR%"), *FPaths::EngineUserDir(), ESearchCase::CaseSensitive);

	// Replace %APPSETTINGSDIR% with the game directory.
	FString AppSettingsDir = FPlatformProcess::ApplicationSettingsDir();
	FPaths::NormalizeFilename(AppSettingsDir);
	Text = Text.Replace( TEXT("%APPSETTINGSDIR%"), *AppSettingsDir, ESearchCase::CaseSensitive );

	const TCHAR* Ptr = Text.Len() > 0 ? *Text : NULL;
	FConfigSection* CurrentSection = NULL;
	bool Done = false;
	while( !Done && Ptr != NULL )
	{
		// Advance past new line characters
		while( *Ptr=='\r' || *Ptr=='\n' )
		{
			Ptr++;
		}			
		// read the next line
		FString TheLine;
		int32 LinesConsumed = 0;
		FParse::LineExtended(&Ptr, TheLine, LinesConsumed, false);
		if (Ptr == NULL || *Ptr == 0)
		{
			Done = true;
		}
		TCHAR* Start = const_cast<TCHAR*>(*TheLine);

		// Strip trailing spaces from the current line
		while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
		{
			Start[FCString::Strlen(Start)-1] = 0;
		}

		// If the first character in the line is [ and last char is ], this line indicates a section name
		if( *Start=='[' && Start[FCString::Strlen(Start)-1]==']' )
		{
			// Remove the brackets
			Start++;
			Start[FCString::Strlen(Start)-1] = 0;

			// If we don't have an existing section by this name, add one
			CurrentSection = Find( Start );
			if( !CurrentSection )
			{
				CurrentSection = &Add( Start, FConfigSection() );
			}
		}

		// Otherwise, if we're currently inside a section, and we haven't reached the end of the stream
		else if( CurrentSection && *Start )
		{
			TCHAR* Value = 0;

			// ignore [comment] lines that start with ;
			if(*Start != (TCHAR)';')
			{
				Value = FCString::Strstr(Start,TEXT("="));
			}

			// Ignore any lines that don't contain a key-value pair
			if( Value )
			{
				// Terminate the propertyname, advancing past the =
				*Value++ = 0;

				// strip leading whitespace from the property name
				while ( *Start && FChar::IsWhitespace(*Start) )
					Start++;

				// Strip trailing spaces from the property name.
				while( *Start && FChar::IsWhitespace(Start[FCString::Strlen(Start)-1]) )
					Start[FCString::Strlen(Start)-1] = 0;

				// Strip leading whitespace from the property value
				while ( *Value && FChar::IsWhitespace(*Value) )
					Value++;

				// strip trailing whitespace from the property value
				while( *Value && FChar::IsWhitespace(Value[FCString::Strlen(Value)-1]) )
					Value[FCString::Strlen(Value)-1] = 0;

				// If this line is delimited by quotes
				if( *Value=='\"' )
				{
					FString PreprocessedValue = FString(Value).TrimQuotes().ReplaceQuotesWithEscapedQuotes();
					const TCHAR* NewValue = *PreprocessedValue;

					FString ProcessedValue;
					//epic moelfke: fixed handling of escaped characters in quoted string
					while (*NewValue && *NewValue != '\"')
					{
						if (*NewValue != '\\') // unescaped character
						{
							ProcessedValue += *NewValue++;
						}
						else if( *++NewValue == '\0')// escape character encountered at end
						{
							break;
						}
						else if (*NewValue == '\\') // escaped backslash "\\"
						{
							ProcessedValue += '\\';
							NewValue++;
						}
						else if (*NewValue == '\"') // escaped double quote "\""
						{
							ProcessedValue += '\"';
							NewValue++;
						}
						else if ( *NewValue == TEXT('n') )
						{
							ProcessedValue += TEXT('\n');
							NewValue++;
						}
						else if( *NewValue == TEXT('u') && NewValue[1] && NewValue[2] && NewValue[3] && NewValue[4] )	// \uXXXX - UNICODE code point
						{
							ProcessedValue += FParse::HexDigit(NewValue[1])*(1<<12) + FParse::HexDigit(NewValue[2])*(1<<8) + FParse::HexDigit(NewValue[3])*(1<<4) + FParse::HexDigit(NewValue[4]);
							NewValue += 5;
						}
						else if( NewValue[1] ) // some other escape sequence, assume it's a hex character value
						{
							ProcessedValue += FParse::HexDigit(NewValue[0])*16 + FParse::HexDigit(NewValue[1]);
							NewValue += 2;
						}
					}

					// Add this pair to the current FConfigSection
					CurrentSection->Add(Start, *ProcessedValue);
				}
				else
				{
					// Add this pair to the current FConfigSection
					CurrentSection->Add(Start, Value);
				}
			}
		}
	}

	// Avoid memory wasted in array slack.
	Shrink();
	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		It.Value().Shrink();
	}
}

void FConfigFile::Read( const FString& Filename )
{
	// we can't read in a file if file IO is disabled
	if (GConfig == NULL || !GConfig->AreFileOperationsDisabled())
	{
		Empty();
		FString Text;

		if( FFileHelper::LoadFileToString( Text, *Filename ) )
		{
			// process the contents of the string
			ProcessInputFileContents(Text);
		}
	}
}

bool FConfigFile::ShouldExportQuotedString(const FString& PropertyValue) const
{
	// The value should be exported as quoted string if it begins with a space (which is stripped on import) or
	// when it contains '//' (interpreted as a comment when importing).
	return **PropertyValue == TEXT(' ') || FCString::Strstr(*PropertyValue, TEXT("//")) != NULL;
}


#if !UE_BUILD_SHIPPING

/** A collection of identifiers which will help us parse the commandline opions. */
namespace CommandlineOverrideSpecifiers
{
	// -ini:IniName:[Section1]:Key1=Value1,[Section2]:Key2=Value2
	FString	IniSwitchIdentifier		= TEXT("-ini:");
	FString	IniNameEndIdentifier	= TEXT(":[");
	TCHAR	SectionStartIdentifier	= TCHAR('[');
	FString PropertyStartIdentifier	= TEXT("]:");
	TCHAR	PropertySeperator		= TCHAR(',');
}

/**
* Looks for any overrides on the commandline for this file
*
* @param File Config to possibly modify
* @param Filename Name of the .ini file to look for overrides
*/
static void OverrideFromCommandline(FConfigFile* File, const FString& Filename)
{
	FString Settings;
	// look for this filename on the commandline in the format:
	//		-ini:IniName:[Section1]:Key1=Value1,[Section2]:Key2=Value2
	// for example:
	//		-ini:Engine:[/Script/Engine.Engine]:bSmoothFrameRate=False,[TextureStreaming]:PoolSize=100
	//			(will update the cache after the final combined engine.ini)
	if (FParse::Value(FCommandLine::Get(), *FString::Printf(TEXT("%s%s"), *CommandlineOverrideSpecifiers::IniSwitchIdentifier, *FPaths::GetBaseFilename(Filename)), Settings, false))
	{
		// break apart on the commas
		TArray<FString> SettingPairs;
		Settings.ParseIntoArray(SettingPairs, &CommandlineOverrideSpecifiers::PropertySeperator, true);
		for (int32 Index = 0; Index < SettingPairs.Num(); Index++)
		{
			// set each one, by splitting on the =
			FString SectionAndKey, Value;
			if (SettingPairs[Index].Split(TEXT("="), &SectionAndKey, &Value))
			{
				// now we need to split off the key from the rest of the section name
				int32 SectionNameEndIndex = SectionAndKey.Find(CommandlineOverrideSpecifiers::PropertyStartIdentifier, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				// check for malformed string
				if (SectionNameEndIndex == INDEX_NONE || SectionNameEndIndex == 0)
				{
					continue;
				}

				// Create the commandline override object
				FConfigCommandlineOverride& CommandlineOption = File->CommandlineOptions[File->CommandlineOptions.Emplace()];
				CommandlineOption.BaseFileName = *FPaths::GetBaseFilename(Filename);
				CommandlineOption.Section = SectionAndKey.Left(SectionNameEndIndex);
				
				// Remove commandline syntax from the section name.
				CommandlineOption.Section = CommandlineOption.Section.Replace(*CommandlineOverrideSpecifiers::IniNameEndIdentifier, TEXT(""));
				CommandlineOption.Section = CommandlineOption.Section.Replace(*CommandlineOverrideSpecifiers::PropertyStartIdentifier, TEXT(""));
				CommandlineOption.Section = CommandlineOption.Section.Replace(&CommandlineOverrideSpecifiers::SectionStartIdentifier, TEXT(""));

				CommandlineOption.PropertyKey = SectionAndKey.Mid(SectionNameEndIndex + CommandlineOverrideSpecifiers::PropertyStartIdentifier.Len());
				CommandlineOption.PropertyValue = Value;

				// now put it into this into the cache
				File->SetString(*CommandlineOption.Section, *CommandlineOption.PropertyKey, *CommandlineOption.PropertyValue);
			}
		}
	}
}
#endif


/**
 * This will completely load .ini file hierarchy into the passed in FConfigFile. The passed in FConfigFile will then
 * have the data after combining all of those .ini 
 *
 * @param FilenameToLoad - this is the path to the file to 
 * @param ConfigFile - This is the FConfigFile which will have the contents of the .ini loaded into and Combined()
 *
 **/
static bool LoadIniFileHierarchy(const FConfigFileHierarchy& HierarchyToLoad, FConfigFile& ConfigFile, const bool bUseCache)
{
	// This shouldn't be getting called if seekfree is enabled on console.
	check(!GUseSeekFreeLoading || !FPlatformProperties::RequiresCookedData());

	// if the file does not exist then return
	if (HierarchyToLoad.Num() == 0)
	{
		//UE_LOG(LogConfig, Warning, TEXT( "LoadIniFileHierarchy was unable to find FilenameToLoad: %s "), *FilenameToLoad);
		return true;
	}
	else
	{
		// If no inis exist or only engine (Base*.ini) inis exist, don't load anything
		int32 NumExistingOptionalInis = 0;
		for( const auto& HierarchyIt : HierarchyToLoad )
		{
			const FIniFilename& IniToLoad = HierarchyIt.Value;
			if (IniToLoad.bRequired == false &&
				 (!IsUsingLocalIniFile(*IniToLoad.Filename, NULL) || IFileManager::Get().FileSize(*IniToLoad.Filename) >= 0))
			{
				NumExistingOptionalInis++;
			}
		}
		if (NumExistingOptionalInis == 0)
		{
			// No point in generating ini
			return true;
		}
	}

	EConfigFileHierarchy FirstCacheIndex = EConfigFileHierarchy::AbsoluteBase;
#if INI_CACHE
	if (bUseCache && HierarchyCache.Num() > 0)
	{
		// Find the last value in the hierarchy that is cached. We can start the load from there
		for (auto& HierarchyIt : HierarchyToLoad)
		{
			if (HierarchyCache.Find(HierarchyIt.Value.CacheKey)) 
			{
				FirstCacheIndex = HierarchyIt.Key;
			}
		}
	}
#endif

	TArray<FDateTime> TimestampsOfInis;

	// Traverse ini list back to front, merging along the way.
	for (auto& HierarchyIt : HierarchyToLoad)
	{
		if (FirstCacheIndex <= HierarchyIt.Key)
		{
			const FIniFilename& IniToLoad = HierarchyIt.Value;
			const FString& IniFileName = IniToLoad.Filename;
			bool bDoProcess = true;
#if INI_CACHE
			bool bShouldCache = IniToLoad.CacheKey.Len() > 0;
			bShouldCache &= bUseCache;
			if ( bShouldCache ) // if we are forcing a load don't mess with the cache
			{
				auto* CachedConfigFile = HierarchyCache.Find(IniToLoad.CacheKey);
				if (CachedConfigFile) 
				{
					ConfigFile = *CachedConfigFile;
					bDoProcess = false;
				}
				ConfigFile.CacheKey = IniToLoad.CacheKey;
			}
			else
			{
				ConfigFile.CacheKey = TEXT("");
			}
#endif
			if (bDoProcess)
			{
				// Spit out friendly error if there was a problem locating .inis (e.g. bad command line parameter or missing folder, ...).
				if (IsUsingLocalIniFile(*IniFileName, NULL) && (IFileManager::Get().FileSize(*IniFileName) < 0))
				{
					if (IniToLoad.bRequired)
					{
						//UE_LOG(LogConfig, Error, TEXT("Couldn't locate '%s' which is required to run '%s'"), *IniToLoad.Filename, FApp::GetGameName() );
						return false;
					}
					else
					{
#if INI_CACHE
						// missing file just add the current config file to the cache
						if ( bShouldCache )
						{
								HierarchyCache.Add(IniToLoad.CacheKey, ConfigFile);
						}
#endif
						continue;
					}
				}

				bool bDoEmptyConfig = false;
				bool bDoCombine = (HierarchyIt.Key != EConfigFileHierarchy::AbsoluteBase);
				//UE_LOG(LogConfig, Log,  TEXT( "Combining configFile: %s" ), *IniList(IniIndex) );
				ProcessIniContents(*(HierarchyIt.Value.Filename), *IniFileName, &ConfigFile, bDoEmptyConfig, bDoCombine);
#if INI_CACHE
				if ( bShouldCache )
				{
					HierarchyCache.Add(IniToLoad.CacheKey, ConfigFile);
				}
#endif
			}
		}
	}

	// Set this configs files source ini hierarchy to show where it was loaded from.
	ConfigFile.SourceIniHierarchy = HierarchyToLoad;

	return true;
}

/**
 * Check if the provided config has a property which matches the one we are providing
 *
 * @param InConfigFile		- The config file which we are looking for a match in
 * @param InSectionName		- The name of the section we want to look for a match in.
 * @param InPropertyName	- The name of the property we are looking to match
 * @param InPropertyValue	- The value of the property which, if found, we are checking a match
 *
 * @return True if a property was found in the InConfigFile which matched the SectionName, Property Name and Value.
 */
bool DoesConfigPropertyValueMatch( FConfigFile* InConfigFile, const FString& InSectionName, const FName& InPropertyName, const FString& InPropertyValue )
{
	bool bFoundAMatch = false;

	// If we have a config file to check against, have a look.
	if( InConfigFile )
	{
		// Check the sections which could match our desired section name
		const FConfigSection* Section =  InConfigFile->Find( InSectionName );

		if( Section )
		{
			// Start Array check, if the property is in an array, we need to iterate over all properties.
			TArray< FString > MatchingProperties;
			Section->MultiFind( InPropertyName, MatchingProperties );

			for( int32 PropertyIndex = 0; PropertyIndex < MatchingProperties.Num() && !bFoundAMatch; PropertyIndex++ )
			{
				const FString& PropertyValue = MatchingProperties[ PropertyIndex ];
				bFoundAMatch = PropertyValue == InPropertyValue;

				// if our properties don't match, run further checks
				if( !bFoundAMatch )
				{
					// Check that the mismatch isn't just a string comparison issue with floats
					if( FDefaultValueHelper::IsStringValidFloat( PropertyValue ) &&
						FDefaultValueHelper::IsStringValidFloat( InPropertyValue ) )
					{
						bFoundAMatch = FCString::Atof( *PropertyValue ) == FCString::Atof( *InPropertyValue );
					}
				}
			}
		}
#if !UE_BUILD_SHIPPING
		else if (FPlatformProperties::RequiresCookedData() == false && InSectionName.StartsWith(TEXT("/Script/")))
		{
			// Guard against short names in ini files
			const FString ShortSectionName = InSectionName.Replace(TEXT("/Script/"), TEXT("")); 
			Section = InConfigFile->Find(ShortSectionName);
			if (Section)
			{
				UE_LOG(LogConfig, Fatal, TEXT("Short config section found while looking for %s"), *InSectionName);
			}
		}
#endif
	}


	return bFoundAMatch;
}


/**
 * Check if the provided property information was set as a commandline override
 *
 * @param InConfigFile		- The config file which we want to check had overridden values
 * @param InSectionName		- The name of the section which we are checking for a match
 * @param InPropertyName		- The name of the property which we are checking for a match
 * @param InPropertyValue	- The value of the property which we are checking for a match
 *
 * @return True if a commandline option was set that matches the input parameters
 */
bool PropertySetFromCommandlineOption(const FConfigFile* InConfigFile, const FString& InSectionName, const FName& InPropertyName, const FString& InPropertyValue)
{
	bool bFromCommandline = false;

#if !UE_BUILD_SHIPPING
	for (const FConfigCommandlineOverride& CommandlineOverride : InConfigFile->CommandlineOptions)
	{
		if (CommandlineOverride.PropertyKey.Equals(InPropertyName.ToString(), ESearchCase::IgnoreCase) &&
			CommandlineOverride.PropertyValue.Equals(InPropertyValue, ESearchCase::IgnoreCase) &&
			CommandlineOverride.Section.Equals(InSectionName, ESearchCase::IgnoreCase) &&
			CommandlineOverride.BaseFileName.Equals(FPaths::GetBaseFilename(InConfigFile->Name.ToString()), ESearchCase::IgnoreCase))
		{
			bFromCommandline = true;
		}
	}
#endif // !UE_BUILD_SHIPPING

	return bFromCommandline;
}

/**
 * Clear the hierarchy cache
 * cos nobody want dat junk no more! bro
 *
 * @param Base ini name of the file hierarchy that we want to clear the cache for
 */
static void ClearHierarchyCache( const TCHAR* BaseIniName )
{
#if INI_CACHE
	// if we are forcing reload from disk then clear the cached hierarchy files
	for ( TMap<FString, FConfigFile>::TIterator It(HierarchyCache); It; ++It )
	{
		if ( It.Key().StartsWith( BaseIniName ) )
		{
			It.RemoveCurrent();
		}
	}
#endif
}

bool FConfigFile::Write( const FString& Filename, bool bDoRemoteWrite/* = true*/, const FString& InitialText/*=FString()*/ )
{
	if( !Dirty || NoSave || FParse::Param( FCommandLine::Get(), TEXT("nowrite")) || 
		(FParse::Param( FCommandLine::Get(), TEXT("Multiprocess"))  && !FParse::Param( FCommandLine::Get(), TEXT("MultiprocessSaveConfig"))) // Is can be useful to save configs with multiprocess if they are given INI overrides
		) 
		return true;

	FString Text = InitialText;

	for( TIterator SectionIterator(*this); SectionIterator; ++SectionIterator )
	{
		const FString& SectionName = SectionIterator.Key();
		const FConfigSection& Section = SectionIterator.Value();

		// Flag to check whether a property was written on this section, 
		// if none we do not want to make any changes to the destination file on this round.
		bool bWroteASectionProperty = false;

		TSet< FName > PropertiesAddedLookup;

		for( FConfigSection::TConstIterator It2(Section); It2; ++It2 )
		{
			const FName PropertyName = It2.Key();
			const FString& PropertyValue = It2.Value();

			// Check if the we've already processed a property of this name. If it was part of an array we may have already written it out.
			if( !PropertiesAddedLookup.Contains( PropertyName ) )
			{
				// Check for an array of differing size. This will trigger a full writeout.
				// This also catches the case where the property doesn't exist in the source in non-array cases
				bool bDifferentNumberOfElements = false;
				{
					const FConfigSection* SourceSection = NULL;
					if( SourceSection )
					{
						TArray< FString > SourceMatchingProperties;
						SourceSection->MultiFind( PropertyName, SourceMatchingProperties );

						TArray< FString > DestMatchingProperties;
						Section.MultiFind( PropertyName, DestMatchingProperties );

						bDifferentNumberOfElements = SourceMatchingProperties.Num() != DestMatchingProperties.Num();
					}
				}

				// check whether the option we are attempting to write out, came from the commandline as a temporary override.
				const bool bOptionIsFromCommandline = PropertySetFromCommandlineOption(this, SectionName, PropertyName, PropertyValue);

				// If we are writing to a default config file and this property is an array, we need to be careful to remove those from higher up the hierarchy
				const FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(Filename);
				const FString AbsoluteGeneratedConfigDir = FPaths::ConvertRelativePathToFull(FPaths::GeneratedConfigDir());
				const bool bIsADefaultIniWrite = !AbsoluteFilename.Contains(AbsoluteGeneratedConfigDir);

				// Check if the property matches the source configs. We do not wanna write it out if so.
				if ((bIsADefaultIniWrite || bDifferentNumberOfElements || !DoesConfigPropertyValueMatch(SourceConfigFile, SectionName, PropertyName, PropertyValue)) && !bOptionIsFromCommandline)
				{
					// If this is the first property we are writing of this section, then print the section name
					if( !bWroteASectionProperty )
					{
						Text += FString::Printf( TEXT("[%s]") LINE_TERMINATOR, *SectionName);
						bWroteASectionProperty = true;
					}

					// Print the property to the file.
					TCHAR QuoteString[2] = {0,0};
					if (ShouldExportQuotedString(PropertyValue))
					{
						QuoteString[0] = TEXT('\"');
					}

					// Write out our property, if it is an array we need to write out the entire array.
					TArray< FString > CompletePropertyToWrite;
					Section.MultiFind( PropertyName, CompletePropertyToWrite, true );

					if( bIsADefaultIniWrite )
					{
						ProcessPropertyAndWriteForDefaults(CompletePropertyToWrite, Text, SectionName, PropertyName.ToString());
					}
					else
					{
						for( int32 Idx = 0; Idx < CompletePropertyToWrite.Num(); Idx++ )
						{
							Text += FString::Printf( TEXT("%s=%s%s%s") LINE_TERMINATOR, 
								*PropertyName.ToString(), QuoteString, *CompletePropertyToWrite[ Idx ], QuoteString);	
						}
					}

					PropertiesAddedLookup.Add( PropertyName );
				}
			}
		}

		// If we wrote any part of the section, then add some whitespace after the section.
		if( bWroteASectionProperty )
		{
			Text += LINE_TERMINATOR;
		}

	}

	// Ensure We have at least something to write
	Text += LINE_TERMINATOR;

	if (bDoRemoteWrite)
	{
		// Write out the remote version (assuming it was loaded)
		FRemoteConfig::Get()->Write(*Filename, Text);
	}
	bool bResult = FFileHelper::SaveStringToFile( Text, *Filename );

#if INI_CACHE
	// if we wrote the config successfully
	if ( bResult && CacheKey.Len() > 0 )
	{
		check( Name != NAME_None );
		ClearHierarchyCache(*Name.ToString());
	}
	
	/*if (bResult && CacheKey.Len() > 0)
	{
		HierarchyCache.Add(CacheKey, *this);
	}*/
#endif

	// File is still dirty if it didn't save.
	Dirty = !bResult;

	// Return if the write was successful
	return bResult;
}



/** Adds any properties that exist in InSourceFile that this config file is missing */
void FConfigFile::AddMissingProperties( const FConfigFile& InSourceFile )
{
	for( TConstIterator SourceSectionIt( InSourceFile ); SourceSectionIt; ++SourceSectionIt )
	{
		const FString& SourceSectionName = SourceSectionIt.Key();
		const FConfigSection& SourceSection = SourceSectionIt.Value();

		{
			// If we don't already have this section, go ahead and add it now
			FConfigSection* DestSection = Find( SourceSectionName );
			if( DestSection == NULL )
			{
				DestSection = &Add( SourceSectionName, FConfigSection() );
				Dirty = true;
			}

			for( FConfigSection::TConstIterator SourcePropertyIt( SourceSection ); SourcePropertyIt; ++SourcePropertyIt )
			{
				const FName SourcePropertyName = SourcePropertyIt.Key();
				
				// If we don't already have this property, go ahead and add it now
				if( DestSection->Find( SourcePropertyName ) == NULL )
				{
					TArray<FString> Results;
					SourceSection.MultiFind(SourcePropertyName, Results, true);
					for (int32 ResultsIdx = 0; ResultsIdx < Results.Num(); ++ResultsIdx)
					{
						DestSection->Add(SourcePropertyName, Results[ResultsIdx]);
						Dirty = true;
					}
				}
			}
		}
	}
}



void FConfigFile::Dump(FOutputDevice& Ar)
{
	Ar.Logf( TEXT("FConfigFile::Dump") );

	for( TMap<FString,FConfigSection>::TIterator It(*this); It; ++It )
	{
		Ar.Logf( TEXT("[%s]"), *It.Key() );
		TArray<FName> KeyNames;

		FConfigSection& Section = It.Value();
		Section.GetKeys(KeyNames);
		for(TArray<FName>::TConstIterator KeyNameIt(KeyNames);KeyNameIt;++KeyNameIt)
		{
			const FName KeyName = *KeyNameIt;

			TArray<FString> Values;
			Section.MultiFind(KeyName,Values,true);

			if ( Values.Num() > 1 )
			{
				for ( int32 ValueIndex = 0; ValueIndex < Values.Num(); ValueIndex++ )
				{
					Ar.Logf(TEXT("	%s[%i]=%s"), *KeyName.ToString(), ValueIndex, *Values[ValueIndex].ReplaceCharWithEscapedChar());
				}
			}
			else
			{
				Ar.Logf(TEXT("	%s=%s"), *KeyName.ToString(), *Values[0].ReplaceCharWithEscapedChar());
			}
		}

		Ar.Log( LINE_TERMINATOR );
	}
}

bool FConfigFile::GetString( const TCHAR* Section, const TCHAR* Key, FString& Value ) const
{
	const FConfigSection* Sec = Find( Section );
	if( Sec == NULL )
	{
		return false;
	}
	const FString* PairString = Sec->Find( Key );
	if( PairString == NULL )
	{
		return false;
	}

	//this prevents our source code text gatherer from trying to gather the following messages
#define LOC_DEFINE_REGION
	if( FCString::Strstr( **PairString, TEXT("LOCTEXT") ) )
	{
		UE_LOG( LogConfig, Warning, TEXT( "FConfigFile::GetString( %s, %s ) contains LOCTEXT"), Section, Key );
		return false;
	}
	else
	{
		Value = **PairString;
		return true;
	}
#undef LOC_DEFINE_REGION
}

bool FConfigFile::GetText( const TCHAR* Section, const TCHAR* Key, FText& Value ) const
{
	const FConfigSection* Sec = Find( Section );
	if( Sec == NULL )
	{
		return false;
	}
	const FString* PairString = Sec->Find( Key );
	if( PairString == NULL )
	{
		return false;
	}
	return FParse::Text( **PairString, Value, Section );
}

bool FConfigFile::GetInt64( const TCHAR* Section, const TCHAR* Key, int64& Value ) const
{
	FString Text; 
	if( GetString( Section, Key, Text ) )
	{
		Value = FCString::Atoi64(*Text);
		return true;
	}
	return false;
}



void FConfigFile::SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value )
{
	FConfigSection* Sec  = Find( Section );
	if( Sec == NULL )
	{
		Sec = &Add( Section, FConfigSection() );
	}

	FString* Str = Sec->Find( Key );
	if( Str == NULL )
	{
		Sec->Add( Key, Value );
		Dirty = 1;
	}
	else if( FCString::Strcmp(**Str,Value)!=0 )
	{
		Dirty = true;
		*Str = Value;
	}
}

void FConfigFile::SetText( const TCHAR* Section, const TCHAR* Key, const FText& Value )
{
	FConfigSection* Sec  = Find( Section );
	if( Sec == NULL )
	{
		Sec = &Add( Section, FConfigSection() );
	}

	FString* Str = Sec->Find( Key );
	const FString StrValue = FTextFriendHelper::AsString( Value );

	if( Str == NULL )
	{
		Sec->Add( Key, StrValue );
		Dirty = 1;
	}
	else if( FCString::Strcmp(**Str, *StrValue)!=0 )
	{
		Dirty = true;
		*Str = StrValue;
	}
}

void FConfigFile::SetInt64( const TCHAR* Section, const TCHAR* Key, int64 Value )
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	FCString::Sprintf( Text, TEXT("%lld"), Value );
	SetString( Section, Key, Text );
}


void FConfigFile::SaveSourceToBackupFile()
{
	FString Text;

	FString BetweenRunsDir = (FPaths::GameIntermediateDir() / TEXT("Config/CoalescedSourceConfigs/"));
	FString Filename = FString::Printf( TEXT( "%s%s.ini" ), *BetweenRunsDir, *Name.ToString() );

	for( TMap<FString,FConfigSection>::TIterator SectionIterator(*SourceConfigFile); SectionIterator; ++SectionIterator )
	{
		const FString& SectionName = SectionIterator.Key();
		const FConfigSection& Section = SectionIterator.Value();

		Text += FString::Printf( TEXT("[%s]") LINE_TERMINATOR, *SectionName);

		for( FConfigSection::TConstIterator PropertyIterator(Section); PropertyIterator; ++PropertyIterator )
		{
			const FName PropertyName = PropertyIterator.Key();
			const FString& PropertyValue = PropertyIterator.Value();

			// Print the property to the file.
			TCHAR QuoteString[2] = {0,0};
			if (ShouldExportQuotedString(PropertyValue))
			{
				QuoteString[0] = TEXT('\"');
			}

			Text += FString::Printf( TEXT("%s=%s%s%s") LINE_TERMINATOR, 
				*PropertyName.ToString(), QuoteString, *PropertyValue, QuoteString);	
		}
		Text += LINE_TERMINATOR;
	}

	if(!FFileHelper::SaveStringToFile( Text, *Filename ))
	{
		UE_LOG(LogConfig, Warning, TEXT("Failed to saved backup for config[%s]"), *Name.ToString());
	}
}


void FConfigFile::ProcessSourceAndCheckAgainstBackup()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		FString BetweenRunsDir = (FPaths::GameIntermediateDir() / TEXT("Config/CoalescedSourceConfigs/"));
		FString BackupFilename = FString::Printf( TEXT( "%s%s.ini" ), *BetweenRunsDir, *Name.ToString() );

		FConfigFile BackupFile;
		ProcessIniContents(*BackupFilename, *BackupFilename, &BackupFile, false, false);

		for( TMap<FString,FConfigSection>::TIterator SectionIterator(*SourceConfigFile); SectionIterator; ++SectionIterator )
		{
			const FString& SectionName = SectionIterator.Key();
			const FConfigSection& SourceSection = SectionIterator.Value();
			const FConfigSection* BackupSection = BackupFile.Find( SectionName );

			if( BackupSection && SourceSection != *BackupSection )
			{
				this->Remove( SectionName );
				this->Add( SectionName, SourceSection );
			}
		}

		SaveSourceToBackupFile();
	}
}


void FConfigFile::ProcessPropertyAndWriteForDefaults( const TArray< FString >& InCompletePropertyToProcess, FString& OutText, const FString& SectionName, const FString& PropertyName )
{
	// Only process against a hierarchy if this config file has one.
	if (SourceIniHierarchy.Num() > 0)
	{
		// Handle array elements from the configs hierarchy.
		if (PropertyName.StartsWith(TEXT("+")) || InCompletePropertyToProcess.Num() > 1)
		{
			// Build a config file out of this default configs hierarchy.
			FConfigCacheIni Hierarchy(EConfigCacheType::Temporary);

			EConfigFileHierarchy HighestFileIndex = EConfigFileHierarchy::AbsoluteBase;
			TArray<EConfigFileHierarchy> ExistingEntries;
			SourceIniHierarchy.GetKeys(ExistingEntries);
			for (const EConfigFileHierarchy& NextEntry : ExistingEntries)
			{
				HighestFileIndex = NextEntry > HighestFileIndex ? NextEntry : HighestFileIndex;
			}

			const FString& LastFileInHierarchy = SourceIniHierarchy.FindChecked(HighestFileIndex).Filename;
			FConfigFile& DefaultConfigFile = Hierarchy.Add(LastFileInHierarchy, FConfigFile());

			for (const auto& HierarchyFileIt : SourceIniHierarchy)
			{
				DefaultConfigFile.Combine(HierarchyFileIt.Value.Filename);
			}

			// Remove any array elements from the default configs hierearchy, we will add these in below
			// Note.	This compensates for an issue where strings in the hierarchy have a slightly different format
			//			to how the config system wishes to serialize them.
			TArray<FString> ArrayProperties;
			Hierarchy.GetArray(*SectionName, *PropertyName.Replace(TEXT("+"), TEXT("")), ArrayProperties, *LastFileInHierarchy);

			for (const FString& NextElement : ArrayProperties)
			{
				FString PropertyNameWithRemoveOp = PropertyName.Replace(TEXT("+"), TEXT("-"));
				OutText += FString::Printf(TEXT("%s=%s") LINE_TERMINATOR, *PropertyNameWithRemoveOp, *NextElement);
			}
		}
	}

	// Write the properties out to a file.
	for ( auto& PropertyIt : InCompletePropertyToProcess)
	{
		FString PropertyValue = *PropertyIt;

		if (ShouldExportQuotedString(PropertyValue))
		{
			PropertyValue = FString::Printf(TEXT("\"%s\""), *PropertyValue);
		}

		OutText += FString::Printf(TEXT("%s=%s") LINE_TERMINATOR, *PropertyName, *PropertyValue);
	}
}


/*-----------------------------------------------------------------------------
	FConfigCacheIni
-----------------------------------------------------------------------------*/

FConfigCacheIni::FConfigCacheIni(EConfigCacheType InType)
	: bAreFileOperationsDisabled(false)
	, bIsReadyForUse(false)
	, Type(InType)
{
}

#if WITH_HOT_RELOAD_CTORS
FConfigCacheIni::FConfigCacheIni()
{
	EnsureRetrievingVTablePtr();
}
#endif // WITH_HOT_RELOAD_CTORS

FConfigCacheIni::~FConfigCacheIni()
{
	Flush( 1 );
}

FConfigFile* FConfigCacheIni::FindConfigFile( const FString& Filename )
{
	return TMap<FString,FConfigFile>::Find( Filename );
}

FConfigFile* FConfigCacheIni::Find( const FString& Filename, bool CreateIfNotFound )
{	
	// check for non-filenames
	if(Filename.Len() == 0)
	{
		return NULL;
	}

	// Get file.
	FConfigFile* Result = TMap<FString,FConfigFile>::Find( Filename );
	// this is || filesize so we load up .int files if file IO is allowed
	if( !Result && !bAreFileOperationsDisabled && (CreateIfNotFound || ( IFileManager::Get().FileSize(*Filename) >= 0 ) ) )
	{
		Result = &Add( Filename, FConfigFile() );
		Result->Read( Filename );
		UE_LOG(LogConfig, Log, TEXT( "GConfig::Find has loaded file:  %s" ), *Filename );
	}
	return Result;
}

void FConfigCacheIni::Flush( bool Read, const FString& Filename )
{
	// never Flush temporary cache objects
	if (Type == EConfigCacheType::Temporary)
	{
		return;
	}

	// write out the files if we can
	if (!bAreFileOperationsDisabled)
	{
		for (TIterator It(*this); It; ++It)
		{
			if (Filename.Len() == 0 || It.Key()==Filename)
			{
				It.Value().Write(*It.Key());
			}
		}
	}
	if( Read )
	{
		// we can't read it back in if file operations are disabled
		if (bAreFileOperationsDisabled)
		{
			UE_LOG(LogConfig, Warning, TEXT("Tried to flush the config cache and read it back in, but File Operations are disabled!!"));
			return;
		}

		if (Filename.Len() != 0)
		{
			Remove(Filename);
		}
		else
		{
			Empty();
		}
	}
}

/**
 * Disables any file IO by the config cache system
 */
void FConfigCacheIni::DisableFileOperations()
{
	bAreFileOperationsDisabled = true;
}

/**
 * Re-enables file IO by the config cache system
 */
void FConfigCacheIni::EnableFileOperations()
{
	bAreFileOperationsDisabled = false;
}

/**
 * Returns whether or not file operations are disabled
 */
bool FConfigCacheIni::AreFileOperationsDisabled()
{
	return bAreFileOperationsDisabled;
}

/**
 * Prases apart an ini section that contains a list of 1-to-N mappings of names in the following format
 *	 [PerMapPackages]
 *	 MapName=Map1
 *	 Package=PackageA
 *	 Package=PackageB
 *	 MapName=Map2
 *	 Package=PackageC
 *	 Package=PackageD
 * 
 * @param Section Name of section to look in
 * @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
 * @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
 * @param OutMap Map containing parsed results
 * @param Filename Filename to use to find the section
 *
 * NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
 */
void FConfigCacheIni::Parse1ToNSectionOfNames(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FName, TArray<FName> >& OutMap, const FString& Filename)
{
	// find the config file object
	FConfigFile* ConfigFile = Find(Filename, 0);
	if (!ConfigFile)
	{
		return;
	}

	// find the section in the file
	FConfigSectionMap* ConfigSection = ConfigFile->Find(Section);
	if (!ConfigSection)
	{
		return;
	}

	TArray<FName>* WorkingList = NULL;
	for( FConfigSectionMap::TIterator It(*ConfigSection); It; ++It )
	{
		// is the current key the 1 key?
		if (It.Key() == KeyOne)
		{
			FName KeyName(*It.Value());

			// look for existing set in the map
			WorkingList = OutMap.Find(KeyName);

			// make a new one if it wasn't there
			if (WorkingList == NULL)
			{
				WorkingList = &OutMap.Add(KeyName, TArray<FName>());
			}
		}
		// is the current key the N key?
		else if (It.Key() == KeyN && WorkingList != NULL)
		{
			// if so, add it to the N list for the current 1 key
			WorkingList->Add(FName(*It.Value()));
		}
		// if it's neither, then reset
		else
		{
			WorkingList = NULL;
		}
	}
}

/**
 * Prases apart an ini section that contains a list of 1-to-N mappings of strings in the following format
 *	 [PerMapPackages]
 *	 MapName=Map1
 *	 Package=PackageA
 *	 Package=PackageB
 *	 MapName=Map2
 *	 Package=PackageC
 *	 Package=PackageD
 * 
 * @param Section Name of section to look in
 * @param KeyOne Key to use for the 1 in the 1-to-N (MapName in the above example)
 * @param KeyN Key to use for the N in the 1-to-N (Package in the above example)
 * @param OutMap Map containing parsed results
 * @param Filename Filename to use to find the section
 *
 * NOTE: The function naming is weird because you can't apparently have an overridden function differnt only by template type params
 */
void FConfigCacheIni::Parse1ToNSectionOfStrings(const TCHAR* Section, const TCHAR* KeyOne, const TCHAR* KeyN, TMap<FString, TArray<FString> >& OutMap, const FString& Filename)
{
	// find the config file object
	FConfigFile* ConfigFile = Find(Filename, 0);
	if (!ConfigFile)
	{
		return;
	}

	// find the section in the file
	FConfigSectionMap* ConfigSection = ConfigFile->Find(Section);
	if (!ConfigSection)
	{
		return;
	}

	TArray<FString>* WorkingList = NULL;
	for( FConfigSectionMap::TIterator It(*ConfigSection); It; ++It )
	{
		// is the current key the 1 key?
		if (It.Key() == KeyOne)
		{
			// look for existing set in the map
			WorkingList = OutMap.Find(*It.Value());

			// make a new one if it wasn't there
			if (WorkingList == NULL)
			{
				WorkingList = &OutMap.Add(*It.Value(), TArray<FString>());
			}
		}
		// is the current key the N key?
		else if (It.Key() == KeyN && WorkingList != NULL)
		{
			// if so, add it to the N list for the current 1 key
			WorkingList->Add(It.Value());
		}
		// if it's neither, then reset
		else
		{
			WorkingList = NULL;
		}
	}
}

void FConfigCacheIni::LoadFile( const FString& Filename, const FConfigFile* Fallback, const TCHAR* PlatformString )
{
	// if the file has some data in it, read it in
	if( !IsUsingLocalIniFile(*Filename, NULL) || (IFileManager::Get().FileSize(*Filename) >= 0) )
	{
		FConfigFile* Result = &Add(Filename, FConfigFile());
		bool bDoEmptyConfig = false;
		bool bDoCombine = false;
		ProcessIniContents(*Filename, *Filename, Result, bDoEmptyConfig, bDoCombine);
		UE_LOG(LogConfig, Log, TEXT( "GConfig::LoadFile has loaded file:  %s" ), *Filename);
	}
	else if( Fallback )
	{
		Add( *Filename, *Fallback );
		UE_LOG(LogConfig, Log, TEXT( "GConfig::LoadFile associated file:  %s" ), *Filename);
	}
	else
	{
		UE_LOG(LogConfig, Warning, TEXT( "FConfigCacheIni::LoadFile failed loading file as it was 0 size.  Filename was:  %s" ), *Filename);
	}

	// Avoid memory wasted in array slack.
	Shrink();
}


void FConfigCacheIni::SetFile( const FString& Filename, const FConfigFile* NewConfigFile )
{
	Add(Filename, *NewConfigFile);
}


void FConfigCacheIni::UnloadFile(const FString& Filename)
{
	FConfigFile* File = Find(Filename, 0);
	if( File )
		Remove( Filename );
}

void FConfigCacheIni::Detach(const FString& Filename)
{
	FConfigFile* File = Find(Filename, 1);
	if( File )
		File->NoSave = 1;
}

#if !UE_BUILD_SHIPPING
/**
 * Checks if the section name is the expected name format (long package name or simple name)
 */
static void CheckLongSectionNames(const TCHAR* Section, FConfigFile* File)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Guard against short names in ini files.
		if (FCString::Strnicmp(Section, TEXT("/Script/"), 8) == 0)
		{
			// Section is a long name
			if (File->Find( Section + 8 ))
			{
				UE_LOG(LogConfig, Fatal, TEXT("Short config section found while looking for %s"), Section);
			}
		}
		else
		{
			// Section is a short name
			FString LongName = FString(TEXT("/Script/")) + Section;
			if (File->Find( *LongName ))
			{
				UE_LOG(LogConfig, Fatal, TEXT("Short config section used instead of long %s"), Section);
			}
		}
	}
}
#endif // UE_BUILD_SHIPPING

bool FConfigCacheIni::GetString( const TCHAR* Section, const TCHAR* Key, FString& Value, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	FConfigFile* File = Find( Filename, 0 );
	if( !File )
	{
		return false;
	}
	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
	{
#if !UE_BUILD_SHIPPING
		CheckLongSectionNames( Section, File );
#endif
		return false;
	}
	FString* PairString = Sec->Find( Key );
	if( !PairString )
	{
		return false;
	}

	//this prevents our source code text gatherer from trying to gather the following messages
#define LOC_DEFINE_REGION
	if( FCString::Strstr( **PairString, TEXT("LOCTEXT") ) )
	{
		UE_LOG( LogConfig, Warning, TEXT( "FConfigCacheIni::GetString( %s, %s, %s ) contains LOCTEXT"), Section, Key, *Filename );
		return false;
	}
	else
	{
		Value = **PairString;
		return true;
	}
#undef LOC_DEFINE_REGION
}

bool FConfigCacheIni::GetText( const TCHAR* Section, const TCHAR* Key, FText& Value, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	FConfigFile* File = Find( Filename, 0 );
	if( !File )
	{
		return false;
	}
	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
	{
#if !UE_BUILD_SHIPPING
		CheckLongSectionNames( Section, File );
#endif
		return false;
	}
	FString* PairString = Sec->Find( Key );
	if( !PairString )
	{
		return false;
	}
	return FParse::Text( **PairString, Value, Section );
}

bool FConfigCacheIni::GetSection( const TCHAR* Section, TArray<FString>& Result, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	Result.Empty();
	FConfigFile* File = Find( Filename, 0 );
	if( !File )
		return 0;
	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
		return 0;
	for( FConfigSection::TIterator It(*Sec); It; ++It )
		new(Result) FString(FString::Printf( TEXT("%s=%s"), *It.Key().ToString(), *It.Value() ));
	return 1;
}

FConfigSection* FConfigCacheIni::GetSectionPrivate( const TCHAR* Section, bool Force, bool Const, const FString& Filename )
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	FConfigFile* File = Find( Filename, Force );
	if( !File )
		return NULL;
	FConfigSection* Sec = File->Find( Section );
	if( !Sec && Force )
		Sec = &File->Add( Section, FConfigSection() );
	if( Sec && (Force || !Const) )
		File->Dirty = 1;
	return Sec;
}

void FConfigCacheIni::SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const FString& Filename )
{
	FConfigFile* File = Find( Filename, 1 );

	if ( !File )
	{
		return;
	}

	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
		Sec = &File->Add( Section, FConfigSection() );
	FString* Str = Sec->Find( Key );
	if( !Str )
	{
		Sec->Add( Key, Value );
		File->Dirty = 1;
	}
	else if( FCString::Stricmp(**Str,Value)!=0 )
	{
		File->Dirty = (FCString::Strcmp(**Str,Value)!=0);
		*Str = Value;
	}
}

void FConfigCacheIni::SetText( const TCHAR* Section, const TCHAR* Key, const FText& Value, const FString& Filename )
{
	FConfigFile* File = Find( Filename, 1 );

	if ( !File )
	{
		return;
	}

	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
	{
		Sec = &File->Add( Section, FConfigSection() );
	}

	FString* Str = Sec->Find( Key );
	const FString StrValue = FTextFriendHelper::AsString( Value );

	if( !Str )
	{
		Sec->Add( Key, StrValue );
		File->Dirty = 1;
	}
	else if( FCString::Stricmp(**Str, *StrValue)!=0 )
	{
		File->Dirty = (FCString::Strcmp(**Str, *StrValue)!=0);
		*Str = StrValue;
	}
}

void FConfigCacheIni::RemoveKey( const TCHAR* Section, const TCHAR* Key, const FString& Filename )
{
	FConfigFile* File = Find( Filename, 1 );
	if ( !File )
	{
		return;
	}

	FConfigSection* Sec = File->Find( Section );
	if( !Sec )
	{
		return;
	}

	if ( Sec->Remove(Key) > 0 )
		File->Dirty = 1;
}

void FConfigCacheIni::EmptySection( const TCHAR* Section, const FString& Filename )
{
	FConfigFile* File = Find( Filename, 0 );
	if( File )
	{
		FConfigSection* Sec = File->Find( Section );
		// remove the section name if there are no more properties for this section
		if( Sec )
		{
			if ( FConfigSection::TIterator(*Sec) )
				Sec->Empty();

			File->Remove(Section);
			if (bAreFileOperationsDisabled == false)
			{
				if (File->Num())
				{
					File->Dirty = 1;
					Flush(0, Filename);
				}
				else
				{
					IFileManager::Get().Delete(*Filename);	
				}
			}
		}
	}
}

void FConfigCacheIni::EmptySectionsMatchingString( const TCHAR* SectionString, const FString& Filename )
{
	FConfigFile* File = Find( Filename, 0 );
	if (File)
	{
		bool bSaveOpsDisabled = bAreFileOperationsDisabled;
		bAreFileOperationsDisabled = true;
		for (FConfigFile::TIterator It(*File); It; ++It)
		{
			if (It.Key().Contains(SectionString) )
			{
				EmptySection(*(It.Key()), Filename);
			}
		}
		bAreFileOperationsDisabled = bSaveOpsDisabled;
	}
}

/**
 * Retrieve a list of all of the config files stored in the cache
 *
 * @param ConfigFilenames Out array to receive the list of filenames
 */
void FConfigCacheIni::GetConfigFilenames(TArray<FString>& ConfigFilenames)
{
	// copy from our map to the array
	for (FConfigCacheIni::TIterator It(*this); It; ++It)
	{
		ConfigFilenames.Add(*(It.Key()));
	}
}

/**
 * Retrieve the names for all sections contained in the file specified by Filename
 *
 * @param	Filename			the file to retrieve section names from
 * @param	out_SectionNames	will receive the list of section names
 *
 * @return	true if the file specified was successfully found;
 */
bool FConfigCacheIni::GetSectionNames( const FString& Filename, TArray<FString>& out_SectionNames )
{
	bool bResult = false;

	FConfigFile* File = Find(Filename, false);
	if ( File != NULL )
	{
		out_SectionNames.Empty(Num());
		for ( FConfigFile::TIterator It(*File); It; ++It )
		{
			// insert each item at the beginning of the array because TIterators return results in reverse order from which they were added
			out_SectionNames.Insert(It.Key(),0);
		}
		bResult = true;
	}

	return bResult;
}

/**
 * Retrieve the names of sections which contain data for the specified PerObjectConfig class.
 *
 * @param	Filename			the file to retrieve section names from
 * @param	SearchClass			the name of the PerObjectConfig class to retrieve sections for.
 * @param	out_SectionNames	will receive the list of section names that correspond to PerObjectConfig sections of the specified class
 * @param	MaxResults			the maximum number of section names to retrieve
 *
 * @return	true if the file specified was found and it contained at least 1 section for the specified class
 */
bool FConfigCacheIni::GetPerObjectConfigSections( const FString& Filename, const FString& SearchClass, TArray<FString>& out_SectionNames, int32 MaxResults )
{
	bool bResult = false;

	MaxResults = FMath::Max(0, MaxResults);
	FConfigFile* File = Find(Filename, false);
	if ( File != NULL )
	{
		out_SectionNames.Empty();
		for ( FConfigFile::TIterator It(*File); It && out_SectionNames.Num() < MaxResults; ++It )
		{
			const FString& SectionName = It.Key();
			
			// determine whether this section corresponds to a PerObjectConfig section
			int32 POCClassDelimiter = SectionName.Find(TEXT(" "));
			if ( POCClassDelimiter != INDEX_NONE )
			{
				// the section name contained a space, which for now we'll assume means that we've found a PerObjectConfig section
				// see if the remainder of the section name matches the class name we're searching for
				if ( SectionName.Mid(POCClassDelimiter + 1) == SearchClass )
				{
					// found a PerObjectConfig section for the class specified - add it to the list
					out_SectionNames.Insert(SectionName,0);
					bResult = true;
				}
			}
		}
	}

	return bResult;
}

void FConfigCacheIni::Exit()
{
	Flush( 1 );
}

void FConfigCacheIni::Dump(FOutputDevice& Ar, const TCHAR* BaseIniName)
{
	if (BaseIniName == NULL)
	{
		Ar.Log( TEXT("Files map:") );
		TMap<FString,FConfigFile>::Dump( Ar );
	}

	for ( TIterator It(*this); It; ++It )
	{
		if (BaseIniName == NULL || FPaths::GetBaseFilename(It.Key()) == BaseIniName)
		{
			Ar.Logf(TEXT("FileName: %s"), *It.Key());
			FConfigFile& File = It.Value();
			for ( FConfigFile::TIterator FileIt(File); FileIt; ++FileIt )
			{
				FConfigSection& Sec = FileIt.Value();
				Ar.Logf(TEXT("   [%s]"), *FileIt.Key());
				for ( FConfigSectionMap::TConstIterator SecIt(Sec); SecIt; ++SecIt )
					Ar.Logf(TEXT("   %s=%s"), *SecIt.Key().ToString(), *SecIt.Value());

				Ar.Log(LINE_TERMINATOR);
			}
		}
	}
}

// Derived functions.
FString FConfigCacheIni::GetStr( const TCHAR* Section, const TCHAR* Key, const FString& Filename )
{
	FString Result;
	GetString( Section, Key, Result, Filename );
	return Result;
}
bool FConfigCacheIni::GetInt
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	int32&				Value,
	const FString&	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::Atoi(*Text);
		return 1;
	}
	return 0;
}
bool FConfigCacheIni::GetFloat
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	float&				Value,
	const FString&	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::Atof(*Text);
		return 1;
	}
	return 0;
}
bool FConfigCacheIni::GetDouble
	(
	const TCHAR*		Section,
	const TCHAR*		Key,
	double&				Value,
	const FString&	Filename
	)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::Atod(*Text);
		return 1;
	}
	return 0;
}


bool FConfigCacheIni::GetBool
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	bool&				Value,
	const FString&	Filename
)
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		Value = FCString::ToBool(*Text);
		return 1;
	}
	return 0;
}
int32 FConfigCacheIni::GetArray
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	TArray<FString>&	out_Arr,
	const FString&	Filename
)
{
	FRemoteConfig::Get()->FinishRead(*Filename); // Ensure the remote file has been loaded and processed
	out_Arr.Empty();
	FConfigFile* File = Find( Filename, 0 );
	if ( File != NULL )
	{
		FConfigSection* Sec = File->Find( Section );
		if ( Sec != NULL )
		{
			TArray<FString> RemapArray;
			Sec->MultiFind(Key, RemapArray);

			// TMultiMap::MultiFind will return the results in reverse order
			out_Arr.AddZeroed(RemapArray.Num());
			for ( int32 RemapIndex = RemapArray.Num() - 1, Index = 0; RemapIndex >= 0; RemapIndex--, Index++ )
			{
				out_Arr[Index] = RemapArray[RemapIndex];
			}
		}
#if !UE_BUILD_SHIPPING
		else
		{
			CheckLongSectionNames( Section, File );
		}
#endif
	}

	return out_Arr.Num();
}
/** Loads a "delimited" list of string
 * @param Section - Section of the ini file to load from
 * @param Key - The key in the section of the ini file to load
 * @param out_Arr - Array to load into
 * @param Delimiter - Break in the strings
 * @param Filename - Ini file to load from
 */
int32 FConfigCacheIni::GetSingleLineArray
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	TArray<FString>&	out_Arr,
	const FString&	Filename
)
{
	FString FullString;
	bool bValueExisted = GetString(Section, Key, FullString, Filename);
	const TCHAR* RawString = *FullString;

	//tokenize the string into out_Arr
	FString NextToken;
	while ( FParse::Token(RawString, NextToken, false) )
	{
		new(out_Arr) FString(NextToken);
	}
	return bValueExisted;
}

bool FConfigCacheIni::GetColor
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FColor&			Value,
 const FString&	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return false;
}

bool FConfigCacheIni::GetVector2D(
	const TCHAR*   Section,
	const TCHAR*   Key,
	FVector2D&     Value,
	const FString& Filename)
{
	FString Text;
	if (GetString(Section, Key, Text, Filename))
	{
		return Value.InitFromString(Text);
	}
	return false;
}


bool FConfigCacheIni::GetVector
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FVector&			Value,
 const FString&	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return false;
}

bool FConfigCacheIni::GetVector4
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FVector4&			Value,
 const FString&	Filename
)
{
	FString Text;
	if(GetString(Section, Key, Text, Filename))
	{
		return Value.InitFromString(Text);
	}
	return false;
}

bool FConfigCacheIni::GetRotator
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 FRotator&			Value,
 const FString&	Filename
 )
{
	FString Text; 
	if( GetString( Section, Key, Text, Filename ) )
	{
		return Value.InitFromString(Text);
	}
	return false;
}

void FConfigCacheIni::SetInt
(
	const TCHAR*	Section,
	const TCHAR*	Key,
	int32				Value,
	const FString&	Filename
)
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	FCString::Sprintf( Text, TEXT("%i"), Value );
	SetString( Section, Key, Text, Filename );
}
void FConfigCacheIni::SetFloat
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	float				Value,
	const FString&	Filename
)
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	FCString::Sprintf( Text, TEXT("%f"), Value );
	SetString( Section, Key, Text, Filename );
}
void FConfigCacheIni::SetDouble
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	double				Value,
	const FString&	Filename
)
{
	TCHAR Text[MAX_SPRINTF]=TEXT("");
	FCString::Sprintf( Text, TEXT("%f"), Value );
	SetString( Section, Key, Text, Filename );
}
void FConfigCacheIni::SetBool
(
	const TCHAR*		Section,
	const TCHAR*		Key,
	bool				Value,
	const FString&	Filename
)
{
	SetString( Section, Key, Value ? TEXT("True") : TEXT("False"), Filename );
}

void FConfigCacheIni::SetArray
(
	const TCHAR*			Section,
	const TCHAR*			Key,
	const TArray<FString>&	Value,
	const FString&		Filename
)
{
	FConfigFile* File = Find( Filename, 1 );
	if (!File)
	{
		return;
	}
	
	FConfigSection* Sec  = File->Find( Section );
	if( !Sec )
		Sec = &File->Add( Section, FConfigSection() );

	if ( Sec->Remove(Key) > 0 )
		File->Dirty = 1;

	for ( int32 i = 0; i < Value.Num(); i++ )
	{
		Sec->Add(Key, *Value[i]);
		File->Dirty = 1;
	}
}
/** Saves a "delimited" list of strings
 * @param Section - Section of the ini file to save to
 * @param Key - The key in the section of the ini file to save
 * @param In_Arr - Array to save from
 * @param Filename - Ini file to save to
 */
void FConfigCacheIni::SetSingleLineArray
(
	const TCHAR*			Section,
	const TCHAR*			Key,
	const TArray<FString>&	In_Arr,
	const FString&		Filename
)
{
	FString FullString;

	//append all strings to single string
	for (int32 i = 0; i < In_Arr.Num(); ++i)
	{
		FullString += In_Arr[i];
		FullString += TEXT(" ");
	}

	//save to ini file
	SetString(Section, Key, *FullString, Filename);
}

void FConfigCacheIni::SetColor
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FColor		Value,
 const FString&	Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}

void FConfigCacheIni::SetVector2D(
	const TCHAR*   Section,
	const TCHAR*   Key,
	FVector2D      Value,
	const FString& Filename)
{
	SetString(Section, Key, *Value.ToString(), Filename);
}

void FConfigCacheIni::SetVector
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FVector		 Value,
 const FString&	Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}

void FConfigCacheIni::SetVector4
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FVector4&	 Value,
 const FString&	Filename
)
{
	SetString(Section, Key, *Value.ToString(), Filename);
}

void FConfigCacheIni::SetRotator
(
 const TCHAR*		Section,
 const TCHAR*		Key,
 const FRotator		Value,
 const FString&	Filename
 )
{
	SetString( Section, Key, *Value.ToString(), Filename );
}


/**
 * Archive for counting config file memory usage.
 */
class FArchiveCountConfigMem : public FArchive
{
public:
	FArchiveCountConfigMem()
	:	Num(0)
	,	Max(0)
	{
		ArIsCountingMemory = true;
	}
	SIZE_T GetNum()
	{
		return Num;
	}
	SIZE_T GetMax()
	{
		return Max;
	}
	void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{
		Num += InNum;
		Max += InMax;
	}
protected:
	SIZE_T Num, Max;
};


/**
 * Tracks the amount of memory used by a single config or loc file
 */
struct FConfigFileMemoryData
{
	FString	ConfigFilename;
	SIZE_T		CurrentSize;
	SIZE_T		MaxSize;

	FConfigFileMemoryData( const FString& InFilename, SIZE_T InSize, SIZE_T InMax )
	: ConfigFilename(InFilename), CurrentSize(InSize), MaxSize(InMax)
	{}
};

/**
 * Tracks the memory data recorded for all loaded config files.
 */
struct FConfigMemoryData
{
	int32 NameIndent;
	int32 SizeIndent;
	int32 MaxSizeIndent;

	TArray<FConfigFileMemoryData> MemoryData;

	FConfigMemoryData()
	: NameIndent(0), SizeIndent(0), MaxSizeIndent(0)
	{}

	void AddConfigFile( const FString& ConfigFilename, FArchiveCountConfigMem& MemAr )
	{
		SIZE_T TotalMem = MemAr.GetNum();
		SIZE_T MaxMem = MemAr.GetMax();

		NameIndent = FMath::Max(NameIndent, ConfigFilename.Len());
		SizeIndent = FMath::Max(SizeIndent, FString::FromInt(TotalMem).Len());
		MaxSizeIndent = FMath::Max(MaxSizeIndent, FString::FromInt(MaxMem).Len());
		
		new(MemoryData) FConfigFileMemoryData( ConfigFilename, TotalMem, MaxMem );
	}

	void SortBySize()
	{
		struct FCompareFConfigFileMemoryData
		{
			FORCEINLINE bool operator()( const FConfigFileMemoryData& A, const FConfigFileMemoryData& B ) const
			{
				return ( B.CurrentSize == A.CurrentSize ) ? ( B.MaxSize < A.MaxSize ) : ( B.CurrentSize < A.CurrentSize );
			}
		};
		MemoryData.Sort( FCompareFConfigFileMemoryData() );
	}
};

/**
 * Dumps memory stats for each file in the config cache to the specified archive.
 *
 * @param	Ar	the output device to dump the results to
 */
void FConfigCacheIni::ShowMemoryUsage( FOutputDevice& Ar )
{
	FConfigMemoryData ConfigCacheMemoryData;

	for ( TIterator FileIt(*this); FileIt; ++FileIt )
	{
		FString Filename = FileIt.Key();
		FConfigFile& ConfigFile = FileIt.Value();

		FArchiveCountConfigMem MemAr;

		// count the bytes used for storing the filename
		MemAr << Filename;

		// count the bytes used for storing the array of SectionName->Section pairs
		MemAr << ConfigFile;
		
		ConfigCacheMemoryData.AddConfigFile(Filename, MemAr);
	}

	// add a little extra spacing between the columns
	ConfigCacheMemoryData.SizeIndent += 10;
	ConfigCacheMemoryData.MaxSizeIndent += 10;

	// record the memory used by the FConfigCacheIni's TMap
	FArchiveCountConfigMem MemAr;
	CountBytes(MemAr);

	SIZE_T TotalMemoryUsage=MemAr.GetNum();
	SIZE_T MaxMemoryUsage=MemAr.GetMax();

	Ar.Log(TEXT("Config cache memory usage:"));
	// print out the header
	Ar.Logf(TEXT("%*s %*s %*s"), ConfigCacheMemoryData.NameIndent, TEXT("FileName"), ConfigCacheMemoryData.SizeIndent, TEXT("NumBytes"), ConfigCacheMemoryData.MaxSizeIndent, TEXT("MaxBytes"));

	ConfigCacheMemoryData.SortBySize();
	for ( int32 Index = 0; Index < ConfigCacheMemoryData.MemoryData.Num(); Index++ )
	{
		FConfigFileMemoryData& ConfigFileMemoryData = ConfigCacheMemoryData.MemoryData[Index];
			Ar.Logf(TEXT("%*s %*u %*u"), 
			ConfigCacheMemoryData.NameIndent, *ConfigFileMemoryData.ConfigFilename,
			ConfigCacheMemoryData.SizeIndent, (uint32)ConfigFileMemoryData.CurrentSize,
			ConfigCacheMemoryData.MaxSizeIndent, (uint32)ConfigFileMemoryData.MaxSize);

		TotalMemoryUsage += ConfigFileMemoryData.CurrentSize;
		MaxMemoryUsage += ConfigFileMemoryData.MaxSize;
	}

	Ar.Logf(TEXT("%*s %*u %*u"), 
		ConfigCacheMemoryData.NameIndent, TEXT("Total"),
		ConfigCacheMemoryData.SizeIndent, (uint32)TotalMemoryUsage,
		ConfigCacheMemoryData.MaxSizeIndent, (uint32)MaxMemoryUsage);
}



SIZE_T FConfigCacheIni::GetMaxMemoryUsage()
{
	// record the memory used by the FConfigCacheIni's TMap
	FArchiveCountConfigMem MemAr;
	CountBytes(MemAr);

	SIZE_T TotalMemoryUsage=MemAr.GetNum();
	SIZE_T MaxMemoryUsage=MemAr.GetMax();


	FConfigMemoryData ConfigCacheMemoryData;

	for ( TIterator FileIt(*this); FileIt; ++FileIt )
	{
		FString Filename = FileIt.Key();
		FConfigFile& ConfigFile = FileIt.Value();

		FArchiveCountConfigMem FileMemAr;

		// count the bytes used for storing the filename
		FileMemAr << Filename;

		// count the bytes used for storing the array of SectionName->Section pairs
		FileMemAr << ConfigFile;

		ConfigCacheMemoryData.AddConfigFile(Filename, FileMemAr);
	}

	for ( int32 Index = 0; Index < ConfigCacheMemoryData.MemoryData.Num(); Index++ )
	{
		FConfigFileMemoryData& ConfigFileMemoryData = ConfigCacheMemoryData.MemoryData[Index];

		TotalMemoryUsage += ConfigFileMemoryData.CurrentSize;
		MaxMemoryUsage += ConfigFileMemoryData.MaxSize;
	}

	return MaxMemoryUsage;
}

bool FConfigCacheIni::ForEachEntry(const FKeyValueSink& Visitor, const TCHAR* Section, const FString& Filename)
{
	FConfigFile* File = Find(Filename, 0);
	if(!File)
	{
		return false;
	}

	FConfigSection* Sec = File->Find(Section);
	if(!Sec)
	{
		return false;
	}

	for(FConfigSectionMap::TIterator It(*Sec); It; ++It)
	{
		Visitor.Execute(*It.Key().GetPlainNameString(), *It.Value());
	}

	return true;
}

/**
 * This will completely load a single .ini file into the passed in FConfigFile.
 *
 * @param FilenameToLoad - this is the path to the file to 
 * @param ConfigFile - This is the FConfigFile which will have the contents of the .ini loaded into
 *
 **/
static void LoadAnIniFile(const FString& FilenameToLoad, FConfigFile& ConfigFile)
{
	// This shouldn't be getting called if seekfree is enabled on console.
	check(!GUseSeekFreeLoading || !FPlatformProperties::RequiresCookedData());

	if( !IsUsingLocalIniFile(*FilenameToLoad, NULL) || (IFileManager::Get().FileSize( *FilenameToLoad ) >= 0) )
	{
		ProcessIniContents(*FilenameToLoad, *FilenameToLoad, &ConfigFile, false, false);
	}
	else
	{
		//UE_LOG(LogConfig, Warning, TEXT( "LoadAnIniFile was unable to find FilenameToLoad: %s "), *FilenameToLoad);
	}
}

/**
 * This will load up two .ini files and then determine if the destination one is outdated.
 * Outdatedness is determined by the following mechanic:
 *
 * When a generated .ini is written out it will store the timestamps of the files it was generated
 * from.  This way whenever the Default__.inis are modified the Generated .ini will view itself as
 * outdated and regenerate it self.
 *
 * Outdatedness also can be affected by commandline params which allow one to delete all .ini, have
 * automated build system etc.
 *
 * @param DestConfigFile			The FConfigFile that will be filled out with the most up to date ini contents
 * @param DestIniFilename			The name of the destination .ini file - it's loaded and checked against source (e.g. Engine.ini)
 * @param SourceIniFilename			The name of the source .ini file (e.g. DefaultEngine.ini)
 */
static bool GenerateDestIniFile(FConfigFile& DestConfigFile, const FString& DestIniFilename, const FConfigFileHierarchy& SourceIniHierarchy, bool bAllowGeneratedINIs, const bool bUseHierarchyCache)
{
	bool bResult = LoadIniFileHierarchy(SourceIniHierarchy, *DestConfigFile.SourceConfigFile, bUseHierarchyCache);
	if ( bResult == false )
	{
		return false;
	}
	LoadAnIniFile(DestIniFilename, DestConfigFile);

#if !UE_BUILD_SHIPPING
	// process any commandline overrides
	OverrideFromCommandline(&DestConfigFile, DestIniFilename);
#endif

	bool bForceRegenerate = false;
	bool bShouldUpdate = FPlatformProperties::RequiresCookedData();

	// Don't try to load any generated files from disk in cooked builds. We will always use the re-generated INIs.
	if (!FPlatformProperties::RequiresCookedData() || bAllowGeneratedINIs)
	{
		// We need to check if the user is using the version of the config system which had the entire contents of the coalesced
		// Source ini hierarchy output, if so we need to update, as it will cause issues with the new way we handle saved config files.
		bool bIsLegacyConfigSystem = false;
		for ( TMap<FString,FConfigSection>::TConstIterator It(DestConfigFile); It; ++It )
		{
			FString SectionName = It.Key();
			if( SectionName == TEXT("IniVersion") || SectionName == TEXT("Engine.Engine") )
			{
				bIsLegacyConfigSystem = true;
				UE_LOG( LogInit, Warning, TEXT( "%s is out of date. It will be regenerated." ), *FPaths::ConvertRelativePathToFull(DestIniFilename) );
				break;
			}
		}

		// Regenerate the ini file?
		if( bIsLegacyConfigSystem || FParse::Param(FCommandLine::Get(),TEXT("REGENERATEINIS")) == true )
		{
			bForceRegenerate = true;
		}
		else if( FParse::Param(FCommandLine::Get(),TEXT("NOAUTOINIUPDATE")) )
		{
			// Flag indicating whether the user has requested 'Yes/No To All'.
			static int32 GIniYesNoToAll = -1;
			// Make sure GIniYesNoToAll's 'uninitialized' value is kosher.
			static_assert(EAppReturnType::YesAll != -1, "EAppReturnType::YesAll must not be -1.");
			static_assert(EAppReturnType::NoAll != -1, "EAppReturnType::NoAll must not be -1.");

			// The file exists but is different.
			// Prompt the user if they haven't already responded with a 'Yes/No To All' answer.
			uint32 YesNoToAll;
			if( GIniYesNoToAll != EAppReturnType::YesAll && GIniYesNoToAll != EAppReturnType::NoAll )
			{
				YesNoToAll = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAll, FText::Format( NSLOCTEXT("Core", "IniFileOutOfDate", "Your ini ({0}) file is outdated. Do you want to automatically update it saving the previous version? Not doing so might cause crashes!"), FText::FromString( DestIniFilename ) ) );
				// Record whether the user responded with a 'Yes/No To All' answer.
				if ( YesNoToAll == EAppReturnType::YesAll || YesNoToAll == EAppReturnType::NoAll )
				{
					GIniYesNoToAll = YesNoToAll;
				}
			}
			else
			{
				// The user has already responded with a 'Yes/No To All' answer, so note it 
				// in the output arg so that calling code can operate on its value.
				YesNoToAll = GIniYesNoToAll;
			}
			// Regenerate the file if approved by the user.
			bShouldUpdate = (YesNoToAll == EAppReturnType::Yes) || (YesNoToAll == EAppReturnType::YesAll);
		}
		else
		{
			bShouldUpdate = true;
		}
	}
	
	// Regenerate the file.
	if( bForceRegenerate )
	{
		bResult = LoadIniFileHierarchy(SourceIniHierarchy, DestConfigFile, bUseHierarchyCache);
		DestConfigFile.SourceConfigFile = new FConfigFile( DestConfigFile );

		// mark it as dirty (caller may want to save)
		DestConfigFile.Dirty = true;
	}
	else if( bShouldUpdate )
	{
		// Merge the .ini files by copying over properties that exist in the default .ini but are
		// missing from the generated .ini
		// NOTE: Most of the time there won't be any properties to add here, since LoadAnIniFile will
		//		 combine properties in the Default .ini with those in the Project .ini
		DestConfigFile.AddMissingProperties(*DestConfigFile.SourceConfigFile);

		// mark it as dirty (caller may want to save)
		DestConfigFile.Dirty = true;
	}
		
	if (!IsUsingLocalIniFile(*DestIniFilename, NULL))
	{
		// Save off a copy of the local file prior to overwriting it with the contents of a remote file
		MakeLocalCopy(*DestIniFilename);
	}

	return bResult;
}

/**
 * Calculates the name of the source (default) .ini file for a given base (ie Engine, Game, etc)
 *
 * @param ConfigDir Path to ini file
 * @param Prefix Prefix for the ini filename (Default/Platform name), may include additional sub-directories
 * @param IniBaseName Base name of the .ini (Engine, Game)
 * 
 * @return Standardized .ini filename
 */
static FString GetSourceIniFilename(const TCHAR* ConfigDir, const TCHAR* Prefix, const TCHAR* BaseIniName)
{
	FString IniFilename;
	
#if !UE_BUILD_SHIPPING
	// Figure out what to look for on the commandline for an override. Disabled in shipping builds for security reasons
	const FString CommandLineSwitch = FString::Printf(TEXT("DEF%sINI="), BaseIniName);	
	if (FParse::Value(FCommandLine::Get(), *CommandLineSwitch, IniFilename) == false)
#endif
	{
		// if it's not found on the commandline, then generate it	
		IniFilename = FString(ConfigDir) / FString::Printf(TEXT("%s%s.ini"), Prefix, BaseIniName);
	}

	FPaths::MakeStandardFilename(IniFilename);
	return IniFilename;
}

/**
 * Creates a chain of ini filenames to load and combine.
 *
 * @param InBaseIniName Ini name.
 * @param InPlatformName Platform name, NULL means to use the current platform
 * @param OutHierarchy An array which is to receive the generated hierachy of ini filenames.
 */
static void GetSourceIniHierarchyFilenames(const TCHAR* InBaseIniName, const TCHAR* InPlatformName, const TCHAR* EngineConfigDir, const TCHAR* SourceConfigDir, FConfigFileHierarchy& OutHierarchy, bool bRequireDefaultIni)
{
	/**************************************************
	 **** CRITICAL NOTES
	 **** If you change this function, you need to also change EnumerateCrossPlatformIniFileNames() in EngineConfiguration.cs!!!
	 **************************************************/
	
	const FString PlatformName(InPlatformName ? InPlatformName : ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));

	// [[[[ ENGINE DEFAULTS ]]]]
	// Engine/Config/Base.ini (included in every ini type, required)
	OutHierarchy.Add(EConfigFileHierarchy::AbsoluteBase, FIniFilename(FString::Printf(TEXT("%sBase.ini"), EngineConfigDir), true));
	// Engine/Config/Base* ini
	OutHierarchy.Add(EConfigFileHierarchy::EngineDirBase, FIniFilename(FString::Printf(TEXT("%sBase%s.ini"), EngineConfigDir, InBaseIniName), false));
	// Engine/Config/NotForLicensees/Base* ini
	OutHierarchy.Add(EConfigFileHierarchy::EngineDirBase_NotForLicensees, FIniFilename(FString::Printf(TEXT("%sNotForLicensees/Base%s.ini"), EngineConfigDir, InBaseIniName), false));
	// Engine/Config/NoRedist/Base* ini
	/////
	// NOTE: 4.7
	// There was a bug that was causing this file to be written out for all users and for all projects, with bad values
	// that would break all projects (external reference errors)
	// Since this file has not been used yet, we will delete it if it exists, and then for 4.8, we will put this back
	// into the hierarchy and remove the delete operation
	// - Remember to fixup EngineConfiguration.cs
	/////
	IFileManager::Get().Delete(*FString::Printf(TEXT("%sNoRedist/Base%s.ini"), EngineConfigDir, InBaseIniName), false, true, true);
	IFileManager::Get().DeleteDirectory(*FString::Printf(TEXT("%sNoRedist"), EngineConfigDir), false, false);
	// OutHierarchy.Add( FIniFilename(FString::Printf(TEXT("%sNoRedist/Base%s.ini"), EngineConfigDir, InBaseIniName), false) );

	// [[[[ PROJECT SETTINGS ]]]]
	// Game/Config/Default* ini
	OutHierarchy.Add(EConfigFileHierarchy::GameDirDefault, FIniFilename(GetSourceIniFilename(SourceConfigDir, TEXT("Default"), InBaseIniName), bRequireDefaultIni));
	// Game/Config/DedicatedServer* ini (knowingly NOT in EngineConfiguration.cs because this is a runtime only check)
	if (IsRunningDedicatedServer())
	{
		OutHierarchy.Add(EConfigFileHierarchy::GameDirDedicatedServer, FIniFilename(FString::Printf(TEXT("%s/DedicatedServer%s.ini"), SourceConfigDir, InBaseIniName), false));
	}
	// Game/Config/NotForLicensees/Default* ini
	OutHierarchy.Add(EConfigFileHierarchy::GameDirDefault_NotForLicensees, FIniFilename(FString::Printf(TEXT("%sNotForLicensees/Default%s.ini"), SourceConfigDir, InBaseIniName), false));
	// Game/Config/NoRedist/Default* ini (Checkpointed here before the platform ini divergence)
	FString HierarchyCheckpointPath = FString::Printf(TEXT("%sNoRedist/Default%s.ini"), SourceConfigDir, InBaseIniName);
	OutHierarchy.Add(EConfigFileHierarchy::GameDirDefault_NoRedist, FIniFilename(HierarchyCheckpointPath, false, GenerateHierarchyCacheKey(OutHierarchy, HierarchyCheckpointPath, InBaseIniName)));
	
	if (PlatformName.Len() > 0)
	{
		// [[[[ PLATFORM DEFAULTS AND PROJECT SETTINGS ]]]]
		// Engine/Config/Platform/Platform* ini
		OutHierarchy.Add(EConfigFileHierarchy::EngineDir_Platform, FIniFilename(FString::Printf(TEXT("%s%s/%s%s.ini"), EngineConfigDir, *PlatformName, *PlatformName, InBaseIniName), false));
		// Engine/Config/NotForLicensees/Platform/Platform* ini
		OutHierarchy.Add(EConfigFileHierarchy::EngineDir_Platform_NotForLicensees, FIniFilename(FString::Printf(TEXT("%sNotForLicensees/%s/%s%s.ini"), EngineConfigDir, *PlatformName, *PlatformName, InBaseIniName), false));
		// Engine/Config/NoRedist/Platform/Platform* ini
		OutHierarchy.Add(EConfigFileHierarchy::EngineDir_Platform_NoRedist, FIniFilename(FString::Printf(TEXT("%sNoRedist/%s/%s%s.ini"), EngineConfigDir, *PlatformName, *PlatformName, InBaseIniName), false));
		// Game/Config/Platform/Platform* ini
		OutHierarchy.Add(EConfigFileHierarchy::GameDir_Platform, FIniFilename(FString::Printf(TEXT("%s%s/%s%s.ini"), SourceConfigDir, *PlatformName, *PlatformName, InBaseIniName), false));
		// Game/Config/NotForLicensee/Platform/Platform* ini
		OutHierarchy.Add(EConfigFileHierarchy::GameDir_Platform_NotForLicensees, FIniFilename(FString::Printf(TEXT("%sNotForLicensees/%s/%s%s.ini"), SourceConfigDir, *PlatformName, *PlatformName, InBaseIniName), false));
		// Game/Config/NoRedist/Platform/Platform* ini
		OutHierarchy.Add(EConfigFileHierarchy::GameDir_Platform_NoRedist, FIniFilename(FString::Printf(TEXT("%sNoRedist/%s/%s%s.ini"), SourceConfigDir, *PlatformName, *PlatformName, InBaseIniName), false));
	}

	// [[[[ GLOBAL USER OVERRIDES ]]]]
	// <AppData>/UE4/EngineConfig/User* ini
	OutHierarchy.Add(EConfigFileHierarchy::UserSettingsDir_EngineDir_User, FIniFilename(FPaths::Combine(FPlatformProcess::UserSettingsDir(), *FString::Printf(TEXT("Unreal Engine/Engine/Config/User%s.ini"), InBaseIniName)), false));
	// <Documents>/UE4/EngineConfig/User* ini
	OutHierarchy.Add(EConfigFileHierarchy::UserDir_User, FIniFilename(FPaths::Combine(FPlatformProcess::UserDir(), *FString::Printf(TEXT("Unreal Engine/Engine/Config/User%s.ini"), InBaseIniName)), false));

	// [[[[ PROJECT USER OVERRIDES ]]]]
	// Game/Config/User* ini (Checkpointed here at the end)
	HierarchyCheckpointPath = FString::Printf(TEXT("%sUser%s.ini"), SourceConfigDir, InBaseIniName);
	OutHierarchy.Add(EConfigFileHierarchy::GameDir_User, FIniFilename(HierarchyCheckpointPath, false, GenerateHierarchyCacheKey(OutHierarchy, HierarchyCheckpointPath, InBaseIniName)));

	// Ensure the hierarchy is in the correct order.
	OutHierarchy.KeySort([](const EConfigFileHierarchy& A, const EConfigFileHierarchy& B) { return (A < B); });
}

/**
 * Calculates the name of a dest (generated) .ini file for a given base (ie Engine, Game, etc)
 * 
 * @param IniBaseName Base name of the .ini (Engine, Game)
 * @param PlatformName Name of the platform to get the .ini path for (NULL means to use the current platform)
 * @param GeneratedConfigDir The base folder that will contain the generated config files.
 * 
 * @return Standardized .ini filename
 */
static FString GetDestIniFilename(const TCHAR* BaseIniName, const TCHAR* PlatformName, const TCHAR* GeneratedConfigDir)
{
	// figure out what to look for on the commandline for an override
	FString CommandLineSwitch = FString::Printf(TEXT("%sINI="), BaseIniName);
	
	// if it's not found on the commandline, then generate it
	FString IniFilename;
	if (FParse::Value(FCommandLine::Get(), *CommandLineSwitch, IniFilename) == false)
	{
		FString Name(PlatformName ? PlatformName : ANSI_TO_TCHAR(FPlatformProperties::PlatformName()));
		// put it all together
		IniFilename = FString::Printf(TEXT("%s%s/%s.ini"), GeneratedConfigDir, *Name, BaseIniName);
	}

	// standardize it!
	FPaths::MakeStandardFilename(IniFilename);
	return IniFilename;
}

void FConfigCacheIni::InitializeConfigSystem()
{
	// Perform any upgrade we need before we load any configuration files
	FConfigManifest::UpgradeFromPreviousVersions();

	// create GConfig
	GConfig = new FConfigCacheIni(EConfigCacheType::DiskBacked);

	// load the main .ini files (unless we're running a program or a gameless UE4Editor.exe, DefaultEngine.ini is required).
	const bool bIsGamelessExe = !FApp::HasGameName();
	const bool bDefaultEngineIniRequired = !bIsGamelessExe && (GIsGameAgnosticExe || FApp::IsGameNameEmpty());
	bool bEngineConfigCreated = FConfigCacheIni::LoadGlobalIniFile(GEngineIni, TEXT("Engine"), NULL, bDefaultEngineIniRequired);

	if ( !bIsGamelessExe )
	{
		// Now check and see if our game is correct if this is a game agnostic binary
		if (GIsGameAgnosticExe && !bEngineConfigCreated)
		{
			const FText AbsolutePath = FText::FromString( IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::GetPath(GEngineIni)) );
			//@todo this is too early to localize
			const FText Message = FText::Format( NSLOCTEXT("Core", "FirstCmdArgMustBeGameName", "The first command line argument must be a game name. ('{0}' must exist and contain a DefaultEngine.ini)"), AbsolutePath );
			if (!GIsBuildMachine)
			{
				FMessageDialog::Open(EAppMsgType::Ok, Message);
			}
			FApp::SetGameName(TEXT("")); // this disables part of the crash reporter to avoid writing log files to a bogus directory
			if (!GIsBuildMachine)
			{
				exit(1);
			}
			UE_LOG(LogInit, Fatal,TEXT("%s"), *Message.ToString());
		}
	}

	FConfigCacheIni::LoadGlobalIniFile(GGameIni, TEXT("Game"));
	FConfigCacheIni::LoadGlobalIniFile(GInputIni, TEXT("Input"));
#if WITH_EDITOR
	// load some editor specific .ini files

	FConfigCacheIni::LoadGlobalIniFile(GEditorIni, TEXT("Editor"));

	// Upgrade editor user settings before loading the editor per project user settings
	FConfigManifest::MigrateEditorUserSettings();
	FConfigCacheIni::LoadGlobalIniFile(GEditorPerProjectIni, TEXT("EditorPerProjectUserSettings"));

	// Project agnostic editor ini files
	static const FString EditorSettingsDir = FPaths::Combine(*FPaths::GameAgnosticSavedDir(), TEXT("Config")) + TEXT("/");
	FConfigCacheIni::LoadGlobalIniFile(GEditorSettingsIni, TEXT("EditorSettings"), NULL, false, false, true, *EditorSettingsDir);
	FConfigCacheIni::LoadGlobalIniFile(GEditorLayoutIni, TEXT("EditorLayout"), NULL, false, false, true, *EditorSettingsDir);
	FConfigCacheIni::LoadGlobalIniFile(GEditorKeyBindingsIni, TEXT("EditorKeyBindings"), NULL, false, false, true, *EditorSettingsDir);

#endif
#if PLATFORM_DESKTOP
	// load some desktop only .ini files
	FConfigCacheIni::LoadGlobalIniFile(GCompatIni, TEXT("Compat"));
	FConfigCacheIni::LoadGlobalIniFile(GLightmassIni, TEXT("Lightmass"));
#endif

	// Load scalability settings.
	FConfigCacheIni::LoadGlobalIniFile(GScalabilityIni, TEXT("Scalability"));
	
	// Load user game settings .ini, allowing merging. This also updates the user .ini if necessary.
	FConfigCacheIni::LoadGlobalIniFile(GGameUserSettingsIni, TEXT("GameUserSettings"));

	// now we can make use of GConfig
	GConfig->bIsReadyForUse = true;
}

bool FConfigCacheIni::LoadGlobalIniFile(FString& FinalIniFilename, const TCHAR* BaseIniName, const TCHAR* Platform, bool bForceReload, bool bRequireDefaultIni, bool bAllowGeneratedIniWhenCooked, const TCHAR* GeneratedConfigDir)
{
	// figure out where the end ini file is
	FinalIniFilename = GetDestIniFilename(BaseIniName, Platform, GeneratedConfigDir);
	
	// Start the loading process for the remote config file when appropriate
	if (FRemoteConfig::Get()->ShouldReadRemoteFile(*FinalIniFilename))
	{
		FRemoteConfig::Get()->Read(*FinalIniFilename, BaseIniName);
	}

	FRemoteConfigAsyncIOInfo* RemoteInfo = FRemoteConfig::Get()->FindConfig(*FinalIniFilename);
	if (RemoteInfo && (!RemoteInfo->bWasProcessed || !FRemoteConfig::Get()->IsFinished(*FinalIniFilename)))
	{
		// Defer processing this remote config file to until it has finish its IO operation
		return false;
	}

	// need to check to see if the file already exists in the GConfigManager's cache
	// if it does exist then we are done, nothing else to do
	if (!bForceReload && GConfig->FindConfigFile(*FinalIniFilename) != NULL)
	{
		//UE_LOG(LogConfig, Log,  TEXT( "Request to load a config file that was already loaded: %s" ), GeneratedIniFile );
		return true;
	}

	// make a new entry in GConfig (overwriting what's already there)
	FConfigFile& NewConfigFile = GConfig->Add(FinalIniFilename, FConfigFile());

	if ( bForceReload )
	{
		ClearHierarchyCache(BaseIniName);
	}


	// calculate the source ini file name,
	GetSourceIniHierarchyFilenames( BaseIniName, Platform, *FPaths::EngineConfigDir(), *FPaths::SourceConfigDir(), NewConfigFile.SourceIniHierarchy, bRequireDefaultIni );

	// Keep a record of the original settings
	NewConfigFile.SourceConfigFile = new FConfigFile();

	// now generate and make sure it's up to date
	bool bResult = GenerateDestIniFile(NewConfigFile, FinalIniFilename, NewConfigFile.SourceIniHierarchy, bAllowGeneratedIniWhenCooked, true);
	NewConfigFile.Name = BaseIniName;

	// don't write anything to disk in cooked builds - we will always use re-generated INI files anyway.
	if ((!FPlatformProperties::RequiresCookedData() || bAllowGeneratedIniWhenCooked)
			// We shouldn't save config files when in multiprocess mode,
			// otherwise we get file contention in XGE shader builds.
			&& !FParse::Param(FCommandLine::Get(), TEXT("Multiprocess")))
	{
		// Check the config system for any changes made to defaults and propagate through to the saved.
		NewConfigFile.ProcessSourceAndCheckAgainstBackup();

		if (bResult)
		{
			// if it was dirtied during the above function, save it out now
			NewConfigFile.Write(FinalIniFilename);
		}
	}

	return bResult;
}

void FConfigCacheIni::LoadLocalIniFile(FConfigFile& ConfigFile, const TCHAR* IniName, bool bGenerateDestIni, const TCHAR* Platform, const bool bForceReload )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FConfigCacheIni::LoadLocalIniFile" ), STAT_FConfigCacheIni_LoadLocalIniFile, STATGROUP_LoadTime );

	LoadExternalIniFile( ConfigFile, IniName, *FPaths::EngineConfigDir(), *FPaths::SourceConfigDir(), bGenerateDestIni, Platform, bForceReload );


	// if bGenerateDestIni is false, that means the .ini is a ready-to-go .ini file, and just needs to be loaded into the FConfigFile
	/*if (!bGenerateDestIni)
	{
		// generate path to the .ini file (not a Default ini, IniName is the complete name of the file, without path)
		FString SourceIniFilename = FString::Printf(TEXT("%s%s.ini"), *FPaths::SourceConfigDir(), IniName);

		// load the .ini file straight up
		LoadAnIniFile(*SourceIniFilename, ConfigFile);
	}
	else
	{
		GetSourceIniHierarchyFilenames( IniName, Platform, *FPaths::EngineConfigDir(), *FPaths::SourceConfigDir(), ConfigFile.SourceIniHierarchy, false );
		
		// Keep a record of the original settings
		ConfigFile.SourceConfigFile = new FConfigFile();

		// now generate and make sure it's up to date (using IniName as a Base for an ini filename)
		const bool bAllowGeneratedINIs = true;
		GenerateDestIniFile(ConfigFile, GetDestIniFilename(IniName, Platform, *FPaths::GeneratedConfigDir()), ConfigFile.SourceIniHierarchy, bAllowGeneratedINIs, true);
	}
	ConfigFile.Name = IniName;*/
}

void FConfigCacheIni::LoadExternalIniFile(FConfigFile& ConfigFile, const TCHAR* IniName, const TCHAR* EngineConfigDir, const TCHAR* SourceConfigDir, bool bGenerateDestIni, const TCHAR* Platform, const bool bForceReload)
{
	// if bGenerateDestIni is false, that means the .ini is a ready-to-go .ini file, and just needs to be loaded into the FConfigFile
	if (!bGenerateDestIni)
	{
		// generate path to the .ini file (not a Default ini, IniName is the complete name of the file, without path)
		FString SourceIniFilename = FString::Printf(TEXT("%s/%s.ini"), SourceConfigDir, IniName);

		// load the .ini file straight up
		LoadAnIniFile(*SourceIniFilename, ConfigFile);
	}
	else
	{
		GetSourceIniHierarchyFilenames( IniName, Platform, EngineConfigDir, SourceConfigDir, ConfigFile.SourceIniHierarchy, false );

		if ( bForceReload )
		{
			ClearHierarchyCache( IniName );
		}


		// Keep a record of the original settings
		ConfigFile.SourceConfigFile = new FConfigFile();

		// now generate and make sure it's up to date (using IniName as a Base for an ini filename)
		const bool bAllowGeneratedINIs = true;
		GenerateDestIniFile(ConfigFile, GetDestIniFilename(IniName, Platform, *FPaths::GeneratedConfigDir()), ConfigFile.SourceIniHierarchy, bAllowGeneratedINIs, true);
	}
	ConfigFile.Name = IniName;
}


/**
 * Needs to be called after GConfig is set and LoadCoalescedFile was called.
 * Loads the state of console variables.
 * Works even if the variable is registered after the ini file was loaded.
 */
void FConfigCacheIni::LoadConsoleVariablesFromINI()
{
	FString Paths[2];
	const TCHAR* Sections[2];

	// First pass tries to read from "../../../Engine/Config/ConsoleVariables.ini" [Startup] section if it exists
	Paths[0] = FPaths::EngineDir() + TEXT("Config/ConsoleVariables.ini");
	Sections[0] = TEXT("Startup");

	// Second pass looks at the Engine.ini [ConsoleVariables] section
	Paths[1] = GEngineIni;
	Sections[1] = TEXT("ConsoleVariables");

	bool bFoundSection = false;

	for(int32 PassIdx=0;PassIdx<2;PassIdx++)
	{
		FConfigSection* Section = GConfig->GetSectionPrivate(Sections[PassIdx], false, true, *Paths[PassIdx]);

		if(Section)
		{
			for(FConfigSectionMap::TConstIterator It(*Section); It; ++It)
			{
				const FString& KeyString = It.Key().GetPlainNameString(); 
				const FString& ValueString = It.Value();

				IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*KeyString);

				if(CVar)
				{
					// Set if the variable exists.
					CVar->Set(*ValueString, ECVF_SetByConsoleVariablesIni);
				}
				else
				{
					// Create a dummy that is used when someone registers the variable later on.
					IConsoleManager::Get().RegisterConsoleVariable(*KeyString, *ValueString, TEXT("IAmNoRealVariable"),
						(uint32)ECVF_Unregistered | (uint32)ECVF_CreatedFromIni | (uint32)ECVF_SetByConsoleVariablesIni);
				}
			}
			bFoundSection = true;
		}
	}

	if(bFoundSection)
	{
		IConsoleManager::Get().CallAllConsoleVariableSinks();
	}
}

void FConfigFile::UpdateSections(const TCHAR* DiskFilename, const TCHAR* IniRootName/*=NULL*/)
{
	// since we don't want any modifications to other sections, we manually process the file, not read into sections, etc
	FString DiskFile;
	FString NewFile;
	bool bIsLastLineEmtpy = false;
	if (FFileHelper::LoadFileToString(DiskFile, DiskFilename))
	{
		// walk each line
		const TCHAR* Ptr = DiskFile.Len() > 0 ? *DiskFile : NULL;
		bool bDone = Ptr ? false : true;
		bool bIsSkippingSection = true;
		while (!bDone)
		{
			// read the next line
			FString TheLine;
			if (FParse::Line(&Ptr, TheLine, true) == false)
			{
				bDone = true;
			}
			else
			{
				// is this line a section? (must be at least [x])
				if (TheLine.Len() > 3 && TheLine[0] == '[' && TheLine[TheLine.Len() - 1] == ']')
				{
					// look to see if this section is one we are going to update; if so, then skip lines until a new section
					FString SectionName = TheLine.Mid(1, TheLine.Len() - 2);
					bIsSkippingSection = Contains(SectionName);
				}

				// if we aren't skipping, then write out the line
				if (!bIsSkippingSection)
				{
					NewFile += TheLine + LINE_TERMINATOR;

					// track if the last line written was empty
					bIsLastLineEmtpy = TheLine.Len() == 0;
				}
			}
		}
	}

	// load the hierarchy up to right before this file
	if (IniRootName != NULL)
	{
		// get the standard ini files
		SourceIniHierarchy.Empty();
		GetSourceIniHierarchyFilenames(IniRootName, NULL, *FPaths::EngineConfigDir(), *FPaths::SourceConfigDir(), SourceIniHierarchy, false);

		// now chop off this file and any after it
		for (auto& HierarchyFileIt : SourceIniHierarchy)
		{
			if (HierarchyFileIt.Value.Filename == DiskFilename)
			{
				for (uint32 FileIdx = (uint32)HierarchyFileIt.Key; FileIdx < (uint32)EConfigFileHierarchy::NumHierarchyFiles; FileIdx++)
				{
					SourceIniHierarchy.Remove((EConfigFileHierarchy)FileIdx);
				}

				break;
			}
		}

		ClearHierarchyCache(IniRootName);

		// Get a collection of the source hierachy properties
		SourceConfigFile = new FConfigFile();

		// now when Write it called below, it will diff against SourceIniHierarchy
		LoadIniFileHierarchy(SourceIniHierarchy, *SourceConfigFile, true );

	}

	// take what we got above (which has the sections skipped), and then append the new sections
	if (Num() > 0 && !bIsLastLineEmtpy)
	{
		// add a blank link between old sections and new (if there are any new sections)
		NewFile += LINE_TERMINATOR;
	}
	Write(DiskFilename, true, NewFile);
}

bool FConfigFile::UpdateSinglePropertyInSection(const TCHAR* DiskFilename, const TCHAR* PropertyName, const TCHAR* SectionName)
{
	// Result of whether the file has been updated on disk.
	bool bSuccessfullyUpdatedFile = false;

	// Since we don't want any modifications to other sections, or properties, we manually process the file, not read into sections
	FString DiskFile;
	if (FFileHelper::LoadFileToString(DiskFile, DiskFilename))
	{
		FString NewFile;
		const FString DecoratedSectionName = FString::Printf(TEXT("[%s]"), SectionName);

		// Track if the these properties belong to the section we are looking for.
		bool bIsCurrentlyWritingSectionProperty = false;
		
		// Track if the section was ever updated.
		bool bSectionWasUpdated = false;
		// Track if the property was ever updated.
		bool bPropertyWasUpdated = false;

		// Iterate through the file, line by line, to find the property we would like to update.
		FString TheLine;
		const TCHAR* Ptr = DiskFile.Len() > 0 ? *DiskFile : NULL;
		while (Ptr != NULL && FParse::Line(&Ptr, TheLine, true))
		{
			const bool bIsASectionHeader = (TheLine.Len() > 3) && (TheLine[0] == '[') && (TheLine[TheLine.Len() - 1] == ']');
			if (bIsASectionHeader)
			{
				// Cache off the old value of whether we had been writing a section.
				const bool bWasWritingSection = bIsCurrentlyWritingSectionProperty;

				if (!bSectionWasUpdated && bWasWritingSection && !bPropertyWasUpdated)
				{
					bPropertyWasUpdated = true;
					// If we have found the property in the file, let's update it with the new value.
					if (const FConfigSection* LocalSection = this->Find(SectionName))
					{
						if (const FString* LocalSectionPropertyStr = LocalSection->Find(PropertyName))
						{
							NewFile += FString::Printf(TEXT("%s=%s"), PropertyName, *(*LocalSectionPropertyStr)) + LINE_TERMINATOR + LINE_TERMINATOR;
						}
					}
				}

				bIsCurrentlyWritingSectionProperty = (TheLine == DecoratedSectionName);
				bSectionWasUpdated |= bIsCurrentlyWritingSectionProperty;

				NewFile += TheLine + LINE_TERMINATOR;
			}
			else if (bIsCurrentlyWritingSectionProperty && TheLine.StartsWith(PropertyName))
			{
				bPropertyWasUpdated = true;
				// If we have found the property in the file, let's update it with the new value.
				if (const FConfigSection* LocalSection = this->Find(SectionName))
				{
					if (const FString* LocalSectionPropertyStr = LocalSection->Find(PropertyName))
					{
						NewFile += FString::Printf(TEXT("%s=%s"), PropertyName, *(*LocalSectionPropertyStr)) + LINE_TERMINATOR;
					}
				}
			}
			else
			{
				NewFile += TheLine + LINE_TERMINATOR;
			}
		}

		if (bSectionWasUpdated == false)
		{
			// If we reached the eof without updating the section, write the header
			NewFile += LINE_TERMINATOR + DecoratedSectionName + LINE_TERMINATOR;
		}

		if (bPropertyWasUpdated == false)
		{
			// If we reached the eof without updating the property, we should write it out.
			bPropertyWasUpdated = true;
			if (const FConfigSection* LocalSection = this->Find(SectionName))
			{
				if (const FString* LocalSectionPropertyStr = LocalSection->Find(PropertyName))
				{
					NewFile += FString::Printf(TEXT("%s=%s"), PropertyName, *(*LocalSectionPropertyStr)) + LINE_TERMINATOR;
				}
			}
		}

		// Write the updated file to disk.
		bSuccessfullyUpdatedFile = FFileHelper::SaveStringToFile(NewFile, DiskFilename);
	}

	return bSuccessfullyUpdatedFile;
}

void ApplyCVarSettingsFromIni(const TCHAR* InSectionName, const TCHAR* InIniFilename, uint32 SetBy)
{
	// Lookup the config section for this section and group number
	TArray<FString> SectionStrings;
	if (GConfig->GetSection(InSectionName, SectionStrings, InIniFilename))
	{
		// Apply each cvar in the group
		for (int32 VarIndex = 0; VarIndex < SectionStrings.Num(); ++VarIndex)
		{
			FString CVarName;
			FString CVarValue;
			SectionStrings[VarIndex].Split(TEXT("="), &CVarName, &CVarValue, ESearchCase::CaseSensitive);

			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*CVarName);
			if (CVar)
			{
				if (!CVar->TestFlags(EConsoleVariableFlags::ECVF_Cheat))
				{
					if (FCString::Stricmp(*CVarValue, TEXT("True")) == 0
						|| FCString::Stricmp(*CVarValue, TEXT("Yes")) == 0
						|| FCString::Stricmp(*CVarValue, TEXT("On")) == 0)
					{
						CVar->Set(1, (EConsoleVariableFlags)SetBy);
					}
					else if(FCString::Stricmp(*CVarValue, TEXT("False")) == 0
						|| FCString::Stricmp(*CVarValue, TEXT("No")) == 0
						|| FCString::Stricmp(*CVarValue, TEXT("Off")) == 0)
					{
						CVar->Set(0, (EConsoleVariableFlags)SetBy);
					}
					else
					{
						CVar->Set(*CVarValue, (EConsoleVariableFlags)SetBy);
					}
				}
			}
			else
			{
				UE_LOG(LogConsoleResponse, Warning, TEXT("Skipping Unknown console variable: '%s = %s'"), *CVarName, *CVarValue);
				UE_LOG(LogConsoleResponse, Warning, TEXT("  Found in ini file '%s', in section '[%s]'"), InIniFilename, InSectionName);
			}
		}
	}
}

void ApplyCVarSettingsGroupFromIni(const TCHAR* InSectionBaseName, int32 InGroupNumber, const TCHAR* InIniFilename, uint32 SetBy)
{
	// Lookup the config section for this section and group number
	FString SectionName = FString::Printf(TEXT("%s@%d"), InSectionBaseName, InGroupNumber);
	ApplyCVarSettingsFromIni(*SectionName,InIniFilename, SetBy);
}
