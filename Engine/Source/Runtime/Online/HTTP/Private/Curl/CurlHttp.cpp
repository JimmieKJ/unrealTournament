// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"
#include "CurlHttp.h"
#include "EngineVersion.h"
#include "CurlHttpManager.h"

#if WITH_LIBCURL

// FCurlHttpRequest

FCurlHttpRequest::FCurlHttpRequest(CURLM * InMultiHandle)
	:	MultiHandle(InMultiHandle)
	,	EasyHandle(NULL)
	,	HeaderList(NULL)
	,	bCanceled(false)
	,	bCompleted(false)
	,	CurlCompletionResult(CURLE_OK)
	,	bEasyHandleAddedToMulti(false)
	,	BytesSent(0)
	,	CompletionStatus(EHttpRequestStatus::NotStarted)
	,	ElapsedTime(0.0f)
	,	TimeSinceLastResponse(0.0f)
{
	check(MultiHandle);
	if (MultiHandle)
	{
		EasyHandle = curl_easy_init();

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

		// set debug functions (FIXME: find a way to do it only if LogHttp is >= Verbose)
		curl_easy_setopt(EasyHandle, CURLOPT_DEBUGDATA, this);
		curl_easy_setopt(EasyHandle, CURLOPT_DEBUGFUNCTION, StaticDebugCallback);
		curl_easy_setopt(EasyHandle, CURLOPT_VERBOSE, 1L);

#endif // !UE_BUILD_SHIPPING && !UE_BUILD_TEST

		// set certificate verification (disable to allow self-signed certificates)
		if (FCurlHttpManager::CurlRequestOptions.bVerifyPeer)
		{
			curl_easy_setopt(EasyHandle, CURLOPT_SSL_VERIFYPEER, 1L);
		}
		else
		{
			curl_easy_setopt(EasyHandle, CURLOPT_SSL_VERIFYPEER, 0L);
		}

		// required for all multi-threaded handles
		curl_easy_setopt(EasyHandle, CURLOPT_NOSIGNAL, 1L);

		// associate with this just in case
		curl_easy_setopt(EasyHandle, CURLOPT_PRIVATE, this);

		if (FCurlHttpManager::CurlRequestOptions.bUseHttpProxy)
		{
			// guaranteed to be valid at this point
			curl_easy_setopt(EasyHandle, CURLOPT_PROXY, TCHAR_TO_ANSI(*FCurlHttpManager::CurlRequestOptions.HttpProxyAddress));
		}

		if (FCurlHttpManager::CurlRequestOptions.bDontReuseConnections)
		{
			curl_easy_setopt(EasyHandle, CURLOPT_FORBID_REUSE, 1L);
		}

		if (FCurlHttpManager::CurlRequestOptions.CertBundlePath)
		{
			curl_easy_setopt(EasyHandle, CURLOPT_CAINFO, FCurlHttpManager::CurlRequestOptions.CertBundlePath);
		}
	}
}

FCurlHttpRequest::~FCurlHttpRequest()
{
	if (EasyHandle)
	{
		// we remove handle earlier if request is canceled
		if (MultiHandle && bEasyHandleAddedToMulti)
		{
			curl_multi_remove_handle(MultiHandle, EasyHandle);
		}

		// cleanup the handle first (that order is used in howtos)
		curl_easy_cleanup(EasyHandle);

		// destroy headers list
		if (HeaderList)
		{
			curl_slist_free_all(HeaderList);
		}
	}
}

FString FCurlHttpRequest::GetURL()
{
	return URL;
}

