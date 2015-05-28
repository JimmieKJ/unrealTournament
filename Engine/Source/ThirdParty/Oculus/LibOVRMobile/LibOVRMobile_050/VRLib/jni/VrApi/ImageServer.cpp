/************************************************************************************

Filename    :   ImageServer.cpp
Content     :   Listen on network ports for requests to capture and send screenshots
				for viewing testers.
Created     :   July 4, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "ImageServer.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <poll.h>

#include "Kernel/OVR_Std.h"
#include "Android/LogUtils.h"

namespace OVR {

static const int IMAGE_SERVER_PORT = 52602;		// both TCP and UDP

// Send this in a broadcast UDP packet to IMAGE_SERVER_PORT and listen for response.
static const char * IMAGE_SERVER_REQUEST = "Image Server?";

ImageServer::ImageServer() : ShutdownSocket( 0 ),
		CurrentResolution( 0 ), SequenceCaptured( 0 ),
		ResampleRenderBuffer( 0 ), FrameBufferObject( 0 ), PixelBufferObject( 0 ),
		PboMappedAddress( 0 ),
		serverThread( 0 ), CountdownToSend( 0 )
{
	LOG( "-------------------- Startup() --------------------" );

	pthread_mutex_init( &ResponseMutex, NULL /* default attributes */ );
	pthread_cond_init( &ResponseCondition, NULL /* default attributes */ );

	pthread_mutex_init( &StartStopMutex, NULL /* default attributes */ );
	pthread_cond_init( &StartStopCondition, NULL /* default attributes */ );

	// spawn the warp thread amd wait for acknowledgment
	pthread_mutex_lock( &StartStopMutex );
	const int createErr = pthread_create( &serverThread, NULL /* default attributes */, &ThreadStarter, this );
	if ( createErr != 0 )
	{
		FAIL( "pthread_create returned %i", createErr );
	}
	pthread_cond_wait( &StartStopCondition, &StartStopMutex );
	pthread_mutex_unlock( &StartStopMutex );

	LOG( "Thread start acknowledged." );
}

void ImageServer::FreeBuffers()
{
	if ( ResampleRenderBuffer )
	{
		glDeleteRenderbuffers( 1, &ResampleRenderBuffer );
		ResampleRenderBuffer = 0;
	}
	if ( FrameBufferObject )
	{
		glDeleteFramebuffers( 1, &FrameBufferObject );
		FrameBufferObject = 0;
	}
	if ( PixelBufferObject )
	{
		if ( PboMappedAddress )
		{
			glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBufferObject );
			glUnmapBuffer_( GL_PIXEL_PACK_BUFFER );
			glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
			PboMappedAddress = NULL;
		}
		glDeleteBuffers( 1, &PixelBufferObject );
		PixelBufferObject = 0;
	}
}

ImageServer::~ImageServer()
{
	LOG( "-------------------- Shutdown() --------------------" );

	// Make sure any outstanding glReadPixels to the PBO on the image server
	// has actually completed.
	glFinish();

	if ( serverThread )
	{
		pthread_mutex_lock( &StartStopMutex );

		// write to the socket to signal shutdown
		char	data;
		write( ShutdownSocket, &data, 1 );

		// also fire the condition, in case it was waiting
		// on TimeWarp to signal completion
		ImageServerResponse	badResponse;
		badResponse.Data = NULL;
		badResponse.Resolution = -1;
		badResponse.Sequence = -1;
		Response.SetState( badResponse );
		pthread_cond_signal( &ResponseCondition );

		LOG( "Waiting on StartStopCondition." );

		pthread_cond_wait( &StartStopCondition, &StartStopMutex );
		pthread_mutex_unlock( &StartStopMutex );

		LOG( "Thread stop acknowledged." );
	}

	// free GL tools
	if ( Quad.vertexArrayObject )
	{
		WarpGeometry_Destroy( &Quad );
	}
	if ( ResampleProg.program )
	{
		WarpProgram_Destroy( &ResampleProg );
	}
	FreeBuffers();
	LOG( "-------------------- Shutdown completed --------------------" );
}

// Shim to call a C++ object from a posix thread start.
void *ImageServer::ThreadStarter( void * parm )
{
	int result = pthread_setname_np( pthread_self(), "ImageServer" );
	if ( result != 0 )
	{
		WARN( "ImageServer: pthread_setname_np failed %s", strerror( result ) );
	}

	ImageServer & is = *(ImageServer *)parm;
	is.ServerThread();
	return NULL;
}

