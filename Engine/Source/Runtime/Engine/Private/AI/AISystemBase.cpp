// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "AI/AISystemBase.h"

UAISystemBase::UAISystemBase(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

FName UAISystemBase::GetAISystemModuleName()
{
	UAISystemBase* AISystemDefaultObject = Cast<UAISystemBase>(StaticClass()->GetDefaultObject());
	return AISystemDefaultObject != NULL ? AISystemDefaultObject->AISystemModuleName : TEXT("");
}

FStringClassReference UAISystemBase::GetAISystemClassName()
{
	UAISystemBase* AISystemDefaultObject = Cast<UAISystemBase>(StaticClass()->GetDefaultObject());
	return AISystemDefaultObject != NULL ? AISystemDefaultObject->AISystemClassName : FStringClassReference();
}

void UAISystemBase::StartPlay()
{

}