// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "HttpPrivatePCH.h"
#include "IOSHTTP.h"
#include "EngineVersion.h"


/****************************************************************************
 * FIOSHttpRequest implementation
 ***************************************************************************/


FIOSHttpRequest::FIOSHttpRequest()
:	CompletionStatus(EHttpRequestStatus::NotStarted)
,	StartRequestTime(0.0)
,	ElapsedTime(0.0f)
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::FIOSHttpRequest()"));
	Request = [[NSMutableURLRequest alloc] init];
}


FIOSHttpRequest::~FIOSHttpRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::~FIOSHttpRequest()"));
	[Request release];
}


FString FIOSHttpRequest::GetURL()
{
	NSString* url = [[Request URL] absoluteString];

	FString URL(url);
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetURL() - %s"), *URL);
	return URL;
}


void FIOSHttpRequest::SetURL(const FString& URL)
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::SetURL() - %s"), *URL);
	
	
	NSURL* url = [NSURL URLWithString:[NSString stringWithFString:URL]];
	[Request setURL:url];
}


FString FIOSHttpRequest::GetURLParameter(const FString& ParameterName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetURLParameter() - %s"), *ParameterName);

	NSString* ParameterNameStr = [NSString stringWithFString:ParameterName];
	NSArray* Parameters = [[[Request URL] query] componentsSeparatedByString:@"&"];
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


FString FIOSHttpRequest::GetHeader(const FString& HeaderName) 
{
	NSString* ConvertedHeaderName = [NSString stringWithFString:HeaderName];

	FString Header([Request valueForHTTPHeaderField:ConvertedHeaderName]);
	
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetHeader() - %s"), *Header);
	return Header;
}


void FIOSHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue) 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::SetHeader() - %s / %s"), *HeaderName, *HeaderValue );
	[Request setValue:[NSString stringWithFString:HeaderValue] forHTTPHeaderField:[NSString stringWithFString:HeaderName]];
}


TArray<FString> FIOSHttpRequest::GetAllHeaders() 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetAllHeaders()"));
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


const TArray<uint8>& FIOSHttpRequest::GetContent() 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetContent()"));
	RequestPayload.Empty();
	RequestPayload.Reserve( GetContentLength() );

	FMemory::Memcpy( RequestPayload.GetData(), [[Request HTTPBody] bytes], RequestPayload.Num() );

	return RequestPayload;
}


void FIOSHttpRequest::SetContent(const TArray<uint8>& ContentPayload) 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::SetContent()"));
	[Request setHTTPBody:[NSData dataWithBytes:ContentPayload.GetData() length:ContentPayload.Num()]];
}


FString FIOSHttpRequest::GetContentType()
{
	FString ContentType = GetHeader(TEXT("Content-Type"));
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetContentType() - %s"), *ContentType);
	return ContentType;
}


int32 FIOSHttpRequest::GetContentLength() 
{
	int Len = [[Request HTTPBody] length];
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetContentLength() - %i"), Len);
	return Len;
}


void FIOSHttpRequest::SetContentAsString(const FString& ContentString) 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::SetContentAsString() - %s"), *ContentString);
	FTCHARToUTF8 Converter(*ContentString);
	
	// The extra length computation here is unfortunate, but it's technically not safe to assume the length is the same.
	[Request setHTTPBody:[NSData dataWithBytes:(ANSICHAR*)Converter.Get() length:Converter.Length()]];
}


FString FIOSHttpRequest::GetVerb()
{
	FString ConvertedVerb([Request HTTPMethod]);
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetVerb() - %s"), *ConvertedVerb);
	return ConvertedVerb;
}


void FIOSHttpRequest::SetVerb(const FString& Verb)
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::SetVerb() - %s"), *Verb);
	[Request setHTTPMethod:[NSString stringWithFString:Verb]];
}


bool FIOSHttpRequest::ProcessRequest() 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::ProcessRequest()"));
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
	else if( Scheme != TCHAR_TO_ANSI(TEXT("http")) && Scheme != TCHAR_TO_ANSI(TEXT("https")))
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


