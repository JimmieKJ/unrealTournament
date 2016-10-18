// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_XMPP_JINGLE

/** steps during login/logout */
namespace ELoginProgress
{
	enum Type
	{
		NotStarted,
		ProcessingLogin,
		ProcessingLogout,
		LoggedIn,
		LoggedOut
	};

	inline const TCHAR* ToString(ELoginProgress::Type EnumVal)
	{
		switch (EnumVal)
		{
			case NotStarted:
				return TEXT("NotStarted");
			case ProcessingLogin:
				return TEXT("ProcessingLogin");
			case ProcessingLogout:
				return TEXT("ProcessingLogout");
			case LoggedIn:
				return TEXT("LoggedIn");
			case LoggedOut:
				return TEXT("LoggedOut");
		};
		return TEXT("");
	}
}

/**
 * WebRTC (formerly libjingle) implementation of Xmpp connection
 * See http://www.webrtc.org/ for more info
 */
class FXmppConnectionJingle
	: public IXmppConnection
	, public FTickerObjectBase
	, public sigslot::has_slots<>
{
public:

	// IXmppConnection

	virtual void SetServer(const FXmppServer& Server) override;
	virtual const FXmppServer& GetServer() const override;
	virtual void Login(const FString& UserId, const FString& Auth) override;
	virtual void Logout() override;

	virtual EXmppLoginStatus::Type GetLoginStatus() const override;
	virtual const FXmppUserJid& GetUserJid() const override;

	virtual FOnXmppLoginComplete& OnLoginComplete() override { return OnXmppLoginCompleteDelegate; }
	virtual FOnXmppLogingChanged& OnLoginChanged() override { return OnXmppLogingChangedDelegate; }
	virtual FOnXmppLogoutComplete& OnLogoutComplete() override { return OnXmppLogoutCompleteDelegate; }

	virtual IXmppPresencePtr Presence() override;
	virtual IXmppPubSubPtr PubSub() override;
	virtual IXmppMessagesPtr Messages() override;
	virtual IXmppMultiUserChatPtr MultiUserChat() override;
	virtual IXmppChatPtr PrivateChat() override;

	// FTickerObjectBase

	virtual bool Tick(float DeltaTime) override;
	
	// FXmppConnectionJingle

	FXmppConnectionJingle();
	virtual ~FXmppConnectionJingle();

	const FString& GetMucDomain() const;
	const FString& GetPubSubDomain() const;

private:

	/** called to kick off thread to establish connection/login */
	void Startup();
	/** called to shutdown thread after disconnect */
	void Shutdown();	
	/** update all stat counters in a given interval */
	void UpdateStatCounters();

	// called on the FXmppConnectionPumpThread
	void HandleLoginChange(ELoginProgress::Type InLastLoginState, ELoginProgress::Type InLoginState);
	void HandlePumpStarting(buzz::XmppPump* XmppPump);
	void HandlePumpQuitting(buzz::XmppPump* XmppPump);
	void HandlePumpTick(buzz::XmppPump* XmppPump);
	
	/** last login state to compare for changes */
	ELoginProgress::Type LastLoginState;
	/** current login state */
	ELoginProgress::Type LoginState;
	/** login state updated from both pump thread and main thread */
	mutable FCriticalSection LoginStateLock;

	/** current server configuration */
	FXmppServer ServerConfig;
	/** current user attempting to login */
	FXmppUserJid UserJid;
	/** cached settings used to connect */
	buzz::XmppClientSettings ClientSettings;
	/** cached domain for all Muc communication */
	FString MucDomain;
	/** cached domain for all PubSub communication */
	FString PubSubDomain;

	/** Frequency of the stat counter update */
	double StatUpdateFreq;
	/** Last time since a stat counter update */
	double LastStatUpdateTime;

	// completion delegates
	FOnXmppLoginComplete OnXmppLoginCompleteDelegate;
	FOnXmppLogingChanged OnXmppLogingChangedDelegate;
	FOnXmppLogoutComplete OnXmppLogoutCompleteDelegate;

	/** access to presence implementation */
	TSharedPtr<class FXmppPresenceJingle, ESPMode::ThreadSafe> PresenceJingle;
	friend class FXmppPresenceJingle;

	/** access to messages implementation */
	TSharedPtr<class FXmppMessagesJingle, ESPMode::ThreadSafe> MessagesJingle;
	friend class FXmppMessagesJingle;

	/** access to private chat implementation */
	TSharedPtr<class FXmppChatJingle, ESPMode::ThreadSafe> ChatJingle;
	friend class FXmppChatJingle;

	/** access to MUC implementation */
	TSharedPtr<class FXmppMultiUserChatJingle, ESPMode::ThreadSafe> MultiUserChatJingle;
	friend class FXmppMultiUserChatJingle;

	/** thread that handles creating a connection and processing messages on xmpp pump */
	class FXmppConnectionPumpThread* PumpThread;
	friend class FXmppConnectionPumpThread;
};

#endif //WITH_XMPP_JINGLE
