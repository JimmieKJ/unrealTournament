// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "HttpThread.h"

// FHttpThread

FHttpThread::FHttpThread()
	:	HttpThreadTickBudget(0.033)
	,	Thread(nullptr)
{
	double TargetTickRate = FHttpModule::Get().GetHttpThreadTickRate();
	if (TargetTickRate > 0.0)
	{
		HttpThreadTickBudget = 1.0 / TargetTickRate;
	}

	UE_LOG(LogHttp, Log, TEXT("HTTP thread tick budget is %.1f ms (tick rate is %f)"), HttpThreadTickBudget, TargetTickRate);
}

FHttpThread::~FHttpThread()
{
	StopThread();
}

void FHttpThread::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("HttpManagerThread"), 128 * 1024, TPri_Normal);
}

void FHttpThread::StopThread()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
}

void FHttpThread::AddRequest(IHttpThreadedRequest* Request)
{
	FScopeLock ScopeLock(&RequestArraysLock);
	PendingThreadedRequests.Add(Request);
}

void FHttpThread::CancelRequest(IHttpThreadedRequest* Request)
{
	FScopeLock ScopeLock(&RequestArraysLock);
	CancelledThreadedRequests.Add(Request);
}

void FHttpThread::GetCompletedRequests(TArray<IHttpThreadedRequest*>& OutCompletedRequests)
{
	FScopeLock ScopeLock(&RequestArraysLock);
	OutCompletedRequests = CompletedThreadedRequests;
	CompletedThreadedRequests.Reset();
}

bool FHttpThread::Init()
{
	return true;
}

void FHttpThread::HttpThreadTick(float DeltaSeconds)
{
	// empty
}

bool FHttpThread::StartThreadedRequest(IHttpThreadedRequest* Request)
{
	return Request->StartThreadedRequest();
}

void FHttpThread::CompleteThreadedRequest(IHttpThreadedRequest* Request)
{
	// empty
}

uint32 FHttpThread::Run()
{
	double LastTime = FPlatformTime::Seconds();
	TArray<IHttpThreadedRequest*> RequestsToCancel;
	TArray<IHttpThreadedRequest*> RequestsToStart;
	TArray<IHttpThreadedRequest*> RequestsToComplete;
	while (!ExitRequest.GetValue())
	{
		double TickBegin = FPlatformTime::Seconds();

		{
			FScopeLock ScopeLock(&RequestArraysLock);

			RequestsToCancel = CancelledThreadedRequests;
			CancelledThreadedRequests.Reset();

			RequestsToStart = PendingThreadedRequests;
			PendingThreadedRequests.Reset();
		}

		// Cancel any pending cancel requests
		for (IHttpThreadedRequest* Request : RequestsToCancel)
		{
			if (RunningThreadedRequests.Remove(Request) > 0)
			{
				RequestsToComplete.Add(Request);
			}
		}
		// Start any pending requests
		for (IHttpThreadedRequest* Request : RequestsToStart)
		{
			if (StartThreadedRequest(Request))
			{
				RunningThreadedRequests.Add(Request);
			}
			else
			{
				RequestsToComplete.Add(Request);
			}
		}

		const double AppTime = FPlatformTime::Seconds();
		const double ElapsedTime = AppTime - LastTime;
		LastTime = AppTime;

		// Tick any running requests
		for (int32 Index = 0; Index < RunningThreadedRequests.Num(); ++Index)
		{
			IHttpThreadedRequest* Request = RunningThreadedRequests[Index];
			Request->TickThreadedRequest(ElapsedTime);
		}

		HttpThreadTick(ElapsedTime);

		// Move any completed requests
		for (int32 Index = 0; Index < RunningThreadedRequests.Num(); ++Index)
		{
			IHttpThreadedRequest* Request = RunningThreadedRequests[Index];
			if (Request->IsThreadedRequestComplete())
			{
				RequestsToComplete.Add(Request);
				RunningThreadedRequests.RemoveAtSwap(Index);
				--Index;
			}
		}

		if (RequestsToComplete.Num() > 0)
		{
			for (IHttpThreadedRequest* Request : RequestsToComplete)
			{
				CompleteThreadedRequest(Request);
			}

			{
				FScopeLock ScopeLock(&RequestArraysLock);
				CompletedThreadedRequests.Append(RequestsToComplete);
			}
			RequestsToComplete.Reset();
		}

		double TickDuration = (FPlatformTime::Seconds() - TickBegin);
		double WaitTime = FMath::Max(0.0, HttpThreadTickBudget - TickDuration);
		FPlatformProcess::SleepNoStats(WaitTime);
	}
	return 0;
}

void FHttpThread::Stop()
{
	ExitRequest.Set(true);
}
	
void FHttpThread::Exit()
{
	// empty
}
