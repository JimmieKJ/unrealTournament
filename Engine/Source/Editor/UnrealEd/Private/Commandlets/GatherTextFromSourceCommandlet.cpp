// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromSourceCommandlet, Log, All);

//////////////////////////////////////////////////////////////////////////
//GatherTextFromSourceCommandlet

#define LOC_DEFINE_REGION

UGatherTextFromSourceCommandlet::UGatherTextFromSourceCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::DefineString(TEXT("#define "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::UndefString(TEXT("#undef "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::IfString(TEXT("#if "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::IfDefString(TEXT("#ifdef "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::ElIfString(TEXT("#elif "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::ElseString(TEXT("#else"));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::EndIfString(TEXT("#endif"));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::DefinedString(TEXT("defined "));
const FString UGatherTextFromSourceCommandlet::FPreProcessorDescriptor::IniNamespaceString(TEXT("["));
const FString UGatherTextFromSourceCommandlet::FMacroDescriptor::TextMacroString(TEXT("TEXT"));
const FString UGatherTextFromSourceCommandlet::ChangelistName(TEXT("Update Localization"));

int32 UGatherTextFromSourceCommandlet::Main( const FString& Params )
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
		UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("No config specified."));
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
		UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// SearchDirectoryPaths
	TArray<FString> SearchDirectoryPaths;
	GetPathArrayFromConfig(*SectionName, TEXT("SearchDirectoryPaths"), SearchDirectoryPaths, GatherTextConfigPath);

	// IncludePaths (DEPRECATED)
	{
		TArray<FString> IncludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);
		if (IncludePaths.Num())
		{
			SearchDirectoryPaths.Append(IncludePaths);
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("IncludePaths detected in section %s. IncludePaths is deprecated, please use SearchDirectoryPaths."), *SectionName);
		}
	}

	if (SearchDirectoryPaths.Num() == 0)
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("No search directory paths in section %s."), *SectionName);
		return -1;
	}

	// ExcludePathFilters
	TArray<FString> ExcludePathFilters;
	GetPathArrayFromConfig(*SectionName, TEXT("ExcludePathFilters"), ExcludePathFilters, GatherTextConfigPath);

	// ExcludePaths (DEPRECATED)
	{
		TArray<FString> ExcludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);
		if (ExcludePaths.Num())
		{
			ExcludePathFilters.Append(ExcludePaths);
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("ExcludePaths detected in section %s. ExcludePaths is deprecated, please use ExcludePathFilters."), *SectionName);
		}
	}

	// FileNameFilters
	TArray<FString> FileNameFilters;
	GetStringArrayFromConfig(*SectionName, TEXT("FileNameFilters"), FileNameFilters, GatherTextConfigPath);

	// SourceFileSearchFilters (DEPRECATED)
	{
		TArray<FString> SourceFileSearchFilters;
		GetStringArrayFromConfig(*SectionName, TEXT("SourceFileSearchFilters"), SourceFileSearchFilters, GatherTextConfigPath);
		if (SourceFileSearchFilters.Num())
		{
			FileNameFilters.Append(SourceFileSearchFilters);
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("SourceFileSearchFilters detected in section %s. SourceFileSearchFilters is deprecated, please use FileNameFilters."), *SectionName);
		}
	}

	if (FileNameFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("No source filters in section %s"), *SectionName);
		return -1;
	}

	//Ensure all filters are unique.
	TArray<FString> UniqueSourceFileSearchFilters;
	for (const FString& SourceFileSearchFilter : FileNameFilters)
	{
		UniqueSourceFileSearchFilters.AddUnique(SourceFileSearchFilter);
	}

	// Search in the root folder for each of the wildcard filters specified and build a list of files
	TArray<FString> AllFoundFiles;

	for (FString& SearchDirectoryPath : SearchDirectoryPaths)
	{
		for (const FString& UniqueSourceFileSearchFilter : UniqueSourceFileSearchFilters)
		{
			TArray<FString> RootSourceFiles;

			IFileManager::Get().FindFilesRecursive(RootSourceFiles, *SearchDirectoryPath, *UniqueSourceFileSearchFilter, true, false,false);

			for (FString& RootSourceFile : RootSourceFiles)
			{
				if (FPaths::IsRelative(RootSourceFile))
				{
					RootSourceFile = FPaths::ConvertRelativePathToFull(RootSourceFile);
				}
			}

			AllFoundFiles.Append(RootSourceFiles);
		}
	}

	TArray<FString> FilesToProcess;
	TArray<FString> RemovedList;

	//Run through all the files found and add any that pass the exclude and filter constraints to PackageFilesToProcess
	for (const FString& FoundFile : AllFoundFiles)
	{
		bool bExclude = false;

		//Ensure it does not match the exclude paths if there are some.
		for (FString& ExcludePath : ExcludePathFilters)
		{
			if (FoundFile.MatchesWildcard(ExcludePath) )
			{
				bExclude = true;
				RemovedList.Add(FoundFile);
				break;
			}
		}

		//If we haven't failed any checks, add it to the array of files to process.
		if( !bExclude )
		{
			FilesToProcess.Add(FoundFile);
		}
	}
	
	// Return if no source files were found
	if( FilesToProcess.Num() == 0 )
	{
		FString SpecifiedDirectoriesString;
		for (FString& SearchDirectoryPath : SearchDirectoryPaths)
		{
			SpecifiedDirectoriesString.Append(FString(SpecifiedDirectoriesString.IsEmpty() ? TEXT("") : TEXT("\n")) + FString::Printf(TEXT("+ %s"), *SearchDirectoryPath));
		}
		for (FString& ExcludePath : ExcludePathFilters)
		{
			SpecifiedDirectoriesString.Append(FString(SpecifiedDirectoriesString.IsEmpty() ? TEXT("") : TEXT("\n")) + FString::Printf(TEXT("- %s"), *ExcludePath));
		}

		FString SourceFileSearchFiltersString;
		for (const auto& Filter : UniqueSourceFileSearchFilters)
		{
			SourceFileSearchFiltersString += FString(SourceFileSearchFiltersString.IsEmpty() ? TEXT("") : TEXT(", ")) + Filter;
		}

		UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("The GatherTextFromSource commandlet couldn't find any source files matching (%s) in the specified directories:\n%s"), *SourceFileSearchFiltersString, *SpecifiedDirectoriesString);
		return -1;
	}

	// Add any manifest dependencies if they were provided
	TArray<FString> ManifestDependenciesList;
	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);
	
	for (const FString& ManifestDependency : ManifestDependenciesList)
	{
		FText OutError;
		if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("The GatherTextFromSource commandlet couldn't load the specified manifest dependency: '%'. %s"), *ManifestDependency, *OutError.ToString());
			return -1;
		}
	}

	// Get the loc macros and their syntax
	TArray<FParsableDescriptor*> Parsables;

	Parsables.Add(new FDefineDescriptor());

	Parsables.Add(new FUndefDescriptor());

	Parsables.Add(new FIfDescriptor());

	Parsables.Add(new FIfDefDescriptor());

	Parsables.Add(new FElIfDescriptor());

	Parsables.Add(new FElseDescriptor());

	Parsables.Add(new FEndIfDescriptor());

	Parsables.Add(new FCommandMacroDescriptor());

	// New Localization System with Namespace as literal argument.
	Parsables.Add(new FStringMacroDescriptor( FString(TEXT("NSLOCTEXT")),
		FMacroDescriptor::FMacroArg(FMacroDescriptor::MAS_Namespace, true),
		FMacroDescriptor::FMacroArg(FMacroDescriptor::MAS_Identifier, true),
		FMacroDescriptor::FMacroArg(FMacroDescriptor::MAS_SourceText, true)));
	
	// New Localization System with Namespace as preprocessor define.
	Parsables.Add(new FStringMacroDescriptor( FString(TEXT("LOCTEXT")),
		FMacroDescriptor::FMacroArg(FMacroDescriptor::MAS_Identifier, true),
		FMacroDescriptor::FMacroArg(FMacroDescriptor::MAS_SourceText, true)));

	Parsables.Add(new FIniNamespaceDescriptor());

	// Init a parse context to track the state of the file parsing 
	FSourceFileParseContext ParseCtxt;
	ParseCtxt.GatherManifestHelper = GatherManifestHelper;

	// Get whether we should gather editor-only data. Typically only useful for the localization of UE4 itself.
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldGatherFromEditorOnlyData"), ParseCtxt.ShouldGatherFromEditorOnlyData, GatherTextConfigPath))
	{
		ParseCtxt.ShouldGatherFromEditorOnlyData = false;
	}

	// Parse all source files for macros and add entries to SourceParsedEntries
	for ( FString& SourceFile : FilesToProcess)
	{
		FString ProjectBasePath;
		if (!FPaths::GameDir().IsEmpty())
		{
			ProjectBasePath = FPaths::GameDir();
		}
		else
		{
			ProjectBasePath = FPaths::EngineDir();
		}

		ParseCtxt.Filename = SourceFile;
		FPaths::MakePathRelativeTo(ParseCtxt.Filename, *ProjectBasePath);
		ParseCtxt.LineNumber = 0;
		ParseCtxt.LineText.Empty();
		ParseCtxt.Namespace.Empty();
		ParseCtxt.ExcludedRegion = false;
		ParseCtxt.WithinBlockComment = false;
		ParseCtxt.WithinLineComment = false;
		ParseCtxt.WithinStringLiteral = false;
		ParseCtxt.WithinNamespaceDefine = false;
		ParseCtxt.WithinStartingLine.Empty();
		ParseCtxt.FlushMacroStack();

		FString SourceFileText;
		if (!FFileHelper::LoadFileToString(SourceFileText, *SourceFile))
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Error, TEXT("GatherTextSource failed to open file %s"), *ParseCtxt.Filename);
		}
		else
		{
			if (!ParseSourceText(SourceFileText, Parsables, ParseCtxt))
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("GatherTextSource error(s) parsing source file %s"), *ParseCtxt.Filename);
			}
			else
			{
				if (ParseCtxt.WithinNamespaceDefine == true)
				{
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Non-matching LOCTEXT_NAMESPACE defines found in %s"), *ParseCtxt.Filename);
				}
			}
		}
	}
	
	// Clear parsables list safely
	for (int32 i=0; i<Parsables.Num(); i++)
	{
		delete Parsables[i];
	}

	return 0;
}

