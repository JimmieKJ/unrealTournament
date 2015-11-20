// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAnalyticsMulticastEditorModule :
	public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

