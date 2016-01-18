// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

#include "llvm/Support/TargetSelect.h"

#include "Core.h"
#include "Templates/UniquePtr.h"
#include "Misc/OutputDevice.h"

struct FIncludeSetKeyFuncs : BaseKeyFuncs < FString, FString, false >
{
	static FORCEINLINE const FString& GetSetKey(const FString& Element)
	{
		return Element;
	}
	static FORCEINLINE bool Matches(const FString& A, const FString& B)
	{
		return (FCString::Strcmp(*A, *B) == 0);
	}
	static FORCEINLINE uint32 GetKeyHash(const FString& Key)
	{
		return FCrc::StrCrc32<TCHAR>(*Key);
	}
};

//typedef TSet<FString, FIncludeSetKeyFuncs, FDefaultSetAllocator> FIncludeSet;
typedef TSet<uint32> FIncludeSet;

struct FIncludeSetMapKeyFuncs : BaseKeyFuncs < TPair<FString, FIncludeSet>, FString, false >
{
	static FORCEINLINE const FString& GetSetKey(const TPair<FString, FIncludeSet>& Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(const FString& A, const FString& B)
	{
		return (FCString::Strcmp(*A, *B) == 0);
	}
	static FORCEINLINE uint32 GetKeyHash(const FString& Key)
	{
		return FCrc::StrCrc32<TCHAR>(*Key);
	}
};

typedef TMap<FString, FIncludeSet, FDefaultSetAllocator, FIncludeSetMapKeyFuncs> FIncludeSetMap;

struct FPreHashedMapKeyFuncs : BaseKeyFuncs < TPair<uint32, FIncludeSet>, uint32, false >
{
	static FORCEINLINE const uint32& GetSetKey(const TPair<uint32, FIncludeSet>& Element)
	{
		return Element.Key;
	}
	static FORCEINLINE bool Matches(const uint32& A, const uint32& B)
	{
		return A == B;
	}
	static FORCEINLINE uint32 GetKeyHash(const uint32& Key)
	{
		return Key;
	}
};

typedef TMap<uint32, FIncludeSet, FDefaultSetAllocator, FPreHashedMapKeyFuncs> FPreHashedIncludeSetMap;


extern TMap<uint32, FString> GHashToFilename;
extern FString OutputFileName;
extern FStringOutputDevice OutputFileContents;

DECLARE_LOG_CATEGORY_EXTERN(LogUnrealCodeAnalyzer, Log, All);
