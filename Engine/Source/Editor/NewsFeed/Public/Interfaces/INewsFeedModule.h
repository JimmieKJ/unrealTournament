// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for the news feed module.
 */
class INewsFeedModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a news feed button widget.
	 *
	 * @return The new widget.
	 */
	virtual TSharedRef<class SWidget> CreateNewsFeedButton( ) = 0;

public:

	/**
	 * Virtual destructor.
	 */
	virtual ~INewsFeedModule( ) { }
};
