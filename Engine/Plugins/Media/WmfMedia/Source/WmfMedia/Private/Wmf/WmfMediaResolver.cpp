// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaPCH.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "WmfMediaByteStream.h"
#include "WmfMediaResolver.h"
#include "WmfMediaResolveState.h"
#include "WmfMediaUtils.h"
#include "AllowWindowsPlatformTypes.h"


/* WmfMediaUrlResolver structors
 *****************************************************************************/

FWmfMediaResolver::FWmfMediaResolver()
	: RefCount(0)
{
	HRESULT Result = ::MFCreateSourceResolver(&SourceResolver);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Error, TEXT("Failed to create media source resolver (%s)"), *WmfMedia::ResultToString(Result));
	}
}


/* WmfMediaUrlResolver interface
 *****************************************************************************/

void FWmfMediaResolver::Cancel()
{
	if (SourceResolver == NULL)
	{
		return;
	}

	if (ResolveState != NULL)
	{
		ResolveState->Invalidate();
		ResolveState.Reset();
	}

	TComPtr<IUnknown> CancelCookieCopy = CancelCookie;

	if (CancelCookieCopy != NULL)
	{
		SourceResolver->CancelObjectCreation(CancelCookieCopy);
		CancelCookie.Reset();
	}
}


bool FWmfMediaResolver::ResolveByteStream(const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, IWmfMediaResolverCallbacks& Callbacks)
{
	if (SourceResolver == NULL)
	{
		return false;
	}

	Cancel();

	TComPtr<FWmfMediaByteStream> ByteStream = new FWmfMediaByteStream(Archive);
	TComPtr<FWmfMediaResolveState> NewResolveState = new(std::nothrow) FWmfMediaResolveState(EWmfMediaResolveType::ByteStream, OriginalUrl, Callbacks);
	HRESULT Result = SourceResolver->BeginCreateObjectFromByteStream(ByteStream, *OriginalUrl, MF_RESOLUTION_MEDIASOURCE, NULL, &CancelCookie, this, NewResolveState);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Warning, TEXT("Failed to resolve byte stream %s (%s)"), *OriginalUrl, *WmfMedia::ResultToString(Result));
		return false;
	}

	ResolveState = NewResolveState;

	return true;
}


bool FWmfMediaResolver::ResolveUrl(const FString& Url, IWmfMediaResolverCallbacks& Callbacks)
{
	if (SourceResolver == NULL)
	{
		return false;
	}

	Cancel();

	TComPtr<FWmfMediaResolveState> NewResolveState = new(std::nothrow) FWmfMediaResolveState(EWmfMediaResolveType::Url, Url, Callbacks);
	HRESULT Result = SourceResolver->BeginCreateObjectFromURL(*Url, MF_RESOLUTION_MEDIASOURCE, NULL, &CancelCookie, this, NewResolveState);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Warning, TEXT("Failed to resolve URL %s (%s)"), *Url, *WmfMedia::ResultToString(Result));
		return false;
	}

	ResolveState = NewResolveState;

	return true;
}


/* IMFAsyncCallback interface
 *****************************************************************************/

STDMETHODIMP_(ULONG) FWmfMediaResolver::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}


STDMETHODIMP FWmfMediaResolver::GetParameters(unsigned long*, unsigned long*)
{
	return E_NOTIMPL;
}


STDMETHODIMP FWmfMediaResolver::Invoke(IMFAsyncResult* AsyncResult)
{
	// are we currently resolving anything?
	TComPtr<FWmfMediaResolveState> ResolveStateCopy = ResolveState;

	if (ResolveStateCopy == NULL)
	{
		return S_OK;
	}

	// is this the resolve state we care about?
	TComPtr<FWmfMediaResolveState> State;

	if (FAILED(AsyncResult->GetState((IUnknown**)&State)) || (State != ResolveStateCopy))
	{
		return S_OK;
	}

	CancelCookie.Reset();

	// get the asynchronous result
	MF_OBJECT_TYPE ObjectType;
	TComPtr<IUnknown> SourceObject;
	HRESULT Result = S_FALSE;

	if (State->Type == EWmfMediaResolveType::ByteStream)
	{
		Result = SourceResolver->EndCreateObjectFromByteStream(AsyncResult, &ObjectType, &SourceObject);
	}
	else if (State->Type == EWmfMediaResolveType::Url)
	{
		Result = SourceResolver->EndCreateObjectFromURL(AsyncResult, &ObjectType, &SourceObject);
	}

	if (SUCCEEDED(Result))
	{
		State->ResolveComplete(SourceObject, State->Url);
	}
	else
	{
		UE_LOG(LogWmfMedia, Warning, TEXT("Failed to finish resolve %s: %s"), *State->Url, *WmfMedia::ResultToString(Result));

		State->ResolveFailed(State->Url);
	}

	ResolveState.Reset();

	return S_OK;
}


#if _MSC_VER == 1900
#pragma warning(push)
#pragma warning(disable:4838)
#endif // _MSC_VER == 1900

STDMETHODIMP FWmfMediaResolver::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FWmfMediaResolver, IMFAsyncCallback),
		{ 0 }
	};

	return QISearch( this, QITab, RefID, Object );
}

#if _MSC_VER == 1900
#pragma warning(pop)
#endif // _MSC_VER == 1900


STDMETHODIMP_(ULONG) FWmfMediaResolver::Release()
{
	int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);
	
	if (CurrentRefCount == 0)
	{
		delete this;
	}

	return CurrentRefCount;
}


#include "HideWindowsPlatformTypes.h"

#endif //WMFMEDIA_SUPPORTED_PLATFORM
