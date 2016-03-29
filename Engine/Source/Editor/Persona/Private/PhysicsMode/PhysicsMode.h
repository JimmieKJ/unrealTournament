// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/////////////////////////////////////////////////////
// FPhysicsEditAppMode

class FPhysicsEditAppMode : public FPersonaAppMode
{
public:
	FPhysicsEditAppMode(TSharedPtr<FPersona> InPersona)
		: FPersonaAppMode(InPersona, FPersonaModes::PhysicsEditMode)
	{
		//TabLayout = ?
	}
};