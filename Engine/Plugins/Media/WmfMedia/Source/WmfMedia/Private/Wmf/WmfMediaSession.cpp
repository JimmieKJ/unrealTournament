// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPCH.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "WmfMediaSession.h"
#include "WmfMediaUtils.h"
#include "AllowWindowsPlatformTypes.h"


/* FWmfMediaSession structors
 *****************************************************************************/

FWmfMediaSession::FWmfMediaSession()
	: Buffering(false)
	, CanScrub(false)
	, Capabilities(0)
	, ChangeRequested(false)
	, CurrentRate(0.0f)
	, CurrentState(EMediaState::Closed)
	, Duration(FTimespan::Zero())
	, Looping(false)
	, RefCount(0)
	, RequestedRate(0.0f)
	, RequestedTime(FTimespan::MinValue())
	, StateChangePending(false)
{ }


FWmfMediaSession::FWmfMediaSession(const FTimespan& InDuration, const TComPtr<IMFTopology>& InTopology)
	: Buffering(false)
	, Capabilities(0)
	, ChangeRequested(false)
	, CurrentRate(0.0f)
	, Duration(InDuration)
	, Looping(false)
	, RefCount(0)
	, RequestedTime(FTimespan::MinValue())
	, RequestedRate(0.0f)
	, StateChangePending(false)
{
	CurrentState = EMediaState::Error;
	
	// create media session
	HRESULT Result = ::MFCreateMediaSession(NULL, &MediaSession);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to create media session (%s)"), *WmfMedia::ResultToString(Result));
		return;
	}

	// assign the media topology
	Result = MediaSession->SetTopology(0, InTopology);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to set topology in media session (%s)"), *WmfMedia::ResultToString(Result));
		return;
	}

	// start media event processing
	Result = MediaSession->BeginGetEvent(this, NULL);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to start media session event processing (%s)"), *WmfMedia::ResultToString(Result));
		return;
	}

	// get presentation clock, if available
	TComPtr<IMFClock> Clock;

	if (SUCCEEDED(MediaSession->GetClock(&Clock)))
	{
		Clock->QueryInterface(IID_PPV_ARGS(&PresentationClock));
	}

	// get rate control and support, if available
	if (SUCCEEDED(::MFGetService(MediaSession, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS(&RateControl))) &&
		SUCCEEDED(::MFGetService(MediaSession, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS(&RateSupport))))
	{
		CanScrub = SUCCEEDED(RateSupport->IsRateSupported(TRUE, 0, NULL));
	}

	CurrentState = EMediaState::Stopped;
	RequestedState = CurrentState;
}


/* IMediaControls interface
 *****************************************************************************/

FTimespan FWmfMediaSession::GetDuration() const
{
	return Duration;
}


float FWmfMediaSession::GetRate() const
{
	return CurrentRate;
}


EMediaState FWmfMediaSession::GetState() const
{
	if (Buffering)
	{
		return EMediaState::Preparing;
	}

	if ((CurrentState == EMediaState::Playing) && FMath::IsNearlyZero(CurrentRate))
	{
		// scrubbing counts as paused
		return EMediaState::Paused;
	}

	return CurrentState;
}


TRange<float> FWmfMediaSession::GetSupportedRates(EMediaPlaybackDirections Direction, bool Unthinned) const
{
	if (RateSupport == NULL)
	{
		return TRange<float>(0.0f);
	}

	float MaxRate = 0.0f;
	float MinRate = 0.0f;

	switch (Direction)
	{
	case EMediaPlaybackDirections::Forward:
		RateSupport->GetSlowestRate(MFRATE_FORWARD, !Unthinned, &MinRate);
		RateSupport->GetFastestRate(MFRATE_FORWARD, !Unthinned, &MaxRate);
		break;

	case EMediaPlaybackDirections::Reverse:
		RateSupport->GetSlowestRate(MFRATE_REVERSE, !Unthinned, &MinRate);
		RateSupport->GetFastestRate(MFRATE_REVERSE, !Unthinned, &MaxRate);
		break;
	}

	return TRange<float>(MinRate, MaxRate);
}


FTimespan FWmfMediaSession::GetTime() const
{
	if (PresentationClock == NULL)
	{
		return FTimespan::Zero();
	}

	if (RequestedTime >= FTimespan::Zero())
	{
		return RequestedTime;
	}

	return GetInternalPosition();
}


bool FWmfMediaSession::IsLooping() const
{
	return Looping;
}


bool FWmfMediaSession::Seek(const FTimespan& Time)
{
	RequestedTime = Time;

	return ChangeState();
}


bool FWmfMediaSession::SetLooping(bool InLooping)
{
	Looping = InLooping;

	return true;
}


