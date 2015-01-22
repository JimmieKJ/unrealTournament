// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for texture format modules.
 */
class ITextureFormatModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the texture format.
	 *
	 * @return The texture format interface.
	 */
	virtual ITextureFormat* GetTextureFormat() = 0;

public:

	/** Virtual destructor. */
	~ITextureFormatModule() { }
};
