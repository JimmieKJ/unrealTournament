/************************************************************************************

Filename    :   SystemActivities.cpp
Content     :   Event handling for system activities
Created     :   February 23, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "OVR.h"
#include "Kernel/OVR_Std.h"
#include "Kernel/OVR_JSON.h"
#include "Android/LogUtils.h"
#include "Android/JniUtils.h"
#include "VrApi/VrApi.h"
#include "VrApi/VrApi_Android.h"

#include "SystemActivities.h"

// System Activities Events -- Why They Exist
//
// When an System Activities activity returns to an application, it sends an intent to bring
// the application to the front. Unfortunately in the case of a Unity application the intent
// data cannot piggy-back on this intent because Unity doesn't overload its 
// UnityNativePlayerActivity::onNewIntent function to set the app's intent data to the new 
// intent data. This means the new intent data is lost after onNewIntent executes and is 
// never accessible to Unity afterwards. This can be fixed by overloading Unity's activity
// class, but that is problematic for Unity developers to an extent that makes in untenable
// for an SDK.
// As a result of this Unity issue, we instead send a broadcast intent to the application
// that started the system activity with an embedded JSON object indicating the action that 
// the initiating application should perform. When received, these actions are pushed onto 
// a single-producer / single-consumer queue so that they can be handled on the appropriate
// thread natively.
// In order to allow applications to later extend their functionality without getting a 
// full API update, applications can query pending events using ovr_GetNextPendingEvent.
// Unfortunately, the Unity integration again makes this more difficult because querying
// from Unity C# code means that the queries happen on the Unity main thread and not the
// Unity render thread. All of UnityPlugin.cpp is set up to run off of the render thread
// and the OvrMobile structure in UnityPlugin is specific to that thread. This made it 
// impossible to call ovr_SendIntent() (which calls ovr_LeaveVrMode) from the same thread
// where the events where being queried.
// To deal with this case, we have two event queues. The first queue always accepts events
// from the Java broadcast receiver. This queue is then read from the VrThread in native
// apps or the Unity main thread in Unity apps. When an event is read that has to be
// processed on the Unity render thread, the event is popped from the main event queue
// and pushed onto the internal event queue. The internal event queue is then read by
// Unity during ovr_HandleHmtEvents() on the Unity render thread. In native apps, both
// queues are handled in VrThread (i.e. there is not really a need for two queues) because
// this greatly simplified the code and educed the footprint for event handling in VrAPI.

namespace OVR {

//==============================================================
// EventData
//
class EventData {
public:
	EventData();
	EventData( void const * data, size_t const size );
	~EventData();

	void	AllocData( size_t const size );
	void	FreeData();

	EventData & operator = ( EventData const & other );

	void const *	GetData() const { return Data; }
	size_t			GetSize() const { return Size; }

private:
	void *	Data;
	size_t	Size;
};

EventData::EventData() :
	Data( NULL ), 
	Size( 0 ) 
{
}

//
EventData::EventData( void const * inData, size_t const inSize ) :
	Data( NULL ),
	Size( 0 )
{
	AllocData( inSize );
	memcpy( Data, inData, Size );	
}

//
EventData::~EventData()
{
	FreeData();
}

//
void EventData::AllocData( size_t const size )
{
	Data = malloc( size );
	Size = size;
}

//
void EventData::FreeData()
{
	free( Data );
	Data = NULL;
	Size = 0;
}

//
EventData & EventData::operator = ( EventData const & other ) 
{
	if ( &other != this ) {
		AllocData( other.GetSize() );
		memcpy( Data, other.GetData(), Size );
	}
	return *this;
}

//==============================================================
// EventQueue
// Simple single-producer / singler-consumer queue for events.
// One slot is always left open to distinquish the full vs. empty 
// cases.
class EventQueue {
public:
	static const int	QUEUE_SIZE = 32;

	EventQueue() :
		EventList(),
		HeadIndex( 0 ),
		TailIndex( 0 ) 
	{
	}
	~EventQueue() {
		for ( int i = 0; i < QUEUE_SIZE; ++i ) 
		{
			delete EventList[i];
		}
	}

	bool			IsEmpty() const { return HeadIndex == TailIndex; }
	bool			IsFull() const;

	bool			Enqueue( EventData const * inData );

	// Dequeue into a pre-allocated buffer
	bool			Dequeue( EventData const * & outData );

private:
	EventData const *	EventList[QUEUE_SIZE];
	volatile int		HeadIndex;
	volatile int		TailIndex;
};

//
bool EventQueue::IsFull() const 
{
	int indexBeforeTail = TailIndex == 0 ? QUEUE_SIZE - 1 : TailIndex - 1;
	if ( HeadIndex == indexBeforeTail ) 
	{
		return true;
	}
	return false;
}

//
bool EventQueue::Enqueue( EventData const * inData ) 
{
	if ( IsFull() ) 
	{
		return false;
	}
	int nextIndex = ( HeadIndex + 1 ) % QUEUE_SIZE;

	EventList[HeadIndex] = inData;

	// move the head after we've written the new item
	HeadIndex = nextIndex;

	return true;
}

//
bool EventQueue::Dequeue( EventData const * & outData ) 
{
	if ( IsEmpty() ) 
	{
		return false;
	}

	outData = EventList[TailIndex];
	EventList[TailIndex] = NULL;

	// move the tail once we've consumed the item
	TailIndex = ( TailIndex + 1 ) % QUEUE_SIZE;

	return true;
}

//==============================================================================================
// Default queue manangement - these functions operate on the main queue that is used by VrApi.
// There may be other queues external to this file, like the queue used to re-queue events for
// the Unity thread.
EventQueue * InternalEventQueue = NULL;
EventQueue * MainEventQueue = NULL;

void SystemActivities_InitEventQueues()
{
	InternalEventQueue = new EventQueue();
	MainEventQueue = new EventQueue();
}

void SystemActivities_ShutdownEventQueues()
{
	delete InternalEventQueue;
	InternalEventQueue = NULL;

	delete MainEventQueue;
	MainEventQueue = NULL;
}

void SystemActivities_AddInternalEvent( char const * data )
{
	EventData * eventData = new EventData( data, OVR_strlen( data ) + 1 );
	InternalEventQueue->Enqueue( eventData );
	LOG( "SystemActivities: queued internal event '%s'", data );
}

void SystemActivities_AddEvent( char const * data )
{
	EventData * eventData = new EventData( data, OVR_strlen( data ) + 1 );
	MainEventQueue->Enqueue( eventData );
	LOG( "SystemActivities: queued event '%s'", data );
}

eVrApiEventStatus SystemActivities_GetNextPendingEvent( EventQueue * queue, char * buffer, unsigned int const bufferSize )
{
	if ( buffer == NULL || bufferSize == 0 ) 
	{
		return VRAPI_EVENT_ERROR_INVALID_BUFFER;
	}
	
	if ( bufferSize < 2 ) 
	{
		buffer[0] = '\0';
		return VRAPI_EVENT_ERROR_INVALID_BUFFER;
	}

	if ( queue == NULL ) 
	{
		return VRAPI_EVENT_ERROR_INTERNAL;
	}

	EventQueue * q = reinterpret_cast< EventQueue* >( queue );
	EventData const * eventData;
	if ( !q->Dequeue( eventData ) ) 
	{
		return VRAPI_EVENT_NOT_PENDING;
	}

	OVR_strncpy( buffer, bufferSize, static_cast< char const * >( eventData->GetData() ), eventData->GetSize() );
	bool overflowed = eventData->GetSize() >= bufferSize;

	delete eventData;
	return overflowed ? VRAPI_EVENT_BUFFER_OVERFLOW : VRAPI_EVENT_PENDING;
}

eVrApiEventStatus SystemActivities_GetNextPendingInternalEvent( char * buffer, unsigned int const bufferSize )
{
	return SystemActivities_GetNextPendingEvent( InternalEventQueue, buffer, bufferSize );
}

eVrApiEventStatus SystemActivities_GetNextPendingMainEvent( char * buffer, unsigned int const bufferSize )
{
	return SystemActivities_GetNextPendingEvent( MainEventQueue, buffer, bufferSize );
}

}	// namespace OVR

