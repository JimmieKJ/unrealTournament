// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Modules/ModuleInterface.h"

class IUserFeedbackModule : public IModuleInterface
{
public:
	
	/** Create a widget which allows the user to send positive or negative feedback about a feature */
	virtual TSharedRef<SWidget> CreateFeedbackWidget(FText Context) const = 0;

};