bool FWmfMediaSession::SetRate(float Rate)
{
	if (!SupportsRate(Rate, false))
	{
		return false;
	}

	RequestedRate = Rate;

	if (FMath::IsNearlyZero(Rate))
	{
		if (!SupportsScrubbing())
		{
			RequestedState = EMediaState::Paused;
		}
	}
	else
	{
		RequestedState = EMediaState::Playing;
	}

	return ChangeState();
}


bool FWmfMediaSession::SupportsRate(float Rate, bool Unthinned) const
{
	if (Rate == 1.0f)
	{
		return true;
	}

	if ((RateControl == NULL) || (RateSupport == NULL))
	{
		return false;
	}

	if (Unthinned)
	{
		return SUCCEEDED(RateSupport->IsRateSupported(FALSE, Rate, NULL));
	}

	return SUCCEEDED(RateSupport->IsRateSupported(TRUE, Rate, NULL));
}


bool FWmfMediaSession::SupportsScrubbing() const
{
	return CanScrub;
}


bool FWmfMediaSession::SupportsSeeking() const
{
	return (((Capabilities & MFSESSIONCAP_SEEK) != 0) && (Duration > FTimespan::Zero()));
}


/* FWmfMediaSession interface
 *****************************************************************************/

bool FWmfMediaSession::SetState(EMediaState NewState)
{
	RequestedState = NewState;

	if ((NewState == EMediaState::Closed) || (NewState == EMediaState::Stopped))
	{
		RequestedTime = FTimespan::Zero();
		RequestedRate = 0.0f;
	}

	return ChangeState();
}


/* IMFAsyncCallback interface
 *****************************************************************************/

STDMETHODIMP_(ULONG) FWmfMediaSession::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}


STDMETHODIMP FWmfMediaSession::GetParameters(unsigned long*, unsigned long*)
{
	return E_NOTIMPL;
}


STDMETHODIMP FWmfMediaSession::Invoke(IMFAsyncResult* AsyncResult)
{
	// get event
	TComPtr<IMFMediaEvent> Event;
	
	if (FAILED(MediaSession->EndGetEvent(AsyncResult, &Event)))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Failed to get event"));
		return S_OK;
	}

	MediaEventType EventType = MEUnknown;
	
	if (FAILED(Event->GetType(&EventType)))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Failed to get session event type"));
		return S_OK;
	}

	// process event
	HRESULT EventResult = S_OK;

	if (FAILED(Event->GetStatus(&EventResult)))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("Failed to get event result"));
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("Media session event: %s: %s"), *WmfMedia::SessionEventToString(EventType), *WmfMedia::ResultToString(EventResult));

	FScopeLock ScopeLock(&CriticalSection);
	{
		switch (EventType)
		{
		case MEBufferingStarted:
			Buffering = true;
			break;

		case MEBufferingStopped:
			Buffering = false;
			break;

		case MEError:
			UE_LOG(LogWmfMedia, Error, TEXT("An error occured in the media session: %s"), *WmfMedia::ResultToString(EventResult));
			UpdateState(EMediaState::Error);
			break;

		case MESessionCapabilitiesChanged:
			Capabilities = ::MFGetAttributeUINT32(Event, MF_EVENT_SESSIONCAPS, Capabilities);
			break;

		case MESessionClosed:
			UpdateState(EMediaState::Closed);
			Capabilities = 0;
			break;

		case MESessionEnded:
			if (Looping)
			{
				RequestedTime = (CurrentRate < 0.0f) ? Duration : FTimespan::Zero();
				ChangeState();
			}
			else
			{
				RequestedState = EMediaState::Stopped;
				UpdateState(EMediaState::Stopped);
			}
			break;

		case MESessionPaused:
			UpdateState(EMediaState::Paused);
			break;

		case MESessionRateChanged:
			// recover active playback rate if rate change failed
			if (FAILED(EventResult) && (CurrentRate == RequestedRate))
			{
				PROPVARIANT Value;
				PropVariantInit(&Value);

				if (SUCCEEDED(Event->GetValue(&Value)) && (Value.vt == VT_R4))
				{
					CurrentRate = Value.fltVal;
				}

				RequestedRate = CurrentRate;
			}

		case MESessionScrubSampleComplete:
			if (GetInternalPosition() == RequestedTime)
			{
				RequestedTime = FTimespan::MinValue();
			}
			break;

		case MESessionStarted:
			UpdateState(EMediaState::Playing);
			break;

		case MESessionStopped:
			UpdateState(EMediaState::Stopped);
			break;
		}

		// bubble up event to player
		SessionEvent.Broadcast(EventType);

		// request the next event
		if (FAILED(MediaSession->BeginGetEvent(this, NULL)))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Failed to request next session event"));

			Capabilities = 0;
			CurrentState = EMediaState::Error;
		}
	}
	
	return S_OK;
}


#if _MSC_VER == 1900
#pragma warning(push)
#pragma warning(disable:4838)
#endif // _MSC_VER == 1900

