// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IPhysXFormatModule.h: Declares the IPhysXFormatModule interface.
=============================================================================*/

#pragma once


/**
 * Interface for PhysX format modules.
 */
class IPhysXFormatModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the PhysX format.
	 */
	virtual IPhysXFormat* GetPhysXFormat( ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	~IPhysXFormatModule( ) { }
};