FString UGatherTextFromSourceCommandlet::UnescapeLiteralCharacterEscapeSequences(const FString& InString)
{
	// We need to un-escape any octal, hex, or universal character sequences that exist in this string to mimic what happens when the string is processed by the compiler
	enum class EParseState : uint8
	{
		Idle,		// Not currently parsing a sequence
		InOct,		// Within an octal sequence (\012)
		InHex,		// Within an hexadecimal sequence (\xBEEF)
		InUTF16,	// Within a UTF-16 sequence (\u1234)
		InUTF32,	// Within a UTF-32 sequence (\U12345678)
	};

	FString RetString;
	RetString.Reserve(InString.Len());

	EParseState ParseState = EParseState::Idle;
	FString EscapedLiteralCharacter;
	for (const TCHAR* CharPtr = *InString; *CharPtr; ++CharPtr)
	{
		const TCHAR CurChar = *CharPtr;

		switch (ParseState)
		{
		case EParseState::Idle:
			{
				const TCHAR NextChar = *(CharPtr + 1);
				if (CurChar == TEXT('\\') && NextChar)
				{
					if (FChar::IsOctDigit(NextChar))
					{
						ParseState = EParseState::InOct;
					}
					else if (NextChar == TEXT('x'))
					{
						// Skip the format marker
						++CharPtr;
						ParseState = EParseState::InHex;
					}
					else if (NextChar == TEXT('u'))
					{
						// Skip the format marker
						++CharPtr;
						ParseState = EParseState::InUTF16;
					}
					else if (NextChar == TEXT('U'))
					{
						// Skip the format marker
						++CharPtr;
						ParseState = EParseState::InUTF32;
					}
				}
				
				if (ParseState == EParseState::Idle)
				{
					RetString.AppendChar(CurChar);
				}
				else
				{
					EscapedLiteralCharacter.Reset();
				}
			}
			break;

		case EParseState::InOct:
			{
				if (FChar::IsOctDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);

					// Octal sequences can only be up-to 3 digits long
					check(EscapedLiteralCharacter.Len() <= 3);
					if (EscapedLiteralCharacter.Len() == 3)
					{
						RetString.AppendChar((TCHAR)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 8));
						ParseState = EParseState::Idle;
						// Deliberately not appending the current character here, as it was already pushed into the escaped literal character string
					}
				}
				else
				{
					RetString.AppendChar((TCHAR)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 8));
					ParseState = EParseState::Idle;
					RetString.AppendChar(CurChar);
				}
			}
			break;

		case EParseState::InHex:
			{
				if (FChar::IsHexDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);
				}
				else
				{
					RetString.AppendChar((TCHAR)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 16));
					ParseState = EParseState::Idle;
					RetString.AppendChar(CurChar);
				}
			}
			break;

		case EParseState::InUTF16:
			{
				if (FChar::IsHexDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);

					// UTF-16 sequences can only be up-to 4 digits long
					check(EscapedLiteralCharacter.Len() <= 4);
					if (EscapedLiteralCharacter.Len() == 4)
					{
						const uint32 UnicodeCodepoint = (uint32)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 16);

						FString UnicodeString;
						if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
						{
							RetString.Append(MoveTemp(UnicodeString));
						}

						ParseState = EParseState::Idle;
						// Deliberately not appending the current character here, as it was already pushed into the escaped literal character string
					}
				}
				else
				{
					const uint32 UnicodeCodepoint = (uint32)FCString::Strtoi(*EscapedLiteralCharacter, nullptr, 16);

					FString UnicodeString;
					if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
					{
						RetString.Append(MoveTemp(UnicodeString));
					}

					ParseState = EParseState::Idle;
					RetString.AppendChar(CurChar);
				}
			}
			break;

		case EParseState::InUTF32:
			{
				if (FChar::IsHexDigit(CurChar))
				{
					EscapedLiteralCharacter.AppendChar(CurChar);

					// UTF-32 sequences can only be up-to 8 digits long
					check(EscapedLiteralCharacter.Len() <= 8);
					if (EscapedLiteralCharacter.Len() == 8)
					{
						const uint32 UnicodeCodepoint = (uint32)FCString::Strtoui64(*EscapedLiteralCharacter, nullptr, 16);

						FString UnicodeString;
						if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
						{
							RetString.Append(MoveTemp(UnicodeString));
						}

						ParseState = EParseState::Idle;
						// Deliberately not appending the current character here, as it was already pushed into the escaped literal character string
					}
				}
				else
				{
					const uint32 UnicodeCodepoint = (uint32)FCString::Strtoui64(*EscapedLiteralCharacter, nullptr, 16);

					FString UnicodeString;
					if (FUnicodeChar::CodepointToString(UnicodeCodepoint, UnicodeString))
					{
						RetString.Append(MoveTemp(UnicodeString));
					}

					ParseState = EParseState::Idle;
					RetString.AppendChar(CurChar);
				}
			}
			break;

		default:
			break;
		}
	}

	return RetString;
}