STDMETHODIMP FWmfMediaSession::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FWmfMediaSession, IMFAsyncCallback),
		{ 0 }
	};

	return QISearch(this, QITab, RefID, Object);
}
#if _MSC_VER == 1900
#pragma warning(pop)
#endif // _MSC_VER == 1900


STDMETHODIMP_(ULONG) FWmfMediaSession::Release()
{
	int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);
	
	if (CurrentRefCount == 0)
	{
		if (MediaSession != NULL)
		{
			MediaSession->Shutdown();
		}

		delete this;
	}

	return CurrentRefCount;
}


/* FWmfVideoSession implementation
 *****************************************************************************/

bool FWmfMediaSession::ChangeState()
{
	// disallow state changes if session was closed or had an error
	if (CurrentState == EMediaState::Closed)
	{
		return false;
	}

	FScopeLock ScopeLock(&CriticalSection);
	{
		if (CurrentState == EMediaState::Error)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Closing media session on error"));
			MediaSession->Close();

			return true;
		}

		// defer state change
		if (StateChangePending)
		{
			ChangeRequested = true;

			return true;
		}

		// close session
		if (RequestedState == EMediaState::Closed)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Closing media session as requested"));
			MediaSession->Close();

			return true;
		}

		// handle rate changes
		if (UpdateRate())
		{
			return true;
		}

		// seek or scrub
		if (RequestedTime >= FTimespan::Zero())
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Starting playback to seek to %s"), *RequestedTime.ToString());

			PROPVARIANT StartPosition;
			{
				StartPosition.vt = VT_I8;
				StartPosition.hVal.QuadPart = RequestedTime.GetTicks();
			}

			HRESULT Result = MediaSession->Start(NULL, &StartPosition);

			if (FAILED(Result))
			{
				UE_LOG(LogWmfMedia, Warning, TEXT("Failed to seek %i"), *WmfMedia::ResultToString(Result));
			}
		}

		// handle state changes
		if (RequestedState == CurrentState)
		{
			return true;
		}

		bool StateChangeFailed = false;

		if (RequestedState == EMediaState::Paused)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Pausing media session as requested"));

			StateChangePending = true;
			StateChangeFailed = FAILED(MediaSession->Pause());
//			RequestedTime = GetTime();
		}
		else if (RequestedState == EMediaState::Playing)
		{
			PROPVARIANT StartPosition;

			if (RequestedTime >= FTimespan::Zero())
			{
				UE_LOG(LogWmfMedia, Verbose, TEXT("Starting playback at %s"), *RequestedTime.ToString());

				StartPosition.vt = VT_I8;
				StartPosition.hVal.QuadPart = RequestedTime.GetTicks();
			}
			else
			{
				UE_LOG(LogWmfMedia, Verbose, TEXT("Starting playback at beginning"));
				PropVariantInit(&StartPosition);
			}

			StateChangePending = true;
			StateChangeFailed = FAILED(MediaSession->Start(NULL, &StartPosition));
		}
		else if (RequestedState == EMediaState::Stopped)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Stopping playback as requested"));

			StateChangePending = true;
			StateChangeFailed = FAILED(MediaSession->Stop());
		}

		if (StateChangeFailed)
		{
			RequestedState = CurrentState;

			return false;
		}
	}

	return true;
}


bool FWmfMediaSession::UpdateRate()
{
	bool Result = false;

	if (RequestedRate != CurrentRate)
	{
		if (CurrentState != EMediaState::Stopped)
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Stopping playback for rate change"));

			if (RequestedTime == FTimespan::MinValue())
			{
				RequestedTime = GetTime();
			}

			MediaSession->Stop();
			Result = true;
		}

		if (SUCCEEDED(RateControl->SetRate(SupportsRate(RequestedRate, true) ? FALSE : TRUE, RequestedRate)))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Set rate from %f to %f"), CurrentRate, RequestedRate);
			CurrentRate = RequestedRate;
		}
		else
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("Failed to set rate from %f to %f"), CurrentRate, RequestedRate);
			RequestedRate = CurrentRate;
		}
	}

	return Result;
}


FTimespan FWmfMediaSession::GetInternalPosition() const
{
	MFTIME ClockTime;

	if (FAILED(PresentationClock->GetTime(&ClockTime)))
	{
		// topology not initialized yet
		return FTimespan::Zero();
	}

	return FTimespan(ClockTime);
}


void FWmfMediaSession::UpdateState(EMediaState CompletedState)
{
	CurrentState = CompletedState;

	if (CompletedState == EMediaState::Playing)
	{
		RequestedTime = FTimespan::MinValue();
	}

	if (CompletedState == RequestedState)
	{
		StateChangePending = false;
		UpdateRate();

		if (RequestedState == EMediaState::Stopped)
		{
			CurrentRate = 0.0f;
			RequestedRate = 0.0f;
		}
	}
	else
	{
		ChangeState();
	}
}


#include "HideWindowsPlatformTypes.h"

#endif //WMFMEDIA_SUPPORTED_PLATFORM
