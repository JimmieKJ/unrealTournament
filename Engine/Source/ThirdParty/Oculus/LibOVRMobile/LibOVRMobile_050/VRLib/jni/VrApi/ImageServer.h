
/************************************************************************************

Filename    :   ImageServer.h
Content     :   Listen on network ports for requests to capture and send screenshots
				for viewing testers.
Created     :   July 4, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_ImageServer_h
#define OVR_ImageServer_h

#include "Kernel/OVR_Lockless.h"
#include "Android/GlUtils.h"

#include "WarpGeometry.h"
#include "WarpProgram.h"

namespace OVR
{

class ImageServerRequest
{
public:
	ImageServerRequest() : Sequence( 0 ), Resolution( 0 ) {}

	int				Sequence;		// incremented each time
	int				Resolution;
};

class ImageServerResponse
{
public:
	ImageServerResponse() : Sequence( 0 ), Resolution( 0 ), Data( NULL ) {}

	int				Sequence;		// incremented each time
	int				Resolution;
	const void *	Data;
};


class ImageServer
{
public:
	ImageServer();
	~ImageServer();

	// Called by TimeWarp before adding the KHR sync object.
	void				EnterWarpSwap( int eyeTexture );

	// Called by TimeWarp after syncing to the previous frame's
	// sync object.
	void				LeaveWarpSwap();

private:

	static void *		ThreadStarter( void * parm );
	void 				ServerThread();
	void				FreeBuffers();

	// When an image request arrives on the network socket,
	// it will be placed in request so TimeWarp can notice it.
	LocklessUpdater<ImageServerRequest>		Request;

	// After TimeWarp has completed a resampling and asynchronous
	// readback of an eye buffer, the results will be placed here.
	// The data only remains valid until the next request is set.
	LocklessUpdater<ImageServerResponse>	Response;

	// Write anything on this to shutdown the server thread
	int					ShutdownSocket;

	// The eye texture is drawn to the ResampleRenderBuffer through
	// the FrameBufferObject, then copied to the PixelBufferObject
	int					CurrentResolution;
	int					SequenceCaptured;
	WarpGeometry		Quad;
	WarpProgram			ResampleProg;
	GLuint				ResampleRenderBuffer;
	GLuint				FrameBufferObject;
	GLuint				PixelBufferObject;
	void *				PboMappedAddress;

    pthread_t			serverThread;		// posix pthread

	pthread_mutex_t		ResponseMutex;
	pthread_cond_t		ResponseCondition;

	pthread_mutex_t		StartStopMutex;
	pthread_cond_t		StartStopCondition;

	int					CountdownToSend;
};

}

#endif	// OVR_ImageServer_h

