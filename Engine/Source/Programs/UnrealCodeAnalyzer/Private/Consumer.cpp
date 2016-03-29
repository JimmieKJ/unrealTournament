// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerPCH.h"
#include "Consumer.h"

using namespace clang;

namespace UnrealCodeAnalyzer
{
	void FConsumer::HandleTranslationUnit(ASTContext& Context)
	{
		Visitor.TraverseDecl(Context.getTranslationUnitDecl());
	}

	FConsumer::FConsumer(ASTContext* Context, StringRef InFile) :
		Visitor(Context, InFile)
	{ }

}
