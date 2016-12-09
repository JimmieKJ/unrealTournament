// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FEpicSurveyCommands : public TCommands<FEpicSurveyCommands>
{
public:
	FEpicSurveyCommands();

	virtual void RegisterCommands() override;

public:

	TSharedPtr< FUICommandInfo > OpenEpicSurvey;
};