FString UGatherTextFromSourceCommandlet::RemoveStringFromTextMacro(const FString& TextMacro, const FString& IdentForLogging, bool& Error)
{
	FString Text;
	Error = true;

	// need to strip text literal out of TextMacro ( format should be TEXT("stringvalue") ) 
	if (!TextMacro.StartsWith(FMacroDescriptor::TextMacroString))
	{
		Error = false;
		//UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Missing TEXT macro in %s"), *IdentForLogging);
		Text = TextMacro.TrimQuotes();
	}
	else
	{
		int32 OpenQuoteIdx = TextMacro.Find(TEXT("\""), ESearchCase::CaseSensitive);
		if (0 > OpenQuoteIdx || TextMacro.Len() - 1 == OpenQuoteIdx)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Missing quotes in %s"), *FLocTextHelper::SanitizeLogOutput(IdentForLogging));
		}
		else
		{
			int32 CloseQuoteIdx = TextMacro.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenQuoteIdx+1);
			if (0 > CloseQuoteIdx)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Missing quotes in %s"), *FLocTextHelper::SanitizeLogOutput(IdentForLogging));
			}
			else
			{
				Text = TextMacro.Mid(OpenQuoteIdx + 1, CloseQuoteIdx - OpenQuoteIdx - 1);
				Error = false;
			}
		}
	}

	if (!Error)
	{
		Text = UnescapeLiteralCharacterEscapeSequences(Text);
	}

	return Text;
}

