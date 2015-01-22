// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Actions/TestPawnAction_Log.h"
#include "TestPawnAction_CallFunction.generated.h"


UCLASS()
class UTestPawnAction_CallFunction : public UTestPawnAction_Log
{
	GENERATED_UCLASS_BODY()

	typedef void(*FFunctionToCall)(UPawnActionsComponent& ActionsComponent, UTestPawnAction_CallFunction& Caller, ETestPawnActionMessage::Type);
	FFunctionToCall FunctionToCall;
	
	static UTestPawnAction_CallFunction* CreateAction(UWorld& World, FTestLogger<int32>& InLogger, FFunctionToCall InFunctionToCall);

	virtual bool Start() override;
	virtual bool Pause(const UPawnAction* PausedBy) override;
	virtual bool Resume() override;
	virtual void OnFinished(EPawnActionResult::Type WithResult) override;
	virtual void OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult) override;
};