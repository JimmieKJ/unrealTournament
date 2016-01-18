// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "OnlineSubsystemTypes.h"
#include "UniqueNetIdWrapper.h"
#include "McpSharedTypes.h"
#include "McpProfileGroup.generated.h"

class UMcpProfile;
class FOnlineSubsystemMcp;
struct FOnlineNotification;

namespace FHttpRetrySystem { class FRequest; }

// trying to avoid header dependency on HTTP code here
DECLARE_DELEGATE_TwoParams(FOnProcessRequestCompleteDelegate, TSharedRef<FHttpRetrySystem::FRequest>&, bool);

/**
 * Delegate type for handling a notification
 *
 * The first parameter is a notification structure
 * Return true if handled, false if error
 */
DECLARE_DELEGATE_OneParam(FMcpHandleNotification, const FOnlineNotification&);

/**
 * Delegate type for a queued query generator, which is used to defer creation of a query until it's ready to be sent
 *
 * The first parameter is the profile this is bound to
 */
DECLARE_DELEGATE_OneParam(FMcpGenerateRequestDelegate, UMcpProfile*);

/** Base class for our UrlContext parameters */
USTRUCT()
struct FBaseUrlContext
{
	GENERATED_USTRUCT_BODY()

public:

	enum EContextCredentials
	{
		CXC_Client,
		CXC_DedicatedServer,
		CXC_Cheater
	};

	FBaseUrlContext()
	{
	}

	const FMcpQueryComplete& GetCallback() const { return Callback; }
	const FString& GetUrlConfig() const { return UrlConfig; }
	EContextCredentials GetCredentials() const { return Credentials; }
	inline void SetCallback(const FMcpQueryComplete& InCallback) { Callback = InCallback; }

protected:
	FBaseUrlContext(const FMcpQueryComplete& InCallback, const TCHAR* InUrlConfig, EContextCredentials InCredentials)
		: Callback(InCallback)
		, UrlConfig(InUrlConfig)
		, Credentials(InCredentials)
	{
	}

	FMcpQueryComplete Callback;
	FString UrlConfig;
	EContextCredentials Credentials;
};

USTRUCT()
struct MCPPROFILESYS_API FDedicatedServerUrlContext : public FBaseUrlContext
{
	GENERATED_USTRUCT_BODY()
public:

	static FDedicatedServerUrlContext Default;

	FDedicatedServerUrlContext(const FMcpQueryComplete& InCallback = FMcpQueryComplete())
		: FBaseUrlContext(InCallback, TEXT("McpDedicatedServerCommandUrl"), CXC_DedicatedServer)
	{
	}
};

USTRUCT()
struct MCPPROFILESYS_API FClientUrlContext : public FBaseUrlContext
{
	GENERATED_USTRUCT_BODY()
public:

	static FClientUrlContext Default;

	FClientUrlContext(const FMcpQueryComplete& InCallback = FMcpQueryComplete())
		: FBaseUrlContext(InCallback, TEXT("McpClientCommandUrl"), CXC_Client)
	{
	}
};

/** Structure for the profiles we loaded for this group (used internally) */
USTRUCT()
struct FProfileEntry
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY()
	FString ProfileId;
	UPROPERTY()
	UMcpProfile* ProfileObject;
};

/** Pairing of pending http request to type of context it should be started with */
USTRUCT()
struct FProfilePendingHttpRequest
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY()
	UMcpProfile* SourceProfile;

	/** If set call this delegate to generate a request instead of sending a request directly */
	FMcpGenerateRequestDelegate GenerateDelegate;

	/** Http request that will be sent when it reaches the end of the queue */
	TSharedPtr<class FOnlineHttpRequest> HttpRequest;

	/** Rather to send the query as a server, client, etc */
	FBaseUrlContext::EContextCredentials Context;

	/** Returns true if this is a Generate request */
	bool IsRequestGenerator() const { return !HttpRequest.IsValid(); }

	FProfilePendingHttpRequest()
		: SourceProfile(nullptr)
		, HttpRequest(nullptr)
		, Context(FBaseUrlContext::CXC_Client)
	{
	}
};

