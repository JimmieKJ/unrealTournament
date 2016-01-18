// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "clang/AST/ASTConsumer.h"
#include "Visitor.h"

namespace UnrealCodeAnalyzer
{
	class FConsumer : public clang::ASTConsumer
	{
	public:
		explicit FConsumer(clang::ASTContext* Context, clang::StringRef InFile);

		virtual void HandleTranslationUnit(clang::ASTContext &Ctx) override;

		FPreHashedIncludeSetMap& GetIncludes()
		{
			return Visitor.GetIncludes();
		}

	private:
		FVisitor Visitor;
	};
}
