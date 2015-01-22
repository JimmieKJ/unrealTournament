// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Actions/PawnAction.h"
#include "TestPawnAction_Log.generated.h"

namespace ETestPawnActionMessage
{
	enum Type
	{
		Started,
		Paused,
		Resumed,
		Finished,
		ChildFinished,
	};
}

UCLASS()
class UTestPawnAction_Log : public UPawnAction
{
	GENERATED_UCLASS_BODY()

	FTestLogger<int32>* Logger;

	static UTestPawnAction_Log* CreateAction(UWorld& World, FTestLogger<int32>& InLogger);

	virtual bool Start() override;
	virtual bool Pause(const UPawnAction* PausedBy) override;
	virtual bool Resume() override;
	virtual void OnFinished(EPawnActionResult::Type WithResult) override;
	virtual void OnChildFinished(UPawnAction& Action, EPawnActionResult::Type WithResult) override;

	FAIResourcesSet& GetRequiredResourcesSet() { return RequiredResources; }
	void SetRequiredResourcesSet(const FAIResourcesSet& InRequiredResources) { RequiredResources = InRequiredResources; }

	/** Testing-hack to call protected function outside of protected environment */
	void Terminate(EPawnActionResult::Type Result) { Finish(Result); }
};
