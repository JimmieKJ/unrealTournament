// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "PerfCounters.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

#define JSON_ARRAY_NAME					TEXT("PerfCounters")
#define JSON_PERFCOUNTER_NAME			TEXT("Name")
#define JSON_PERFCOUNTER_SIZE_IN_BYTES	TEXT("SizeInBytes")

FPerfCounters::FPerfCounters(const FString& InUniqueInstanceId)
: UniqueInstanceId(InUniqueInstanceId)
, Socket(nullptr)
{
}

FPerfCounters::~FPerfCounters()
{
	if (Socket)
	{
		ISocketSubsystem* SocketSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSystem)
		{
			SocketSystem->DestroySocket(Socket);
		}
		Socket = nullptr;
	}
}

bool FPerfCounters::Initialize()
{
	// get the requested port from the command line (if specified)
	int32 StatsPort = -1;
	FParse::Value(FCommandLine::Get(), TEXT("statsPort="), StatsPort);
	if (StatsPort < 0)
	{
		UE_LOG(LogPerfCounters, Log, TEXT("FPerfCounters JSON socket disabled."));
		return true;
	}

	// get the socket subsystem
	ISocketSubsystem* SocketSystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSystem == nullptr)
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to get socket subsystem"));
		return false;
	}

	// make our listen socket
	Socket = SocketSystem->CreateSocket(NAME_Stream, TEXT("FPerfCounters"));
	if (Socket == nullptr)
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to allocate stream socket"));
		return false;
	}

	// make us non blocking
	Socket->SetNonBlocking(true);

	// create a localhost binding for the requested port
	TSharedRef<FInternetAddr> LocalhostAddr = SocketSystem->CreateInternetAddr(0x7f000001 /* 127.0.0.1 */, StatsPort);
	if (!Socket->Bind(*LocalhostAddr))
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to bind to %s"), *LocalhostAddr->ToString(true));
		return false;
	}
	StatsPort = Socket->GetPortNo();

	// log the port
	UE_LOG(LogPerfCounters, Display, TEXT("FPerfCounters listening on port %d"), StatsPort);

	// for now, jack this up so we can send in one go
	int32 NewSize;
	Socket->SetSendBufferSize(512 * 1024, NewSize); // best effort 512k buffer to avoid not being able to send in one go

	// listen on the port
	if (!Socket->Listen(16))
	{
		UE_LOG(LogPerfCounters, Error, TEXT("FPerfCounters unable to listen on socket"));
		return false;
	}

	return true;
}

FString FPerfCounters::ToJson() const
{
	FString JsonStr;
	TSharedRef< TJsonWriter<> > Json = TJsonWriterFactory<>::Create(&JsonStr);
	Json->WriteObjectStart();
	for (const auto& It : PerfCounterMap)
	{
		const FJsonVariant& JsonValue = It.Value;
		switch (JsonValue.Format)
		{
		case FJsonVariant::String:
			Json->WriteValue(It.Key, JsonValue.StringValue);
			break;
		case FJsonVariant::Number:
			Json->WriteValue(It.Key, JsonValue.NumberValue);
			break;
		case FJsonVariant::Callback:
			if (JsonValue.CallbackValue.IsBound())
			{
				Json->WriteIdentifierPrefix(It.Key);
				JsonValue.CallbackValue.Execute(Json);
			}
			else
			{
				// write an explict null since the callback is unbound and the implication is this would have been an object
				Json->WriteNull(It.Key);
			}
			break;
		case FJsonVariant::Null:
		default:
			// don't write anything since wash may expect a scalar
			break;
		}
	}
	Json->WriteObjectEnd();
	Json->Close();
	return JsonStr;
}

bool FPerfCounters::Tick(float DeltaTime)
{
	// if we didn't get a socket, don't tick
	if (Socket == nullptr)
	{
		return false;
	}

	// accept any connections
	static const FString PerfCounterRequest = TEXT("FPerfCounters Request");
	FSocket* IncomingConnection = Socket->Accept(PerfCounterRequest);
	if (IncomingConnection)
	{
		// handle the connection
		int32 BytesSent = 0;
		FTCHARToUTF8 ConvertToUtf8(*ToJson());
		bool bSuccess = IncomingConnection->Send(reinterpret_cast<const uint8*>(ConvertToUtf8.Get()), ConvertToUtf8.Length(), BytesSent);
		if (!bSuccess || BytesSent != ConvertToUtf8.Length())
		{
			UE_LOG(LogPerfCounters, Warning, TEXT("FPerfCounters was unable to send a JSON response (or sent partial response)"));
		}
		IncomingConnection->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(IncomingConnection);
	}

	// keep ticking
	return true;
}

void FPerfCounters::SetNumber(const FString& Name, double Value) 
{
	FJsonVariant& JsonValue = PerfCounterMap.FindOrAdd(Name);
	JsonValue.Format = FJsonVariant::Number;
	JsonValue.NumberValue = Value;
}

void FPerfCounters::SetString(const FString& Name, const FString& Value)
{
	FJsonVariant& JsonValue = PerfCounterMap.FindOrAdd(Name);
	JsonValue.Format = FJsonVariant::String;
	JsonValue.StringValue = Value;
}

void FPerfCounters::SetJson(const FString& Name, const FProduceJsonCounterValue& Callback)
{
	FJsonVariant& JsonValue = PerfCounterMap.FindOrAdd(Name);
	JsonValue.Format = FJsonVariant::Callback;
	JsonValue.CallbackValue = Callback;
}