//#if OVR_BYTE_ORDER == OVR_BIG_ENDIAN
//uint16_t _htons( uint16_t hostshort ) { return hostshort; }
//uint16_t _ntohs( uint16_t netshort ) { return netshort; }
//#else
//uint16_t _htons( uint16_t hostshort ) { return (((hostshort>>8)&0xff)|((hostshort&0xff)<<8)); }
//uint16_t _ntohs( uint16_t netshort ) { return (((netshort>>8)&0xff)|((netshort&0xff)<<8)); }
//#endif

int BindToPort( const int sock, const int port )
{
	sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons( port );
	const int bindRet = bind( sock, (sockaddr *)&local, sizeof(local) );
	if ( bindRet == -1 )
	{
		LOG( "bind: %s", strerror(errno) );
		return -1;
	}
	if ( port == 0 )
	{
		sockaddr_in actual;
		int actualSize = sizeof( actual );
		const int gsnRet = getsockname( sock, (sockaddr *)&actual, &actualSize );
		if ( gsnRet == -1 )
		{
			FAIL( "getsockname: %s", strerror(errno) );
		}
		return ntohs( actual.sin_port );
	}
	else
	{
		return port;
	}
}

//static short	testData[256*256];

void ImageServer::ServerThread()
{
	// Listen for broadcast packets looking for image servers.
	int					UdpSocket = 0;

	// Listen for a new client attachment.
	int					TcpAcceptSocket = 0;

	// Listen for image resolution requests and send back raw data.
	int					TcpClientSocket = 0;

	// Send / receive a shutdown request
	int		shutdownSockets[2] = {};
	const int spr = socketpair( AF_LOCAL, SOCK_SEQPACKET, 0, shutdownSockets );
	if ( spr == -1 )
	{
		FAIL( "socketpair: %s", strerror( errno ) );
	}
	ShutdownSocket = shutdownSockets[1];
	LOG( "socketPair returned %i, %i", shutdownSockets[0], shutdownSockets[1] );

	// signal that we have started and created our shutdown socket
	pthread_cond_signal( &StartStopCondition );

	// block on the set of UDP port, TCP port, and command port
	struct pollfd pollfds[4];
	pollfds[0].fd = shutdownSockets[0];
	pollfds[0].events = POLLIN;

	// Open a TCP socket for accepting new clients.  Because the image server
	// gets stopped and started with each enter / leave VR, the port may
	// not be fully shutdown from the previous time, so we can't use a well-know
	// port number.
	TcpAcceptSocket = socket( AF_INET, SOCK_STREAM, 0 );
	int acceptSocketPort = -1;
	for ( ; ; )
	{
		LOG( "Trying to bind TcpAcceptSocket");
		acceptSocketPort = BindToPort( TcpAcceptSocket, 0 );
		if ( acceptSocketPort != -1 )
		{
			LOG( "Succeeded");
			break;
		}
		// still need to check for shutdown
		pollfds[0].revents = 0;
		poll( pollfds, 1, 1000);
		if ( pollfds[0].revents & POLLIN )
		{
			LOG( "ShutdownRequest received while trying to open TcpAcceptSocket" );
			goto cleanup;
		}
	}

	if ( -1 == listen( TcpAcceptSocket, 1 ) )
	{
		FAIL( "listen: %s", strerror( errno ) );
	}

	// UDP sockets should always close immediately, but if another
	// app is in the process of crashing, it may still hold the port
	// open, so try once a second to open it.
	UdpSocket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	for ( ; ; )
	{
		LOG( "Trying to bind UdpSocket");
		int r = BindToPort( UdpSocket, IMAGE_SERVER_PORT );
		if ( r != -1 )
		{
			LOG( "Succeeded");
			break;
		}
		// still need to check for shutdown
		pollfds[0].revents = 0;
		poll( pollfds, 1, 1000);
		if ( pollfds[0].revents & POLLIN )
		{
			LOG( "ShutdownRequest received while trying to open UdpSocket" );
			goto cleanup;
		}
	}

	while( 1 )
	{

		if ( TcpClientSocket > 0 )
		{
			// If we are already connected to a client, we are only listening on that
			// port plus our shutdown request
			pollfds[1].fd = TcpClientSocket;
			pollfds[1].events = POLLIN;
			pollfds[2].fd = -1;
			pollfds[3].fd = -1;
		}
		else
		{
			pollfds[1].fd = -1;
			pollfds[2].fd = TcpAcceptSocket;
			pollfds[2].events = POLLIN;
			pollfds[3].fd = UdpSocket;
			pollfds[3].events = POLLIN;
		}

		pollfds[0].revents = 0;
		pollfds[1].revents = 0;
		pollfds[2].revents = 0;
		pollfds[3].revents = 0;
		poll( pollfds, 4, 1000);

		if ( pollfds[0].revents & POLLIN )	// Shutdown request
		{
			LOG( "ShutdownRequest received" );
			break;
		}

		if (  pollfds[2].revents & POLLIN )	// TcpAcceptSocket
		{
			TcpClientSocket = accept(TcpAcceptSocket, NULL, NULL );

			LOG( "Accepted a TCP connection: %i", TcpClientSocket );
			continue;
		}

		if (  pollfds[3].revents & POLLIN )	// UdpSocket
		{
			sockaddr_in		from;
			int				fromAddrSize = sizeof( from );
			char			request[128];

			const int r = recvfrom( UdpSocket, request, sizeof( request )-1, 0, (sockaddr *)&from, &fromAddrSize );
			if ( r < 0 )
			{
				if ( errno != 11 )	// try again
				{
    				LOG( "recvfrom: %s", strerror(errno) );
				}
				continue;
			}
			request[r] = 0;

			if ( strcmp( request, IMAGE_SERVER_REQUEST ) )
			{
				LOG( "bad request on UDP port: %s", request );
				continue;
			}

			// reply with the port number of our TCP accept socket
			char			response[128];
			sprintf( response, "%i", acceptSocketPort );
			LOG( "replying to request \"%s\" with \"%s\"", request, response );
			const int sndRet = sendto( UdpSocket, (void *)response, strlen( response ),
					0, (sockaddr *)&from, sizeof(from) );
			if ( sndRet == -1 )
			{
				LOG( "sendto: %s", strerror(errno) );
			}
			continue;
		}

		if ( pollfds[1].revents & POLLIN )
		{
			char	data[128];
			const int r = read( TcpClientSocket, data, sizeof( data ) - 1 );
			if ( r <= 0 )
			{
				LOG( "Client read %i", r );
				close( TcpClientSocket );
				TcpClientSocket = 0;
				continue;
			}
			data[r] = 0;
			ImageServerRequest	isr;

			isr.Resolution = atoi( data );
			if ( isr.Resolution < 64 || isr.Resolution > 2048 )
			{
				LOG( "Rejecting resolution request: %s", data );
				close( TcpClientSocket );
				TcpClientSocket = 0;
				continue;
			}

			const ImageServerRequest prev = Request.GetState();
			isr.Sequence = prev.Sequence + 1;

			// TimeWarp will notice this on the next frame and start a capture.
			Request.SetState( isr );

			// Block until we hear from TimeWarp
			pthread_mutex_lock( &ResponseMutex );
			pthread_cond_wait( &ResponseCondition, &ResponseMutex );
			pthread_mutex_unlock( &ResponseMutex );
			// Send the image data to the client.
			const ImageServerResponse resp = Response.GetState();
			if ( resp.Sequence == -1 )
			{
				LOG( "Exiting due to resp.sequence == -1" );
				break;
			}
			if ( resp.Resolution != isr.Resolution )
			{
				// Not what they were expecting, so close the socket
				LOG( "Response resolution %i != %i", resp.Resolution, isr.Resolution );
				close( TcpClientSocket );
				TcpClientSocket = 0;
				continue;
			}

			const int bytes = resp.Resolution * resp.Resolution * 2;
			const int w = write( TcpClientSocket, resp.Data, bytes );
			if ( w != bytes )
			{
				LOG( "Only write %i of %i bytes", w, bytes );
				close( TcpClientSocket );
				TcpClientSocket = 0;
			}

		}
    }

cleanup:

	if ( UdpSocket > 0 )
	{
	    LOG( "Closing UdpSocket" );
		close( UdpSocket );
	}
	if ( TcpAcceptSocket > 0 )
	{
	    LOG( "Closing TcpAcceptSocket" );
		close( TcpAcceptSocket );
	}
	if ( TcpClientSocket > 0 )
	{
	    LOG( "Closing TcpClientSocket" );
		close( TcpClientSocket );
	}
	for ( int i = 0 ; i < 2 ; i++ )
	{
		if ( shutdownSockets[i] > 0 )
		{
		    LOG( "Closing shutdownSocket %i", i );
			close( shutdownSockets[i] );
		}
	}

	// signal that we are done
	LOG( "Signalling condition" );
	pthread_cond_signal( &StartStopCondition );
}