FString UGatherTextFromSourceCommandlet::StripCommentsFromToken(const FString& InToken, FSourceFileParseContext& Context)
{
	check(!Context.WithinBlockComment);
	check(!Context.WithinLineComment);
	check(!Context.WithinStringLiteral);

	// Remove both block and inline comments from the given token
	FString StrippedToken;
	StrippedToken.Reserve(InToken.Len());

	TCHAR WithinQuote = 0;
	bool bIgnoreNextQuote = false;
	for (const TCHAR* Char = *InToken; *Char; ++Char)
	{
		if (WithinQuote != 0)
		{
			StrippedToken += *Char;

			if (!bIgnoreNextQuote)
			{
				if (*Char == TEXT('\\'))
				{
					bIgnoreNextQuote = true;
					continue;
				}

				if (*Char == WithinQuote)
				{
					// Found an unescaped closing quote - we are no longer within quotes
					WithinQuote = 0;
				}
			}

			bIgnoreNextQuote = false;
		}
		else
		{
			if (*Char == TEXT('/'))
			{
				const TCHAR* NextChar = Char + 1;

				if (*NextChar == TEXT('/'))
				{
					// Found an inline quote - this strips the remainder of the string so just break out of the loop
					break;
				}

				if (*NextChar == TEXT('*'))
				{
					// Found a block comment - skip all characters until we find the closing quote
					Context.WithinBlockComment = true;
					++Char; // Skip over the opening slash, and the for loop will skip over the *
					continue;
				}
			}

			if (Context.WithinBlockComment)
			{
				if (*Char == TEXT('*'))
				{
					const TCHAR* NextChar = Char + 1;

					if (*NextChar == TEXT('/'))
					{
						// Found the end of a block comment
						Context.WithinBlockComment = false;
						++Char; // Skip over the opening *, and the for loop will skip over the slash
						continue;
					}
				}

				// Skip over all characters while within a block comment
				continue;
			}

			StrippedToken += *Char;

			if (*Char == TEXT('"') || *Char == TEXT('\''))
			{
				// We found an opening quote - keep track of it until we find a matching closing quote
				WithinQuote = *Char;
			}
		}
	}

	return StrippedToken.Trim().TrimTrailing();
}

