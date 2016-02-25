// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

DECLARE_DELEGATE_TwoParams(FOnMessageProcessed, bool /*Process success*/, const FString& /*SanitizedMessage*/);
DECLARE_DELEGATE_TwoParams(FOnMessageArrayProcessed, bool /*Process success*/, const TArray<FString>& /*SanitizedMessages*/);

struct FSanitizeMessage
{
	FSanitizeMessage(const FString& InRawMessage, FOnMessageProcessed InProcessCompleteDelegate)
		: RawMessage(InRawMessage)
		, CompletionDelegate(InProcessCompleteDelegate)
	{}

	FString RawMessage;
	FOnMessageProcessed CompletionDelegate;
};

struct FMultiPartMessage
{
	TArray<FString> AlreadyProcessedMessages;
	TArray<int32> AlreadyProcessedIndex;
	TArray<FString> SanitizedMessages;
	FString MessageToSanatize;
	bool bCompleted;
	int32 PartNumber;
};

class IMessageSanitizer
{
protected:
	IMessageSanitizer() {};

public:
	virtual ~IMessageSanitizer() {};
	virtual void SanitizeMessage(const FString& Message, FOnMessageProcessed CompletionDelegate) = 0;
	virtual void SanitizeMessageArray(const TArray<FString>& MessageArray, FOnMessageArrayProcessed CompletionDelegate) = 0;
};

typedef TSharedPtr<IMessageSanitizer, ESPMode::ThreadSafe> IMessageSanitizerPtr;

