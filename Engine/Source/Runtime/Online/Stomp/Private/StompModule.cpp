// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StompModule.h"
#include "Modules/ModuleManager.h"
#include "IStompClient.h"
#include "StompLog.h"

// FStompModule
IMPLEMENT_MODULE(FStompModule, Stomp);

#if WITH_STOMP

#include "StompModule.h"
#include "StompClient.h"
#include "WebSocketsModule.h"

#endif // #if WITH_STOMP

DEFINE_LOG_CATEGORY(LogStomp);

static const FTimespan DefaultPingInterval = FTimespan::FromSeconds(30);

FStompModule* FStompModule::Singleton = nullptr;

void FStompModule::StartupModule()
{
#if WITH_STOMP
	FModuleManager::LoadModuleChecked<FWebSocketsModule>("WebSockets");
	Singleton = this;
#endif // #if WITH_STOMP
}

void FStompModule::ShutdownModule()
{
	Singleton = nullptr;
}

FStompModule& FStompModule::Get()
{
	return *Singleton;
}

#if WITH_STOMP

TSharedRef<IStompClient> FStompModule::CreateClient(const FString& Url)
{
	return MakeShareable(new FStompClient(Url, DefaultPingInterval, DefaultPingInterval));
}

#endif // #if WITH_STOMP
