// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HttpRequestAdapter.h"

/**
 * Helpers of various types for the retry system
 */
namespace FHttpRetrySystem
{
    typedef uint32 RetryLimitCountType;
    typedef double RetryTimeoutRelativeSecondsType;

    inline RetryLimitCountType             RetryLimitCount(uint32 Value)             { return Value; }
    inline RetryTimeoutRelativeSecondsType RetryTimeoutRelativeSeconds(double Value) { return Value; }

    template <typename  IntrinsicType>
    IntrinsicType TZero();

    template <> inline float                           TZero<float>()                           { return 0.0f; }
    template <> inline RetryLimitCountType             TZero<RetryLimitCountType>()             { return RetryLimitCount(0); }
    template <> inline RetryTimeoutRelativeSecondsType TZero<RetryTimeoutRelativeSecondsType>() { return RetryTimeoutRelativeSeconds(0.0); }

    /**
     * TOptionalSetting merges a bool and an intrinsic value to remove the need
     * for having special values to indicate if the option is valid
     */
    template <typename IntrinsicType>
    struct TOptionalSetting
    {
        TOptionalSetting() : bUseValue(false), Value(TZero<IntrinsicType>())                          {}
        explicit TOptionalSetting(IntrinsicType InValue) : bUseValue(true), Value(InValue)                     {}
        TOptionalSetting(const TOptionalSetting& InValue) : bUseValue(InValue.bUseValue), Value(InValue.Value) {}

        static TOptionalSetting Unused()                       { return TOptionalSetting(); }
        static TOptionalSetting Create(IntrinsicType InValue)  { return TOptionalSetting(InValue); }

        bool            bUseValue;
        IntrinsicType   Value;
    };

    typedef TOptionalSetting<float>                           FRandomFailureRateSetting;
    typedef TOptionalSetting<RetryLimitCountType>             FRetryLimitCountSetting;
    typedef TOptionalSetting<RetryTimeoutRelativeSecondsType> FRetryTimeoutRelativeSecondsSetting;
	typedef TSet<int32> FRetryResponseCodes;
};


namespace FHttpRetrySystem
{
    /**
     * class FRequest is what the retry system accepts as inputs
     */
    class FRequest : public FHttpRequestAdapterBase
    {
    public:
        struct EStatus
        {
            enum Type
            {
                NotStarted = 0,
                Processing,
                ProcessingLockout,
                Cancelled,
                FailedRetry,
                FailedTimeout,
                Succeeded
            };
        };

        /**
        * Delegate called when an FRequest completes
        *
        * @param first parameter -  original FRequest
        * @param second parameter - indicates whether or not the request completed successfully
        */
        DECLARE_DELEGATE_TwoParams(FOnProcessRequestCompleteDelegate, TSharedRef<class FRequest>&, bool);

    public:
        HTTP_API FRequest(
			const TSharedRef<IHttpRequest>& HttpRequest,
			const FRetryLimitCountSetting& InRetryLimitCountOverride = FRetryLimitCountSetting::Unused(),
			const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride = FRetryTimeoutRelativeSecondsSetting::Unused(),
			const FRetryResponseCodes& InRetryResponseCodes = FRetryResponseCodes());

        EStatus::Type                      GetStatus()                { return Status; }
        FOnProcessRequestCompleteDelegate& OnProcessRequestComplete() { return OnProcessRequestCompleteDelegate; }

    protected:
        friend class FManager;

        bool ProcessRequest() { return HttpRequest->ProcessRequest(); }
        void CancelRequest()  { HttpRequest->CancelRequest(); }

        EStatus::Type                        Status;

        FRetryLimitCountSetting              RetryLimitCountOverride;
        FRetryTimeoutRelativeSecondsSetting  RetryTimeoutRelativeSecondsOverride;
		FRetryResponseCodes					 RetryResponseCodesOverride;

        FOnProcessRequestCompleteDelegate    OnProcessRequestCompleteDelegate;
    };
}

namespace FHttpRetrySystem
{
    class FManager
    {
    public:
        // FManager
		HTTP_API FManager(const FRetryLimitCountSetting& InRetryLimitCountDefault, const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault, const FRetryResponseCodes& InRetryResponseCodesDefault);

        /**
         * Updates the entries in the list of retry requests. Optional parameters are for future connection health assessment
         *
         * @param FileCount       optional parameter that will be filled with the total files updated
         * @param FailingCount    optional parameter that will be filled with the total files that have are in a retrying state
         * @param FailedCount     optional parameter that will be filled with the total files that have failed
         * @param CompletedCount  optional parameter that will be filled with the total files that have completed
         *
         * @return                true if there are no failures or retries
         */
        HTTP_API bool Update(uint32* FileCount = NULL, uint32* FailingCount = NULL, uint32* FailedCount = NULL, uint32* CompletedCount = NULL);

        HTTP_API bool ProcessRequest(TSharedRef<FRequest>& HttpRequest);
        HTTP_API void CancelRequest (TSharedRef<FRequest>& HttpRequest);

        void SetRandomFailureRate(float Value) { RandomFailureRate = FRandomFailureRateSetting::Create(Value); }

    protected:
        struct FHttpRetryRequestEntry
        {
            FHttpRetryRequestEntry(TSharedRef<FRequest>& HttpRequestValue);

            bool                    bShouldCancel;
            uint32                  CurrentRetryCount;
            double                  RequestStartTimeAbsoluteSeconds;
            double                  LockoutEndTimeAbsoluteSeconds;

            TSharedRef<FRequest>	HttpRequest;
        };

        // @return true if there is a no formal response to the request
        // @TODO return true if a variety of 5xx errors are the result of a formal response
        bool ShouldRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry);

        // @return true if retry chances have not been exhausted
        bool CanRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry);

        // @return true if the retry request has timed out
        bool HasTimedOut(const FHttpRetryRequestEntry& HttpRetryRequestEntry, const double NowAbsoluteSeconds);

        // @return number of seconds to lockout for
        float GetLockoutPeriodSeconds(const FHttpRetryRequestEntry& HttpRetryRequestEntry);

        // Default configuration for the retry system
        FRandomFailureRateSetting            RandomFailureRate;
        FRetryLimitCountSetting              RetryLimitCountDefault;
        FRetryTimeoutRelativeSecondsSetting  RetryTimeoutRelativeSecondsDefault;
		FRetryResponseCodes					 RetryResponseCodesDefault;

        TArray<FHttpRetryRequestEntry>        RequestList;
    };
}