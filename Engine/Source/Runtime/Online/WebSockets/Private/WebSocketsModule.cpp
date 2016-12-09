// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WebSocketsModule.h"
#include "WebSocketsLog.h"

#if WITH_WEBSOCKETS
#include "PlatformWebSocket.h"
#endif // #if WITH_WEBSOCKETS

DEFINE_LOG_CATEGORY(LogWebSockets);

// FWebSocketsModule
IMPLEMENT_MODULE(FWebSocketsModule, WebSockets);

FWebSocketsModule* FWebSocketsModule::Singleton = nullptr;

void FWebSocketsModule::StartupModule()
{
	Singleton = this;
}

void FWebSocketsModule::ShutdownModule()
{
	
	Singleton = nullptr;
}

FWebSocketsModule& FWebSocketsModule::Get()
{
	return *Singleton;
}

#if WITH_WEBSOCKETS
TSharedRef<IWebSocket> FWebSocketsModule::CreateWebSocket(const FString& Url, const TArray<FString>& Protocols)
{
	return MakeShareable(new FPlatformWebSocket(Url, Protocols));
}

TSharedRef<IWebSocket> FWebSocketsModule::CreateWebSocket(const FString& Url, const FString& Protocol)
{
	TArray<FString> Protocols;
	Protocols.Add(Protocol);
	return MakeShareable(new FPlatformWebSocket(Url, Protocols));
}
#endif // #if WITH_WEBSOCKETS