bool UGatherTextFromSourceCommandlet::ParseSourceText(const FString& Text, const TArray<UGatherTextFromSourceCommandlet::FParsableDescriptor*>& Parsables, FSourceFileParseContext& ParseCtxt)
{
	// Create array of ints, one for each parsable we're looking for.
	TArray<int32> ParsableMatchCounters;
	ParsableMatchCounters.AddZeroed(Parsables.Num());

	// Cache array of tokens
	TArray<FString> ParsableTokens;
	for (int32 ParIdx=0; ParIdx<Parsables.Num(); ParIdx++)
	{
		ParsableTokens.Add(Parsables[ParIdx]->GetToken());
	}

	// Split the file into lines of 
	TArray<FString> TextLines;
	Text.ParseIntoArrayLines(TextLines, false);

	// Move through the text lines looking for the tokens that denote the items in the Parsables list
	for (int32 LineIdx = 0; LineIdx < TextLines.Num(); LineIdx++)
	{
		const FString& Line = TextLines[LineIdx].TrimTrailing();
		if( Line.IsEmpty() )
			continue;

		// Use these pending vars to defer parsing a token hit until longer tokens can't hit too
		int32 PendingParseIdx = -1;
		const TCHAR* ParsePoint = NULL;
		ParsableMatchCounters.Empty(Parsables.Num());
		ParsableMatchCounters.AddZeroed(Parsables.Num());
		ParseCtxt.LineNumber = LineIdx + 1;
		ParseCtxt.LineText = Line;
		ParseCtxt.EndParsingCurrentLine = false;

		const TCHAR* Cursor = *Line;
		bool EndOfLine = false;
		while (!EndOfLine && !ParseCtxt.EndParsingCurrentLine)
		{
			// Check if we're starting comments or string literals. Begins *at* "//" or "/*".
			if (!ParseCtxt.WithinLineComment && !ParseCtxt.WithinBlockComment && !ParseCtxt.WithinStringLiteral)
			{
				const TCHAR* ForwardCursor = Cursor;
				if(*ForwardCursor == TEXT('/'))
				{
					++ForwardCursor;
					switch(*ForwardCursor)
					{
					case TEXT('/'):
						{
							ParseCtxt.WithinLineComment = true;
							ParseCtxt.WithinStartingLine = ParseCtxt.LineText;
						}
						break;
					case TEXT('*'):
						{
							ParseCtxt.WithinBlockComment = true;
							ParseCtxt.WithinStartingLine = ParseCtxt.LineText;
					}
						break;
					}
				}
			}

			if (!ParseCtxt.WithinLineComment && !ParseCtxt.WithinBlockComment && !ParseCtxt.WithinStringLiteral)
			{
				if (*Cursor == TEXT('\"') && Cursor >= *Line)
				{
					const TCHAR* ReverseCursor = Cursor;
					--ReverseCursor;
					if ((*ReverseCursor != TEXT('\\') && *ReverseCursor != TEXT('\'') && ReverseCursor >= *Line) || ReverseCursor < *Line)
					{
						ParseCtxt.WithinStringLiteral = true;
						ParseCtxt.WithinStartingLine = ParseCtxt.LineText;
					}
					else 
					{
						bool IsEscaped = false;
						//if the backslash or single quote is itself escaped then the quote is good
						while (*(--ReverseCursor) == TEXT('\\') && ReverseCursor >= *Line)
						{
							IsEscaped = !IsEscaped;
						}

						if (IsEscaped)
						{
							ParseCtxt.WithinStringLiteral = true;
							ParseCtxt.WithinStartingLine = ParseCtxt.LineText;
						}
						else
						{
							//   check for '"'
							ReverseCursor = Cursor;
							--ReverseCursor;
							const TCHAR* ForwardCursor = Cursor;
							++ForwardCursor;
							if (*ReverseCursor == TEXT('\'') && *ForwardCursor != TEXT('\'') && ReverseCursor >= *Line)
							{
								ParseCtxt.WithinStringLiteral = true;
								ParseCtxt.WithinStartingLine = ParseCtxt.LineText;
							}
						}
					}
				}
			}
			else if (ParseCtxt.WithinStringLiteral)
			{
				const TCHAR* ReverseCursor = Cursor;
				if (*ReverseCursor == TEXT('\"') && ReverseCursor >= *Line)
				{
					--ReverseCursor;
					if ((*ReverseCursor != TEXT('\\') && *ReverseCursor != TEXT('\'') && ReverseCursor >= *Line) || ReverseCursor < *Line)
					{
						ParseCtxt.WithinStringLiteral = false;
					}
					else
					{
						bool IsEscaped = false;
						//if the backslash or single quote is itself escaped then the quote is good
						while (*(--ReverseCursor) == TEXT('\\') && ReverseCursor >= *Line)
						{
							IsEscaped = !IsEscaped;
						}

						if (IsEscaped)
						{
							ParseCtxt.WithinStringLiteral = false;
						}
						else
						{
							//   check for '"'
							ReverseCursor = Cursor;
							--ReverseCursor;
							const TCHAR* ForwardCursor = Cursor;
							++ForwardCursor;
							if (*ReverseCursor == TEXT('\'') && *ForwardCursor != TEXT('\'') && ReverseCursor >= *Line)
							{
								ParseCtxt.WithinStringLiteral = false;
							}
						}
					}
				}
			}

			// Check if we're ending comments. Ends *after* "*/".
			if(ParseCtxt.WithinBlockComment)
			{
				const TCHAR* ReverseCursor = Cursor;
				if(*ReverseCursor == TEXT('/') && ReverseCursor >= *Line)
				{
					--ReverseCursor;
					if(*ReverseCursor == TEXT('*')  && ReverseCursor >= *Line)
					{
						ParseCtxt.WithinBlockComment = false;
					}
				}
			}

			for (int32 ParIdx=0; ParIdx<Parsables.Num(); ParIdx++)
			{
				FString& Token = ParsableTokens[ParIdx];

				if (*Cursor == Token[ParsableMatchCounters[ParIdx]])
				{
					// Char at cursor matches the next char in the parsable's identifying token
					if (Token.Len() == ++(ParsableMatchCounters[ParIdx]))
					{
						// don't immediately parse - this parsable has seen its entire token but a longer one could be about to hit too
						const TCHAR* TokenStart = Cursor + 1 - Token.Len();
						if (0 > PendingParseIdx || ParsePoint >= TokenStart)
						{
							PendingParseIdx = ParIdx;
							ParsePoint = TokenStart;
						}
					}
				}
				else
				{
					// Char at cursor doesn't match the next char in the parsable's identifying token
					// Reset the counter to start of the token
					ParsableMatchCounters[ParIdx] = 0;
				}
			}

			// Now check PendingParse and only run it if there are no better candidates
			if (0 <= PendingParseIdx)
			{
				bool MustDefer = false; // pending will be deferred if another parsable has a equal and greater number of matched chars
				if( !Parsables[PendingParseIdx]->OverridesLongerTokens() )
				{
					for (int32 ParIdx=0; ParIdx<Parsables.Num(); ParIdx++)
					{
						if (PendingParseIdx != ParIdx)
						{
							if (ParsableMatchCounters[ParIdx] >= ParsableTokens[PendingParseIdx].Len())
							{
								// a longer token is matching so defer
								MustDefer = true;
							}
						}
					}
				}

				if (!MustDefer)
				{
					// Do the parse now
					Parsables[PendingParseIdx]->TryParse(FString(ParsePoint), ParseCtxt);
					for(auto& Element : ParsableMatchCounters)
					{
						Element = 0;
					}
					PendingParseIdx = -1;
					ParsePoint = NULL;
				}
			}

			EndOfLine = ('\0' == *(++Cursor)) ? true : false;
			if(EndOfLine)
			{
				ParseCtxt.WithinLineComment = false;
			}
		}
	}

	return true;
}

