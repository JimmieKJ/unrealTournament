// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "HttpPrivatePCH.h"
#include "AppleHTTP.h"
#include "EngineVersion.h"


/****************************************************************************
 * FAppleHttpRequest implementation
 ***************************************************************************/


FAppleHttpRequest::FAppleHttpRequest()
:	Connection(NULL)
,	CompletionStatus(EHttpRequestStatus::NotStarted)
,	ProgressBytesSent(0)
,	StartRequestTime(0.0)
,	ElapsedTime(0.0f)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::FAppleHttpRequest()"));
	Request = [[NSMutableURLRequest alloc] init];
	[Request setTimeoutInterval: FHttpModule::Get().GetHttpTimeout()];
}


FAppleHttpRequest::~FAppleHttpRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::~FAppleHttpRequest()"));
	check(Connection == nullptr);
	[Request release];
}


FString FAppleHttpRequest::GetURL()
{
	SCOPED_AUTORELEASE_POOL;
	NSURL* URL = Request.URL;
	if (URL != nullptr)
	{
		FString ConvertedURL(URL.absoluteString);
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetURL() - %s"), *ConvertedURL);
		return ConvertedURL;
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetURL() - NULL"));
		return FString();
	}
}


void FAppleHttpRequest::SetURL(const FString& URL)
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetURL() - %s"), *URL);
	[Request setURL: [NSURL URLWithString: URL.GetNSString()]];
}


FString FAppleHttpRequest::GetURLParameter(const FString& ParameterName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetURLParameter() - %s"), *ParameterName);

	NSString* ParameterNameStr = ParameterName.GetNSString();
	NSArray* Parameters = [[[Request URL] query] componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters)
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = [KeyValue objectAtIndex:0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame)
		{
			return FString([KeyValue objectAtIndex:1]);
		}
	}
	return FString();
}


FString FAppleHttpRequest::GetHeader(const FString& HeaderName)
{
	FString Header([Request valueForHTTPHeaderField:HeaderName.GetNSString()]);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetHeader() - %s"), *Header);
	return Header;
}


void FAppleHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetHeader() - %s / %s"), *HeaderName, *HeaderValue );
	[Request setValue: HeaderValue.GetNSString() forHTTPHeaderField: HeaderName.GetNSString()];
}

void FAppleHttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
    if (!HeaderName.IsEmpty() && !AdditionalHeaderValue.IsEmpty())
    {
        NSDictionary* Headers = [Request allHTTPHeaderFields];
        NSString* PreviousHeaderValuePtr = [Headers objectForKey: HeaderName.GetNSString()];
        FString PreviousValue(PreviousHeaderValuePtr);
		FString NewValue;
		if (PreviousValue != nullptr && !PreviousValue.IsEmpty())
		{
			NewValue = PreviousValue + TEXT(", ");
		}
		NewValue += AdditionalHeaderValue;

        SetHeader(HeaderName, NewValue);
	}
}

TArray<FString> FAppleHttpRequest::GetAllHeaders()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetAllHeaders()"));
	NSDictionary* Headers = [Request allHTTPHeaderFields];
	TArray<FString> Result;
	Result.Reserve([Headers count]);
	for (NSString* Key in [Headers allKeys])
	{
		FString ConvertedValue([Headers objectForKey:Key]);
		FString ConvertedKey(Key);
		UE_LOG(LogHttp, Verbose, TEXT("Header= %s, Key= %s"), *ConvertedValue, *ConvertedKey);

		Result.Add( FString::Printf( TEXT("%s: %s"), *ConvertedKey, *ConvertedValue ) );
	}
	return Result;
}


const TArray<uint8>& FAppleHttpRequest::GetContent()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetContent()"));
	NSData* Body = Request.HTTPBody; // accessing HTTPBody will call copy on the value, increasing its retain count
	RequestPayload.Empty();
	RequestPayload.Append((const uint8*)[[Request HTTPBody] bytes], GetContentLength());
	return RequestPayload;
}


void FAppleHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetContent()"));
	[Request setHTTPBody:[NSData dataWithBytes:ContentPayload.GetData() length:ContentPayload.Num()]];
}


FString FAppleHttpRequest::GetContentType()
{
	FString ContentType = GetHeader(TEXT("Content-Type"));
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetContentType() - %s"), *ContentType);
	return ContentType;
}


int32 FAppleHttpRequest::GetContentLength()
{
	int Len = [[Request HTTPBody] length];
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetContentLength() - %i"), Len);
	return Len;
}


void FAppleHttpRequest::SetContentAsString(const FString& ContentString)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetContentAsString() - %s"), *ContentString);
	FTCHARToUTF8 Converter(*ContentString);
	
	// The extra length computation here is unfortunate, but it's technically not safe to assume the length is the same.
	[Request setHTTPBody:[NSData dataWithBytes:(ANSICHAR*)Converter.Get() length:Converter.Length()]];
}


FString FAppleHttpRequest::GetVerb()
{
	FString ConvertedVerb([Request HTTPMethod]);
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetVerb() - %s"), *ConvertedVerb);
	return ConvertedVerb;
}


void FAppleHttpRequest::SetVerb(const FString& Verb)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::SetVerb() - %s"), *Verb);
	[Request setHTTPMethod: Verb.GetNSString()];
}


bool FAppleHttpRequest::ProcessRequest()
{
	SCOPED_AUTORELEASE_POOL;
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::ProcessRequest()"));
	bool bStarted = false;

	FString Scheme([[Request URL] scheme]);
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


FHttpRequestCompleteDelegate& FAppleHttpRequest::OnProcessRequestComplete()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FAppleHttpRequest::OnRequestProgress() 
{
	UE_LOG(LogHttp, VeryVerbose, TEXT("FAppleHttpRequest::OnRequestProgress()"));
	return RequestProgressDelegate;
}


bool FAppleHttpRequest::StartRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::StartRequest()"));
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

	Response = MakeShareable( new FAppleHttpResponse( *this ) );

	// Create the connection, tell it to run in the main run loop, and kick it off.
	Connection = [[NSURLConnection alloc] initWithRequest:Request delegate:Response->ResponseWrapper startImmediately:NO];
	if( Connection != NULL && Response->ResponseWrapper != NULL )
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
		CompletionStatus = EHttpRequestStatus::Failed;
	}
	StartRequestTime = FPlatformTime::Seconds();
	// reset the elapsed time.
	ElapsedTime = 0.0f;

	return bStarted;
}

void FAppleHttpRequest::FinishedRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::FinishedRequest()"));
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

		Response = NULL;
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), NULL, false);
	}

	// Clean up session/request handles that may have been created
	CleanupRequest();

	// Remove from global list since processing is now complete
	if (FHttpModule::Get().GetHttpManager().IsValidRequest(this))
	{
		FHttpModule::Get().GetHttpManager().RemoveRequest(SharedThis(this));
	}
}


void FAppleHttpRequest::CleanupRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::CleanupRequest()"));

	if(CompletionStatus == EHttpRequestStatus::Processing)
	{
		CancelRequest();
	}
	if(Connection != NULL)
	{
		[Connection release];
		Connection = NULL;
	}
}


void FAppleHttpRequest::CancelRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::CancelRequest()"));
	if(Connection != NULL)
	{
		[Connection cancel];
	}
	FinishedRequest();
}


EHttpRequestStatus::Type FAppleHttpRequest::GetStatus()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpRequest::GetStatus()"));
	return CompletionStatus;
}


const FHttpResponsePtr FAppleHttpRequest::GetResponse() const
{
	return Response;
}


void FAppleHttpRequest::Tick(float DeltaSeconds)
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

float FAppleHttpRequest::GetElapsedTime()
{
	return ElapsedTime;
}


/****************************************************************************
 * FHttpResponseAppleWrapper implementation
 ***************************************************************************/

