// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerPCH.h"
#include "ThreadSafety/TSConsumer.h"

using namespace clang;

namespace UnrealCodeAnalyzer
{
	void FTSConsumer::HandleTranslationUnit(ASTContext& Context)
	{
		Visitor.TraverseDecl(Context.getTranslationUnitDecl());
	}

	FTSConsumer::FTSConsumer(ASTContext* Context, StringRef InFile) :
		Visitor(Context, InFile)
	{ }

}