bool UGatherTextFromSourceCommandlet::FSourceFileParseContext::AddManifestText( const FString& Token, const FString& InNamespace, const FString& SourceText, const FManifestContext& Context )
{
	const bool bIsEditorOnly = EvaluateMacroStack() == EMacroBlockState::EditorOnly;

	if (!bIsEditorOnly || ShouldGatherFromEditorOnlyData)
	{
		FString EntryDescription = FString::Printf( TEXT("In %s macro at %s - line %d:%s"),
			*Token,
			*Filename, 
			LineNumber, 
			*LineText);
		FLocItem Source( SourceText.ReplaceEscapedCharWithChar() );
		return GatherManifestHelper->AddSourceText(InNamespace, Source, Context, &EntryDescription);
	}

	return false;
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::PushMacroBlock( const FString& InBlockCtx )
{
	MacroBlockStack.Push(InBlockCtx);
	CachedMacroBlockState.Reset();
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::PopMacroBlock()
{
	if (MacroBlockStack.Num() > 0)
	{
		MacroBlockStack.Pop(/*bAllowShrinking*/false);
		CachedMacroBlockState.Reset();
	}
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::FlushMacroStack()
{
	MacroBlockStack.Reset();
}

UGatherTextFromSourceCommandlet::EMacroBlockState UGatherTextFromSourceCommandlet::FSourceFileParseContext::EvaluateMacroStack() const
{
	if (CachedMacroBlockState.IsSet())
	{
		return CachedMacroBlockState.GetValue();
	}

	static const FString WithEditorString = TEXT("WITH_EDITOR");
	static const FString WithEditorOnlyDataString = TEXT("WITH_EDITORONLY_DATA");

	CachedMacroBlockState = EMacroBlockState::Normal;
	for (const FString& BlockCtx : MacroBlockStack)
	{
		if (BlockCtx.Equals(WithEditorString, ESearchCase::CaseSensitive) || BlockCtx.Equals(WithEditorOnlyDataString, ESearchCase::CaseSensitive))
		{
			CachedMacroBlockState = EMacroBlockState::EditorOnly;
			break;
		}
	}

	return CachedMacroBlockState.GetValue();
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::SetDefine(const FString& InDefineCtx)
{
	static const FString LocDefRegionString = TEXT("LOC_DEFINE_REGION");
	static const FString LocNamespaceString = TEXT("LOCTEXT_NAMESPACE");

	if (InDefineCtx.Equals(LocDefRegionString, ESearchCase::CaseSensitive))
	{
		// #define LOC_DEFINE_REGION
		if (ExcludedRegion)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Found a '#define LOC_DEFINE_REGION' while still within another '#define LOC_DEFINE_REGION'. File %s at line %d"), *Filename, LineNumber);
		}
		else
		{
			ExcludedRegion = true;
		}
		return;
	}
	else if (!ExcludedRegion)
	{
		if (InDefineCtx.StartsWith(LocNamespaceString, ESearchCase::CaseSensitive) && InDefineCtx.IsValidIndex(LocNamespaceString.Len()) && (FText::IsWhitespace(InDefineCtx[LocNamespaceString.Len()]) || InDefineCtx[LocNamespaceString.Len()] == TEXT('"')))
		{
			// #define LOCTEXT_NAMESPACE <namespace>
			if (WithinNamespaceDefine)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Found a '#define LOCTEXT_NAMESPACE' while still within another '#define LOCTEXT_NAMESPACE'. File %s at line %d"), *Filename, LineNumber);
			}
			else
			{
				FString RemainingText = InDefineCtx.RightChop(LocNamespaceString.Len()).Trim();

				bool RemoveStringError;
				const FString DefineDesc = FString::Printf(TEXT("%s define %s(%d):%s"), *RemainingText, *Filename, LineNumber, *LineText);
				FString NewNamespace = RemoveStringFromTextMacro(RemainingText, DefineDesc, RemoveStringError);

				if (!RemoveStringError)
				{
					Namespace = MoveTemp(NewNamespace);
					WithinNamespaceDefine = true;
				}
			}
			return;
		}
	}
}

void UGatherTextFromSourceCommandlet::FSourceFileParseContext::RemoveDefine(const FString& InDefineCtx)
{
	static const FString LocDefRegionString = TEXT("LOC_DEFINE_REGION");
	static const FString LocNamespaceString = TEXT("LOCTEXT_NAMESPACE");

	if (InDefineCtx.Equals(LocDefRegionString, ESearchCase::CaseSensitive))
	{
		// #undef LOC_DEFINE_REGION
		if (!ExcludedRegion)
		{
			UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Found an '#undef LOC_DEFINE_REGION' without a corresponding '#define LOC_DEFINE_REGION'. File %s at line %d"), *Filename, LineNumber);
		}
		else
		{
			ExcludedRegion = false;
		}
		return;
	}
	else if (!ExcludedRegion)
	{
		if (InDefineCtx.Equals(LocNamespaceString, ESearchCase::CaseSensitive))
		{
			// #undef LOCTEXT_NAMESPACE
			if (!WithinNamespaceDefine)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Found an '#undef LOCTEXT_NAMESPACE' without a corresponding '#define LOCTEXT_NAMESPACE'. File %s at line %d"), *Filename, LineNumber);
			}
			else
			{
				Namespace.Empty();
				WithinNamespaceDefine = false;
			}
			return;
		}
	}
}

void UGatherTextFromSourceCommandlet::FDefineDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #define <defname>
	//  or
	// #define <defname> <value>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).Trim();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		Context.SetDefine(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FUndefDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #undef <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).Trim();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		Context.RemoveDefine(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FIfDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #if <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).Trim();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		// Handle "#if defined <defname>"
		if (RemainingText.StartsWith(DefinedString, ESearchCase::CaseSensitive))
		{
			RemainingText = RemainingText.RightChop(DefinedString.Len()).Trim();
		}

		Context.PushMacroBlock(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FIfDefDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #ifdef <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).Trim();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		Context.PushMacroBlock(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FElIfDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #elif <defname>

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		FString RemainingText = Text.RightChop(GetToken().Len()).Trim();
		RemainingText = StripCommentsFromToken(RemainingText, Context);

		// Handle "#elif defined <defname>"
		if (RemainingText.StartsWith(DefinedString, ESearchCase::CaseSensitive))
		{
			RemainingText = RemainingText.RightChop(DefinedString.Len()).Trim();
		}

		Context.PopMacroBlock(); // Pop the current #if or #ifdef state
		Context.PushMacroBlock(RemainingText);
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FElseDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #else

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		Context.PopMacroBlock(); // Pop the current #if or #ifdef state
		Context.PushMacroBlock(FString());
		Context.EndParsingCurrentLine = true;
	}
}

