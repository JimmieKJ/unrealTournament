// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

// Dependencies.
#include "Core.h"
#include "RHI.h"
#include "NullRHI.h"


/** Implements the NullDrv module as a dynamic RHI providing module. */
class FNullDynamicRHIModule
	: public IDynamicRHIModule
{
public:

	// IDynamicRHIModule

	virtual bool SupportsDynamicReloading() override { return false; }
	virtual bool IsSupported() override;

	virtual FDynamicRHI* CreateRHI() override
	{
		return new FNullDynamicRHI();
	}
};
