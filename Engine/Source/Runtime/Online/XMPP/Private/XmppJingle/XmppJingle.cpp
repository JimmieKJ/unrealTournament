// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "XmppPrivatePCH.h"
#include "XmppJingle.h"
#include "XmppConnectionJingle.h"

#if WITH_XMPP_JINGLE

void FXmppJingle::Init()
{
	if (UE_LOG_ACTIVE(LogXmpp, Verbose))
	{
		// also enable built-in verbose logging for rtc lib
		rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);
	}	
	rtc::InitializeSSL();
}

void FXmppJingle::Cleanup()
{
	rtc::CleanupSSL();
}

TSharedRef<IXmppConnection> FXmppJingle::CreateConnection()
{
	return MakeShareable(new FXmppConnectionJingle());
}

void FXmppJingle::ConvertToJid(FXmppUserJid& OutJid, const buzz::Jid& InJid)
{
	OutJid.Id = UTF8_TO_TCHAR(InJid.node().c_str());
	OutJid.Domain = UTF8_TO_TCHAR(InJid.domain().c_str());
	OutJid.Resource = UTF8_TO_TCHAR(InJid.resource().c_str());
}

void FXmppJingle::ConvertFromJid(buzz::Jid& OutJid, const FXmppUserJid& InJid)
{
	// domain cannot be empty
	FString Domain = !InJid.Domain.IsEmpty() ? InJid.Domain : TEXT("unknown");

	OutJid = buzz::Jid(
		TCHAR_TO_UTF8(*InJid.Id),
		TCHAR_TO_UTF8(*Domain),
		TCHAR_TO_UTF8(*InJid.Resource)
		);
}

#endif //WITH_XMPP_JINGLE
