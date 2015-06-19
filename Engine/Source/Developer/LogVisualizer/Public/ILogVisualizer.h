// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class ILogVisualizer : public IModuleInterface, public IModularFeature
{
public:
	virtual void Goto(float Timestamp, FName LogOwner = NAME_None) = 0;
	virtual void GotoNextItem() = 0;
	virtual void GotoPreviousItem() = 0;
};