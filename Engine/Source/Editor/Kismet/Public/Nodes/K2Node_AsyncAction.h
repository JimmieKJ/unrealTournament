// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "K2Node_AsyncAction.generated.h"

/** !!! The proxy object should have RF_StrongRefOnFrame flag. !!! */

UCLASS()
class KISMET_API UK2Node_AsyncAction : public UK2Node_BaseAsyncTask
{
	GENERATED_UCLASS_BODY()
	
	// UK2Node interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	// End of UK2Node interface
};