// Called by TimeWarp before adding the KHR sync object.
void ImageServer::EnterWarpSwap( int eyeTexture )
{
	ImageServerRequest	request = Request.GetState();
	if ( request.Sequence <= SequenceCaptured )
	{
		return;
	}
	SequenceCaptured = request.Sequence;

	// create GL objects if necessary
	if ( !ResampleProg.program )
	{
		WarpProgram_Create( &ResampleProg,
			"uniform highp mat4 Mvpm;\n"
			"attribute vec4 Position;\n"
			"attribute vec2 TexCoord;\n"
			"varying  highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = Position;\n"
			"   oTexCoord = vec2( TexCoord.x, 1.0 - TexCoord.y );\n"	// need to flip Y
			"}\n"
		,
			"uniform sampler2D Texture0;\n"
			"varying highp vec2 oTexCoord;\n"
			"void main()\n"
			"{\n"
			"	gl_FragColor = texture2D( Texture0, oTexCoord );\n"
			"}\n"
		);
	}

	if ( !Quad.vertexArrayObject )
	{
		WarpGeometry_CreateQuad( &Quad );
	}

	// If resolution has changed, delete and reallocate the buffers
	// These might still be in use, do we trust the driver or try to
	// deal with it ourselves?
	if ( request.Resolution != CurrentResolution )
	{
		CurrentResolution = request.Resolution;
		FreeBuffers();
	}

	// Allocate any resources we need
	if ( !ResampleRenderBuffer )
	{
		LOG( "Alloc %i res renderbuffer", CurrentResolution );
		glGenRenderbuffers( 1, &ResampleRenderBuffer );
		glBindRenderbuffer( GL_RENDERBUFFER, ResampleRenderBuffer );
		glRenderbufferStorage( GL_RENDERBUFFER, GL_RGB565, CurrentResolution, CurrentResolution );
	}

	if ( !FrameBufferObject )
	{
		LOG( "Alloc FrameBufferObject" );
		glGenFramebuffers( 1, &FrameBufferObject );
		glBindFramebuffer( GL_FRAMEBUFFER, FrameBufferObject );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_RENDERBUFFER, ResampleRenderBuffer );
		const GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
		if (status != GL_FRAMEBUFFER_COMPLETE )
		{
			LOG( "FBO is not complete: 0x%x", status );
		}
	}

	if ( !PixelBufferObject )
	{
		LOG( "Alloc PixelBufferObject" );
		glGenBuffers( 1, &PixelBufferObject );
		glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBufferObject );
		glBufferData( GL_PIXEL_PACK_BUFFER, CurrentResolution*CurrentResolution*2, NULL,
                GL_DYNAMIC_READ );
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	}

	// Render the FBO
	glBindFramebuffer( GL_FRAMEBUFFER, FrameBufferObject );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_SCISSOR_TEST );
	GL_InvalidateFramebuffer( INV_FBO, true, false );
	glViewport( 0, 0, CurrentResolution, CurrentResolution );
	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, eyeTexture );
	glUseProgram( ResampleProg.program );
	glBindVertexArrayOES_( Quad.vertexArrayObject );
	glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL );
	glBindVertexArrayOES_( 0 );
	glUseProgram( 0 );

	// unmap the previous PBO mapping
	if ( PboMappedAddress )
	{
		glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBufferObject );
		glUnmapBuffer_( GL_PIXEL_PACK_BUFFER );
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		PboMappedAddress = NULL;
	}

	// Issue an async read into our PBO