FString FCurlHttpRequest::GetURLParameter(const FString& ParameterName)
{
	TArray<FString> StringElements;

	int32 NumElems = URL.ParseIntoArray(StringElements, TEXT("&"), true);
	check(NumElems == StringElements.Num());
	
	FString ParamValDelimiter(TEXT("="));
	for (int Idx = 0; Idx < NumElems; ++Idx )
	{
		FString Param, Value;
		if (StringElements[Idx].Split(ParamValDelimiter, &Param, &Value) && Param == ParameterName)
		{
			// unescape
			auto Converter = StringCast<ANSICHAR>(*Value);
			char * EscapedAnsi = (char *)Converter.Get();
			int32 EscapedLength = Converter.Length();

			int32 UnescapedLength = 0;	
			char * UnescapedAnsi = curl_easy_unescape(EasyHandle, EscapedAnsi, EscapedLength, &UnescapedLength);
			
			FString UnescapedValue(ANSI_TO_TCHAR(UnescapedAnsi));
			curl_free(UnescapedAnsi);
			
			return UnescapedValue;
		}
	}

	return FString();
}

FString FCurlHttpRequest::GetHeader(const FString& HeaderName)
{
	FString Result;

	FString* Header = Headers.Find(HeaderName);
	if (Header != NULL)
	{
		Result = *Header;
	}
	
	return Result;
}

TArray<FString> FCurlHttpRequest::GetAllHeaders()
{
	TArray<FString> Result;
	for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
	{
		Result.Add(It.Key() + TEXT(": ") + It.Value());
	}
	return Result;
}

FString FCurlHttpRequest::GetContentType()
{
	return GetHeader(TEXT( "Content-Type" ));
}

int32 FCurlHttpRequest::GetContentLength()
{
	return RequestPayload.Num();
}

const TArray<uint8>& FCurlHttpRequest::GetContent()
{
	return RequestPayload;
}

void FCurlHttpRequest::SetVerb(const FString& InVerb)
{
	check(EasyHandle);

	Verb = InVerb.ToUpper();
}

void FCurlHttpRequest::SetURL(const FString& InURL)
{
	check(EasyHandle);
	URL = InURL;
}

void FCurlHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	RequestPayload = ContentPayload;
}

void FCurlHttpRequest::SetContentAsString(const FString& ContentString)
{
	FTCHARToUTF8 Converter(*ContentString);
	RequestPayload.SetNum(Converter.Length());
	FMemory::Memcpy(RequestPayload.GetData(), (uint8*)(ANSICHAR*)Converter.Get(), RequestPayload.Num());
}

void FCurlHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	Headers.Add(HeaderName, HeaderValue);
}

FString FCurlHttpRequest::GetVerb()
{
	return Verb;
}

size_t FCurlHttpRequest::StaticUploadCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void* UserData)
{
	check(Ptr);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->UploadCallback(Ptr, SizeInBlocks, BlockSizeInBytes);
}

size_t FCurlHttpRequest::StaticReceiveResponseHeaderCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void* UserData)
{
	check(Ptr);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->ReceiveResponseHeaderCallback(Ptr, SizeInBlocks, BlockSizeInBytes);	
}

size_t FCurlHttpRequest::StaticReceiveResponseBodyCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes, void* UserData)
{
	check(Ptr);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->ReceiveResponseBodyCallback(Ptr, SizeInBlocks, BlockSizeInBytes);	
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
size_t FCurlHttpRequest::StaticDebugCallback(CURL * Handle, curl_infotype DebugInfoType, char * DebugInfo, size_t DebugInfoSize, void* UserData)
{
	check(Handle);
	check(UserData);

	// dispatch
	FCurlHttpRequest* Request = reinterpret_cast<FCurlHttpRequest*>(UserData);
	return Request->DebugCallback(Handle, DebugInfoType, DebugInfo, DebugInfoSize);
}
#endif // #if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

size_t FCurlHttpRequest::ReceiveResponseHeaderCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes)
{
	check(Response.IsValid());

	if (Response.IsValid())
	{
		TimeSinceLastResponse = 0.0f;

		uint32 HeaderSize = SizeInBlocks * BlockSizeInBytes;
		if (HeaderSize > 0 && HeaderSize <= CURL_MAX_HTTP_HEADER)
		{
			TArray<char> AnsiHeader;
			AnsiHeader.AddUninitialized(HeaderSize + 1);

			FMemory::Memcpy(AnsiHeader.GetData(), Ptr, HeaderSize);
			AnsiHeader[HeaderSize] = 0;

			FString Header(ANSI_TO_TCHAR(AnsiHeader.GetData()));
			// kill \n\r
			Header = Header.Replace(TEXT("\n"), TEXT(""));
			Header = Header.Replace(TEXT("\r"), TEXT(""));

			UE_LOG(LogHttp, Verbose, TEXT("%p: Received response header '%s'."), this, *Header);

			FString HeaderName, Param;
			if (Header.Split(TEXT(": "), &HeaderName, &Param))
			{
				Response->Headers.Add(HeaderName, Param);
			}
			return HeaderSize;
		}
		else
		{
			UE_LOG(LogHttp, Warning, TEXT("%p: Could not process response header for request - header size (%d) is invalid."), this, HeaderSize);
		}
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("%p: Could not download response header for request - response not valid."), this);
	}

	return 0;
}

