// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Runtime/Online/HTTP/Public/Interfaces/IHttpBase.h"
#include "Runtime/Online/HTTP/Public/Interfaces/IHttpRequest.h"
#include "Runtime/Online/HTTP/Public/Interfaces/IHttpResponse.h"
#include "StringConv.h"


/**
 * A generic http request
 */
class FGenericPlatformHttpRequest : public IHttpRequest
{
public:
	// IHttpBase
	virtual FString GetURL() override { return TEXT(""); }
	virtual FString GetURLParameter(const FString& ParameterName) override { return TEXT(""); }
	virtual FString GetHeader(const FString& HeaderName) override { return TEXT(""); }
	virtual TArray<FString> GetAllHeaders() override { return TArray<FString>(); }
	virtual FString GetContentType() override { return TEXT(""); }
	virtual int32 GetContentLength() override { return 0; }
	virtual const TArray<uint8>& GetContent() override { static TArray<uint8> Temp; return Temp; }

	// IHttpRequest
	virtual FString GetVerb() override { return TEXT(""); }
	virtual void SetVerb(const FString& Verb) override {}
	virtual void SetURL(const FString& URL) override {}
	virtual void SetContent(const TArray<uint8>& ContentPayload) override {}
	virtual void SetContentAsString(const FString& ContentString) override {}
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override {}
	virtual bool ProcessRequest() override { return false; }
	virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() override { static FHttpRequestCompleteDelegate RequestCompleteDelegate; return RequestCompleteDelegate; }
	virtual FHttpRequestProgressDelegate& OnRequestProgress() override { static FHttpRequestProgressDelegate RequestProgressDelegate; return RequestProgressDelegate; }
	virtual void CancelRequest() override {}
	virtual EHttpRequestStatus::Type GetStatus() override { return EHttpRequestStatus::NotStarted; }
	virtual const FHttpResponsePtr GetResponse() const override { return nullptr; }
	virtual void Tick(float DeltaSeconds) override {}
	virtual float GetElapsedTime() override { return 0.0f; }
};


void FGenericPlatformHttp::Init()
{
}


void FGenericPlatformHttp::Shutdown()
{
}


IHttpRequest* FGenericPlatformHttp::ConstructRequest()
{
	return new FGenericPlatformHttpRequest();
}


static bool IsAllowedChar(UTF8CHAR LookupChar)
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

	return AllowedTable[LookupChar];
}


FString FGenericPlatformHttp::UrlEncode(const FString &UnencodedString)
{
	FTCHARToUTF8 Converter(*UnencodedString);	//url encoding must be encoded over each utf-8 byte
	const UTF8CHAR* UTF8Data = (UTF8CHAR*) Converter.Get();	//converter uses ANSI instead of UTF8CHAR - not sure why - but other code seems to just do this cast. In this case it really doesn't matter
	FString EncodedString = TEXT("");
	
	TCHAR Buffer[2] = { 0, 0 };

	for (int32 ByteIdx = 0, Length = Converter.Length(); ByteIdx < Length; ++ByteIdx)
	{
		UTF8CHAR ByteToEncode = UTF8Data[ByteIdx];
		
		if (IsAllowedChar(ByteToEncode))
		{
			Buffer[0] = ByteToEncode;
			FString TmpString = Buffer;
			EncodedString += TmpString;
		}
		else if (ByteToEncode != '\0')
		{
			EncodedString += TEXT("%");
			EncodedString += FString::Printf(TEXT("%.2X"), ByteToEncode);
		}
	}
	return EncodedString;
}
