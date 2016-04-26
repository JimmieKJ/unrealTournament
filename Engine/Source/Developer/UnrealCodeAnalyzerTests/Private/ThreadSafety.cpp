// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealCodeAnalyzerTestsPCH.h"
#include "ThreadSafety.h"

int32 GGlobalValue;
#define MY_DEFINE(x) GGlobalValue = x;

void GGlobalFunc()
{}

class FClassWithStaticFunction
{
public:
	static void StaticFunction()
	{ }
};

class FStructWithStaticFunction
{
public:
	static void StaticFunction()
	{ }
};

void FunctionUsingInt(int32 Param)
{

}

void UObjectDerivedThreadSafetyTest::PostInitProperties()
{
	Super::PostInitProperties();
	
	MY_DEFINE(3);
	GGlobalValue = 5;
	GGlobalFunc();
	FClassWithStaticFunction::StaticFunction();
	FStructWithStaticFunction::StaticFunction();
	FunctionUsingInt(GGlobalValue);
}

void UObjectDerivedThreadSafetyTest::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	MY_DEFINE(3);
	GGlobalValue = 5;
	GGlobalFunc();
	FClassWithStaticFunction::StaticFunction();
	FStructWithStaticFunction::StaticFunction();
	FunctionUsingInt(GGlobalValue);
}

UObjectDerivedThreadSafetyTest::UObjectDerivedThreadSafetyTest()
{
	MY_DEFINE(4);
	GGlobalFunc();
	GGlobalValue = 5;
	FClassWithStaticFunction::StaticFunction();
	FStructWithStaticFunction::StaticFunction();
	FunctionUsingInt(GGlobalValue);
}