size_t FCurlHttpRequest::ReceiveResponseBodyCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes)
{
	check(Response.IsValid());

	if (Response.IsValid())
	{
		TimeSinceLastResponse = 0.0f;

		uint32 SizeToDownload = SizeInBlocks * BlockSizeInBytes;

		UE_LOG(LogHttp, Verbose, TEXT("%p: ReceiveResponseBodyCallback: %d bytes out of %d received. (SizeInBlocks=%d, BlockSizeInBytes=%d, Response->TotalBytesRead=%d, Response->GetContentLength()=%d, SizeToDownload=%d (<-this will get returned from the callback))"),
			this,
			static_cast<int32>(Response->TotalBytesRead + SizeToDownload), Response->GetContentLength(),
			static_cast<int32>(SizeInBlocks), static_cast<int32>(BlockSizeInBytes), Response->TotalBytesRead, Response->GetContentLength(), static_cast<int32>(SizeToDownload)
			);

		// note that we can be passed 0 bytes if file transmitted has 0 length
		if (SizeToDownload > 0)
		{
			Response->Payload.AddUninitialized(SizeToDownload);

			// save
			FMemory::Memcpy( static_cast< uint8* >( Response->Payload.GetData() ) + Response->TotalBytesRead, Ptr, SizeToDownload );
			Response->TotalBytesRead += SizeToDownload;

			// Update response progress
			OnRequestProgress().ExecuteIfBound(SharedThis(this), BytesSent, Response->TotalBytesRead);

			return SizeToDownload;
		}
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("%p: Could not download response data for request - response not valid."), this);
	}

	return 0;	// request will fail with write error if we had non-zero bytes to download
}