#if 1

	glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBufferObject );
	glReadPixels( 0, 0, CurrentResolution, CurrentResolution, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0 );
	// back to normal memory read operations
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );

#else
	GL_CheckErrors( "before read" );

	static short	pixels[256*256];
	glReadPixels( 0, 0, CurrentResolution, CurrentResolution, GL_RGB,
			GL_UNSIGNED_SHORT_5_6_5, (GLvoid *)pixels );
	ImageServerResponse	response;
	response.Data = (void *)pixels;
	response.Resolution = CurrentResolution;
	response.Sequence = request.Sequence;
	Response.SetState( response );
#endif
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	GL_CheckErrors( "after read" );

	CountdownToSend = 2;
}

// Called by TimeWarp after syncing to the previous frame's
// sync object.
void ImageServer::LeaveWarpSwap()
{
	// we are only guaranteed the readback has completed on the second following frame
	if ( CountdownToSend == 0 || --CountdownToSend != 0 )
	{
		return;
	}

	ImageServerRequest	request = Request.GetState();

	glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBufferObject );

	PboMappedAddress = glMapBufferRange_( GL_PIXEL_PACK_BUFFER, 0,
			CurrentResolution*CurrentResolution*2, GL_MAP_READ_BIT );
	if ( !PboMappedAddress )
	{
		FAIL( "Couldn't map PBO" );
	}

	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );

	ImageServerResponse	response;
	response.Data = (void *)PboMappedAddress;
	response.Resolution = CurrentResolution;
	response.Sequence = request.Sequence;
	Response.SetState( response );

	pthread_cond_signal( &ResponseCondition );
}

}	// namespace OVR
