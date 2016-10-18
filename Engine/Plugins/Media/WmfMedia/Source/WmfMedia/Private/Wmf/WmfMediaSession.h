// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMediaControls.h"
#include "AllowWindowsPlatformTypes.h"


// forward declarations
enum class EMediaPlaybackDirections;


/**
 * Implements a media session that for handling asynchronous commands and callbacks.
 *
 * Many of the media playback features are asynchronous and do not take place
 * immediately, such as seeking and playback rate changes. A media session may
 * generate events during playback that are handled in this class.
 *
 * Windows Media Foundation also queues up most playback commands, which may have
 * undesired side effects, such as unresponsive or sluggish user interfaces. For
 * this reason, this helper class also implements a mechanism to manage pending
 * operations efficiently.
 */
class FWmfMediaSession
	: public IMFAsyncCallback
	, public IMediaControls
{
public:

	/** Default constructor. */
	FWmfMediaSession();

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDuration The duration of the media.
	 * @param InTopology The playback topology to use.
	 */
	FWmfMediaSession(const FTimespan& InDuration, const TComPtr<IMFTopology>& InTopology);

	/** Virtual destructor. */
	virtual ~FWmfMediaSession() { }

public:

	//~ IMediaControls interface

	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual TRange<float> GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek(const FTimespan& Time) override;
	virtual bool SetLooping(bool Looping) override;
	virtual bool SetRate(float Rate) override;
	virtual bool SupportsRate(float Rate, bool Unthinned) const override;
	virtual bool SupportsScrubbing() const override;
	virtual bool SupportsSeeking() const override;

public:

	/**
	 * Gets the session capabilities.
	 *
	 * @return Capabilities bit mask.
	 */
	DWORD GetCapabilities() const
	{
		return Capabilities;
	}

	/**
	 * Sets the media state.
	 *
	 * @param NewState The media state to set.
	 * @return true if the state will be changed, false otherwise.
	 */
	bool SetState(EMediaState NewState);

public:

	/** Gets an event delegate that is invoked when the last presentation segment finished playing. */
	DECLARE_EVENT_OneParam(FWmfMediaSession, FOnSessionEvent, MediaEventType /*EventType*/)
	FOnSessionEvent& OnSessionEvent()
	{
		return SessionEvent;
	}

public:

	//~ IMFAsyncCallback interface

	STDMETHODIMP_(ULONG) AddRef();
	STDMETHODIMP GetParameters(unsigned long*, unsigned long*);
	STDMETHODIMP Invoke(IMFAsyncResult* AsyncResult);
	STDMETHODIMP QueryInterface(REFIID RefID, void** Object);
	STDMETHODIMP_(ULONG) Release();

protected:

	/**
	 * Attempts to change the session state using the requested settings.
	 *
	 * @return true if the state is being changed, false otherwise.
	 */
	bool ChangeState();

	/**
	 * Updates the CurrentPlayRate if it is different than the RequestedPlayRate.
	 *
	 * @return Whether playback was stopped as a result of the update.
	 */
	bool UpdateRate();

	/**
	 * Gets the playback position as known by the internal WMF clock.
	 *
	 * @return The internal playback position.
	 */
	FTimespan GetInternalPosition() const;

	/**
	 * Updates the state machine after a state transition was completed.
	 *
	 * @param CompletedState The state that was completed.
	 */
	void UpdateState(EMediaState CompletedState);

private:

	/** Whether the media source is buffering data. */
	bool Buffering;

	/** Caches whether the media supports scrubbing. */
	bool CanScrub;

	/** The session's capabilities. */
	DWORD Capabilities;

	/** Whether a new state change has been requested. */
	bool ChangeRequested;

	/** Critical section for locking access to this object. */
	mutable FCriticalSection CriticalSection;

	/** The current playback rate. */
	float CurrentRate;

	/** The current playback state. */
	EMediaState CurrentState;

	/** The duration of the media. */
	FTimespan Duration;

	/** The media session that handles all playback. */
	TComPtr<IMFMediaSession> MediaSession;

	/** Whether playback should loop to the beginning. */
	bool Looping;

	/** The media session's clock. */
	TComPtr<IMFPresentationClock> PresentationClock;

	/** Optional interface for controlling playback rates. */
	TComPtr<IMFRateControl> RateControl;

	/** Optional interface for querying supported playback rates. */
	TComPtr<IMFRateSupport> RateSupport;

	/** Holds a reference counter for this instance. */
	int32 RefCount;

	/** The requested playback rate. */
	float RequestedRate;

	/** The requested playback position. */
	FTimespan RequestedTime;

	/** The requested playback state. */
	EMediaState RequestedState;

	/** Whether a state change is currently in progress. */
	bool StateChangePending;

private:

	/** Holds an event delegate that is invoked when an event occurs in the session. */
	FOnSessionEvent SessionEvent;
};


#include "HideWindowsPlatformTypes.h"
