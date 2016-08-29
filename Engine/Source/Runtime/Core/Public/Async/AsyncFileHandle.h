// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(USE_NEW_ASYNC_IO)
#error "USE_NEW_ASYNC_IO must be defined."
#endif

#if USE_NEW_ASYNC_IO

#include "Function.h"

// Note on threading. Like the rest of the filesystem platform abstraction, these methods are threadsafe, but it is expected you are not concurrently _using_ these data structures. 

class IAsyncReadRequest;
typedef TFunction<void(bool bWasCancelled, IAsyncReadRequest*)> FAsyncFileCallBack;

class CORE_API IAsyncReadRequest
{
protected:
	union
	{
		PTRINT Size;
		uint8* Memory;
	};
	FAsyncFileCallBack* Callback;
	bool bDataIsReady;
	bool bCompleteAndCallbackCalled;
	bool bCompleteSync;
	bool bCanceled;
	const bool bSizeRequest;
public:

	FORCEINLINE IAsyncReadRequest(FAsyncFileCallBack* InCallback, bool bInSizeRequest)
		: Callback(InCallback)
		, bDataIsReady(false)
		, bCompleteAndCallbackCalled(false)
		, bCompleteSync(false)
		, bCanceled(false)
		, bSizeRequest(bInSizeRequest)
	{
		if (bSizeRequest)
		{
			Size = -1;
		}
		else
		{
			Memory = nullptr;
		}
	}

	/* Not legal to destroy the request until it is complete. */
	virtual ~IAsyncReadRequest()
	{
		check(bCompleteAndCallbackCalled);
		if (!bSizeRequest && Memory)
		{
			FMemory::Free(Memory);
		}
	}

	/**
	* Nonblocking poll of the state of completion.
	* @return true if the request is complete
	**/
	FORCEINLINE bool PollCompletion()
	{
		return bCompleteAndCallbackCalled;
	}

	/**
	* Waits for the request to complete, but not longer than the given time limit
	* @param TimeLimitSeconds	Zero to wait forever, otherwise the maximum amount of time to wait.
	* @return true if the request is complete
	**/
	FORCEINLINE bool WaitCompletion(float TimeLimitSeconds = 0.0f)
	{
		if (PollCompletion())
		{
			return true;
		}
		WaitCompletionImpl(TimeLimitSeconds);
		return PollCompletion();
	}

	/** Cancel the request. This is a non-blocking async call and so does not ensure completion! **/
	FORCEINLINE void Cancel()
	{
		bCanceled = true;
		bDataIsReady = true;
		FPlatformMisc::MemoryBarrier();
		if (!PollCompletion())
		{
			return CancelImpl();
		}
	}

	/**
	* Return the size of a completed size request. Not legal to call unless the request is complete.
	* @return Returned size of the file or -1 if the file was not found, the request was canceled or other error.
	**/
	FORCEINLINE int64 GetSizeResults()
	{
		check(bDataIsReady && bSizeRequest);
		return bCanceled ? -1 : Size;
	}

	/**
	* Return the bytes of a completed read request. Not legal to call unless the request is complete.
	* @return Returned memory block which if non-null contains the bytes read. Caller owns the memory block and must call FMemory::Free on it when done. Can be null if the file was not found or could not be read or the request was cancelled.
	**/
	FORCEINLINE uint8* GetReadResults()
	{
		check(bDataIsReady && !bSizeRequest);
		uint8* Result = Memory;
		if (bCanceled && Result)
		{
			FMemory::Free(Result);
			Result = nullptr;
		}
		Memory = nullptr;
		return Result;
	}

protected:
	/**
	* Waits for the request to complete, but not longer than the given time limit
	* @param TimeLimitSeconds	Zero to wait forever, otherwise the maximum amount of time to wait.
	* @return true if the request is complete
	**/
	virtual void WaitCompletionImpl(float TimeLimitSeconds) = 0;

	/** Cancel the request. This is a non-blocking async call and so does not ensure completion! **/
	virtual void CancelImpl() = 0;

	void SetComplete()
	{
		bDataIsReady = true;
		FPlatformMisc::MemoryBarrier();
		if (Callback)
		{
			(*Callback)(bCanceled, this);
		}
		FPlatformMisc::MemoryBarrier();
		bCompleteAndCallbackCalled = true;
		FPlatformMisc::MemoryBarrier();
	}
};

class CORE_API IAsyncReadFileHandle
{
public:
	/** Destructor, also the only way to close the file handle. It is not legal to delete an async file with outstanding requests. **/
	virtual ~IAsyncReadFileHandle()
	{
	}

	/**
	* Request the size of the file. This is also essentially the existence check.
	* @param CompleteCallback		Called from an arbitrary thread when the request is complete. Can be nullptr, if non-null, must remain valid until it is called. It will always be called.
	* @return A request for the size. This is owned by the caller and must be deleted by the caller.
	**/
	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) = 0;

	/**
	* Submit an async request and/or wait for an async request
	* @param Offset					Offset into the file to start reading.
	* @param BytesToRead			number of bytes to read.
	* @param CompleteCallback		Called from an arbitrary thread when the request is complete. Can be nullptr, if non-null, must remain valid until it is called. It will always be called.
	* @return A request for the read. This is owned by the caller and must be deleted by the caller.
	**/
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriority Priority = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr) = 0;
};


#endif // USE_NEW_ASYNC_IO