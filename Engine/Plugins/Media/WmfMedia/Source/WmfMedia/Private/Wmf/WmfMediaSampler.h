// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


/**
 * Implements a callback object for the sample grabber sink.
 *
 * 
 */
class FWmfMediaSampler
	: public IMFSampleGrabberSinkCallback
{
public:
	DECLARE_DELEGATE(CommandDelegate)

	/** Default constructor. */
	FWmfMediaSampler()
		: RefCount(0)
	{ }

	/** Virtual destructor. */
	virtual ~FWmfMediaSampler() { }

public:

	/**
	 * Registers a media sink with this sampler.
	 *
	 * @param Sink The media sink to register.
	 * @return true on success, false otherwise.
	 * @see UnregisterSink
	 */
	bool RegisterSink( const IMediaSinkRef& Sink )
	{
		return Commands.Enqueue(FSimpleDelegate::CreateRaw(this, &FWmfMediaSampler::HandleRegisterSink, IMediaSinkWeakPtr(Sink)));
	}

	/**
	 * Unregisters a media sink with this sampler.
	 *
	 * @param Sink The media sink to unregister.
	 * @return true on success, false otherwise.
	 * @see RegisterSink
	 */
	bool UnregisterSink( const IMediaSinkRef& Sink )
	{
		return Commands.Enqueue(FSimpleDelegate::CreateRaw(this, &FWmfMediaSampler::HandleUnregisterSink, IMediaSinkWeakPtr(Sink)));
	}

public:

	// IMFSampleGrabberSinkCallback interface

	STDMETHODIMP_(ULONG) AddRef()
	{
		return FPlatformAtomics::InterlockedIncrement(&RefCount);
	}

	STDMETHODIMP OnClockPause( MFTIME SystemTime)
	{
		return S_OK;
	}

	STDMETHODIMP OnClockRestart( MFTIME SystemTime)
	{
		return S_OK;
	}

	STDMETHODIMP OnClockSetRate( MFTIME SystemTime, float flRate)
	{
		return S_OK;
	}

	STDMETHODIMP OnClockStart( MFTIME SystemTime, LONGLONG llClockStartOffset)
	{
		return S_OK;
	}

	STDMETHODIMP OnClockStop( MFTIME SystemTime)
	{
		return S_OK;
	}

	STDMETHODIMP OnProcessSample( REFGUID MajorMediaType, DWORD SampleFlags, LONGLONG SampleTime, LONGLONG SampleDuration, const BYTE* SampleBuffer, DWORD SampleSize)
	{
		// process pending commands
		CommandDelegate Command;

		while (Commands.Dequeue(Command))
		{
			Command.Execute();
		}

		// notify all registered media sinks
		for (IMediaSinkWeakPtr& SinkPtr : Sinks)
		{
			IMediaSinkPtr Sink = SinkPtr.Pin();

			if (Sink.IsValid())
			{
				Sink->ProcessMediaSample(SampleBuffer, SampleSize, FTimespan(SampleDuration), FTimespan(SampleTime));
			}
		}

		return S_OK;
	}

	STDMETHODIMP OnSetPresentationClock( IMFPresentationClock* Clock)
	{
		return S_OK;
	}

	STDMETHODIMP OnShutdown()
	{
		return S_OK;
	}


#if _MSC_VER == 1900
#pragma warning(push)
#pragma warning(disable:4838)
#endif // _MSC_VER == 1900
	STDMETHODIMP QueryInterface( REFIID RefID, void** Object )
	{
		static const QITAB QITab[] =
		{
			QITABENT(FWmfMediaSampler, IMFSampleGrabberSinkCallback),
			{ 0 }
		};

		return QISearch(this, QITab, RefID, Object);
	}
#if _MSC_VER == 1900
#pragma warning(pop)
#endif // _MSC_VER == 1900

	STDMETHODIMP_(ULONG) Release()
	{
		int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);

		if (CurrentRefCount == 0)
		{
			delete this;
		}

		return CurrentRefCount;
	}

private:

	void HandleRegisterSink( IMediaSinkWeakPtr Sink )
	{
		Sinks.AddUnique(Sink);
	}

	void HandleUnregisterSink( IMediaSinkWeakPtr Sink )
	{
		Sinks.RemoveSingle(Sink);
	}

private:

	/** Holds the router command queue. */
	TQueue<CommandDelegate, EQueueMode::Mpsc> Commands;

	/** The collection of registered media sinks. */
	TArray<IMediaSinkWeakPtr> Sinks;

	/** Holds a reference counter for this instance. */
	int32 RefCount;
};


#include "HideWindowsPlatformTypes.h"
