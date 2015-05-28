// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveEditorDevice.h"
#include "LiveEditorWizardBase.h"

class FLiveEditorDeviceSetupWizard : public FLiveEditorWizardBase
{
public:
	FLiveEditorDeviceSetupWizard();
	virtual ~FLiveEditorDeviceSetupWizard();

	void Start( PmDeviceID _DeviceID, struct FLiveEditorDeviceData &Data );
	void Configure( bool bHasButtons, bool bHasEndlessEncoders );

	virtual FText GetAdvanceButtonText() const override;

protected:
	virtual void OnWizardFinished( struct FLiveEditorDeviceData &Data ) override;

private:
	struct FConfigurationState *ConfigState;
};
