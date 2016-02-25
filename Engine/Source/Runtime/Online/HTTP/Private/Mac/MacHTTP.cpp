// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "HttpPrivatePCH.h"
#include "MacHTTP.h"
#include "EngineVersion.h"
#include "Security/Security.h"

/****************************************************************************
 * FMacHttpRequest implementation
 ***************************************************************************/


FMacHttpRequest::FMacHttpRequest()
:	Connection(nullptr)
,	CompletionStatus(EHttpRequestStatus::NotStarted)
,	ProgressBytesSent(0)
,	StartRequestTime(0.0)
,	ElapsedTime(0.0f)
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::FMacHttpRequest()"));
	Request = [[NSMutableURLRequest alloc] init];
	Request.timeoutInterval = FHttpModule::Get().GetHttpTimeout();
}


FMacHttpRequest::~FMacHttpRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::~FMacHttpRequest()"));
	check(Connection == nullptr);
	[Request release];
}


FString FMacHttpRequest::GetURL()
{
	SCOPED_AUTORELEASE_POOL;
	NSURL* URL = Request.URL;
	if (URL != nullptr)
	{
		FString ConvertedURL(URL.absoluteString);
		UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetURL() - %s"), *ConvertedURL);
		return ConvertedURL;
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetURL() - NULL"));
		return FString();
	}
}

void FMacHttpRequest::SetURL(const FString& URL)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::SetURL() - %s"), *URL);
	Request.URL = [NSURL URLWithString: URL.GetNSString()];
}


FString FMacHttpRequest::GetURLParameter(const FString& ParameterName)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetURLParameter() - %s"), *ParameterName);

	NSString* ParameterNameStr = ParameterName.GetNSString();
	NSArray* Parameters = [Request.URL.query componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters)
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = KeyValue[0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame)
		{
			return FString(KeyValue[1]);
		}
	}
	return FString();
}


FString FMacHttpRequest::GetHeader(const FString& HeaderName)
{
	SCOPED_AUTORELEASE_POOL;
	FString Header([Request valueForHTTPHeaderField:HeaderName.GetNSString()]);
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetHeader() - %s"), *Header);
	return Header;
}


void FMacHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::SetHeader() - %s / %s"), *HeaderName, *HeaderValue );
	[Request setValue: HeaderValue.GetNSString() forHTTPHeaderField: HeaderName.GetNSString()];
}


TArray<FString> FMacHttpRequest::GetAllHeaders()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetAllHeaders()"));
	NSDictionary* Headers = Request.allHTTPHeaderFields;
	TArray<FString> Result;
	Result.Reserve(Headers.count);
	for (NSString* Key in Headers.allKeys)
	{
		FString ConvertedValue(Headers[Key]);
		FString ConvertedKey(Key);
		UE_LOG(LogHttp, Verbose, TEXT("Header= %s, Key= %s"), *ConvertedValue, *ConvertedKey);

		Result.Add( FString::Printf( TEXT("%s: %s"), *ConvertedKey, *ConvertedValue ) );
	}
	return Result;
}


const TArray<uint8>& FMacHttpRequest::GetContent()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetContent()"));
	NSData* Body = Request.HTTPBody; // accessing HTTPBody will call copy on the value, increasing its retain count
	RequestPayload.Empty();
	RequestPayload.Append((const uint8*)Body.bytes, Body.length);
	return RequestPayload;
}


void FMacHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::SetContent()"));
	Request.HTTPBody = [NSData dataWithBytes:ContentPayload.GetData() length:ContentPayload.Num()];
}


FString FMacHttpRequest::GetContentType()
{
	FString ContentType = GetHeader(TEXT("Content-Type"));
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetContentType() - %s"), *ContentType);
	return ContentType;
}


int32 FMacHttpRequest::GetContentLength()
{
	SCOPED_AUTORELEASE_POOL;
	NSData* Body = Request.HTTPBody;
	int Len = Body.length;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetContentLength() - %i"), Len);
	return Len;
}


void FMacHttpRequest::SetContentAsString(const FString& ContentString)
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::SetContentAsString() - %s"), *ContentString);
	FTCHARToUTF8 Converter(*ContentString);
	
	// The extra length computation here is unfortunate, but it's technically not safe to assume the length is the same.
	Request.HTTPBody = [NSData dataWithBytes:(ANSICHAR*)Converter.Get() length:Converter.Length()];
}


