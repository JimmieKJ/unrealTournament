// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Core.h"
#include "MacWindow.h"

enum EMacEventSendMethod
{
	Async,
	Sync
};

@interface NSEvent (FCachedWindowAccess)
@property (readwrite, nonatomic) NSPoint windowPosition;

-(void)CacheWindow;
-(NSWindow*)GetWindow;
-(void)ResetWindow;
@end

class FMacEvent
{
	// Constructor for an NSEvent based FMacEvent
	FMacEvent(NSEvent* const Event);

	// Constructor for an NSNotification based FMacEvent
	FMacEvent(NSNotification* const Notification);

	// Send the event to the game run loop for processing using the specified SendMethod.
	static void SendToGameRunLoop(FMacEvent const* const Event, EMacEventSendMethod SendMethod, NSArray* SendModes = @[ NSDefaultRunLoopMode ]);

public:
	// Send an NSEvent to the Game run loop as an FMacEvent using the specified SendMethod
	static void SendToGameRunLoop(NSEvent* const Event, EMacEventSendMethod SendMethod, NSArray* SendModes = @[ NSDefaultRunLoopMode ]);

	// Send an NSNotification to the Game run loop as an FMacEvent using the specified SendMethod
	static void SendToGameRunLoop(NSNotification* const Notification, EMacEventSendMethod SendMethod, NSArray* SendModes = @[ NSDefaultRunLoopMode ]);

	// Destructor
	~FMacEvent();

	// Get the NSEvent for this FMacEvent, nil if not an NSEvent.
	NSEvent* GetEvent() const;

	// Get the NSNotification for this FMacEvent, nil if not an NSNotification.
	NSNotification* GetNotification() const;

private:
	NSObject* EventData; // The retained NSEvent or NSNotification for this event or nil.
};
