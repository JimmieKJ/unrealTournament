// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveEditorDevice.h"
#include "LiveEditorWizardBase.h"

class FLiveEditorBlueprintBindingWizard : public FLiveEditorWizardBase
{
public:
	FLiveEditorBlueprintBindingWizard();
	virtual ~FLiveEditorBlueprintBindingWizard();

	void Start( class UBlueprint &_Blueprint );

protected:
	virtual void OnWizardFinished( struct FLiveEditorDeviceData &Data );

private:
	bool GenerateStates();

private:
	class UBlueprint *Blueprint;
};
