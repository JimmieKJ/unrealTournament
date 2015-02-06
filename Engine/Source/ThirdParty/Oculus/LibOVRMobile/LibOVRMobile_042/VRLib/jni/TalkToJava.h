/************************************************************************************

Filename    :   TalkToJava.h
Content     :   Thread and JNI management for making java calls in the background
Created     :   February 26, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/
#ifndef OVR_TalkToJava_h
#define OVR_TalkToJava_h

/*
 * Any call to java from the VR thread is a potential hitch.  Some, like
 * SurfaceTexture update calls are unavoidable, as they must be done on
 * the thread with a GL context, but most others should be run in a separate
 * thread, which is facilitated by this class.
 *
 * Hitches are less of an issue now with async TimeWarp, but very long pauses
 * would still cause animation freezes and black edge pull in, even though
 * TimeWarp is still head tracking. Making Java calls from a SCHED_FIFO
 * thread is probably also inadvisable.
 */

#include "MessageQueue.h"
#include <jni.h>
#include <pthread.h>

namespace OVR
{

class TalkToJavaInterface
{
public:
	// A subclass of this interface will be called on a dedicated
	// thread with normal scheduling priority that has a JNI set up.
	//
	// The command string will be freed by TalkToJava.
	//
	// A local reference frame is established around this call,
	// so it isn't necessary to free local references that are
	// generated, such as java strings.
	//
	// Java exceptions will be checked and cleared after each invocation.
	virtual void	TtjCommand( JNIEnv & jni, const char * commandString ) = 0;
};

class TalkToJava
{
public:
	TalkToJava() :
		Interface( NULL ),
		Jvm( NULL ),
		Jni( NULL ),
		TtjThread( 0 ),
		TtjMessageQueue( 1000 ) // no need to be stingy with queue size
		{};

	~TalkToJava();	// synchronously shuts down the thread

	// Spawns a separate thread that will issue java calls
	// that could take more than a couple milliseconds to
	// execute.
	void	Init( JavaVM & javaVM_, TalkToJavaInterface & interface );

	MessageQueue	& GetMessageQueue() { return TtjMessageQueue; };

private:
	static void * ThreadStarter( void * parm );
	void		TtjThreadFunction();
	void		Command( const char *msg );

	TalkToJavaInterface	* Interface;

	JavaVM *		Jvm;
	JNIEnv *		Jni;
	pthread_t		TtjThread;
	MessageQueue	TtjMessageQueue;
};

}

#endif	// OVR_TalkToJava_h
