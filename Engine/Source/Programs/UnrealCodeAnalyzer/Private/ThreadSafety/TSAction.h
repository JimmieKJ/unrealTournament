// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "clang/Frontend/FrontendAction.h"

namespace UnrealCodeAnalyzer
{
	class FTSConsumer;

	class FTSAction : public clang::ASTFrontendAction
	{
	public:
		virtual clang::ASTConsumer* CreateASTConsumer(clang::CompilerInstance& Compiler, clang::StringRef InFile) override;
		virtual bool BeginSourceFileAction(clang::CompilerInstance &CI, clang::StringRef Filename) override;
		virtual void EndSourceFileAction() override;

	private:
		FTSConsumer* Consumer;
		clang::StringRef Filename;
		clang::SourceManager* SourceManager;
	};
}
