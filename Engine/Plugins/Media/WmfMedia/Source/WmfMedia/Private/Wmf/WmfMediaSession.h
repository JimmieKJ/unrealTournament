// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


// forward declarations
enum class EMediaPlaybackDirections;


/**
 * Enumerates possible states of media playback.
 */
enum class EMediaStates
{
	/** Video has been closed and cannot be played again. */
	Closed,

	/** Unrecoverable error occurred during playback. */
	Error,

	/** Playback has been paused, but can be resumed. */
	Paused,

	/** Video is currently playing. */
	Playing,

	/** Playback has been stopped, but can be restarted. */
	Stopped
};


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
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InDuration The duration of the media.
	 * @param InTopology The playback topology to use.
	 */
	FWmfMediaSession( const FTimespan& InDuration, const TComPtr<IMFTopology>& InTopology );

	/** Virtual destructor. */
	virtual ~FWmfMediaSession() { }

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
	 * Gets the current playback position.
	 *
	 * @return Presentation clock time, or FTimespan::MinValue if no clock is available.
	 */
	FTimespan GetPosition() const;

	/**
	 * Gets the current playback rate.
	 *
	 * @return Playback rate.
	 */
	float GetRate() const
	{
		return CurrentRate;
	}

	/**
	 * Gets the state of this session.
	 *
	 * @return Media session state.
	 */
	EMediaStates GetState() const
	{
		return CurrentState;
	}

	/**
	 * Gets the supported playback rate for the specified rate type.
	 *
	 * @param Direction The playback direction.
	 * @param Unthinned Whether the rates are for unthinned playback.
	 * @return The supported playback rate.
	 */
	TRange<float> GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const;

	/**
	 * Checks whether playback is currently looping.
	 *
	 * @return true if looping, false otherwise.
	 */
	bool IsLooping() const
	{
		return Looping;
	}

	/**
	 * Checks whether the specified playback rate is supported.
	 *
	 * @param Rate The rate to check (can be negative for reverse play).
	 * @param Unthinned Whether no frames should be dropped at the given rate.
	 * @return true if the rate is supported, false otherwise.
	 */
	bool IsRateSupported(float Rate, bool Unthinned) const;

	/**
	 * Sets whether playback should loop back to the beginning.
	 *
	 * @param InLooping Whether playback should be looped.
	 */
	void SetLooping(bool InLooping)
	{
		Looping = InLooping;
	}

	/**
	 * Changes the playback position of the media.
	 *
	 * @param Position The position to set.
	 * @return true if the position will be changed, false otherwise.
	 */
	bool SetPosition(const FTimespan& Position);

	/**
	 * Sets the playback rate.
	 *
	 * @param Rate The rate to set.
	 * @return true if the rate will be changed, false otherwise.
	 */
	bool SetRate(float Rate);

	/**
	 * Sets the media state.
	 *
	 * @param NewState The media state to set.
	 * @return true if the state will be changed, false otherwise.
	 */
	bool SetState(EMediaStates NewState);

	/**
	 * Checks whether this session supports scrubbing.
	 *
	 * @return true if scrubbing is supported, false otherwise.
	 */
	bool SupportsScrubbing() const
	{
		return CanScrub;
	}

public:

	/** Gets an event delegate that is invoked when an error occured. */
	DECLARE_EVENT_OneParam(FWmfMediaSession, FOnError, HRESULT /*Error*/)
	FOnError& OnError()
	{
		return ErrorEvent;
	}

public:

	// IMFAsyncCallback interface

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
	void UpdateState(EMediaStates CompletedState);

private:

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
	EMediaStates CurrentState;

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

	/** The requested playback position. */
	FTimespan RequestedPosition;

	/** The requested playback rate. */
	float RequestedRate;

	/** The requested playback state. */
	EMediaStates RequestedState;

	/** Whether a state change is currently in progress. */
	bool StateChangePending;

private:

	/** Holds an event delegate that is invoked when an error occurred. */
	FOnError ErrorEvent;
};


#include "HideWindowsPlatformTypes.h"
