// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Mac implementation of an Http request
 */
class FMacHttpRequest : public IHttpRequest
{
private:
	// This is the NSMutableURLRequest, all our Mac functionality will deal with this.
	NSMutableURLRequest* Request;

	// This is the connection our request is sent along.
	NSURLConnection* Connection;


public:
	// implementation friends
	friend class FMacHttpResponse;


	// Begin IHttpBase interface
	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;
	// End IHttpBase interface

	// Begin IHttpRequest interface
	virtual FString GetVerb() override;
	virtual void SetVerb(const FString& Verb) override;
	virtual void SetURL(const FString& URL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	virtual FHttpRequestProgressDelegate& OnRequestProgress() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() override;
	// End IHttpRequest interface


	/**
	 * Constructor
	 */
	FMacHttpRequest();

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FMacHttpRequest();


private:

	/**
	 * Create the session connection and initiate the web request
	 *
	 * @return true if the request was started
	 */
	bool StartRequest();

	/**
	 * Process state for a finished request that no longer needs to be ticked
	 * Calls the completion delegate
	 */
	void FinishedRequest();

	/**
	 * Close session/request handles and unregister callbacks
	 */
	void CleanupRequest();


private:
	/** The response object which we will use to pair with this request */
	TSharedPtr<class FMacHttpResponse,ESPMode::ThreadSafe> Response;

	/** BYTE array payload to use with the request. Typically for a POST */
	TArray<uint8> RequestPayload;

	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;

	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;

	/** Current status of request being processed */
	EHttpRequestStatus::Type CompletionStatus;

	/** Number of bytes sent to progress update */
	int32 ProgressBytesSent;

	/** Start of the request */
	double StartRequestTime;

	/** Time taken to complete/cancel the request. */
	float ElapsedTime;
};


/**
 * Mac Response Wrapper which will be used for it's delegates to receive responses.
 */
@interface FHttpResponseMacWrapper : NSObject
{
	/** Holds the payload as we receive it. */
	TArray<uint8> Payload;
}
/** A handle for the response */
@property(retain) NSHTTPURLResponse* Response;
/** Flag whether the response is ready */
@property BOOL bIsReady;
/** When the response is complete, indicates whether the response was received without error. */
@property BOOL bHadError;
/** The total number of bytes written out during the request/response */
@property int32 BytesWritten;

/** Delegate called when we send data. See Mac docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didSendBodyData:(NSInteger)bytesWritten totalBytesWritten:(NSInteger)totalBytesWritten totalBytesExpectedToWrite:(NSInteger)totalBytesExpectedToWrite;
/** Delegate called with we receive a response. See Mac docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response;
/** Delegate called with we receive data. See Mac docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didReceiveData:(NSData *)data;
/** Delegate called with we complete with an error. See Mac docs for when/how this should be used. */
-(void) connection:(NSURLConnection *)connection didFailWithError:(NSError *)error;
/** Delegate called with we complete successfully. See Mac docs for when/how this should be used. */
-(void) connectionDidFinishLoading:(NSURLConnection *)connection;

- (TArray<uint8>&)getPayload;
- (int32)getBytesWritten;
@end


/**
 * Mac implementation of an Http response
 */
class FMacHttpResponse : public IHttpResponse
{
private:
	// This is the NSHTTPURLResponse, all our functionality will deal with.
	FHttpResponseMacWrapper* ResponseWrapper;

	/** Request that owns this response */
	const FMacHttpRequest& Request;


public:
	// implementation friends
	friend class FMacHttpRequest;


	// Begin IHttpBase interface
	virtual FString GetURL() override;
	virtual FString GetURLParameter(const FString& ParameterName) override;
	virtual FString GetHeader(const FString& HeaderName) override;
	virtual TArray<FString> GetAllHeaders() override;	
	virtual FString GetContentType() override;
	virtual int32 GetContentLength() override;
	virtual const TArray<uint8>& GetContent() override;
	// End IHttpBase interface

	// Begin IHttpResponse interface
	virtual int32 GetResponseCode() override;
	virtual FString GetContentAsString() override;
	// End IHttpResponse interface

	NSHTTPURLResponse* GetResponseObj();

	/**
	 * Check whether a response is ready or not.
	 */
	bool IsReady();
	
	/**
	 * Check whether a response had an error.
	 */
	bool HadError();

	/**
	 * Get the number of bytes received so far
	 */
	const int32 GetNumBytesReceived() const;

	/**
	* Get the number of bytes sent so far
	*/
	const int32 GetNumBytesWritten() const;

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FMacHttpResponse(const FMacHttpRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FMacHttpResponse();


private:

	/** BYTE array to fill in as the response is read via didReceiveData */
	TArray<uint8> Payload;
};