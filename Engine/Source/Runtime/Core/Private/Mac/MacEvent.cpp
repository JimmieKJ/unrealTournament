// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "MacEvent.h"
#include "MacApplication.h"
#include "CocoaThread.h"
#include "CocoaWindow.h"
#import <objc/runtime.h>

static char PositionPropertyKey;

@implementation NSEvent (FCachedWindowAccess)

@dynamic windowPosition;

-(void)CacheWindow
{
	_window = [[self window] retain];
	self.windowPosition = _window.frame.origin;
}
-(NSWindow*)GetWindow
{
	return _window;
}
-(void)setWindowPosition:(NSPoint)position
{
	objc_setAssociatedObject(self, &PositionPropertyKey, [NSValue valueWithPoint:position], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
-(NSPoint)windowPosition
{
	return [(NSValue*)objc_getAssociatedObject(self, &PositionPropertyKey) pointValue];
}
-(void)ResetWindow
{
	[_window release];
	_window = nil;
}
@end

FMacEvent::FMacEvent(NSEvent* const Event)
: EventData(Event)
{
	SCOPED_AUTORELEASE_POOL;
	check(Event);
	[Event retain];
	[Event CacheWindow];
}

FMacEvent::FMacEvent(NSNotification* const Notification)
: EventData(Notification)
{
	SCOPED_AUTORELEASE_POOL;
	check(Notification);
	[Notification retain];
}

void FMacEvent::SendToGameRunLoop(FMacEvent const* const Event, EMacEventSendMethod SendMethod, NSArray* SendModes)
{
	SCOPED_AUTORELEASE_POOL;
	dispatch_block_t Block = ^{ FMacApplication::ProcessEvent(Event); };
	const bool bWait = (SendMethod == EMacEventSendMethod::Sync);
	
	GameThreadCall(Block, SendModes, bWait);
}
	
void FMacEvent::SendToGameRunLoop(NSEvent* const Event, EMacEventSendMethod SendMethod, NSArray* SendModes)
{
	if(MacApplication)
	{
		FMacEvent* MacEvent = new FMacEvent(Event);
		FMacEvent::SendToGameRunLoop(MacEvent, SendMethod, SendModes);
	}
}

void FMacEvent::SendToGameRunLoop(NSNotification* const Notification, EMacEventSendMethod SendMethod, NSArray* SendModes)
{
	if(MacApplication)
	{
		FMacEvent* MacEvent = new FMacEvent(Notification);
		FMacEvent::SendToGameRunLoop(MacEvent, SendMethod, SendModes);
	}
}

FMacEvent::~FMacEvent()
{
	if(EventData)
	{
		SCOPED_AUTORELEASE_POOL;
		[EventData release];
		EventData = nil;
	}
}

NSEvent* FMacEvent::GetEvent() const
{
	SCOPED_AUTORELEASE_POOL;
	if(EventData && [EventData isKindOfClass:[NSEvent class]])
	{
		return (NSEvent*)EventData;
	}
	else
	{
		return nil;
	}
}

NSNotification* FMacEvent::GetNotification() const
{
	SCOPED_AUTORELEASE_POOL;
	if(EventData && [EventData isKindOfClass:[NSNotification class]])
	{
		return (NSNotification*)EventData;
	}
	else
	{
		return nil;
	}
}
