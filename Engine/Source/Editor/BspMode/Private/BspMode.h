// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

/**
 * Mode Toolkit for the Bsp
 */
class FBspMode : public FEdMode
{
public:

	virtual bool UsesToolkits() const override { return false; }

	virtual bool UsesPropertyWidgets() const override { return true; }
};