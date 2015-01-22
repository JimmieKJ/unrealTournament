// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for image wrapper modules.
 */
class IImageWrapperModule
	: public IModuleInterface
{
public:

	/**  
	 * Create a helper of a specific type 
	 *
	 * @param InFormat - The type of image we want to deal with
	 * @return The helper base class to manage the data
	 */
	virtual IImageWrapperPtr CreateImageWrapper( const EImageFormat::Type InFormat ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~IImageWrapperModule( ) { }
};
