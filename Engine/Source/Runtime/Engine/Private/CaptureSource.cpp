// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CaptureSource.h: CaptureSource implementation
=============================================================================*/
#include "EnginePrivate.h"

#include "CapturePin.h"
#include "CaptureSource.h"


#if PLATFORM_WINDOWS && !UE_BUILD_MINIMAL

FCaptureSource::FCaptureSource(IUnknown *pUnk, HRESULT *phr)
           : CSource(NAME("PushSource"), pUnk, CLSID_CaptureSource)
{
	CapturePin = new FCapturePin(phr, this);

	if (phr)
	{
		if (CapturePin == NULL)
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}  
}


FCaptureSource::~FCaptureSource()
{
	delete CapturePin;
}


CUnknown * WINAPI FCaptureSource::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
	FCaptureSource *pNewFilter = new FCaptureSource(pUnk, phr );

	if (phr)
	{
		if (pNewFilter == NULL) 
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}
	return pNewFilter;

}

#endif //#if PLATFORM_WINDOWS
