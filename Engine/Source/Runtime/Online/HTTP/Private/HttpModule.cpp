// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "HttpTests.h"

DEFINE_LOG_CATEGORY(LogHttp);

// FHttpModule

IMPLEMENT_MODULE(FHttpModule, HTTP);

FHttpModule* FHttpModule::Singleton = NULL;

FCriticalSection FHttpManager::RequestLock;

void FHttpModule::StartupModule()
{	
	Singleton = this;
	MaxReadBufferSize = 256 * 1024;

	FPlatformHttp::Init();

	HttpManager = FPlatformHttp::CreatePlatformHttpManager();
	if (NULL == HttpManager)
	{
		// platform does not provide specific HTTP manager, use generic one
		HttpManager = new FHttpManager();
	}

	HttpTimeout = 300.0f;
	GConfig->GetFloat(TEXT("HTTP"), TEXT("HttpTimeout"), HttpTimeout, GEngineIni);

	HttpConnectionTimeout = -1;
	GConfig->GetFloat(TEXT("HTTP"), TEXT("HttpConnectionTimeout"), HttpConnectionTimeout, GEngineIni);

	HttpReceiveTimeout = HttpConnectionTimeout;
	GConfig->GetFloat(TEXT("HTTP"), TEXT("HttpReceiveTimeout"), HttpReceiveTimeout, GEngineIni);

	HttpSendTimeout = HttpConnectionTimeout;
	GConfig->GetFloat(TEXT("HTTP"), TEXT("HttpSendTimeout"), HttpSendTimeout, GEngineIni);

	HttpMaxConnectionsPerServer = 16;
	GConfig->GetInt(TEXT("HTTP"), TEXT("HttpMaxConnectionsPerServer"), HttpMaxConnectionsPerServer, GEngineIni);
	
	bEnableHttp = true;
	GConfig->GetBool(TEXT("HTTP"), TEXT("bEnableHttp"), bEnableHttp, GEngineIni);
}

void FHttpModule::ShutdownModule()
{
#if PLATFORM_WINDOWS

	extern bool bUseCurl;
	if (!bUseCurl)
	{
		// due to peculiarities of some platforms (notably Windows with WinInet implementation) we need to shutdown platform http first,
		// then delete the manager. It is more logical to have reverse order of their creation though. Proper fix
		// would be refactoring HTTP platform abstraction to make HttpManager a proper part of it.
		FPlatformHttp::Shutdown();

		delete HttpManager;	// can be passed NULLs
	}
	else
#endif	// PLATFORM_WINDOWS
	{
		// at least on Linux, the code in HTTP manager (e.g. request destructors) expects platform to be initialized yet
		delete HttpManager;	// can be passed NULLs

		FPlatformHttp::Shutdown();
	}

	HttpManager = nullptr;
	Singleton = nullptr;
}

bool FHttpModule::HandleHTTPCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd, TEXT("TEST")))
	{
		int32 Iterations=1;
		FString IterationsStr;
		FParse::Token(Cmd, IterationsStr, true);
		if (!IterationsStr.IsEmpty())
		{
			Iterations = FCString::Atoi(*IterationsStr);
		}		
		FString Url;
		FParse::Token(Cmd, Url, true);
		if (Url.IsEmpty())
		{
			Url = TEXT("http://www.google.com");
		}		
		FHttpTest* HttpTest = new FHttpTest(TEXT("GET"),TEXT(""),Url,Iterations);
		HttpTest->Run();
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPREQ")))
	{
		GetHttpManager().DumpRequests(Ar);
	}
	return true;	
}

bool FHttpModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with HTTP
	if (FParse::Command(&Cmd, TEXT("HTTP")))
	{
		return HandleHTTPCommand( Cmd, Ar );
	}
	return false;
}

FHttpModule& FHttpModule::Get()
{
	if (Singleton == NULL)
	{
		check(IsInGameThread());
		FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");
	}
	check(Singleton != NULL);
	return *Singleton;
}

TSharedRef<IHttpRequest> FHttpModule::CreateRequest()
{
	// Create the platform specific Http request instance
	return TSharedRef<IHttpRequest>( FPlatformHttp::ConstructRequest() );
}

// FHttpManager

FHttpManager::FHttpManager()
	:	FTickerObjectBase(0.0f)
	,	DeferredDestroyDelay(10.0f)
{
	
}

FHttpManager::~FHttpManager()
{
	
}

bool FHttpManager::Tick(float DeltaSeconds)
{
	FScopeLock ScopeLock(&RequestLock);

	// Tick each active request
	for (TArray<TSharedRef<class IHttpRequest> >::TIterator It(Requests); It; ++It)
	{
		TSharedRef<class IHttpRequest> Request = *It;
		Request->Tick(DeltaSeconds);
	}
	// Tick any pending destroy objects
	for (int Idx=0; Idx < PendingDestroyRequests.Num(); Idx++)
	{
		FRequestPendingDestroy& Request = PendingDestroyRequests[Idx];
		Request.TimeLeft -= DeltaSeconds;
		if (Request.TimeLeft <= 0)
		{	
			PendingDestroyRequests.RemoveAt(Idx--);
		}		
	}
	// keep ticking
	return true;
}

void FHttpManager::AddRequest(TSharedRef<class IHttpRequest> Request)
{
	FScopeLock ScopeLock(&RequestLock);

	Requests.AddUnique(Request);
}

void FHttpManager::RemoveRequest(TSharedRef<class IHttpRequest> Request)
{
	FScopeLock ScopeLock(&RequestLock);

	// Keep track of requests that have been removed to be destroyed later
	PendingDestroyRequests.AddUnique(FRequestPendingDestroy(DeferredDestroyDelay,Request));

	Requests.RemoveSingle(Request);
}

bool FHttpManager::IsValidRequest(class IHttpRequest* RequestPtr)
{
	FScopeLock ScopeLock(&RequestLock);

	bool bResult = false;
	for (TArray<TSharedRef<class IHttpRequest> >::TConstIterator It(Requests); It; ++It)
	{
		const TSharedRef<class IHttpRequest>& Request = *It;

		if (&Request.Get() == RequestPtr)
		{
			bResult = true;
			break;
		}
	}
	return bResult;
}

void FHttpManager::DumpRequests(FOutputDevice& Ar)
{
	FScopeLock ScopeLock(&RequestLock);
	
	Ar.Logf(TEXT("------- (%d) Http Requests"), Requests.Num());
	for (TArray<TSharedRef<class IHttpRequest> >::TIterator It(Requests); It; ++It)
	{
		TSharedRef<class IHttpRequest> Request = *It;
		Ar.Logf(TEXT("	verb=[%s] url=[%s] status=%s"),
			*Request->GetVerb(), *Request->GetURL(), EHttpRequestStatus::ToString(Request->GetStatus()));
	}
}
