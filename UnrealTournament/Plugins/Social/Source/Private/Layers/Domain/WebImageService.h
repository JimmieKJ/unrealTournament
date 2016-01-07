// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Implement the Web Image Service
*/
class FWebImageService
	: public TSharedFromThis<FWebImageService>
{
public:

	/** Destructor. */
	virtual ~FWebImageService() {};

	/*
	 * Request an image to start downloading
	 * @param ImageUrl The Url where the image resides
	 * @param BrushName The name you will use to get the image back from GetBrush
	 */
	virtual void RequestImage(FString ImageUrl, FString BrushName) = 0;


	/*
	* Request a title icon to start downloading
	* @param ClientID The ID of the client we want the image for
	*/
	virtual void RequestTitleIcon(FString ClientID) = 0;

	virtual void SetDefaultTitleIcon(const FSlateBrush* Brush) = 0;

	/*
	* Return an image for a previously requested Url
	* @param BrushName The name of the brush you requested earlier
	* @return The Image for the url or a default if its not finished downloading yet
	*/
	virtual const FSlateBrush* GetBrush(FString BrushName) const = 0;
};

/**
* Creates the implementation of a chat manager.
*
* @return the newly created FriendViewModel implementation.
*/
FACTORY(TSharedRef< class FWebImageService >, FWebImageService);
