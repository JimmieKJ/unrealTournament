// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef __KismetCompilerBackend_h__
#define __KismetCompilerBackend_h__

#pragma once

#include "KismetCompiler.h"

//////////////////////////////////////////////////////////////////////////
// IKismetCompilerBackend

class IKismetCompilerBackend
{
};

//////////////////////////////////////////////////////////////////////////
// FKismetCppBackend

class IKismetCppBackend : public IKismetCompilerBackend
{
public:
	static IKismetCppBackend* Create(UEdGraphSchema_K2* InSchema, FKismetCompilerContext& InContext);

	virtual const FString& GetBody() const = 0;
	virtual const FString& GetHeader() const = 0;
	virtual void GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly = false) = 0;

	virtual ~IKismetCppBackend() {}
};

//////////////////////////////////////////////////////////////////////////
// FKismetVMBackend

class FKismetCompilerVMBackend : public IKismetCompilerBackend
{
public:
	typedef TMap<FBlueprintCompiledStatement*, CodeSkipSizeType> TStatementToSkipSizeMap;
protected:
	UBlueprint* Blueprint;
	UEdGraphSchema_K2* Schema;
	FCompilerResultsLog& MessageLog;
	FKismetCompilerContext& CompilerContext;

	TStatementToSkipSizeMap UbergraphStatementLabelMap;
public:
	FKismetCompilerVMBackend(UBlueprint* InBlueprint, UEdGraphSchema_K2* InSchema, FKismetCompilerContext& InContext)
		: Blueprint(InBlueprint)
		, Schema(InSchema)
		, MessageLog(InContext.MessageLog)
		, CompilerContext(InContext)
	{
	}

	void GenerateCodeFromClass(UClass* SourceClass, TIndirectArray<FKismetFunctionContext>& Functions, bool bGenerateStubsOnly=false);

protected:
	/** Builds both the header declaration and body implementation of a function */
	void ConstructFunction(FKismetFunctionContext& FunctionContext, bool bIsUbergraph, bool bGenerateStubOnly);
};

//////////////////////////////////////////////////////////////////////////

#endif	// __KismetCompilerBackend_h__
