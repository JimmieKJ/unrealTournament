// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Runtime/Analytics/AnalyticsET/Private/AnalyticsETPrivatePCH.h"

#include "Core.h"
#include "Guid.h"
#include "Json.h"
#include "SecureHash.h"
#include "Runtime/Analytics/AnalyticsET/Public/AnalyticsET.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Online/HTTP/Public/Http.h"
#include "EngineVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Display, All);

IMPLEMENT_MODULE( FAnalyticsET, AnalyticsET );

class FAnalyticsProviderET : 
	public IAnalyticsProvider,
	public FTickerObjectBase
{
public:
	FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues);

	// FTickerObjectBase

	bool Tick(float DeltaSeconds) override;

	// IAnalyticsProvider

	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;

	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;

	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;

	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;

	virtual ~FAnalyticsProviderET();

	FString GetAPIKey() const { return APIKey; }

private:
	bool bSessionInProgress;
	/** ET Game API Key - Get from your account manager */
	FString APIKey;
	/** ET API Server */
	FString APIServer;
	/** the unique UserID as passed to ET. */
	FString UserID;
	/** The session ID */
	FString SessionID;
	/** Cached build type as a string */
	FString BuildType;
	/** The AppVersion passed to ET. */
	FString AppVersion;
	/** True if we are sending to the data router*/
	bool UseDataRouter;
	/** The URL to which uploads are sent when using the data router*/
	FString DataRouterUploadURL;
	/** Max number of analytics events to cache before pushing to server */
	const int32 MaxCachedNumEvents;
	/** Max time that can elapse before pushing cached events to server */
	const float MaxCachedElapsedTime;
	/** Allows events to not be cached when -AnalyticsDisableCaching is used. This should only be used for debugging as caching significantly reduces bandwidth overhead per event. */
	bool bShouldCacheEvents;
	/** Current countdown timer to keep track of MaxCachedElapsedTime push */
	float FlushEventsCountdown;
	/**
	 * Analytics event entry to be cached
	 */
	struct FAnalyticsEventEntry
	{
		/** name of event */
		FString EventName;
		/** optional list of attributes */
		TArray<FAnalyticsEventAttribute> Attributes;
		/** local time when event was triggered */
		FDateTime TimeStamp;
		/**
		 * Constructor
		 */
		FAnalyticsEventEntry(const FString& InEventName, const TArray<FAnalyticsEventAttribute>& InAttributes, FDateTime InTimeStamp=FDateTime::UtcNow())
			: EventName(InEventName)
			, Attributes(InAttributes)
			, TimeStamp(InTimeStamp)
		{}
	};
	/** List of analytic events pending a server update */
	TArray<FAnalyticsEventEntry> CachedEvents;

	/** Critical section for updating the CachedEvents */
	FCriticalSection CachedEventsCS;

	/**
	 * Delegate called when an event Http request completes
	 */
	void EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	/**
	 * Delegate called when an event Http request completes (for DataRouter)
	 */
	void EventRequestCompleteDataRouter(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
};

void FAnalyticsET::StartupModule()
{
	// Make sure http is loaded so that we can flush events during module shutdown
	FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");
}

void FAnalyticsET::ShutdownModule()
{
}

TSharedPtr<IAnalyticsProvider> FAnalyticsET::CreateAnalyticsProvider(const FAnalytics::FProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		Config ConfigValues;
		ConfigValues.APIKeyET = GetConfigValue.Execute(Config::GetKeyNameForAPIKey(), true);
		ConfigValues.APIServerET = GetConfigValue.Execute(Config::GetKeyNameForAPIServer(), false);
		ConfigValues.AppVersionET = GetConfigValue.Execute(Config::GetKeyNameForAppVersion(), false);
		ConfigValues.UseDataRouterET = GetConfigValue.Execute(Config::GetKeyNameForUseDataRouter(), false);
		ConfigValues.DataRouterUploadURLET = GetConfigValue.Execute(Config::GetKeyNameForDataRouterUploadURL(), false);
		return CreateAnalyticsProvider(ConfigValues);
	}
	else
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider called with an unbound delegate"));
	}
	return NULL;
}