size_t FCurlHttpRequest::UploadCallback(void* Ptr, size_t SizeInBlocks, size_t BlockSizeInBytes)
{
	TimeSinceLastResponse = 0.0f;

	size_t SizeToSend = RequestPayload.Num() - BytesSent;
	size_t SizeToSendThisTime = 0;

	if (SizeToSend != 0)
	{
		OnRequestProgress().ExecuteIfBound(SharedThis(this), BytesSent, 0);

		SizeToSendThisTime = FMath::Min(SizeToSend, SizeInBlocks * BlockSizeInBytes);
		if (SizeToSendThisTime != 0)
		{
			// static cast just ensures that this is uint8* in fact
			FMemory::Memcpy(Ptr, static_cast< uint8* >( RequestPayload.GetData() ) + BytesSent, SizeToSendThisTime);
			BytesSent += SizeToSendThisTime;
		}
	}

	UE_LOG(LogHttp, Verbose, TEXT("%p: UploadCallback: %d bytes out of %d sent. (SizeInBlocks=%d, BlockSizeInBytes=%d, RequestPayload.Num()=%d, BytesSent=%d, SizeToSend=%d, SizeToSendThisTime=%d (<-this will get returned from the callback))"), 
		this,
		static_cast< int32 >(BytesSent), RequestPayload.Num(),
		static_cast< int32 >(SizeInBlocks), static_cast< int32 >(BlockSizeInBytes), RequestPayload.Num(), static_cast< int32 >(BytesSent), static_cast< int32 >(SizeToSend),
		static_cast< int32 >(SizeToSendThisTime)
		);

	return SizeToSendThisTime;
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
size_t FCurlHttpRequest::DebugCallback(CURL * Handle, curl_infotype DebugInfoType, char * DebugInfo, size_t DebugInfoSize)
{
	check(Handle == EasyHandle);	// make sure it's us

	switch(DebugInfoType)
	{
		case CURLINFO_TEXT:
			{
				// in this case DebugInfo is a C string (see http://curl.haxx.se/libcurl/c/debug.html)
				FString DebugText(ANSI_TO_TCHAR(DebugInfo));
				DebugText.ReplaceInline(TEXT("\n"), TEXT(""), ESearchCase::CaseSensitive);
				DebugText.ReplaceInline(TEXT("\r"), TEXT(""), ESearchCase::CaseSensitive);
				UE_LOG(LogHttp, VeryVerbose, TEXT("%p: '%s'"), this, *DebugText);
			}
			break;

		case CURLINFO_HEADER_IN:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Received header (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_HEADER_OUT:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Sent header (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_DATA_IN:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Received data (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_DATA_OUT:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Sent data (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_SSL_DATA_IN:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Received SSL data (%d bytes)"), this, DebugInfoSize);
			break;

		case CURLINFO_SSL_DATA_OUT:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: Sent SSL data (%d bytes)"), this, DebugInfoSize);
			break;

		default:
			UE_LOG(LogHttp, VeryVerbose, TEXT("%p: DebugCallback: Unknown DebugInfoType=%d (DebugInfoSize: %d bytes)"), this, (int32)DebugInfoType, DebugInfoSize);
			break;
	}
	
	return 0;
}

#endif // !UE_BUILD_SHIPPING && !UE_BUILD_TEST

bool IsURLEncoded(const TArray<uint8> & Payload)
{
	static char AllowedChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
	static bool bTableFilled = false;
	static bool AllowedTable[256] = { false };

	if (!bTableFilled)
	{
		for (int32 Idx = 0; Idx < ARRAY_COUNT(AllowedChars) - 1; ++Idx)	// -1 to avoid trailing 0
		{
			uint8 AllowedCharIdx = static_cast<uint8>(AllowedChars[Idx]);
			check(AllowedCharIdx < ARRAY_COUNT(AllowedTable));
			AllowedTable[AllowedCharIdx] = true;
		}

		bTableFilled = true;
	}

	const int32 Num = Payload.Num();
	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		if (!AllowedTable[Payload[Idx]])
			return false;
	}

	return true;
}

bool FCurlHttpRequest::StartRequest()
{
	check(EasyHandle);

	bCompleted = false;
	bCanceled = false;

	curl_slist_free_all(HeaderList);
	HeaderList = nullptr;

	// default no verb to a GET
	if (Verb.IsEmpty())
	{
		Verb = TEXT("GET");
	}

	UE_LOG(LogHttp, Verbose, TEXT("%p: URL='%s'"), this, *URL);
	UE_LOG(LogHttp, Verbose, TEXT("%p: Verb='%s'"), this, *Verb);
	UE_LOG(LogHttp, Verbose, TEXT("%p: Custom headers are %s"), this, Headers.Num() ? TEXT("present") : TEXT("NOT present"));
	UE_LOG(LogHttp, Verbose, TEXT("%p: Payload size=%d"), this, RequestPayload.Num());

	// set up URL
	// Disabled http request processing
	if (!FHttpModule::Get().IsHttpEnabled())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Http disabled. Skipping request. url=%s"), *GetURL());
		return false;
	}
	// Prevent overlapped requests using the same instance
	else if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
		return false;
	}
	// Nothing to do without a valid URL
	else if (URL.IsEmpty())
	{
		UE_LOG(LogHttp, Log, TEXT("Cannot process HTTP request: URL is empty"));
		return false;
	}	

	curl_easy_setopt(EasyHandle, CURLOPT_URL, TCHAR_TO_ANSI(*URL));

	// set up verb (note that Verb is expected to be uppercase only)
	if (Verb == TEXT("POST"))
	{
		curl_easy_setopt(EasyHandle, CURLOPT_POST, 1L);

		// If we don't pass any other Content-Type, RequestPayload is assumed to be URL-encoded by this time
		// (if we pass, don't check here and trust the request)
		check(!GetHeader("Content-Type").IsEmpty() || IsURLEncoded(RequestPayload));

		curl_easy_setopt(EasyHandle, CURLOPT_POSTFIELDS, RequestPayload.GetData());
		curl_easy_setopt(EasyHandle, CURLOPT_POSTFIELDSIZE, RequestPayload.Num());
	}
	else if (Verb == TEXT("PUT"))
	{
		curl_easy_setopt(EasyHandle, CURLOPT_UPLOAD, 1L);
		// this pointer will be passed to read function
		curl_easy_setopt(EasyHandle, CURLOPT_READDATA, this);
		curl_easy_setopt(EasyHandle, CURLOPT_READFUNCTION, StaticUploadCallback);
		curl_easy_setopt(EasyHandle, CURLOPT_INFILESIZE, RequestPayload.Num());

		// reset the counter
		BytesSent = 0;
	}
	else if (Verb == TEXT("GET"))
	{
		// technically might not be needed unless we reuse the handles
		curl_easy_setopt(EasyHandle, CURLOPT_HTTPGET, 1L);
	}
	else if (Verb == TEXT("HEAD"))
	{
		curl_easy_setopt(EasyHandle, CURLOPT_NOBODY, 1L);
	}
	else if (Verb == TEXT("DELETE"))
	{
		curl_easy_setopt(EasyHandle, CURLOPT_CUSTOMREQUEST, "DELETE");

		// If we don't pass any other Content-Type, RequestPayload is assumed to be URL-encoded by this time
		// (if we pass, don't check here and trust the request)
		check(!GetHeader("Content-Type").IsEmpty() || IsURLEncoded(RequestPayload));

		curl_easy_setopt(EasyHandle, CURLOPT_POSTFIELDS, RequestPayload.GetData());
		curl_easy_setopt(EasyHandle, CURLOPT_POSTFIELDSIZE, RequestPayload.Num());
	}
	else
	{
		UE_LOG(LogHttp, Fatal, TEXT("Unsupported verb '%s', can be perhaps added with CURLOPT_CUSTOMREQUEST"), *Verb);
		FPlatformMisc::DebugBreak();
	}

	// set up header function to receive response headers
	curl_easy_setopt(EasyHandle, CURLOPT_HEADERDATA, this);
	curl_easy_setopt(EasyHandle, CURLOPT_HEADERFUNCTION, StaticReceiveResponseHeaderCallback);

	// set up write function to receive response payload
	curl_easy_setopt(EasyHandle, CURLOPT_WRITEDATA, this);
	curl_easy_setopt(EasyHandle, CURLOPT_WRITEFUNCTION, StaticReceiveResponseBodyCallback);

	// set up headers
	if (GetHeader("User-Agent").IsEmpty())
	{
		SetHeader(TEXT("User-Agent"), FString::Printf(TEXT("game=%s, engine=UE4, version=%s"), FApp::GetGameName(), *GEngineVersion.ToString()));
	}

	// content-length should be present http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.4
	if (GetHeader("Content-Length").IsEmpty())
	{
		SetHeader(TEXT("Content-Length"), FString::Printf(TEXT("%d"), RequestPayload.Num()));
	}

	// Add "Pragma: no-cache" to mimic WinInet behavior
	if (GetHeader("Pragma").IsEmpty())
	{
		SetHeader(TEXT("Pragma"), TEXT("no-cache"));
	}

	// Remove "Expect: 100-continue" since this is supposed to cause problematic behavior on Amazon ELB (and WinInet doesn't send that either)
	// (also see http://www.iandennismiller.com/posts/curl-http1-1-100-continue-and-multipartform-data-post.html , http://the-stickman.com/web-development/php-and-curl-disabling-100-continue-header/ )
	if (GetHeader("Expect").IsEmpty())
	{
		SetHeader(TEXT("Expect"), TEXT(""));
	}

	TArray<FString> AllHeaders = GetAllHeaders();
	const int32 NumAllHeaders = AllHeaders.Num();
	for (int32 Idx = 0; Idx < NumAllHeaders; ++Idx)
	{
		UE_LOG(LogHttp, Verbose, TEXT("%p: Adding header '%s'"), this, *AllHeaders[Idx] );
		HeaderList = curl_slist_append(HeaderList, TCHAR_TO_UTF8(*AllHeaders[Idx]));
	}

	if (HeaderList)
	{
		curl_easy_setopt(EasyHandle, CURLOPT_HTTPHEADER, HeaderList);
	}

	// add the handle for real processing
	bEasyHandleAddedToMulti = (curl_multi_add_handle(MultiHandle, EasyHandle) == 0);

	return bEasyHandleAddedToMulti;
}

bool FCurlHttpRequest::ProcessRequest()
{
	check(EasyHandle);

	if (!StartRequest())
	{
		UE_LOG(LogHttp, Warning, TEXT("Could not set libcurl options for easy handle, processing HTTP request failed. Increase verbosity for additional information."));

		// No response since connection failed
		Response = NULL;
		// Cleanup and call delegate
		FinishedRequest();

		return false;
	}

	// Mark as in-flight to prevent overlapped requests using the same object
	CompletionStatus = EHttpRequestStatus::Processing;
	// Response object to handle data that comes back after starting this request
	Response = MakeShareable(new FCurlHttpResponse(*this));
	// Add to global list while being processed so that the ref counted request does not get deleted
	FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
	// reset timeout
	ElapsedTime = 0.0f;
	
	UE_LOG(LogHttp, Verbose, TEXT("%p: request (easy handle:%p) has been added to multi handle for processing"), this, EasyHandle );

	return true;
}

FHttpRequestCompleteDelegate& FCurlHttpRequest::OnProcessRequestComplete()
{
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FCurlHttpRequest::OnRequestProgress()
{
	return RequestProgressDelegate;
}

void FCurlHttpRequest::CancelRequest()
{
	bCanceled = true;
    
    // Cleanup cancel request and fire off any necessary delegates.
    FinishedRequest();
}

EHttpRequestStatus::Type FCurlHttpRequest::GetStatus()
{
	return CompletionStatus;
}

const FHttpResponsePtr FCurlHttpRequest::GetResponse() const
{
	return Response;
}

void FCurlHttpRequest::Tick(float DeltaSeconds)
{
	// keep track of elapsed seconds
	ElapsedTime += DeltaSeconds;
	TimeSinceLastResponse += DeltaSeconds;

	// check for true completion/cancellation
	if (bCompleted && 
		ElapsedTime >= FHttpModule::Get().GetHttpDelayTime())
	{
		FinishedRequest();
	}
	else if (bCanceled)
	{
		FinishedRequest();
	}
	else
	{
		const float HttpTimeout = FHttpModule::Get().GetHttpTimeout();
		if (HttpTimeout > 0 && TimeSinceLastResponse >= HttpTimeout)
		{
			UE_LOG(LogHttp, Warning, TEXT("Timeout processing Http request. %p"),
				this);

			// finish it off since it is timeout
			FinishedRequest();
		}
	}
}

void FCurlHttpRequest::FinishedRequest()
{
	// if completed, get more info
	if (bCompleted)
	{
		if (Response.IsValid())
		{
			Response->bSucceeded = (CURLE_OK == CurlCompletionResult);

			// get the information
			long HttpCode = 0;
			if (0 == curl_easy_getinfo(EasyHandle, CURLINFO_RESPONSE_CODE, &HttpCode))
			{
				Response->HttpCode = HttpCode;
			}

			double ContentLengthDownload = 0.0;
			if (0 == curl_easy_getinfo(EasyHandle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &ContentLengthDownload))
			{
				Response->ContentLength = static_cast< int32 >(ContentLengthDownload);
			}
		}
	}
	
	// if just finished, mark as stopped async processing
	if (Response.IsValid())
	{
		Response->bIsReady = true;
	}

	// Clean up session/request handles that may have been created
	CleanupRequest();

	if (Response.IsValid() &&
		Response->bSucceeded)
	{
		UE_LOG(LogHttp, Verbose, TEXT("%p: request has been successfully processed. HTTP code: %d, content length: %d, actual payload size: %d"), 
			this, Response->HttpCode, Response->ContentLength, Response->Payload.Num() );

		// Mark last request attempt as completed successfully
		CompletionStatus = EHttpRequestStatus::Succeeded;
		// Call delegate with valid request/response objects
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),Response,true);
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("%p: request failed, libcurl error: %d (%s)"), this, (int32)CurlCompletionResult, ANSI_TO_TCHAR(curl_easy_strerror(CurlCompletionResult)));

		// Mark last request attempt as completed but failed
		CompletionStatus = EHttpRequestStatus::Failed;
		// No response since connection failed
		Response = NULL;
		// Call delegate with failure
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this),NULL,false);
	}
	// Remove from global list since processing is now complete
	FHttpModule::Get().GetHttpManager().RemoveRequest(SharedThis(this));
}

