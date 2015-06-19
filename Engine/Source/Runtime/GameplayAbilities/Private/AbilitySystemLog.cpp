// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "AbilitySystemPrivatePCH.h"

DEFINE_LOG_CATEGORY(LogAbilitySystem);
DEFINE_LOG_CATEGORY(VLogAbilitySystem);

AbilitySystemLogScope::~AbilitySystemLogScope()
{
	AbilitySystemLog::PopScope();
}

void AbilitySystemLogScope::Init()
{
	PrintedInThisScope = false;
	AbilitySystemLog::PushScope(this);
}

// ----------------------------------------------

AbilitySystemLog * GetInstance()
{
	static AbilitySystemLog Instance;
	return &Instance;
}

FString AbilitySystemLog::Log(ELogVerbosity::Type Verbosity, FString Log)
{
#if !NO_LOGGING
	if (!LogAbilitySystem.IsSuppressed(Verbosity))
	{
		AbilitySystemLog * Instance = GetInstance();

		for (int32 idx=0; idx < Instance->ScopeStack.Num(); ++idx)
		{
			AbilitySystemLogScope *Scope = Instance->ScopeStack[idx];
			if (!Scope->PrintedInThisScope && !Scope->ScopeName.IsEmpty())
			{
				if (Instance->NeedNewLine)
				{
					UE_LOG(LogAbilitySystem, Log, TEXT(""));
					Instance->NeedNewLine = false;
				}

				Scope->PrintedInThisScope = true;
				int32 ident = (2 * idx);
				FString IndentStrX = FString::Printf(TEXT("%*s"), ident, TEXT(""));
				UE_LOG(LogAbilitySystem, Log, TEXT("%s<%s>"), *IndentStrX, *Scope->ScopeName);
			}
		}

		FString IndentStr = FString::Printf(TEXT("%*s"), Instance->Indent, TEXT(""));
		return IndentStr + Log;
	}
#endif
	
	return Log;
}

void AbilitySystemLog::PushScope(AbilitySystemLogScope * Scope)
{
	AbilitySystemLog * Instance = GetInstance();

	Instance->Indent += 2;
	Instance->ScopeStack.Push(Scope);
}

void AbilitySystemLog::PopScope()
{
	AbilitySystemLog * Instance = GetInstance();

	Instance->NeedNewLine = true;
	Instance->Indent -= 2;
	check(Instance->Indent >= 0);

	if (Instance->ScopeStack.Top()->PrintedInThisScope)
	{
		if (!Instance->ScopeStack.Top()->ScopeName.IsEmpty())
		{
			FString IndentStrX = FString::Printf(TEXT("%*s"), Instance->Indent, TEXT(""));
			UE_LOG(LogAbilitySystem, Log, TEXT("%s</%s>"), *IndentStrX, *Instance->ScopeStack.Top()->ScopeName);
		}
	}
	
	Instance->ScopeStack.Pop();
}