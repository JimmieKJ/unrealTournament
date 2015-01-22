// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScreenShotManager.cpp: Implements the FScreenShotManager class.
=============================================================================*/

#pragma once


/**
 * Implements the ScreenShotManager that contains screen shot data.
 */
class FScreenShotManager
	: public IScreenShotManager 
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InMessageBus - The message bus to use.
	 */
	FScreenShotManager( const IMessageBusRef& InMessageBus );

public:

	/**
	* Create some dummy data to test the UI
	*/
	void CreateData( );

public:

	// Begin IScreenShotManager interface

	virtual void GenerateLists() override;

	virtual TArray< TSharedPtr<FString> >& GetCachedPlatfomList(  ) override;

	virtual TArray<IScreenShotDataPtr>& GetLists() override;

	virtual void RegisterScreenShotUpdate(const FOnScreenFilterChanged& InDelegate )override;

	virtual void SetFilter( TSharedPtr< ScreenShotFilterCollection > InFilter ) override;

	virtual void SetDisplayEveryNthScreenshot(int32 NewNth) override;

	// End IScreenShotManager interface

private:

	// Handles FAutomationWorkerScreenImage messages.
	void HandleScreenShotMessage( const FAutomationWorkerScreenImage& Message, const IMessageContextRef& Context );

private:

	// Holds the list of active platforms
	TArray<TSharedPtr<FString> > CachedPlatformList;

	// Holds the messaging endpoint.
	FMessageEndpointPtr MessageEndpoint;

	// Holds the array of created screen shot data items
	TArray< FScreenShotDataItem > ScreenShotDataArray;

	// Holds the root of the screen shot tree
	TSharedPtr< IScreenShotData > ScreenShotRoot;

	// Holds a delegate to be invoked when the screen shot filter has changed.
	FOnScreenFilterChanged ScreenFilterChangedDelegate;
};
