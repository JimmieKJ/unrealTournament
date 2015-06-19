// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "HttpPrivatePCH.h"

#include "HttpRetrySystem.h"

FHttpRetrySystem::FRequest::FRequest(TSharedRef<IHttpRequest>& HttpRequest, FHttpRetrySystem::FRetryLimitCountSetting InRetryLimitCountOverride, FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting InRetryTimeoutRelativeSecondsOverride)
    : FHttpRequestAdapterBase(HttpRequest)
    , Status(FHttpRetrySystem::FRequest::EStatus::NotStarted)
    , RetryLimitCountOverride(InRetryLimitCountOverride)
    , RetryTimeoutRelativeSecondsOverride(InRetryTimeoutRelativeSecondsOverride)
{
    // if the InRetryTimeoutRelativeSecondsOverride override is being used the value cannot be negative
    check(!(InRetryTimeoutRelativeSecondsOverride.bUseValue) || (InRetryTimeoutRelativeSecondsOverride.Value >= 0.0));
}

FHttpRetrySystem::FManager::FManager(FHttpRetrySystem::FRetryLimitCountSetting InRetryLimitCountDefault, FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting InRetryTimeoutRelativeSecondsDefault)
    : RandomFailureRate(FRandomFailureRateSetting::Unused())
    , RetryLimitCountDefault(InRetryLimitCountDefault)
    , RetryTimeoutRelativeSecondsDefault(InRetryTimeoutRelativeSecondsDefault)
{}

bool FHttpRetrySystem::FManager::ShouldRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

    FHttpResponsePtr Response = HttpRetryRequestEntry.HttpRequest->GetResponse();
    if (!Response.IsValid() || Response->GetResponseCode() == 0)
    {
        bResult = true;
    }

    return bResult;
}

bool FHttpRetrySystem::FManager::CanRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

    bool bShouldTestCurrentRetryCount = false;
    double RetryLimitCount = 0;
    if (HttpRetryRequestEntry.HttpRequest->RetryLimitCountOverride.bUseValue)
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = HttpRetryRequestEntry.HttpRequest->RetryLimitCountOverride.Value;
    }
    else if (RetryLimitCountDefault.bUseValue)
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = RetryLimitCountDefault.Value;
    }

    if (bShouldTestCurrentRetryCount)
    {
        if (HttpRetryRequestEntry.CurrentRetryCount < RetryLimitCount)
        {
            bResult = true;
        }
    }

    return bResult;
}

bool FHttpRetrySystem::FManager::HasTimedOut(const FHttpRetryRequestEntry& HttpRetryRequestEntry, const double NowAbsoluteSeconds)
{
    bool bResult = false;

    bool bShouldTestRetryTimeout = false;
    double RetryTimeoutAbsoluteSeconds = HttpRetryRequestEntry.RequestStartTimeAbsoluteSeconds;
    if (HttpRetryRequestEntry.HttpRequest->RetryTimeoutRelativeSecondsOverride.bUseValue)
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += HttpRetryRequestEntry.HttpRequest->RetryTimeoutRelativeSecondsOverride.Value;
    }
    else if (RetryTimeoutRelativeSecondsDefault.bUseValue)
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += RetryTimeoutRelativeSecondsDefault.Value;
    }

    if (bShouldTestRetryTimeout)
    {
        if (NowAbsoluteSeconds >= RetryTimeoutAbsoluteSeconds)
        {
            bResult = true;
        }
    }

    return bResult;
}

float FHttpRetrySystem::FManager::GetLockoutPeriodSeconds(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    float lockoutTime = 0.0f;

    if(HttpRetryRequestEntry.CurrentRetryCount >= 1)
    {
        lockoutTime = 5.0f + 5.0f * ((HttpRetryRequestEntry.CurrentRetryCount - 1) >> 1);
        lockoutTime = lockoutTime > 30.0f ? 30.0f : lockoutTime;
    }

    return lockoutTime;
}

static FRandomStream temp(4435261);