FHttpRequestCompleteDelegate& FIOSHttpRequest::OnProcessRequestComplete() 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::OnProcessRequestComplete()"));
	return RequestCompleteDelegate;
}

FHttpRequestProgressDelegate& FIOSHttpRequest::OnRequestProgress() 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::OnRequestProgress()"));
	return RequestProgressDelegate;
}

bool FIOSHttpRequest::StartRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::StartRequest()"));
	bool bStarted = false;

	// set the content-length and user-agent
	if(GetContentLength() > 0)
	{
		[Request setValue:[NSString stringWithFormat:@"%d", GetContentLength()] forHTTPHeaderField:@"Content-Length"];
	}

	NSString* Tag = [NSString stringWithFString:FString::Printf(TEXT("UE4-%s,UE4Ver(%s)"), FApp::GetGameName(), *GEngineVersion.ToString())];
	[Request addValue:Tag forHTTPHeaderField:@"User-Agent"];

	Response = MakeShareable( new FIOSHttpResponse( *this ) );

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

void FIOSHttpRequest::FinishedRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::FinishedRequest()"));
	ElapsedTime = (float)(FPlatformTime::Seconds() - StartRequestTime);
	if( Response.IsValid() && !Response->HadError())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Request succeeded"));
		CompletionStatus = EHttpRequestStatus::Succeeded;

		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), Response, true);
	}
	else
	{
		UE_LOG(LogHttp, Verbose, TEXT("Request failed"));
		CompletionStatus = EHttpRequestStatus::Failed;

		Response = NULL;
		OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), NULL, false);
	}

	// Clean up session/request handles that may have been created
	CleanupRequest();

	// Remove from global list since processing is now complete
	FHttpModule::Get().GetHttpManager().RemoveRequest(SharedThis(this));
}


void FIOSHttpRequest::CleanupRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::CleanupRequest()"));
	if(CompletionStatus == EHttpRequestStatus::Processing)
	{
		CancelRequest();
	}
}


void FIOSHttpRequest::CancelRequest() 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::CancelRequest()"));
	if(Connection != nil)
	{
		[Connection cancel];
        [Connection release];
		Connection = nil;
	}
    
    // Cleanup cancel request and fire off any necessary delegates.
    FinishedRequest();
}


EHttpRequestStatus::Type FIOSHttpRequest::GetStatus() 
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpRequest::GetStatus()"));
	return CompletionStatus;
}


const FHttpResponsePtr FIOSHttpRequest::GetResponse() const
{
	return Response;
}


void FIOSHttpRequest::Tick(float DeltaSeconds) 
{
	if(CompletionStatus == EHttpRequestStatus::Processing)
	{
		if( Response->IsReady() )
		{
			FinishedRequest();
		}
	}
}

float FIOSHttpRequest::GetElapsedTime()
{
	return ElapsedTime;
}



/****************************************************************************
 * FHttpResponseIOSWrapper implementation
 ***************************************************************************/

@implementation FHttpResponseIOSWrapper
@synthesize Response;
@synthesize Payload;
@synthesize bIsReady;
@synthesize bHadError;


-(FHttpResponseIOSWrapper*) init
{
	UE_LOG(LogHttp, Verbose, TEXT("-(FHttpResponseIOSWrapper*) init"));
	self = [super init];
	bIsReady = false;
	
	return self;
}


-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse:(NSURLResponse *)response"));
	self.Response = (NSHTTPURLResponse*)response;
	
	// presize the payload container if possible
	self.Payload = [NSMutableData dataWithCapacity:([response expectedContentLength] != NSURLResponseUnknownLength ? [response expectedContentLength] : 0)];
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveResponse: expectedContentLength = %d. Length = %d: %p"), [response expectedContentLength], [self.Payload length], self);
}


-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	[self.Payload appendData:data];
	UE_LOG(LogHttp, Verbose, TEXT("didReceiveData with %d bytes. After Append, Payload Length = %d: %p"), [data length], [self.Payload length], self);
}