FString FMacHttpRequest::GetVerb()
{
	FString ConvertedVerb(Request.HTTPMethod);
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetVerb() - %s"), *ConvertedVerb);
	return ConvertedVerb;
}


void FMacHttpRequest::SetVerb(const FString& Verb)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::SetVerb() - %s"), *Verb);
	Request.HTTPMethod = Verb.GetNSString();
}


bool FMacHttpRequest::ProcessRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::ProcessRequest()"));
	bool bStarted = false;

	FString Scheme(Request.URL.scheme);
	Scheme = Scheme.ToLower();

	// Prevent overlapped requests using the same instance
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
	}
	else if(GetURL().Len() == 0)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
	}
	else if( Scheme != TEXT("http") && Scheme != TEXT("https"))
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not a valid HTTP request. %p"), *GetURL(), this);
	}
	else
	{
		bStarted = StartRequest();
	}

	if( !bStarted )
	{
		FinishedRequest();
	}

	return bStarted;
}


FHttpRequestCompleteDelegate& FMacHttpRequest::OnProcessRequestComplete()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FMacHttpRequest::OnRequestProgress() 
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FMacHttpRequest::OnRequestProgress()"));
	return RequestProgressDelegate;
}


bool FMacHttpRequest::StartRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::StartRequest()"));
	bool bStarted = false;

	// set the content-length and user-agent
	if(GetContentLength() > 0)
	{
		[Request setValue:[NSString stringWithFormat:@"%d", GetContentLength()] forHTTPHeaderField:@"Content-Length"];
	}

	const FString UserAgent = GetHeader("User-Agent");
	if(UserAgent.IsEmpty())
	{
		NSString* Tag = FString::Printf(TEXT("UE4-%s,UE4Ver(%s)"), FApp::GetGameName(), *FEngineVersion::Current().ToString()).GetNSString();
		[Request addValue:Tag forHTTPHeaderField:@"User-Agent"];
	}
	else
	{
		[Request addValue:UserAgent.GetNSString() forHTTPHeaderField:@"User-Agent"];
	}

	Response = MakeShareable( new FMacHttpResponse( *this ) );

	// Create the connection, tell it to run in the main run loop, and kick it off.
	Connection = [[NSURLConnection alloc] initWithRequest:Request delegate:Response->ResponseWrapper startImmediately:NO];
	if( Connection != nullptr && Response->ResponseWrapper != nullptr )
	{
		CompletionStatus = EHttpRequestStatus::Processing;
		[Connection scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
		[Connection start];
		UE_LOG(LogHttp, Verbose, TEXT("[Connection start]"));

		bStarted = true;
		// Add to global list while being processed so that the ref counted request does not get deleted
		FHttpModule::Get().GetHttpManager().AddRequest(SharedThis(this));
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Could not initialize Internet connection."));
		CompletionStatus = EHttpRequestStatus::Failed_ConnectionError;
	}
	StartRequestTime = FPlatformTime::Seconds();
	// reset the elapsed time.
	ElapsedTime = 0.0f;

	return bStarted;
}

void FMacHttpRequest::FinishedRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::FinishedRequest()"));
	ElapsedTime = (float)(FPlatformTime::Seconds() - StartRequestTime);
	if( Response.IsValid() && Response->IsReady() && !Response->HadError())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Request succeeded"));
		CompletionStatus = EHttpRequestStatus::Succeeded;

		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), Response, true);
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("Request failed"));
		FString URL([[Request URL] absoluteString]);
		CompletionStatus = EHttpRequestStatus::Failed;

		Response = nullptr;
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), nullptr, false);
	}

	// Clean up session/request handles that may have been created
	CleanupRequest();

	// Remove from global list since processing is now complete
	if (FHttpModule::Get().GetHttpManager().IsValidRequest(this))
	{
		FHttpModule::Get().GetHttpManager().RemoveRequest(SharedThis(this));
	}
}


void FMacHttpRequest::CleanupRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::CleanupRequest()"));

	if(Connection != nullptr)
	{
		[Connection release];
		Connection = nullptr;
	}
}


void FMacHttpRequest::CancelRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::CancelRequest()"));
	if(Connection != nullptr)
	{
		[Connection cancel];
	}
	FinishedRequest();
}


