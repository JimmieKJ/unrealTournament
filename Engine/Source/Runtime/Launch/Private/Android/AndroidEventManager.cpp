// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LaunchPrivatePCH.h"
#include "AndroidEventManager.h"
#include "AndroidApplication.h"
#include "AudioDevice.h"
#include "CallbackDevice.h"
#include <android/native_window.h> 
#include <android/native_window_jni.h> 


DEFINE_LOG_CATEGORY(LogAndroidEvents);

FAppEventManager* FAppEventManager::sInstance = NULL;


FAppEventManager* FAppEventManager::GetInstance()
{
	if(!sInstance)
	{
		sInstance = new FAppEventManager();
	}

	return sInstance;
}


void FAppEventManager::Tick()
{
	while (!Queue.IsEmpty())
	{
		bool bDestroyWindow = false;

		FAppEventData Event = DequeueAppEvent();

		switch (Event.State)
		{
		case APP_EVENT_STATE_WINDOW_CREATED:
			check(FirstInitialized);
			bCreateWindow = true;
			PendingWindow = (ANativeWindow*)Event.Data;
			break;
		case APP_EVENT_STATE_WINDOW_RESIZED:
			bWindowChanged = true;
			break;
		case APP_EVENT_STATE_WINDOW_CHANGED:
			bWindowChanged = true;
			break;
		case APP_EVENT_STATE_SAVE_STATE:
			bSaveState = true; //todo android: handle save state.
			break;
		case APP_EVENT_STATE_WINDOW_DESTROYED:
			if (GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHMDConnected())
			{
				// delay the destruction until after the renderer teardown on GearVR
				bDestroyWindow = true;
			}
			else
			{
				FAndroidAppEntry::DestroyWindow();
				FPlatformMisc::SetHardwareWindow(NULL);
			}
			bHaveWindow = false;
			break;
		case APP_EVENT_STATE_ON_START:
			//doing nothing here
			break;
		case APP_EVENT_STATE_ON_DESTROY:
			if (FTaskGraphInterface::IsRunning())
			{
				FGraphEventRef WillTerminateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
				}, TStatId(), NULL, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(WillTerminateTask);
			}
			GIsRequestingExit = true; //destroy immediately. Game will shutdown.
			break;
		case APP_EVENT_STATE_ON_STOP:
			bHaveGame = false;
			break;
		case APP_EVENT_STATE_ON_PAUSE:
			bHaveGame = false;
			break;
		case APP_EVENT_STATE_ON_RESUME:
			bHaveGame = true;
			break;

		// window focus events that follow their own  heirarchy, and might or might not respect App main events heirarchy

		case APP_EVENT_STATE_WINDOW_GAINED_FOCUS: 
			bWindowInFocus = true;
			break;
		case APP_EVENT_STATE_WINDOW_LOST_FOCUS:
			bWindowInFocus = false;
			break;

		default:
			UE_LOG(LogAndroidEvents, Display, TEXT("Application Event : %u  not handled. "), Event.State);
		}

		if (bCreateWindow)
		{
			// wait until activity is in focus.
			if (bWindowInFocus) 
			{
				ExecWindowCreated();
				bCreateWindow = false;
				bHaveWindow = true;
			}
		}

		if (bWindowChanged)
		{
			// wait until window is valid and created. 
			if(FPlatformMisc::GetHardwareWindow() != NULL)
			{
				if(GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame)
				{
					ExecWindowChanged();
					bWindowChanged = false;
					bHaveWindow = true;
				}
			}
		}

		if (!bRunning && bHaveWindow && bHaveGame)
		{
			ResumeRendering();
			ResumeAudio();

			// broadcast events after the rendering thread has resumed
			if (FTaskGraphInterface::IsRunning())
			{
				FGraphEventRef EnterForegroundTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
				}, TStatId(), NULL, ENamedThreads::GameThread);
				FGraphEventRef ReactivateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
				}, TStatId(), EnterForegroundTask, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(ReactivateTask);
			}

			bRunning = true;
		}
		else if (bRunning && (!bHaveWindow || !bHaveGame))
		{
			// broadcast events before rendering thread suspends
			if (FTaskGraphInterface::IsRunning())
			{
				FGraphEventRef DeactivateTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
				}, TStatId(), NULL, ENamedThreads::GameThread);
				FGraphEventRef EnterBackgroundTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
				}, TStatId(), DeactivateTask, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(EnterBackgroundTask);
			}

			PauseRendering();
			PauseAudio();

			bRunning = false;
		}

		if (bDestroyWindow)
		{
			FAndroidAppEntry::DestroyWindow();
			FPlatformMisc::SetHardwareWindow(NULL);
			bDestroyWindow = false;
		}
	}

	if (!bRunning && FirstInitialized)
	{
		EventHandlerEvent->Wait();
	}
}