TSharedPtr<IAnalyticsProvider> FAnalyticsET::CreateAnalyticsProvider(const Config& ConfigValues) const
{
	// If we didn't have a proper APIKey, return NULL
	if (ConfigValues.APIKeyET.IsEmpty())
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider config not contain required parameter %s"), *Config::GetKeyNameForAPIKey());
		return NULL;
	}
	return TSharedPtr<IAnalyticsProvider>(new FAnalyticsProviderET(ConfigValues));
}

/**
 * Perform any initialization.
 */
FAnalyticsProviderET::FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues)
	:bSessionInProgress(false)
	, MaxCachedNumEvents(20)
	, MaxCachedElapsedTime(60.0f)
	, bShouldCacheEvents(!FParse::Param(FCommandLine::Get(), TEXT("ANALYTICSDISABLECACHING")))
	, FlushEventsCountdown(MaxCachedElapsedTime)
{
	// if we are not caching events, we are operating in debug mode. Turn on super-verbose analytics logging
	if (!bShouldCacheEvents)
	{
		UE_SET_LOG_VERBOSITY(LogAnalytics, VeryVerbose);
	}

	APIKey = ConfigValues.APIKeyET;
	UE_LOG(LogAnalytics, Verbose, TEXT("[%s] Initializing ET Analytics provider"), *APIKey);

	// allow the APIServer value to be empty and use defaults.
	APIServer = ConfigValues.APIServerET.IsEmpty() 
		? FAnalyticsET::Config::GetDefaultAPIServer()
		: ConfigValues.APIServerET;

	// allow the DataRouterUploadURL value to be empty and use defaults.
	DataRouterUploadURL = ConfigValues.DataRouterUploadURLET.IsEmpty()
		? FAnalyticsET::Config::GetDefaultDataRouterUploadURL()
		: ConfigValues.DataRouterUploadURLET;

	// determine if we are using the data router
	UseDataRouter = FCString::ToBool(*ConfigValues.UseDataRouterET);

	// default to GEngineVersion if one is not provided, append GEngineVersion otherwise.
	FString ConfigAppVersion = ConfigValues.AppVersionET;
	// Allow the cmdline to force a specific AppVersion so it can be set dynamically.
	FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSAPPVERSION="), ConfigAppVersion, false);
	AppVersion = ConfigAppVersion.IsEmpty() 
		? GEngineVersion.ToString() 
		: ConfigAppVersion.Replace(TEXT("%VERSION%"), *GEngineVersion.ToString(), ESearchCase::CaseSensitive);

	UE_LOG(LogAnalytics, Log, TEXT("[%s] APIServer = %s. AppVersion = %s"), *APIKey, *APIServer, *AppVersion);
	if (UseDataRouter)
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] DataRouterUploadURL = %s. AppVersion = %s"), *APIKey, *DataRouterUploadURL, *AppVersion);
	}

	// cache the build type string
	FAnalytics::BuildType BuildTypeEnum = FAnalytics::Get().GetBuildType();
	BuildType = 
		BuildTypeEnum == FAnalytics::Debug 
			? "Debug"
			: BuildTypeEnum == FAnalytics::Development
				? "Development"
				: BuildTypeEnum == FAnalytics::Release
					? "Release"
					: BuildTypeEnum == FAnalytics::Test
						? "Test"
						: "UNKNOWN";

	// see if there is a cmdline supplied UserID.
#if !UE_BUILD_SHIPPING
	FString ConfigUserID;
	if (FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), ConfigUserID, false))
	{
		SetUserID(ConfigUserID);
	}
#endif // !UE_BUILD_SHIPPING
}

bool FAnalyticsProviderET::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnalyticsProviderET_Tick);

	if (CachedEvents.Num() > 0)
	{
		// Countdown to flush
		FlushEventsCountdown -= DeltaSeconds;
		// If reached countdown or already at max cached events then flush
		if (FlushEventsCountdown <= 0 ||
			CachedEvents.Num() >= MaxCachedNumEvents)
		{
			FlushEvents();
		}
	}
	return true;
}

FAnalyticsProviderET::~FAnalyticsProviderET()
{
	UE_LOG(LogAnalytics, Verbose, TEXT("[%s] Destroying ET Analytics provider"), *APIKey);
	EndSession();
}