EHttpRequestStatus::Type FMacHttpRequest::GetStatus()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpRequest::GetStatus()"));
	return CompletionStatus;
}


const FHttpResponsePtr FMacHttpRequest::GetResponse() const
{
	return Response;
}


void FMacHttpRequest::Tick(float DeltaSeconds)
{
	if( CompletionStatus == EHttpRequestStatus::Processing || Response->HadError() )
	{
		if (OnRequestProgress().IsBound())
		{
			const int32 BytesWritten = Response->GetNumBytesWritten();
			const int32 BytesRead = Response->GetNumBytesReceived();
			if (BytesWritten > 0 || BytesRead > 0)
			{
				OnRequestProgress().Execute(SharedThis(this), BytesWritten, BytesRead);
			}
		}
		if( Response->IsReady() )
		{
			FinishedRequest();
		}
	}
}

float FMacHttpRequest::GetElapsedTime()
{
	return ElapsedTime;
}


/****************************************************************************
 * FHttpResponseMacWrapper implementation
 ***************************************************************************/

@implementation FHttpResponseMacWrapper
@synthesize Response;
@synthesize bIsReady;
@synthesize bHadError;
@synthesize BytesWritten;


-(FHttpResponseMacWrapper*) init
{
	UE_LOG(LogHttp, Verbose, TEXT("-(FHttpResponseMacWrapper*) init"));
	self = [super init];
	bIsReady = false;
	
	return self;
}


-(void) connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite
{
	UE_LOG(LogHttp, Verbose, TEXT("didSendBodyData:(NSInteger)bytesWritten totalBytes:Written:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite"));
	self.BytesWritten = totalBytesWritten;
	UE_LOG(LogHttp, Verbose, TEXT("didSendBodyData: totalBytesWritten = %d, totalBytesExpectedToWrite = %d: %p"), totalBytesWritten, totalBytesExpectedToWrite, self);
}

-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse:(NSURLResponse *)response"));
	self.Response = (NSHTTPURLResponse*)response;
	
	// presize the payload container if possible
	Payload.Empty();
	Payload.Reserve([response expectedContentLength] != NSURLResponseUnknownLength ? [response expectedContentLength] : 0);
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse: expectedContentLength = %d. Length = %d: %p"), [response expectedContentLength], Payload.Max(), self);
}


-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	Payload.Append((const uint8*)[data bytes], [data length]);
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveData with %d bytes. After Append, Payload Length = %d: %p"), [data length], Payload.Num(), self);
}


-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	self.bIsReady = YES;
	self.bHadError = YES;
	UE_LOG(LogHttp, Warning, TEXT("didFailWithError. Http request failed - %s %s: %p"), 
		*FString([error localizedDescription]),
		*FString([[error userInfo] objectForKey:NSURLErrorFailingURLStringErrorKey]),
		self);
	// Log more details if verbose logging is enabled and this is an SSL error
	if (UE_LOG_ACTIVE(LogHttp, Verbose))
	{
		SecTrustRef PeerTrustInfo = reinterpret_cast<SecTrustRef>([[error userInfo] objectForKey:NSURLErrorFailingURLPeerTrustErrorKey]);
		if (PeerTrustInfo != nullptr)
		{
			SecTrustResultType TrustResult = 0;
			SecTrustGetTrustResult(PeerTrustInfo, &TrustResult);

			FString TrustResultString;
			switch (TrustResult)
			{
#define MAP_TO_RESULTSTRING(Constant) case Constant: TrustResultString = TEXT(#Constant); break;
			MAP_TO_RESULTSTRING(kSecTrustResultInvalid)
			MAP_TO_RESULTSTRING(kSecTrustResultProceed)
			MAP_TO_RESULTSTRING(kSecTrustResultDeny)
			MAP_TO_RESULTSTRING(kSecTrustResultUnspecified)
			MAP_TO_RESULTSTRING(kSecTrustResultRecoverableTrustFailure)
			MAP_TO_RESULTSTRING(kSecTrustResultFatalTrustFailure)
			MAP_TO_RESULTSTRING(kSecTrustResultOtherError)
#undef MAP_TO_RESULTSTRING
			default:
				TrustResultString = TEXT("unknown");
				break;
			}
			UE_LOG(LogHttp, Verbose, TEXT("didFailWithError. SSL trust result: %s (%d)"), *TrustResultString, TrustResult);
		}
	}
}