FAppEventManager::FAppEventManager():
	FirstInitialized(false)
	,bCreateWindow(false)
	,bWindowChanged(false)
	,bWindowInFocus(false)
	,bSaveState(false)
	,bAudioPaused(false)
	,PendingWindow(NULL)
	,bHaveWindow(false)
	,bHaveGame(false)
	,bRunning(false)
{
	pthread_mutex_init(&MainMutex, NULL);
	pthread_mutex_init(&QueueMutex, NULL);
}


void FAppEventManager::HandleWindowCreated(void* InWindow)
{
	int rc = pthread_mutex_lock(&MainMutex);
	check(rc == 0);
	bool AlreadyInited = FirstInitialized;
	rc = pthread_mutex_unlock(&MainMutex);
	check(rc == 0);

	if(AlreadyInited)
	{
		EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CREATED, InWindow );
	}
	else
	{
		//This cannot wait until first tick. 

		rc = pthread_mutex_lock(&MainMutex);
		check(rc == 0);
		
		check(FPlatformMisc::GetHardwareWindow() == NULL);
		FPlatformMisc::SetHardwareWindow(InWindow);
		FirstInitialized = true;

		rc = pthread_mutex_unlock(&MainMutex);
		check(rc == 0);

		EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CREATED, InWindow );
	}
}


void FAppEventManager::SetEventHandlerEvent(FEvent* InEventHandlerEvent)
{
	EventHandlerEvent = InEventHandlerEvent;
}


void FAppEventManager::PauseRendering()
{
	if(GUseThreadedRendering )
	{
		if (GIsThreadedRendering)
		{
			StopRenderingThread(); 
		}
	}
	else
	{
		RHIReleaseThreadOwnership();
	}
}


void FAppEventManager::ResumeRendering()
{
	if( GUseThreadedRendering )
	{
		if (!GIsThreadedRendering)
		{
			StartRenderingThread();
		}
	}
	else
	{
		RHIAcquireThreadOwnership();
	}
}


void FAppEventManager::ExecWindowCreated()
{
	UE_LOG(LogAndroidEvents, Display, TEXT("ExecWindowCreated"));
	check(PendingWindow)

	FPlatformMisc::SetHardwareWindow(PendingWindow);
	PendingWindow = NULL;
	
	FAndroidAppEntry::ReInitWindow();
}


void FAppEventManager::ExecWindowChanged()
{
	FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect();
	UE_LOG(LogAndroidEvents, Display, TEXT("ExecWindowChanged : width: %u, height:  %u"), ScreenRect.Right, ScreenRect.Bottom);

	if(GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame)
	{
		GEngine->GameViewport->ViewportFrame->ResizeFrame(ScreenRect.Right, ScreenRect.Bottom, EWindowMode::Fullscreen);
	}
}


void FAppEventManager::PauseAudio()
{
	bAudioPaused = true;

	if (GEngine->GetMainAudioDevice())
	{
		GEngine->GetMainAudioDevice()->Suspend(false);
	}
}


void FAppEventManager::ResumeAudio()
{
	bAudioPaused = false;

	if (GEngine->GetMainAudioDevice())
	{
		GEngine->GetMainAudioDevice()->Suspend(true);
	}
}


void FAppEventManager::EnqueueAppEvent(EAppEventState InState, void* InData)
{
	FAppEventData Event;
	Event.State = InState;
	Event.Data = InData;

	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);
	Queue.Enqueue(Event);
	 rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LogAndroidEvents: EnqueueAppEvent : %u, %u"), InState, (uintptr_t)InData);
}


FAppEventData FAppEventManager::DequeueAppEvent()
{
	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);

	FAppEventData OutData;
	Queue.Dequeue( OutData );

	rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	UE_LOG(LogAndroidEvents, Display, TEXT("DequeueAppEvent : %u, %u"), OutData.State, (uintptr_t)OutData.Data)

	return OutData;
}


bool FAppEventManager::IsGamePaused()
{
	return !bRunning;
}

bool FAppEventManager::WaitForEventInQueue(EAppEventState InState, double TimeoutSeconds)
{
	bool FoundEvent = false;
	double StopTime = FPlatformTime::Seconds() + TimeoutSeconds;

	TQueue<FAppEventData, EQueueMode::Spsc> HoldingQueue;
	while (!FoundEvent)
	{
		int rc = pthread_mutex_lock(&QueueMutex);
		check(rc == 0);

		// Copy the existing queue (and check for our event)
		while (!Queue.IsEmpty())
		{
			FAppEventData OutData;
			Queue.Dequeue(OutData);

			if (OutData.State == InState)
				FoundEvent = true;

			HoldingQueue.Enqueue(OutData);
		}

		if (FoundEvent)
			break;

		// Time expired?
		if (FPlatformTime::Seconds() > StopTime)
			break;

		// Unlock for new events and wait a bit before trying again
		rc = pthread_mutex_unlock(&QueueMutex);
		check(rc == 0);
		FPlatformProcess::Sleep(0.01f);
	}

	// Add events back to queue from holding
	while (!HoldingQueue.IsEmpty())
	{
		FAppEventData OutData;
		HoldingQueue.Dequeue(OutData);
		Queue.Enqueue(OutData);
	}

	int rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	return FoundEvent;
}
