// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Intended categories:
//	Log - This happened. What gameplay programmers may care about to debug
//	Verbose - This is why this happened. What you may turn on to debug the skill system code.
//  VeryVerbose - This is what didn't happen, and why. Extreme printf debugging
//


GAMEPLAYABILITIES_API DECLARE_LOG_CATEGORY_EXTERN(LogAbilitySystem, Warning, All);

#define ABILITY_LOG(Verbosity, Format, ...) \
{ \
	FString Str = AbilitySystemLog::Log(ELogVerbosity::Verbosity, FString::Printf(Format, ##__VA_ARGS__)); \
	UE_LOG(LogAbilitySystem, Verbosity, TEXT("%s"), *Str); \
}

#define ABILITY_LOG_TOKENPASTE_INNER(x,y) x##y
#define ABILITY_LOG_TOKENPASTE(x,y) ABILITY_LOG_TOKENPASTE_INNER(x,y)
#define ABILITY_LOG_SCOPE( Format, ... ) AbilitySystemLogScope ABILITY_LOG_TOKENPASTE(LogScope,__LINE__)( FString::Printf(Format, ##__VA_ARGS__));

struct AbilitySystemLogScope
{
	AbilitySystemLogScope()
	{
		Init();
	}

	AbilitySystemLogScope(FString InScopeName) :
		ScopeName(InScopeName)
	{
		Init();
	}

	~AbilitySystemLogScope();
	
	FString ScopeName;
	bool PrintedInThisScope;

private:

	void Init();
};

class AbilitySystemLog
{
public:

	AbilitySystemLog()
	{
		Indent = 0;
		NeedNewLine = false;
	}

	static FString Log(ELogVerbosity::Type, FString Str);

	static void PushScope(AbilitySystemLogScope* Scope);

	static void PopScope();	

	int32	Indent;
	TArray<AbilitySystemLogScope*>	ScopeStack;
	bool NeedNewLine;
};