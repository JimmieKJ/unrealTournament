// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IHttpRequest.h"

/** 
  * Adapter class for IHttpRequest abstract interface
  * does not fully expose the wrapped interface in the base. This allows client defined marshalling of the requests when end point permissions are at issue.
  */

class FHttpRequestAdapterBase
{
public:
    FHttpRequestAdapterBase(TSharedRef<IHttpRequest>& InHttpRequest) : HttpRequest(InHttpRequest)
    {}

    FString                       GetURL()    	                                                   { return HttpRequest->GetURL(); }
    FString                       GetURLParameter(const FString& ParameterName)                    { return HttpRequest->GetURLParameter(ParameterName); }
    FString                       GetHeader(const FString& HeaderName)                             { return HttpRequest->GetHeader(HeaderName); }
    TArray<FString>               GetAllHeaders()                                                  { return HttpRequest->GetAllHeaders(); }
    FString                       GetContentType()                                                 { return HttpRequest->GetContentType(); }
    int32                         GetContentLength()                                               { return HttpRequest->GetContentLength(); }
    const TArray<uint8>&          GetContent()                                                     { return HttpRequest->GetContent(); }

    FString                       GetVerb() 						                               { return HttpRequest->GetVerb(); }
    void                          SetVerb(const FString& Verb) 	                                   { HttpRequest->SetVerb(Verb); }
    void                          SetURL(const FString& URL) 		                               { HttpRequest->SetURL(URL); }
    void                          SetContent(const TArray<uint8>& ContentPayload)                  { HttpRequest->SetContent(ContentPayload); }
    void                          SetContentAsString(const FString& ContentString)                 { HttpRequest->SetContentAsString(ContentString); }
    void                          SetHeader(const FString& HeaderName, const FString& HeaderValue) { HttpRequest->SetHeader(HeaderName, HeaderValue); }
    const FHttpResponsePtr        GetResponse() const                                              { return HttpRequest->GetResponse(); }
    float                         GetElapsedTime()                                                 { return HttpRequest->GetElapsedTime(); }

protected:
    TSharedRef<IHttpRequest> HttpRequest;
};

class FHttpRequestAdapter : public FHttpRequestAdapterBase
{
public:
    FHttpRequestAdapter(TSharedRef<IHttpRequest>& InHttpRequest)
        : FHttpRequestAdapterBase(InHttpRequest)
    {}

    FHttpRequestCompleteDelegate&   OnProcessRequestComplete()                                 { return HttpRequest->OnProcessRequestComplete(); }
    FHttpRequestProgressDelegate&   OnRequestProgress()                                        { return HttpRequest->OnRequestProgress(); }

    EHttpRequestStatus::Type        GetStatus()                                                { return HttpRequest->GetStatus(); }

    bool                            ProcessRequest()                                           { return HttpRequest->ProcessRequest(); }
    void                            CancelRequest()                                            { HttpRequest->CancelRequest(); }
    void                            Tick(float DeltaSeconds)                                   { HttpRequest->Tick(DeltaSeconds); }
};