-(void) connectionDidFinishLoading:(NSURLConnection *)connection
{
	UE_LOG(LogHttp, Verbose, TEXT("connectionDidFinishLoading: %p"), self);
	self.bIsReady = YES;
}

- (TArray<uint8>&)getPayload
{
	return Payload;
}

-(int32)getBytesWritten
{
	return self.BytesWritten;
}

@end




/****************************************************************************
 * FMacHTTPResponse implementation
 **************************************************************************/

FMacHttpResponse::FMacHttpResponse(const FMacHttpRequest& InRequest)
	: Request( InRequest )
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::FMacHttpResponse()"));
	ResponseWrapper = [[FHttpResponseMacWrapper alloc] init];
}


FMacHttpResponse::~FMacHttpResponse()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::~FMacHttpResponse()"));
	[ResponseWrapper getPayload].Empty();

	[ResponseWrapper release];
	ResponseWrapper = nil;
}


FString FMacHttpResponse::GetURL()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetURL()"));
	return FString(Request.Request.URL.query);
}


FString FMacHttpResponse::GetURLParameter(const FString& ParameterName)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetURLParameter()"));

	NSString* ParameterNameStr = ParameterName.GetNSString();
	NSArray* Parameters = [[[Request.Request URL] query] componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters)
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = [KeyValue objectAtIndex:0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame)
		{
			return FString([[KeyValue objectAtIndex:1] stringByRemovingPercentEncoding]);
		}
	}
	return FString();
}


FString FMacHttpResponse::GetHeader(const FString& HeaderName)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetHeader()"));

	NSString* ConvertedHeaderName = HeaderName.GetNSString();
	return FString([[[ResponseWrapper Response] allHeaderFields] objectForKey:ConvertedHeaderName]);
}


TArray<FString> FMacHttpResponse::GetAllHeaders()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetAllHeaders()"));

	NSDictionary* Headers = [GetResponseObj() allHeaderFields];
	TArray<FString> Result;
	Result.Reserve([Headers count]);
	for (NSString* Key in [Headers allKeys])
	{
		FString ConvertedValue([Headers objectForKey:Key]);
		FString ConvertedKey(Key);
		Result.Add( FString::Printf( TEXT("%s: %s"), *ConvertedKey, *ConvertedValue ) );
	}
	return Result;
}


FString FMacHttpResponse::GetContentType()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetContentType()"));

	return GetHeader( TEXT( "Content-Type" ) );
}


int32 FMacHttpResponse::GetContentLength()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetContentLength()"));

	return [ResponseWrapper getPayload].Num();
}


const TArray<uint8>& FMacHttpResponse::GetContent()
{
	if( !IsReady() )
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"), &Request);
	}
	else
	{
		Payload = [ResponseWrapper getPayload];
		UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetContent() - Num: %i"), [ResponseWrapper getPayload].Num());
	}

	return Payload;
}


FString FMacHttpResponse::GetContentAsString()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetContentAsString()"));

	// Fill in our data.
	GetContent();

	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed( Payload.Num() + 1 );
	FMemory::Memcpy( ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num() );

	return UTF8_TO_TCHAR( ZeroTerminatedPayload.GetData() );
}


int32 FMacHttpResponse::GetResponseCode()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetResponseCode()"));

	return [GetResponseObj() statusCode];
}


NSHTTPURLResponse* FMacHttpResponse::GetResponseObj()
{
	UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::GetResponseObj()"));

	return [ResponseWrapper Response];
}


bool FMacHttpResponse::IsReady()
{
	bool Ready = [ResponseWrapper bIsReady];

	if( Ready )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::IsReady()"));
	}

	return Ready;
}

bool FMacHttpResponse::HadError()
{
	bool bHadError = [ResponseWrapper bHadError];
	
	if( bHadError )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FMacHttpResponse::HadError()"));
	}
	
	return bHadError;
}

const int32 FMacHttpResponse::GetNumBytesReceived() const
{
	return [ResponseWrapper getPayload].Num();
}

const int32 FMacHttpResponse::GetNumBytesWritten() const
{
	int32 NumBytesWritten = [ResponseWrapper getBytesWritten];
	return NumBytesWritten;
}
