// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "clang/Frontend/FrontendAction.h"

namespace UnrealCodeAnalyzer
{
	class FMacroCallback;
	class FIncludeDirectiveCallback;
	class FConsumer;
	class FAction : public clang::ASTFrontendAction
	{
	public:
		virtual clang::ASTConsumer* CreateASTConsumer(clang::CompilerInstance& Compiler, clang::StringRef InFile) override;
		virtual bool BeginSourceFileAction(clang::CompilerInstance &CI, clang::StringRef Filename) override;
		virtual void EndSourceFileAction() override;

		friend class FIncludeDirectiveCallback;
	private:
		FIncludeDirectiveCallback* IncludeDirectiveCallback;
		FMacroCallback* MacroCallback;
		FConsumer* ASTConsumer;
		clang::StringRef Filename;
		clang::SourceManager* SourceManager;
		TMap<FString, TDoubleLinkedList<FString>> IncludeChains;
	};
}
