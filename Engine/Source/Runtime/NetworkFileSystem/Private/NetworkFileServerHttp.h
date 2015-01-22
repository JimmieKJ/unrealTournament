// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NetworkFileServerHttp.h: Declares the NetworkFileServerHttp class.

	This allows NFS to use a http server for serving Unreal Files. 

=============================================================================*/
#pragma  once

#if ENABLE_HTTP_FOR_NFS

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#include "libwebsockets.h"

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif


class FNetworkFileServerHttp
	:	public INetworkFileServer // It is a NetworkFileServer
	,	private FRunnable // Also spins up a thread but others don't need to know. 
{

public: 
	FNetworkFileServerHttp(int32 InPort, const FFileRequestDelegate* InFileRequestDelegate, 
		const FRecompileShadersDelegate* InRecompileShadersDelegate, const TArray<ITargetPlatform*>& InActiveTargetPlatforms );

	// INetworkFileServer Interface.

	virtual bool IsItReadyToAcceptConnections(void) const; 
	virtual FString GetSupportedProtocol() const override;
	virtual bool GetAddressList(TArray<TSharedPtr<FInternetAddr> >& OutAddresses) const override;
	virtual int32 NumConnections() const;
	virtual void Shutdown();

	virtual ~FNetworkFileServerHttp(); 


	// static functions. callbacks for libwebsocket. 
	static int CallBack_HTTP(	struct libwebsocket_context *context,
	struct libwebsocket *wsi,
		enum libwebsocket_callback_reasons reason, void *user,
		void *in, size_t len); 

private:  

	//FRunnable Interface. 
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	virtual void Exit();

	static void Process (FArchive&, TArray<uint8>&, FNetworkFileServerHttp* );


	// factory method for creating a new Client Connection. 
	class FNetworkFileServerClientConnectionHTTP* CreateNewConnection(); 

	// Holds a delegate to be invoked on every sync request.
	FFileRequestDelegate FileRequestDelegate;

	// Holds a delegate to be invoked when a client requests a shader recompile.
	FRecompileShadersDelegate RecompileShadersDelegate;

	// cached copy of the active target platforms (if any)
	const TArray<ITargetPlatform*> ActiveTargetPlatforms;

	// libwebsocket context. All access to the library happens via this context. 
	struct libwebsocket_context *Context;

	// Service Http connections on this thread.
	FRunnableThread* WorkerThread;

	// port on which this http server runs. 
	int32 Port; 

	// used to send simple message.
	FThreadSafeCounter StopRequested; 

	// Has successfully Initialized;
	FThreadSafeCounter Ready; 

	// Clients being served.
	TMap< FGuid, FNetworkFileServerClientConnectionHTTP* > RequestHandlers; 
};

#endif