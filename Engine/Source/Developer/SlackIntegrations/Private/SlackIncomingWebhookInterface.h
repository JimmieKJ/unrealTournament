// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISlackIncomingWebhookInterface.h"

class IHttpRequest;

/**
 * Concrete implementation of ISlackIncomingWebhookInterface in module's private code.
 * Sends Incoming Webhook messages to Slack.
 */
class FSlackIncomingWebhookInterface : public ISlackIncomingWebhookInterface
{
public:
	/** Empty virtual destructor because class has virtual methods */
	virtual ~FSlackIncomingWebhookInterface() {}

	/**
	 * Send an incoming webhook message to Slack
	 * @param InWebhook		Webhook parameters sets where and how to send the message
	 * @param InMessage		The message to send to Slack
	 * @return				True if the http request was send successfully, otherwise false.
	 */
	virtual bool SendMessage(const FSlackIncomingWebhook& InWebhook, const FSlackMessage& InMessage) const override;

private:
	/** Helper used to make http requests by SendMessage() */
	TSharedRef<IHttpRequest> CreateHttpRequest() const;

	/**
	 * Callback from HTTP library when a request has completed
	 * @param HttpRequest The request object
	 * @param HttpResponse The response from the server
	 * @param bSucceeded Whether a response was successfully received
	 */
	void OnProcessRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) const;

	/** Builds the json payload bytes to send in the http request to Slack */
	static void GetPayload(const FSlackIncomingWebhook& InWebhook, const FSlackMessage& InMessage, TArray<uint8>& OutPayload);

	/** Helper that escapes characters in the payload to make valid json */
	static FString JsonEncode(const FString& InString);
};