/**
 * Start capturing stats for upload
 * Uses the unique ApiKey associated with your app
 */
bool FAnalyticsProviderET::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	UE_LOG(LogAnalytics, Log, TEXT("[%s] AnalyticsET::StartSession"),*APIKey);

	// end/flush previous session before staring new one
	if (bSessionInProgress)
	{
		EndSession();
	}

	FGuid SessionGUID;
	FPlatformMisc::CreateGuid(SessionGUID);
	SessionID = SessionGUID.ToString(EGuidFormats::DigitsWithHyphensInBraces);

	// always ensure we send a few specific attributes on session start.
	TArray<FAnalyticsEventAttribute> AppendedAttributes(Attributes);
	// this is for legacy reasons (we used to use this ID, so helps us create old->new mappings).
	AppendedAttributes.Emplace(TEXT("UniqueDeviceId"), FPlatformMisc::GetUniqueDeviceId());
	// we should always know what platform is hosting this session.
	AppendedAttributes.Emplace(TEXT("Platform"), FString(FPlatformProperties::IniPlatformName()));

	RecordEvent(TEXT("SessionStart"), AppendedAttributes);
	bSessionInProgress = !UserID.IsEmpty();
	return bSessionInProgress;
}

/**
 * End capturing stats and queue the upload 
 */
void FAnalyticsProviderET::EndSession()
{
	if (bSessionInProgress)
	{
		RecordEvent(TEXT("SessionEnd"), TArray<FAnalyticsEventAttribute>());
		FlushEvents();
		SessionID.Empty();
	}
	bSessionInProgress = false;
}

void FAnalyticsProviderET::FlushEvents()
{
	// Make sure we don't try to flush too many times. When we are not caching events it's possible this can be called when there are no events in the array.
	if (CachedEvents.Num() == 0)
	{
		return;
	}

	// There are much better ways to do this, but since most events are recorded and handled on the same (game) thread,
	// this is probably mostly fine for now, and simply favoring not crashing at the moment
	FScopeLock ScopedLock(&CachedEventsCS);

	if(ensure(FModuleManager::Get().IsModuleLoaded("HTTP")))
	{
		FString Payload;

		FDateTime CurrentTime = FDateTime::UtcNow();

		TSharedRef< TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR> > > JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >::Create(&Payload);
		JsonWriter->WriteObjectStart();
		if (UseDataRouter)
		{
			JsonWriter->WriteValue(TEXT("SessionID"), SessionID);
			JsonWriter->WriteValue(TEXT("UserID"), UserID);
		}
		JsonWriter->WriteArrayStart(TEXT("Events"));
		for (int32 EventIdx = 0; EventIdx < CachedEvents.Num(); EventIdx++)
		{
			const FAnalyticsEventEntry& Entry = CachedEvents[EventIdx];
			// event entry
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("EventName"), Entry.EventName);
			FString DateOffset = (CurrentTime - Entry.TimeStamp).ToString();
			JsonWriter->WriteValue(TEXT("DateOffset"), DateOffset);
			JsonWriter->WriteValue(TEXT("IsEditor"), FString::FromInt(GIsEditor));
			if (Entry.Attributes.Num() > 0)
			{
				// optional attributes for this event
				for (int32 AttrIdx = 0; AttrIdx < Entry.Attributes.Num(); AttrIdx++)
				{
					const FAnalyticsEventAttribute& Attr = Entry.Attributes[AttrIdx];				
					JsonWriter->WriteValue(Attr.AttrName, Attr.AttrValue);
				}
			}
			JsonWriter->WriteObjectEnd();
		}
		JsonWriter->WriteArrayEnd();
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		FString URLPath = FString::Printf(TEXT("CollectData.1?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s"),
			*FGenericPlatformHttp::UrlEncode(SessionID),
			*FGenericPlatformHttp::UrlEncode(APIKey),
			*FGenericPlatformHttp::UrlEncode(AppVersion),
			*FGenericPlatformHttp::UrlEncode(UserID));

		// Recreate the URLPath for logging because we do not want to escape the parameters when logging.
		// We cannot simply UrlEncode the entire Path after logging it because UrlEncode(Params) != UrlEncode(Param1) & UrlEncode(Param2) ...
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] AnalyticsET URL:CollectData.1?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s. Payload:%s"), 
			*APIKey, 
			*SessionID,
			*APIKey,
			*AppVersion,
			*UserID,
			*Payload);

		// Create/send Http request for an event
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));

		HttpRequest->SetURL(APIServer + URLPath);
		HttpRequest->SetVerb(TEXT("POST"));
		HttpRequest->SetContentAsString(Payload);
		HttpRequest->OnProcessRequestComplete().BindRaw(this, &FAnalyticsProviderET::EventRequestComplete);

 		HttpRequest->ProcessRequest();

		if (UseDataRouter)
		{
			// If we're using the DataRouter backend, then submit the same request to the new DataRouter backend
			// NOTE - This branch is temp, and we will eventually use the DataRouter path exclusively

			// Create/send Http request for an event
			HttpRequest = FHttpModule::Get().CreateRequest();
			HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));

			// TODO need agent here??
			HttpRequest->SetURL(
				FString::Printf(TEXT("%s?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s&IsEditor=%s&AppEnvironment=%s&UploadType=eteventstream"),
				*DataRouterUploadURL,
				*FGenericPlatformHttp::UrlEncode(SessionID),
				*FGenericPlatformHttp::UrlEncode(APIKey), 
				*FGenericPlatformHttp::UrlEncode(AppVersion),
				*FGenericPlatformHttp::UrlEncode(UserID),
				*FGenericPlatformHttp::UrlEncode(FString::FromInt(GIsEditor)),
				*FGenericPlatformHttp::UrlEncode(BuildType)
			));

			HttpRequest->SetVerb(TEXT("POST"));
			HttpRequest->SetContentAsString(Payload);
			HttpRequest->OnProcessRequestComplete().BindRaw(this, &FAnalyticsProviderET::EventRequestCompleteDataRouter);
			HttpRequest->ProcessRequest();
		}

		FlushEventsCountdown = MaxCachedElapsedTime;
		CachedEvents.Empty();
	}
}