@implementation FHttpResponseAppleWrapper
@synthesize Response;
@synthesize bIsReady;
@synthesize bHadError;
@synthesize BytesWritten;


-(FHttpResponseAppleWrapper*) init
{
	UE_LOG(LogHttp, Verbose, TEXT("-(FHttpResponseAppleWrapper*) init"));
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
 * FAppleHTTPResponse implementation
 **************************************************************************/

FAppleHttpResponse::FAppleHttpResponse(const FAppleHttpRequest& InRequest)
	: Request( InRequest )
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::FAppleHttpResponse()"));
	ResponseWrapper = [[FHttpResponseAppleWrapper alloc] init];
}


FAppleHttpResponse::~FAppleHttpResponse()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::~FAppleHttpResponse()"));
	[ResponseWrapper getPayload].Empty();

	[ResponseWrapper release];
	ResponseWrapper = nil;
}


FString FAppleHttpResponse::GetURL()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetURL()"));
	return FString([[Request.Request URL] query]);
}


FString FAppleHttpResponse::GetURLParameter(const FString& ParameterName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetURLParameter()"));

	NSString* ParameterNameStr = ParameterName.GetNSString();
	NSArray* Parameters = [[[Request.Request URL] query] componentsSeparatedByString:@"&"];
	for (NSString* Parameter in Parameters)
	{
		NSArray* KeyValue = [Parameter componentsSeparatedByString:@"="];
		NSString* Key = [KeyValue objectAtIndex:0];
		if ([Key compare:ParameterNameStr] == NSOrderedSame)
		{
			return FString([[KeyValue objectAtIndex:1] stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding]);
		}
	}
	return FString();
}


FString FAppleHttpResponse::GetHeader(const FString& HeaderName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetHeader()"));

	NSString* ConvertedHeaderName = HeaderName.GetNSString();
	return FString([[[ResponseWrapper Response] allHeaderFields] objectForKey:ConvertedHeaderName]);
}


TArray<FString> FAppleHttpResponse::GetAllHeaders()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetAllHeaders()"));

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


FString FAppleHttpResponse::GetContentType()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContentType()"));

	return GetHeader( TEXT( "Content-Type" ) );
}


int32 FAppleHttpResponse::GetContentLength()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContentLength()"));

	return [ResponseWrapper getPayload].Num();
}


const TArray<uint8>& FAppleHttpResponse::GetContent()
{
	if( !IsReady() )
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"), &Request);
	}
	else
	{
		Payload = [ResponseWrapper getPayload];
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContent() - Num: %i"), [ResponseWrapper getPayload].Num());
	}

	return Payload;
}


FString FAppleHttpResponse::GetContentAsString()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetContentAsString()"));

	// Fill in our data.
	GetContent();

	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed( Payload.Num() + 1 );
	FMemory::Memcpy( ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num() );

	return UTF8_TO_TCHAR( ZeroTerminatedPayload.GetData() );
}


int32 FAppleHttpResponse::GetResponseCode()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetResponseCode()"));

	return [GetResponseObj() statusCode];
}


NSHTTPURLResponse* FAppleHttpResponse::GetResponseObj()
{
	UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::GetResponseObj()"));

	return [ResponseWrapper Response];
}


bool FAppleHttpResponse::IsReady()
{
	bool Ready = [ResponseWrapper bIsReady];

	if( Ready )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::IsReady()"));
	}

	return Ready;
}

bool FAppleHttpResponse::HadError()
{
	bool bHadError = [ResponseWrapper bHadError];
	
	if( bHadError )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FAppleHttpResponse::HadError()"));
	}
	
	return bHadError;
}

const int32 FAppleHttpResponse::GetNumBytesReceived() const
{
	return [ResponseWrapper getPayload].Num();
}

const int32 FAppleHttpResponse::GetNumBytesWritten() const
{
    int32 NumBytesWritten = [ResponseWrapper getBytesWritten];
    return NumBytesWritten;
}
