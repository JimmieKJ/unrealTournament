// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"

class IWebSocket;

/**
 * Module for web socket implementations
 */
class WEBSOCKETS_API FWebSocketsModule :
	public IModuleInterface
{

public:

	// FWebSocketModule

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static FWebSocketsModule& Get();

#if WITH_WEBSOCKETS
	/**
	 * Instantiates a new web socket for the current platform
	 *
	 * @param Url The URL to which to connect; this should be the URL to which the WebSocket server will respond.
	 * @param Protocols a list of protocols the client will handle.
	 * @return new IWebSocket instance
	 */
	virtual TSharedRef<IWebSocket> CreateWebSocket(const FString& Url, const TArray<FString>& Protocols);


	/**
	 * Instantiates a new web socket for the current platform
	 *
	 * @param Url The URL to which to connect; this should be the URL to which the WebSocket server will respond.
	 * @param Protocol an optional sub-protocol. If missing, an empty string is assumed.
	 * @return new IWebSocket instance
	 */
	virtual TSharedRef<IWebSocket> CreateWebSocket(const FString& Url, const FString& Protocol = FString());
#endif // #if WITH_WEBSOCKETS

private:

	// IModuleInterface

	/**
	 * Called when WebSockets module is loaded
	 * Initialize implementation specific parts of WebSockets handling
	 */
	virtual void StartupModule() override;
	
	/**
	 * Called when WebSockets module is unloaded
	 * Shutdown implementation specific parts of WebSockets handling
	 */
	virtual void ShutdownModule() override;


	/** singleton for the module while loaded and available */
	static FWebSocketsModule* Singleton;
};
