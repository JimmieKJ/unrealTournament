// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include <SDL.h>

#if PLATFORM_HTML5_BROWSER
#include <html5.h>
#endif 


/**
 * Interface class for HTML5 input devices                 
 */
class FHTML5InputInterface
{
public:
	/** Initialize the interface */
	static TSharedRef< FHTML5InputInterface > Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );

public:

	~FHTML5InputInterface() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler);

	/** Tick the interface (i.e check for new controllers) */
	void Tick( float DeltaTime, const SDL_Event& Event,TSharedRef < FGenericWindow>& ApplicationWindow );
	/**
	 * Poll for controller state and send events if needed
	 */
	void SendControllerEvents();


private:

	FHTML5InputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler, const TSharedPtr< ICursor >& InCursor );


private:

	TSharedRef< FGenericApplicationMessageHandler > MessageHandler;
	const TSharedPtr< ICursor > Cursor;

	TBitArray<FDefaultBitArrayAllocator> KeyStates;

#if PLATFORM_HTML5_BROWSER

	EmscriptenGamepadEvent PrevGamePadState[5]; 
	double LastPressedTime[5][15];

#endif 

};