// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "Actions/PawnAction_BlueprintBase.h"

namespace
{
	FORCEINLINE bool HasBlueprintFunction(FName FuncName, const UObject* Ob, const UClass* StopAtClass)
	{
		return (Ob->GetClass()->FindFunctionByName(FuncName)->GetOuter() != StopAtClass);
	}
}

UPawnAction_BlueprintBase::UPawnAction_BlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UClass* StopAtClass = UPawnAction_BlueprintBase::StaticClass();
	bWantsTick = HasBlueprintFunction(TEXT("ActionTick"), this, StopAtClass);
}

void UPawnAction_BlueprintBase::Tick(float DeltaTime)
{
	// no need to call super implementation
	ActionTick(GetPawn(), DeltaTime);
}

bool UPawnAction_BlueprintBase::Start()
{
	const bool bHasBeenEverStarted = HasBeenStarted();
	const bool bSuperResult = Super::Start();

	if (bHasBeenEverStarted == false && bSuperResult == true)
	{
		ActionStart(GetPawn());
	}

	return bSuperResult;
}

bool UPawnAction_BlueprintBase::Pause(const UPawnAction* PausedBy)
{
	const bool bResult = Super::Pause(PausedBy);
	if (bResult)
	{
		ActionPause(GetPawn());
	}
	return bResult;
}

bool UPawnAction_BlueprintBase::Resume()
{
	const bool bResult = Super::Resume();
	if (bResult)
	{
		ActionResume(GetPawn());
	}

	return bResult;
}

void UPawnAction_BlueprintBase::OnFinished(EPawnActionResult::Type WithResult)
{
	ActionFinished(GetPawn(), WithResult);
}