void UGatherTextFromSourceCommandlet::FEndIfDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// #endif

	if (!Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		Context.PopMacroBlock(); // Pop the current #if or #ifdef state
		Context.EndParsingCurrentLine = true;
	}
}

bool UGatherTextFromSourceCommandlet::FMacroDescriptor::ParseArgsFromMacro(const FString& Text, TArray<FString>& Args, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// NAME(param0, param1, param2, etc)

	bool Success = false;

	FString RemainingText = Text.RightChop(GetToken().Len()).Trim();
	int32 OpenBracketIdx = RemainingText.Find(TEXT("("));
	if (0 > OpenBracketIdx)
	{
		UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Missing bracket '(' in %s macro in %s(%d):%s"), *GetToken(), *Context.Filename, Context.LineNumber, *FLocTextHelper::SanitizeLogOutput(Context.LineText));
		//Dont assume this is an error. It's more likely trying to parse something it shouldn't be.
		return false;
	}
	else
	{
		Args.Empty();

		bool bInDblQuotes = false;
		bool bInSglQuotes = false;
		int32 BracketStack = 1;
		bool bEscapeNextChar = false;

		const TCHAR* ArgStart = *RemainingText + OpenBracketIdx + 1;
		const TCHAR* Cursor = ArgStart;
		for (; 0 < BracketStack && '\0' != *Cursor; ++Cursor)
		{
			if (bEscapeNextChar)
			{
				bEscapeNextChar = false;
			}
			else if ((bInDblQuotes || bInSglQuotes) && !bEscapeNextChar && '\\' == *Cursor)
			{
				bEscapeNextChar = true;
			}
			else if (bInDblQuotes)
			{
				if ('\"' == *Cursor)
				{
					bInDblQuotes = false;
				}
			}
			else if (bInSglQuotes)
			{
				if ('\'' == *Cursor)
				{
					bInSglQuotes = false;
				}
			}
			else if ('\"' == *Cursor)
			{
				bInDblQuotes = true;
			}
			else if ('\'' == *Cursor)
			{
				bInSglQuotes = true;
			}
			else if ('(' == *Cursor)
			{
				++BracketStack;
			}
			else if (')' == *Cursor)
			{
				--BracketStack;

				if (0 > BracketStack)
				{
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Unexpected bracket ')' in %s macro in %s(%d):%s"), *GetToken(), *Context.Filename, Context.LineNumber, *FLocTextHelper::SanitizeLogOutput(Context.LineText));
					return false;
				}
			}
			else if (1 == BracketStack && ',' == *Cursor)
			{
				// create argument from ArgStart to Cursor and set Start next char
				Args.Add(FString(Cursor - ArgStart, ArgStart));
				ArgStart = Cursor + 1;
			}
		}

		if (0 == BracketStack)
		{
			Args.Add(FString(Cursor - ArgStart - 1, ArgStart));
		}
		else
		{
			Args.Add(FString(ArgStart));
		}

		Success = 0 < Args.Num() ? true : false;	
	}

	return Success;
}

bool UGatherTextFromSourceCommandlet::FMacroDescriptor::PrepareArgument(FString& Argument, bool IsAutoText, const FString& IdentForLogging, bool& OutHasQuotes)
{
	bool Error = false;
	if (!IsAutoText)
	{
		Argument = RemoveStringFromTextMacro(Argument, IdentForLogging, Error);
		OutHasQuotes = Error ? false : true;
	}
	else
	{
		Argument = Argument.TrimTrailing().TrimQuotes(&OutHasQuotes);
		Argument = UnescapeLiteralCharacterEscapeSequences(Argument);
	}
	return Error ? false : true;
}