-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	self.bIsReady = YES;
	self.bHadError = YES;
	UE_LOG(LogHttp, Warning, TEXT("didFailWithError. Http request failed - %s %s: %p"), 
		*FString([error localizedDescription]),
		*FString([[error userInfo] objectForKey:NSURLErrorFailingURLStringErrorKey]),
		self);
	[connection release];
}


-(void) connectionDidFinishLoading:(NSURLConnection *)connection
{
	UE_LOG(LogHttp, Warning, TEXT("connectionDidFinishLoading: %p"), self);
	self.bIsReady = YES;
	[connection release];
}

@end




/****************************************************************************
 * FIOSHTTPResponse implementation
 **************************************************************************/

FIOSHttpResponse::FIOSHttpResponse(const FIOSHttpRequest& InRequest)
	: Request( InRequest )
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::FIOSHttpResponse()"));
	ResponseWrapper = [[FHttpResponseIOSWrapper alloc] init];
}


FIOSHttpResponse::~FIOSHttpResponse()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::~FIOSHttpResponse()"));
    if( ResponseWrapper != nil )
    {
        [ResponseWrapper release];
        ResponseWrapper = nil;
    }
}


FString FIOSHttpResponse::GetURL()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetURL()"));
	return FString([[Request.Request URL] query]);
}


FString FIOSHttpResponse::GetURLParameter(const FString& ParameterName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetURLParameter()"));

	NSString* ParameterNameStr = [NSString stringWithFString:ParameterName];
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


FString FIOSHttpResponse::GetHeader(const FString& HeaderName)
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetHeader()"));

	NSString* ConvertedHeaderName = [NSString stringWithFString:HeaderName];
	return FString([[[ResponseWrapper Response] allHeaderFields] objectForKey:ConvertedHeaderName]);
}


TArray<FString> FIOSHttpResponse::GetAllHeaders()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetAllHeaders()"));

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


FString FIOSHttpResponse::GetContentType()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetContentType()"));

	return GetHeader( TEXT( "Content-Type" ) );
}


int32 FIOSHttpResponse::GetContentLength()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetContentLength()"));

	return [[ResponseWrapper Payload] length];
}


const TArray<uint8>& FIOSHttpResponse::GetContent()
{
	if( !IsReady() )
	{
		UE_LOG(LogHttp, Warning, TEXT("Payload is incomplete. Response still processing. %p"), &Request);
	}
	else
	{
		Payload.Empty();
		Payload.AddZeroed( [[ResponseWrapper Payload] length] );
	
		FMemory::Memcpy( Payload.GetData(), [[ResponseWrapper Payload] bytes], [[ResponseWrapper Payload] length] );
		UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetContent() - Num: %i"), [[ResponseWrapper Payload] length]);
	}

	return Payload;
}


FString FIOSHttpResponse::GetContentAsString()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetContentAsString()"));

	// Fill in our data.
	GetContent();

	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed( Payload.Num() + 1 );
	FMemory::Memcpy( ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num() );

	return UTF8_TO_TCHAR( ZeroTerminatedPayload.GetData() );
}


int32 FIOSHttpResponse::GetResponseCode()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetResponseCode()"));

	return [GetResponseObj() statusCode];
}


NSHTTPURLResponse* FIOSHttpResponse::GetResponseObj()
{
	UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::GetResponseObj()"));

	return [ResponseWrapper Response];
}


bool FIOSHttpResponse::IsReady()
{
	bool Ready = [ResponseWrapper bIsReady];

	if( Ready )
	{
		UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::IsReady() = true"));
	}

	return Ready;
}


bool FIOSHttpResponse::HadError()
{
	bool bHadError = [ResponseWrapper bHadError];
	
	if (bHadError)
	{
		UE_LOG(LogHttp, Verbose, TEXT("FIOSHttpResponse::HadError() = true"));
	}

	return bHadError;
}