/**
 * This object represents a collection of MCP profiles for a single account. These are all for the same 
 * title. Multi-profile is more a way to split things up for efficiency/organization.
 */
UCLASS(config = Engine)
class MCPPROFILESYS_API UMcpProfileGroup : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	/** Has Initialize() been called on this profile */
	FORCEINLINE bool IsInitialized() const { return bIsInitialized; }

	/** Allow setting this if the game wants to */
	FORCEINLINE void SetPlayerName(const FString& InPlayerName) { PlayerName = InPlayerName; }

	/** Getter for great justice */
	FORCEINLINE FOnlineSubsystemMcp* GetOnlineSubMcp() const { return OnlineSubMcp; }

	/** The epic account ID that this profile belongs to */
	FORCEINLINE const FUniqueNetIdWrapper& GetGameAccountId() const { return GameAccountId; }

	/** The epic account ID that is used to contact the server (this determines permissions/credentials) */
	FORCEINLINE const FUniqueNetIdWrapper& GetCredentialsAccountId() const { return CredentialsAccountId; }

	/** Change the epic account that we should use for credentials/authentication when contacting MCP (this user should be logged in already) */
	FORCEINLINE void SetCredentialsAccountId(const FUniqueNetIdWrapper& AccountId) { check(AccountId.IsValid()); CredentialsAccountId = AccountId; }

	/** Are we acting on this player's account in a server capacity (different privileges) */
	FORCEINLINE bool IsActingAsServer() const { return bIsServer; }

	/** Expose a profile to the game with a specific class. */
	UMcpProfile* AddProfile(const FString& ProfileId, TSubclassOf<UMcpProfile> ProfileClass, bool bReplaceExisting);

	/** Templated version of AddProfile */
	template<typename UCLASS_T>
	FORCEINLINE UCLASS_T* AddProfile(const FString& ProfileId, bool bReplaceExisting) { return CastChecked<UCLASS_T>(AddProfile(ProfileId, UCLASS_T::StaticClass(), bReplaceExisting)); }

	/** Get the named profile (it must already have had AddProfile called) */
	UMcpProfile* GetProfile(const FString& ProfileId) const;

	/** Account display name of the player we created this profile for (mostly for logging) */
	FORCEINLINE const FString& GetPlayerName() const { return PlayerName; }

	/** Calls ForceQueryProfile on all profiles in this group. This is mostly for cheats */
	void RefreshAllProfiles();

	/** 
	 * Gets the current UTC time as seen by MCP (may not be "correct", or maybe we're wrong, we don't care)
	 * If both the client and MCP have correct clocks this should be roughly equivalent to FDateTime::UtcNow()
	 */
	FORCEINLINE FDateTime GetServerDateTime(const FDateTime& InClientTime = FDateTime::UtcNow()) const { return InClientTime + LocalTimeOffset; }

	/** Accepts a date that came from MCP and convert it to use the client's clock. This allows it to be comparable with other locally generated UtcNow times. */
	FORCEINLINE FDateTime GetClientDateTime(const FDateTime& InServerTime) const { return InServerTime - LocalTimeOffset; }

	/** This is the changelist number MCP was built from (not set until we have contacted MCP at least once). This may change at runtime if the service upgrades. */
	FORCEINLINE const FString& GetMcpVersion() const { return LastMcpVersion; }
	
	/** Is the provided instant within the current reset interval (as defined by HoursPerInterval) */
	bool IsCurrentInterval(const FDateTime& Instant, int32 HoursPerInterval, FDateTime* IntervalEnd = nullptr) const;

	/** 
	 * Get the begin and end datetimes for the reset interval surrounding Now.
	 * Reset intervals are locked to the gregorian calendar (Both Start and End will have the same calendar date)
	 * They are also rounded to the nearest hour (based on IntervalsPerDay) with the final interval in the day having a variable length.
	 * @param StartOut The beginning of the interval (output).
	 * @param EndOut The end of the interval (output).
	 * @param Center The datetime used to construct the interval (will fall within StartOut and EndOut). 
	 * @param HoursPerInterval The number of hours in a normal interval (the final interval of the day will vary to accomodate this
	 */
	void GetRefreshInterval(FDateTime& StartOut, FDateTime& EndOut, const FDateTime& Center, int32 HoursPerInterval) const;