void UGatherTextFromSourceCommandlet::FCommandMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// UI_COMMAND(LocKey, DefaultLangString, DefaultLangTooltipString, <IgnoredParam>, <IgnoredParam>)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> Arguments;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), Arguments, Context))
		{
			if (Arguments.Num() != 5)
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Too many arguments in command %s macro in %s(%d):%s"), *GetToken(), *Context.Filename, Context.LineNumber, *FLocTextHelper::SanitizeLogOutput(Context.LineText));
			}
			else
			{
				FString Identifier = Arguments[0].Trim();
				const FString UICommandRootNamespace = TEXT("UICommands");
				FString Namespace = Context.WithinNamespaceDefine && !Context.Namespace.IsEmpty() ? FString::Printf( TEXT("%s.%s"), *UICommandRootNamespace, *Context.Namespace) : UICommandRootNamespace;
				FString SourceLocation = FString( Context.Filename + TEXT(" - line ") + FString::FromInt(Context.LineNumber) );
				FString SourceText = Arguments[1].Trim();

				if ( Identifier.IsEmpty() )
				{
					//The command doesn't have an identifier so we can't gather it
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("UICOMMAND macro doesn't have unique identifier. %s"), *SourceLocation );
					return;
				}

				// parse DefaultLangString argument - this arg will be in quotes without TEXT macro
				bool HasQuotes;
				FString MacroDesc = FString::Printf(TEXT("\"FriendlyName\" argument in %s macro %s(%d):%s"), *GetToken(), *Context.Filename, Context.LineNumber, *Context.LineText);
				if ( PrepareArgument(SourceText, true, MacroDesc, HasQuotes) )
				{
					if ( HasQuotes && !Identifier.IsEmpty() && !SourceText.IsEmpty() )
					{
						// First create the command entry
						FManifestContext CommandContext;
						CommandContext.Key = Identifier;
						CommandContext.SourceLocation = SourceLocation;

						Context.AddManifestText( GetToken(), Namespace, SourceText, CommandContext );

						// parse DefaultLangTooltipString argument - this arg will be in quotes without TEXT macro
						FString TooltipSourceText = Arguments[2].Trim();
						MacroDesc = FString::Printf(TEXT("\"InDescription\" argument in %s macro %s(%d):%s"), *GetToken(), *Context.Filename, Context.LineNumber, *Context.LineText);
						if (PrepareArgument(TooltipSourceText, true, MacroDesc, HasQuotes))
						{
							if (HasQuotes && !TooltipSourceText.IsEmpty())
							{
								// Create the tooltip entry
								FManifestContext CommandTooltipContext;
								CommandTooltipContext.Key = Identifier + TEXT("_ToolTip");
								CommandTooltipContext.SourceLocation = SourceLocation;

								Context.AddManifestText( GetToken(), Namespace, TooltipSourceText, CommandTooltipContext );
							}
						}
					}
				}
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FStringMacroDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// MACRONAME(param0, param1, param2)

	if (!Context.ExcludedRegion && !Context.WithinBlockComment && !Context.WithinLineComment && !Context.WithinStringLiteral)
	{
		TArray<FString> ArgArray;
		if (ParseArgsFromMacro(StripCommentsFromToken(Text, Context), ArgArray, Context))
		{
			int32 NumArgs = ArgArray.Num();

			if (NumArgs != Arguments.Num())
			{
				UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Too many arguments in %s macro in %s(%d):%s"), *GetToken(), *Context.Filename, Context.LineNumber, *FLocTextHelper::SanitizeLogOutput(Context.LineText));
			}
			else
			{
				FString Identifier;
				FString Namespace = Context.Namespace;
				FString SourceLocation = FString( Context.Filename + TEXT(" - line ") + FString::FromInt(Context.LineNumber) );
				FString SourceText;

				bool ArgParseError = false;
				for (int32 ArgIdx=0; ArgIdx<Arguments.Num(); ArgIdx++)
				{
					FMacroArg Arg = Arguments[ArgIdx];
					FString ArgText = ArgArray[ArgIdx].Trim();

					bool HasQuotes;
					FString MacroDesc = FString::Printf(TEXT("argument %d of %d in localization macro %s %s(%d):%s"), ArgIdx+1, Arguments.Num(), *GetToken(), *Context.Filename, Context.LineNumber, *FLocTextHelper::SanitizeLogOutput(Context.LineText));
					if (!PrepareArgument(ArgText, Arg.IsAutoText, MacroDesc, HasQuotes))
					{
						ArgParseError = true;
						break;
					}

					switch (Arg.Semantic)
					{
					case MAS_Namespace:
						{
							Namespace = ArgText;
						}
						break;
					case MAS_Identifier:
						{
							Identifier = ArgText;
						}
						break;
					case MAS_SourceText:
						{
							SourceText = ArgText;
						}
						break;
					}
				}

				if ( Identifier.IsEmpty() )
				{
					//The command doesn't have an identifier so we can't gather it
					UE_LOG(LogGatherTextFromSourceCommandlet, Warning, TEXT("Macro doesn't have unique identifier. %s"), *SourceLocation );
					return;
				}

				if (!ArgParseError && !Identifier.IsEmpty() && !SourceText.IsEmpty())
				{
					FManifestContext MacroContext;
					MacroContext.Key = Identifier;
					MacroContext.SourceLocation = SourceLocation;

					Context.AddManifestText( GetToken(), Namespace, SourceText, MacroContext );
				}
			}
		}
	}
}

void UGatherTextFromSourceCommandlet::FIniNamespaceDescriptor::TryParse(const FString& Text, FSourceFileParseContext& Context) const
{
	// Attempt to parse something of the format
	// [<config section name>]
	if (!Context.ExcludedRegion)
	{
		if( FCString::Stricmp( *FPaths::GetExtension( Context.Filename, false ), TEXT("ini") ) == 0 &&
			Context.LineText[ 0 ] == '[' )
		{
			int32 ClosingBracket;
			if( Text.FindChar( ']', ClosingBracket ) && ClosingBracket > 1 )
			{
				Context.Namespace = Text.Mid( 1, ClosingBracket - 1 );
				Context.EndParsingCurrentLine = true;
			}
		}
	}
}

#undef LOC_DEFINE_REGION
