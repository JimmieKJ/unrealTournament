// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPrivatePCH.h"
#include "AllowWindowsPlatformTypes.h"


/* FWmfByteStream structors
 *****************************************************************************/

FWmfMediaByteStream::FWmfMediaByteStream( const TSharedRef<TArray<uint8>>& InBuffer )
	: AsyncReadInProgress(false)
	, Buffer(InBuffer)
	, Position(0)
	, RefCount(0)
{ }


/* IMFAsyncCallback interface
 *****************************************************************************/

STDMETHODIMP_(ULONG) FWmfMediaByteStream::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}


STDMETHODIMP FWmfMediaByteStream::GetParameters( unsigned long*, unsigned long*)
{
	return E_NOTIMPL;
}


STDMETHODIMP FWmfMediaByteStream::Invoke( IMFAsyncResult* AsyncResult )
{
	// recover read state
	IUnknown* State = NULL;
	HRESULT Result = AsyncResult->GetState(&State);
	check(SUCCEEDED(Result));

	IMFAsyncResult* CallerResult = NULL;
	Result = State->QueryInterface(IID_PPV_ARGS(&CallerResult));
	check(SUCCEEDED(Result));

	IUnknown* Unknown = NULL;
	
	if (CallerResult != NULL)
	{
		Result = CallerResult->GetObject(&Unknown);
		check(SUCCEEDED(Result));
	}

	FWmfMediaReadState* ReadState = static_cast<FWmfMediaReadState*>(Unknown);

	// perform the read
	ULONG cbRead;
	Read(ReadState->GetReadBuffer(), ReadState->GetReadBufferSize() - ReadState->GetBytesRead(), &cbRead);
	ReadState->AddBytesRead(cbRead);

	// notify caller
	if (CallerResult != NULL)
	{
		CallerResult->SetStatus(Result);
		::MFInvokeCallback(CallerResult);
	}

	// clean up
	if (CallerResult != NULL)
	{
		CallerResult->Release();
	}

	if (Unknown != NULL)
	{
		Unknown->Release();
	}

	if (State != NULL)
	{
		State->Release();
	}

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::QueryInterface( REFIID RefID, void** Object )
{
	static const QITAB QITab[] =
	{
		QITABENT(FWmfMediaByteStream, IMFByteStream),
		QITABENT(FWmfMediaByteStream, IMFAsyncCallback),
		{ 0 }
	};

	return QISearch( this, QITab, RefID, Object );
}


STDMETHODIMP_(ULONG) FWmfMediaByteStream::Release()
{
	int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);
	
	if (CurrentRefCount == 0)
	{
		delete this;
	}

	return CurrentRefCount;
}


/* IMFByteStream interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaByteStream::BeginRead( BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState )
{
	if ((pCallback == NULL) || (pb == NULL))
	{
		return E_INVALIDARG;
	}

	FWmfMediaReadState* ReadState = new(std::nothrow) FWmfMediaReadState(pb, cb);

	if (ReadState == NULL)
	{
		return E_OUTOFMEMORY;
	}

	IMFAsyncResult* AsyncResult = NULL;
	HRESULT Result = ::MFCreateAsyncResult(ReadState, pCallback, punkState, &AsyncResult);
	ReadState->Release();

	if (SUCCEEDED(Result))
	{
		AsyncReadInProgress = true;
		Result = ::MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, this, AsyncResult);
		AsyncResult->Release();
	}

	return Result;
}


STDMETHODIMP FWmfMediaByteStream::BeginWrite( const BYTE* pb, ULONG cb, IMFAsyncCallback* pCallback, IUnknown* punkState )
{
	return E_NOTIMPL;
}


STDMETHODIMP FWmfMediaByteStream::Close()
{
	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::EndRead( IMFAsyncResult* pResult, ULONG* pcbRead )
{
	if (pcbRead == NULL)
	{
		return E_INVALIDARG;
	}

	IUnknown* Unknown;
	{
		pResult->GetObject(&Unknown);
		FWmfMediaReadState* ReadState = static_cast<FWmfMediaReadState*>(Unknown);
		*pcbRead = (ULONG)ReadState->GetBytesRead();

		Unknown->Release();
	}

	AsyncReadInProgress = false;

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::EndWrite( IMFAsyncResult* pResult, ULONG* pcbWritten )
{
	return E_NOTIMPL;
}


STDMETHODIMP FWmfMediaByteStream::Flush()
{
	return E_NOTIMPL;
}


STDMETHODIMP FWmfMediaByteStream::GetCapabilities( DWORD* pdwCapabilities )
{
	FScopeLock ScopeLock(&CriticalSection);
	*pdwCapabilities = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE;

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::GetCurrentPosition( QWORD* pqwPosition )
{
	FScopeLock ScopeLock(&CriticalSection);
	*pqwPosition = Position;

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::GetLength( QWORD* pqwLength )
{
	FScopeLock ScopeLock(&CriticalSection);
	*pqwLength = (QWORD)Buffer->Num();

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::IsEndOfStream( BOOL* pfEndOfStream )
{
	if (pfEndOfStream == NULL)
	{
		return E_INVALIDARG;
	}

	FScopeLock ScopeLock(&CriticalSection);
	*pfEndOfStream = (Position >= Buffer->Num()) ? TRUE : FALSE;

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::Read( BYTE* pb, ULONG cb, ULONG* pcbRead )
{
	FScopeLock ScopeLock(&CriticalSection);

	ULONG BytesToRead = cb;

	if (BytesToRead > (ULONG)Buffer->Num())
	{
		BytesToRead = Buffer->Num();
	}

	if ((Buffer->Num() - BytesToRead) < Position)
	{
		BytesToRead = Buffer->Num() - Position;
	}

	if (BytesToRead > 0)
	{
		FMemory::Memcpy(pb, ((BYTE*)Buffer->GetData() + Position), BytesToRead);
	}

	if (pcbRead != NULL)
	{
		*pcbRead = BytesToRead;
	}	

	Position += BytesToRead;

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::Seek( MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG qwSeekOffset, DWORD dwSeekFlags, QWORD* pqwCurrentPosition )
{
	FScopeLock ScopeLock(&CriticalSection);

	if (AsyncReadInProgress)
	{
		return S_FALSE;
	}

	if (SeekOrigin == msoCurrent)
	{
		Position += qwSeekOffset;
	}
	else
	{
		Position = qwSeekOffset;
	}

	if (pqwCurrentPosition != NULL)
	{
		*pqwCurrentPosition = (QWORD)Position;
	}

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::SetLength( QWORD qwLength )
{
	return E_NOTIMPL;
}


STDMETHODIMP FWmfMediaByteStream::SetCurrentPosition( QWORD qwPosition )
{
	FScopeLock ScopeLock(&CriticalSection);

	if (AsyncReadInProgress)
	{
		return S_FALSE;
	}

	Position = (uint64)qwPosition;

	return S_OK;
}


STDMETHODIMP FWmfMediaByteStream::Write( const BYTE* pb, ULONG cb, ULONG* pcbWritten )
{
	return E_NOTIMPL;
}


#include "HideWindowsPlatformTypes.h"
