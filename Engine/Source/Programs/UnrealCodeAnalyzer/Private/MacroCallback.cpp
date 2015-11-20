// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerPCH.h"
#include "MacroCallback.h"

namespace UnrealCodeAnalyzer
{
	ELogVerbosity::Type LogVerbosity = ELogVerbosity::Log;

	bool FMacroCallback::IsBuiltInMacro(const clang::MacroDirective* MacroDirective)
	{
		return MacroDirective->getMacroInfo()->isBuiltinMacro();
	}

	FMacroCallback::FMacroCallback(clang::StringRef Filename, clang::SourceManager& SourceManager, const clang::Preprocessor& Preprocessor) :
		Filename(Filename), SourceManager(SourceManager), MacroDefinitionToFile(), Preprocessor(Preprocessor)
	{ }

	void FMacroCallback::MacroExpands(const clang::Token &MacroNameTok, const clang::MacroDirective *MD, clang::SourceRange Range, const clang::MacroArgs *Args)
	{
		auto MacroName = MacroNameTok.getIdentifierInfo()->getName();
		if (IsBuiltInMacro(MD) || IsMacroDefinedInCommandLine(MacroName) || IsPredefinedMacro(MacroName))
		{
			return;
		}

		if (MacroDefinitionToFile.Contains(FString(MacroName.data())))
		{
			auto MacroDefinitionFilename = FString(*MacroDefinitionToFile[MacroName.data()]);
			MacroDefinitionFilename.ReplaceInline(TEXT("\\"), TEXT("/"));
			auto MacroDefinitionFilenameHash = FIncludeSetMapKeyFuncs::GetKeyHash(MacroDefinitionFilename);

			auto MacroExpansionFilename = FString(SourceManager.getFilename(Range.getBegin()).data());
			MacroExpansionFilename.ReplaceInline(TEXT("\\"), TEXT("/"));
			auto MacroExpansionFilenameHash = FIncludeSetMapKeyFuncs::GetKeyHash(MacroExpansionFilename);

			if (!Includes.Contains(MacroExpansionFilenameHash))
			{
				Includes.FindOrAdd(MacroExpansionFilenameHash).Add(MacroDefinitionFilenameHash);
			}

			GHashToFilename.Add(MacroDefinitionFilenameHash, MacroDefinitionFilename);
			GHashToFilename.Add(MacroExpansionFilenameHash, MacroExpansionFilename);

			return;
		}
		
		OutputFileContents.Logf(TEXT("Internal error. Found usage of unknown macro %s."), ANSI_TO_TCHAR(MacroName.data()));
	}

	void FMacroCallback::MacroDefined(const clang::Token &MacroNameTok, const clang::MacroDirective *MD)
	{
		auto MacroName = MacroNameTok.getIdentifierInfo()->getName();

		if (IsMacroDefinedInCommandLine(MacroName) || IsBuiltInMacro(MD) || IsPredefinedMacro(MacroName))
		{
			return;
		}

		auto MacroDefinitionFile = FString(SourceManager.getFilename(MacroNameTok.getLocation()).data());
		MacroDefinitionFile.ReplaceInline(TEXT("\\"), TEXT("/"));
		if (MacroDefinitionFile.IsEmpty())
		{
			OutputFileContents.Logf(TEXT("Found unexpected definition of macro %s."), ANSI_TO_TCHAR(MacroName.data()));
		}
		MacroDefinitionToFile.Add(MacroName.data(), MacroDefinitionFile);
	}

	bool FMacroCallback::IsMacroDefinedInCommandLine(clang::StringRef MacroName)
	{
		auto PreprocessorOptions = Preprocessor.getPreprocessorOpts();
		auto PotentialMacro = std::find_if(
			PreprocessorOptions.Macros.begin(),
			PreprocessorOptions.Macros.end(),
			[&MacroName](const std::pair<std::string, bool>& Option)
		{
			return Option.first.find(MacroName) != std::string::npos;
		});
		
		if (PotentialMacro == PreprocessorOptions.Macros.end())
		{
			// Macro not found in command-line
			return false;
		}

		return (PotentialMacro->second == 0);
	}

	bool FMacroCallback::IsPredefinedMacro(clang::StringRef Macro)
	{
		return Preprocessor.getPredefines().find(Macro) != std::string::npos;
	}

	void FMacroCallback::If(clang::SourceLocation Loc, clang::SourceRange ConditionRange, ConditionValueKind ConditionValue)
	{

	}

	void FMacroCallback::Elif(clang::SourceLocation Loc, clang::SourceRange ConditionRange, ConditionValueKind ConditionValue, clang::SourceLocation IfLoc)
	{

	}

	void FMacroCallback::Ifdef(clang::SourceLocation Loc, const clang::Token &MacroNameTok, const clang::MacroDirective *MD)
	{

	}

	void FMacroCallback::Ifndef(clang::SourceLocation Loc, const clang::Token &MacroNameTok, const clang::MacroDirective *MD)
	{

	}

}
