// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

class FIOSProfileWizard : public ILauncherProfileWizard
{
public:
	virtual FText GetName() const override;
	virtual FText GetDescription() const override;
	virtual void HandleCreateLauncherProfile(const ILauncherProfileManagerRef& ProfileManager) override;
};