void FCurlHttpRequest::CleanupRequest()
{
	// remove the handle from multi
	if (curl_multi_remove_handle(MultiHandle, EasyHandle) == 0)
	{
		bEasyHandleAddedToMulti = false;
	}
}

float FCurlHttpRequest::GetElapsedTime()
{
	return ElapsedTime;
}


// FCurlHttpRequest

FCurlHttpResponse::FCurlHttpResponse(FCurlHttpRequest& InRequest)
	:	Request(InRequest)
	,	TotalBytesRead(0)
	,	HttpCode(EHttpResponseCodes::Unknown)
	,	ContentLength(0)
	,	bIsReady(0)
	,	bSucceeded(0)
{
}

FCurlHttpResponse::~FCurlHttpResponse()
{	
}

FString FCurlHttpResponse::GetURL()
{
	return Request.GetURL();
}

FString FCurlHttpResponse::GetURLParameter(const FString& ParameterName)
{
	return Request.GetURLParameter(ParameterName);
}

FString FCurlHttpResponse::GetHeader(const FString& HeaderName)
{
	FString Result(TEXT(""));
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached header [%s]. Response still processing. %p"),
			*HeaderName, &Request);
	}
	else
	{
		FString* Header = Headers.Find(HeaderName);
		if (Header != NULL)
		{
			return *Header;
		}
	}
	return Result;
}

TArray<FString> FCurlHttpResponse::GetAllHeaders()	
{
	TArray<FString> Result;
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Can't get cached headers. Response still processing. %p"),&Request);
	}
	else
	{
		for (TMap<FString, FString>::TConstIterator It(Headers); It; ++It)
		{
			Result.Add(It.Key() + TEXT(": ") + It.Value());
		}
	}
	return Result;
}

FString FCurlHttpResponse::GetContentType()
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FCurlHttpResponse::GetContentLength()
{
	return ContentLength;
}

const TArray<uint8>& FCurlHttpResponse::GetContent()
{
	if (!bIsReady)
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"),&Request);
	}
	return Payload;
}

int32 FCurlHttpResponse::GetResponseCode()
{
	return HttpCode;
}

FString FCurlHttpResponse::GetContentAsString()
{
	TArray<uint8> ZeroTerminatedPayload(GetContent());
	ZeroTerminatedPayload.Add(0);
	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}

#endif //WITH_LIBCURL
