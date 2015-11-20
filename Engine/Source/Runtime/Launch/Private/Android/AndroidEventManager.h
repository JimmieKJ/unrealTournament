// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#ifndef ANDROID_EVENT_MANAGER_H
#define ANDROID_EVENT_MANAGER_H

struct ANativeWindow;
DECLARE_LOG_CATEGORY_EXTERN(LogAndroidEvents, Log, All);

enum EAppEventState
{
	APP_EVENT_STATE_WINDOW_CREATED,
	APP_EVENT_STATE_WINDOW_RESIZED,
	APP_EVENT_STATE_WINDOW_CHANGED,
	APP_EVENT_STATE_WINDOW_DESTROYED,
	APP_EVENT_STATE_WINDOW_REDRAW_NEEDED,
	APP_EVENT_STATE_ON_DESTROY,
	APP_EVENT_STATE_ON_PAUSE,
	APP_EVENT_STATE_ON_RESUME,
	APP_EVENT_STATE_ON_STOP,
	APP_EVENT_STATE_ON_START,
	APP_EVENT_STATE_WINDOW_LOST_FOCUS,
	APP_EVENT_STATE_WINDOW_GAINED_FOCUS,
	APP_EVENT_STATE_SAVE_STATE,
	APP_EVENT_STATE_INVALID = -1,
};

struct FAppEventData
{
	EAppEventState State;
	void* Data;

	FAppEventData():
		State(APP_EVENT_STATE_INVALID)
		,Data(NULL)
	{	}
};

class FAppEventManager
{
public:
	static FAppEventManager* GetInstance();
	
	void Tick();
	void EnqueueAppEvent(EAppEventState InState, void* InData = NULL);
	void SetEventHandlerEvent(FEvent* InEventHandlerEvent);
	void HandleWindowCreated(void* InWindow);
	bool IsGamePaused();
	bool WaitForEventInQueue(EAppEventState InState, double TimeoutSeconds);

protected:
	FAppEventManager();

private:
	FAppEventData DequeueAppEvent();
	void PauseRendering();
	void ResumeRendering();
	void PauseAudio();
	void ResumeAudio();

	void ExecWindowCreated();
	void ExecWindowResized();
	
	static FAppEventManager* sInstance;

	pthread_mutex_t QueueMutex;			//@todo android: can probably get rid of this now that we're using an mpsc queue
	TQueue<FAppEventData, EQueueMode::Mpsc> Queue;
	
	pthread_mutex_t MainMutex;

	FEvent* EventHandlerEvent;			// triggered every time the event handler thread fires off an event

	//states
	bool FirstInitialized;
	bool bCreateWindow;
	ANativeWindow* PendingWindow;

	bool bWindowInFocus;
	bool bSaveState;
	bool bAudioPaused;

	bool bHaveWindow;
	bool bHaveGame;
	bool bRunning;
};


#endif
