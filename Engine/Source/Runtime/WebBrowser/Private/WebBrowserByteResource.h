// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif

#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
#include "include/cef_resource_handler.h"
#pragma pop_macro("OVERRIDE")

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformTypes.h"
#endif


/**
 * Implements a resource handler that will return the contents of a string as the result.
 */
class FWebBrowserByteResource
	: public CefResourceHandler
{
public:
	/**
	 */
	FWebBrowserByteResource(const void* InBuffer, int32 InSize) : Position(0), Size(InSize)
	{
		Buffer = new unsigned char[Size];
		FMemory::Memcpy(Buffer, InBuffer, Size);
	}
	
	~FWebBrowserByteResource()
	{
		delete[] Buffer;
	}
	
	// CefResourceHandler interface
	virtual void Cancel() override;
	virtual void GetResponseHeaders(CefRefPtr<CefResponse> Response, int64& ResponseLength, CefString& RedirectUrl) override;
	virtual bool ProcessRequest(CefRefPtr<CefRequest> Request, CefRefPtr<CefCallback> Callback) override;
	virtual bool ReadResponse(void* DataOut, int BytesToRead, int& BytesRead, CefRefPtr<CefCallback> Callback) override;
	
private:
	int32 Position;
	int32 Size;
	unsigned char* Buffer;
	
	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FWebBrowserByteResource);
};


#endif
