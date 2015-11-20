// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerPCH.h"
#include "Action.h"
#include "Consumer.h"
#include "MacroCallback.h"

namespace UnrealCodeAnalyzer
{
	void MergeIncludes(FPreHashedIncludeSetMap& OutResult, const FPreHashedIncludeSetMap& ToAdd)
	{
		for (auto& Kvp : ToAdd)
		{
			if (!OutResult.Contains(Kvp.Key))
			{
				OutResult.Add(Kvp.Key, Kvp.Value);
			}
			else
			{
				for (auto& Include : Kvp.Value)
				{
					OutResult[Kvp.Key].Add(Include);
				}
			}
		}
	}

	void FlattenIncludesInternal(
		uint32 FilenameHash,
		const FIncludeSet& IncludeStatements,
		FPreHashedIncludeSetMap& FlattenedIncludes,
		const FPreHashedIncludeSetMap& Includes)
	{
		for (auto& IncludeStatement : IncludeStatements)
		{
			// Don't add self to includes.
			bool bAlreadyInSet;
			FlattenedIncludes[FilenameHash].Add(IncludeStatement, &bAlreadyInSet);

			// If include was already parsed, there's no need to do it again.
			if (!bAlreadyInSet && Includes.Contains(IncludeStatement))
			{
				FlattenIncludesInternal(FilenameHash, Includes[IncludeStatement], FlattenedIncludes, Includes);
			}
		}
	}

	FPreHashedIncludeSetMap FlattenIncludes(const FPreHashedIncludeSetMap& Includes)
	{
		FPreHashedIncludeSetMap FlattenedIncludes;

		for (auto& Kvp : Includes)
		{
			auto FilenameHash = Kvp.Key;
			auto IncludeStatements = Kvp.Value;
			FlattenedIncludes.Add(FilenameHash, FIncludeSet());
			FlattenIncludesInternal(FilenameHash, IncludeStatements, FlattenedIncludes, Includes);
		}

		return FlattenedIncludes;
	}

	clang::ASTConsumer* FAction::CreateASTConsumer(clang::CompilerInstance& Compiler, clang::StringRef InFile)
	{
		Filename = InFile;
		SourceManager = &Compiler.getSourceManager();
		ASTConsumer = new FConsumer(&Compiler.getASTContext(), InFile);
		return ASTConsumer;
	}

	bool FAction::BeginSourceFileAction(clang::CompilerInstance &CI, clang::StringRef Filename)
	{
		MacroCallback = new FMacroCallback(Filename, CI.getSourceManager(), CI.getPreprocessor());
		CI.getPreprocessor().addPPCallbacks(MacroCallback);
		
		return true;
	}

	void FAction::EndSourceFileAction()
	{
		auto MainFileID = SourceManager->getMainFileID();
		auto MainFilename = FString(SourceManager->getFilename(SourceManager->getLocForStartOfFile(MainFileID)).data());
		MainFilename.ReplaceInline(TEXT("\\"), TEXT("/"));
		auto MainFilenameHash = FIncludeSetMapKeyFuncs::GetKeyHash(MainFilename);
		auto Includes = MacroCallback->GetIncludes();
		MergeIncludes(Includes, ASTConsumer->GetIncludes());

		auto FlattenedIncludes = FlattenIncludes(Includes);
		if (FlattenedIncludes.Contains(MainFilenameHash))
		{
			auto Test = FlattenedIncludes[MainFilenameHash];
			for (auto Filename : FlattenedIncludes[MainFilenameHash])
			{
				OutputFileContents.Logf(TEXT("%s"), *(GHashToFilename[Filename]));
			}
		}
	}

}