void FAnalyticsProviderET::SetUserID(const FString& InUserID)
{
	// command-line specified user ID overrides all attempts to reset it.
	if (!FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), UserID, false))
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] SetUserId %s"), *APIKey, *InUserID);
		UserID = InUserID;
	}
	else if (UserID != InUserID)
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] Overriding SetUserId %s with cmdline UserId of %s."), *APIKey, *InUserID, *UserID);
	}
}

FString FAnalyticsProviderET::GetUserID() const
{
	return UserID;
}

FString FAnalyticsProviderET::GetSessionID() const
{
	return SessionID;
}

bool FAnalyticsProviderET::SetSessionID(const FString& InSessionID)
{
	if (bSessionInProgress)
	{
		SessionID = InSessionID;
		UE_LOG(LogAnalytics, Log, TEXT("[%s] Forcing SessionID to %s."), *APIKey, *SessionID);
		return true;
	}
	return false;
}

/** Helper to log any ET event. Used by all the LogXXX functions. */
void FAnalyticsProviderET::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	// There are much better ways to do this, but since most events are recorded and handled on the same (game) thread,
	// this is probably mostly fine for now, and simply favoring not crashing at the moment
	FScopeLock ScopedLock(&CachedEventsCS);
	CachedEvents.Add(FAnalyticsEventEntry(EventName, Attributes));
	// if we aren't caching events, flush immediately. This is really only for debugging as it will significantly affect bandwidth.
	if (!bShouldCacheEvents)
	{
		FlushEvents();
	}
}


void FAnalyticsProviderET::EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid())
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET response for [%s]. Code: %d. Payload: %s"), *APIKey, *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
	}
	else
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET response for [%s]. No response"), *APIKey, *HttpRequest->GetURL());
	}
}

void FAnalyticsProviderET::EventRequestCompleteDataRouter(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (bSucceeded && HttpResponse.IsValid())
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET (DataRouter) response for [%s]. Code: %d. Payload: %s"), *APIKey, *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
	}
	else
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET (DataRouter) response for [%s]. No response"), *APIKey, *HttpRequest->GetURL());
	}
}