public:

	/** Build a full Url from a route and profile Id (and internal game account) */
	FString FormatUrl(const FString& Route, const FString& AccountId, const FString& ProfileId) const;

	/** FormatUrl with fewer params (not all are always relevant) */
	FORCEINLINE FString FormatUrl(const FString& Route, const FString& ProfileId = FString()) const { return FormatUrl(Route, GetGameAccountId().ToString(), ProfileId); }

	/** Send an HTTP request that initiates a profile command */
	bool TriggerMcpRpcCommand(UFunction* Function, void* Parameters, UMcpProfile* SourceProfile);

	/** Send an HTTP request that initiates a profile command */
	bool TriggerMcpRpcCommand(const FString& CommandName, const UStruct* Struct, const void* Data, int32 CheckFlags, const FBaseUrlContext& Context, UMcpProfile* SourceProfile);

	/** Queue up a request serially, triggers the request if its the only one. SourceProfile may be null for non revision commands */
	void QueueRequest(const TSharedRef<class FOnlineHttpRequest>& HttpRequest, FBaseUrlContext::EContextCredentials Context, UMcpProfile* SourceProfile, bool bImmediate = false);

	/** Queue up a request generator, which will send a request when it reaches the front of the queue */
	void QueueRequestGenerator(FMcpGenerateRequestDelegate RequestGenerator, UMcpProfile* SourceProfile);
	
protected:
	friend class UMcpProfileManager;

	/**
	 * Initialize this profile group with the account info for the player it represents
	 * @param OnlineSys The online subsystem pointer
	 * @param bIsServer Are we acting as a server for this player (determines which endpoint context we hit for QueryProfile and a few other commands)
	 * @param GameAccountId The epic account ID for the player this group represents
	 */
	virtual void Initialize(FOnlineSubsystemMcp* OnlineSys, bool bIsServer, const FUniqueNetIdWrapper& GameAccountId);

private:
	/** Generic delegate called for any ServiceRequest functions */
	void GenericResponse_HttpRequestComplete(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest, bool bSucceeded, FString FunctionName, FMcpQueryComplete Callback);

	/** This is called for ALL requests */
	void QueueRequest_Completed(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest, bool bSucceeded, FOnProcessRequestCompleteDelegate OriginalDelegate);

	/** Called when a request generator hits the front of the queue */
	void HandleRequestGenerator(const FProfilePendingHttpRequest& PendingRequest);

	/** Kick off http request from queue once ready */
	void SendRequestNow(const FProfilePendingHttpRequest& PendingRequest);

private:

	/** Pointer to the OSS given at init time. This may be NULL in nomcp mode. */
	FOnlineSubsystemMcp* OnlineSubMcp;
	
	/** List of profiles we have loaded for this group */
	UPROPERTY()
	TArray<FProfileEntry> ProfileList;

	/** Has Initialize been called (used for checks) */
	UPROPERTY()
	bool bIsInitialized;

	/** The display name for this player. This is set by SetPlayerName() and will be used in logging. */
	UPROPERTY()
	FString PlayerName;

	/** Are we acting in a server capacity */
	UPROPERTY()
	bool bIsServer;

	/** The account this profile belongs to */
	FUniqueNetIdWrapper GameAccountId;

	/** The account that is used for credentials on MCP requests (by default this is the same as GameAccountId) */
	FUniqueNetIdWrapper CredentialsAccountId;

	/** The CL/Build of the MCP we're contacting. This is updated every time we receive a reply. */
	UPROPERTY()
	FString LastMcpVersion;

	/** Timespan to add to local time to get to MCP server time */
	UPROPERTY()
	FTimespan LocalTimeOffset;

	/** All currently queued http requests that are pending execution */
	UPROPERTY()
	TArray<FProfilePendingHttpRequest> PendingRequests;

	/** True when we are in the middle of processing a query generation callback */
	UPROPERTY()
	bool bIsProcessingRequestGenerator;
};