bool FHttpRetrySystem::FManager::Update(uint32* FileCount, uint32* FailingCount, uint32* FailedCount, uint32* CompletedCount)
{
	bool bIsGreen = true;

	if (FileCount != nullptr)
	{
		*FileCount = RequestList.Num();
	}

	const double NowAbsoluteSeconds = FPlatformTime::Seconds();

	// Basic algorithm
	// for each managed item
	//    if the item hasn't timed out
	//       if the item's retry state is NotStarted
	//          if the item's request's state is not NotStarted
	//             move the item's retry state to Processing
	//          endif
	//       endif
	//       if the item's retry state is Processing
	//          if the item's request's state is Failed
	//             flag return code to false
	//             if the item can be retried
	//                increment FailingCount if applicable
	//                retry the item's request
	//                increment the item's retry count
	//             else
	//                increment FailedCount if applicable
	//                set the item's retry state to FailedRetry
	//             endif
	//          else if the item's request's state is Succeeded
	//          endif
	//       endif
	//    else
	//       flag return code to false
	//       set the item's retry state to FailedTimeout
	//       increment FailedCount if applicable
	//    endif
	//    if the item's retry state is FailedRetry
	//       do stuff
	//    endif
	//    if the item's retry state is FailedTimeout
	//       do stuff
	//    endif
	//    if the item's retry state is Succeeded
	//       do stuff
	//    endif
	// endfor

	int32 index = 0;
	while (index < RequestList.Num())
	{
		FHttpRetryRequestEntry& HttpRetryRequestEntry = RequestList[index];
		TSharedRef<FHttpRetrySystem::FRequest>& HttpRetryRequest = HttpRetryRequestEntry.HttpRequest;

		EHttpRequestStatus::Type RequestStatus = HttpRetryRequest->HttpRequest->GetStatus();

        if (!HasTimedOut(HttpRetryRequestEntry, NowAbsoluteSeconds))
		{
            if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::NotStarted)
			{
				if (RequestStatus != EHttpRequestStatus::NotStarted)
				{
                    HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Processing;
				}
			}

            if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Processing)
			{
				bool forceFail = false;

                // Code to simulate request failure
                if (RequestStatus == EHttpRequestStatus::Succeeded && RandomFailureRate.bUseValue)
				{
					float random = temp.GetFraction();
                    if (random < RandomFailureRate.Value)
					{
						forceFail = true;
					}
				}

                if ((RequestStatus == EHttpRequestStatus::Failed) || forceFail)
				{
					bIsGreen = false;
                    if(HttpRetryRequestEntry.bShouldCancel == false)
                    {
                        if (forceFail || (ShouldRetry(HttpRetryRequestEntry) && CanRetry(HttpRetryRequestEntry)))
					    {
                            float lockoutPeriod = GetLockoutPeriodSeconds(HttpRetryRequestEntry);

                            if(lockoutPeriod > 0.0f)
                            {
                                UE_LOG(LogHttp, Warning, TEXT("Lockout of %fs on %s"), lockoutPeriod, *(HttpRetryRequest->GetURL()));
                            }

                            HttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds = NowAbsoluteSeconds + lockoutPeriod;
                            HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::ProcessingLockout;
					    }
					    else
					    {
						    UE_LOG(LogHttp, Warning, TEXT("Retry exhausted on %s"), *(HttpRetryRequest->GetURL()));
						    if (FailedCount != nullptr)
						    {
							    ++(*FailedCount);
						    }
                            HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedRetry;
					    }
                    }
                    else
                    {
                        UE_LOG(LogHttp, Warning, TEXT("Request cancelled on %s"), *(HttpRetryRequest->GetURL()));
                        HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Cancelled;
                    }
                }
				else if (RequestStatus == EHttpRequestStatus::Succeeded)
				{
					if (HttpRetryRequestEntry.CurrentRetryCount > 0)
					{
						UE_LOG(LogHttp, Warning, TEXT("Success on %s"), *(HttpRetryRequest->GetURL()));
					}

					if (CompletedCount != nullptr)
					{
						++(*CompletedCount);
					}

                    HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Succeeded;
				}
			}

            if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::ProcessingLockout)
            {
                if (NowAbsoluteSeconds >= HttpRetryRequestEntry.LockoutEndTimeAbsoluteSeconds)
                {
                    // if this fails the HttpRequest's state will be failed which will cause the retry logic to kick(as expected)
                    bool success = HttpRetryRequest->HttpRequest->ProcessRequest();
                    if (success)
                    {
                        UE_LOG(LogHttp, Warning, TEXT("Retry %d on %s"), HttpRetryRequestEntry.CurrentRetryCount + 1, *(HttpRetryRequest->GetURL()));

                        ++HttpRetryRequestEntry.CurrentRetryCount;
                        HttpRetryRequest->Status = FRequest::EStatus::Processing;
                    }
                }

                if (FailingCount != nullptr)
                {
                    ++(*FailingCount);
                }
            }
        }
		else
		{
			UE_LOG(LogHttp, Warning, TEXT("Timeout on %s"), HttpRetryRequestEntry.CurrentRetryCount + 1, *(HttpRetryRequest->GetURL()));
			bIsGreen = false;
            HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedTimeout;
			if (FailedCount != nullptr)
			{
				++(*FailedCount);
			}
		}

		bool bWasCompleted = false;
		bool bWasSuccessful = false;

        if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Cancelled ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedRetry ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedTimeout ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded)
		{
			bWasCompleted = true;
            bWasSuccessful = HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded;
		}

		if (bWasCompleted)
		{
			HttpRetryRequest->OnProcessRequestComplete().ExecuteIfBound(HttpRetryRequest, bWasSuccessful);
		}

        if(bWasSuccessful)
        {
            if(CompletedCount != nullptr)
            {
                ++(*CompletedCount);
            }
        }

		if (bWasCompleted)
		{
			RequestList.RemoveAtSwap(index);
		}
		else
		{
			++index;
		}
	}

	return bIsGreen;
}

FHttpRetrySystem::FManager::FHttpRetryRequestEntry::FHttpRetryRequestEntry(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest)
    : bShouldCancel(false)
    , CurrentRetryCount(0)
    , RequestStartTimeAbsoluteSeconds(0.0)
    , HttpRequest(InHttpRequest)
{}

bool FHttpRetrySystem::FManager::ProcessRequest(TSharedRef<FHttpRetrySystem::FRequest>& HttpRequest)
{
	bool bResult = HttpRequest->ProcessRequest();

	if (bResult)
	{
        RequestList.Add(FHttpRetryRequestEntry(HttpRequest));
	}

	return bResult;
}

void FHttpRetrySystem::FManager::CancelRequest(TSharedRef<FHttpRetrySystem::FRequest>& HttpRequest)
{
    for (int32 i = 0; i < RequestList.Num(); ++i)
    {
        FHttpRetryRequestEntry& EntryRef = RequestList[i];

        if(EntryRef.HttpRequest == HttpRequest)
        {
            EntryRef.bShouldCancel = true;
        }
    }
    HttpRequest->CancelRequest();

    /*
    if (bResult)
    {
        RequestList.Add(FHttpRetryRequestEntry(HttpRequest));
    }

    return bResult;
    */
}
