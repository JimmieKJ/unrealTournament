// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "clang/AST/ASTConsumer.h"
#include "TSVisitor.h"

namespace UnrealCodeAnalyzer
{
	class FTSConsumer : public clang::ASTConsumer
	{
	public:
		explicit FTSConsumer(clang::ASTContext* Context, clang::StringRef InFile);

		virtual void HandleTranslationUnit(clang::ASTContext &Ctx) override;

	private:
		FTSVisitor Visitor;
	};
}
