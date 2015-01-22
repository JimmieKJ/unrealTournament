// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Platform File Module Interface
 */
class IPlatformFileModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a platform file instance.
	 *
	 * @return Platform file instance.
	 */
	virtual IPlatformFile* GetPlatformFile() = 0;
};
