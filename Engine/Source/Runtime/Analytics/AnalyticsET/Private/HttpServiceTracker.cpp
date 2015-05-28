// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AnalyticsETPrivatePCH.h"
#include "HttpServiceTracker.h"
#include "AnalyticsEventAttribute.h"
#include "AnalyticsET.h"
#include "IHttpResponse.h"

bool FHttpServiceTracker::Tick(float DeltaTime)
{
	// flush events at the specified interval.
	if (FPlatformTime::Seconds() > NextFlushTime)
	{
		if (AnalyticsProvider.IsValid())
		{
			TArray<FAnalyticsEventAttribute> Attrs;
			Attrs.Reserve(10);
			// one event per endpoint.
			for (const auto& MetricsMapPair : EndpointMetricsMap)
			{
				Attrs.Reset();
				Attrs.Emplace(TEXT("FailCount"), MetricsMapPair.Value.FailCount);
				Attrs.Emplace(TEXT("SuccessCount"), MetricsMapPair.Value.SuccessCount);
				// We may have had no successful requests, so these values would be undefined.
				if (MetricsMapPair.Value.SuccessCount > 0)
				{
					Attrs.Emplace(TEXT("DownloadBytesSuccessTotal"), MetricsMapPair.Value.DownloadBytesSuccessTotal);
					Attrs.Emplace(TEXT("ElapsedTimeSuccessTotal"), MetricsMapPair.Value.ElapsedTimeSuccessTotal);
					Attrs.Emplace(TEXT("ElapsedTimeSuccessMin"), MetricsMapPair.Value.ElapsedTimeSuccessMin);
					Attrs.Emplace(TEXT("ElapsedTimeSuccessMax"), MetricsMapPair.Value.ElapsedTimeSuccessMax);
				}
				if (MetricsMapPair.Value.FailCount > 0)
				{
					Attrs.Emplace(TEXT("DownloadBytesFailTotal"), MetricsMapPair.Value.DownloadBytesFailTotal);
					Attrs.Emplace(TEXT("ElapsedTimeFailTotal"), MetricsMapPair.Value.ElapsedTimeFailTotal);
					Attrs.Emplace(TEXT("ElapsedTimeFailMin"), MetricsMapPair.Value.ElapsedTimeFailMin);
					Attrs.Emplace(TEXT("ElapsedTimeFailMax"), MetricsMapPair.Value.ElapsedTimeFailMax);
				}
				// one attribute per response code.
				for (const auto& ResponseCodeMapPair : MetricsMapPair.Value.ResponseCodes)
				{
					Attrs.Emplace(FString(TEXT("Code-")) + LexicalConversion::ToString(ResponseCodeMapPair.Key), ResponseCodeMapPair.Value);
				}
				AnalyticsProvider->RecordEvent(MetricsMapPair.Key.ToString(), Attrs);
			}
			// force an immediate flush always. We already summarized.
			AnalyticsProvider->FlushEvents();
		}
		EndpointMetricsMap.Reset();
		NextFlushTime += FlushIntervalSec;
	}
	return true;
}

void FHttpServiceTracker::TrackRequest(const FHttpRequestPtr& Request, FName EndpointName)
{
	auto& Metrics = EndpointMetricsMap.FindOrAdd(EndpointName);
	Metrics.TrackRequest(Request);
}

FHttpServiceTracker::FHttpServiceTracker(const FHttpServiceTrackerConfig& Config) 
	: FlushIntervalSec((float)Config.AggregationInterval.GetTotalSeconds())
	, NextFlushTime(FPlatformTime::Seconds() + FlushIntervalSec)
{
	FAnalyticsET::Config AnalyticsConfig;
	AnalyticsConfig.APIKeyET = Config.APIKey;
	AnalyticsConfig.APIServerET = Config.APIServer;
	AnalyticsConfig.AppVersionET = Config.ApiVersion;
	AnalyticsProvider = FAnalyticsET::Get().CreateAnalyticsProvider(AnalyticsConfig);
	// we'll just use the MachineID for the User. It actually won't matter much for this service.
	AnalyticsProvider->SetUserID(FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits).ToLower());
	// Note we also don't start/stop the session. The AnalyticsET provider allows this, and this enables our collector
	// to receive ONLY monitoring events.
}

bool FHttpServiceTracker::EndpointMetrics::IsSuccessfulResponse(int32 ResponseCode)
{
	return ResponseCode >= 200 && ResponseCode < 400;
}

void FHttpServiceTracker::EndpointMetrics::TrackRequest(const FHttpRequestPtr& HttpRequest)
{
	if(HttpRequest.IsValid())
	{
		// keep a histogram of response codes
		const FHttpResponsePtr HttpResponse = HttpRequest->GetResponse();

		const int32 ResponseCode = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0;
		// track all responses in a histogram
		ResponseCodes.FindOrAdd(ResponseCode)++;
		const float ElapsedTime = HttpRequest->GetElapsedTime();
		const int64 DownloadBytes = HttpResponse.IsValid() ? HttpResponse->GetContent().Num() : 0;
		// track successes/fails separately
		if (IsSuccessfulResponse(ResponseCode))
		{
			++SuccessCount;
			// sum elapsed time for average calc
			ElapsedTimeSuccessTotal += ElapsedTime;
			ElapsedTimeSuccessMax = FMath::Max(ElapsedTimeSuccessMax, ElapsedTime);
			ElapsedTimeSuccessMin = FMath::Min(ElapsedTimeSuccessMin, ElapsedTime);
			// sum download rate for average calc
			DownloadBytesSuccessTotal += DownloadBytes;
		}
		else
		{
			++FailCount;
			// sum elapsed time for average calc
			ElapsedTimeFailTotal += ElapsedTime;
			ElapsedTimeFailMax = FMath::Max(ElapsedTimeFailMax, ElapsedTime);
			ElapsedTimeFailMin = FMath::Min(ElapsedTimeFailMin, ElapsedTime);
			// sum download rate for average calc
			DownloadBytesFailTotal += DownloadBytes;
		}
	}
}

FHttpServiceTracker::EndpointMetrics::EndpointMetrics() : DownloadBytesSuccessTotal(0L)
, ElapsedTimeSuccessTotal(0.0f)
, ElapsedTimeSuccessMin(FLT_MAX)
, ElapsedTimeSuccessMax(-FLT_MAX)
, DownloadBytesFailTotal(0L)
, ElapsedTimeFailTotal(0.0f)
, ElapsedTimeFailMin(FLT_MAX)
, ElapsedTimeFailMax(-FLT_MAX)
, SuccessCount(0)
, FailCount(0)
{

}
