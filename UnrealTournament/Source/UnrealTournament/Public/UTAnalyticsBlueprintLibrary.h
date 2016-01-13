// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTAnalyticsBlueprintLibrary.generated.h"

/** Blueprint accessible version of the analytics event struct */
USTRUCT(BlueprintType)
struct FUTAnalyticsEventAttr
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UTAnalytics")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UTAnalytics")
	FString Value;
};

UCLASS()
class UUTAnalyticsBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Starts an analytics session without any custom attributes specified */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static bool StartSession();

	/** Starts an analytics session with custom attributes specified */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static bool StartSessionWithAttributes(const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Ends an analytics session */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void EndSession();

	/** Requests that any cached events be sent immediately */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void FlushEvents();

	/** Records an event has happened by name without any attributes (an event counter) */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordEvent(const FString& EventName);

	/** Records an event has happened by name with a single attribute */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordEventWithAttribute(const FString& EventName, const FString& AttributeName, const FString& AttributeValue);

	/** Records an event has happened by name with a single attribute */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordEventWithAttributes(const FString& EventName, const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Records an in-game item was purchased using the specified in-game currency */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordItemPurchase(const FString& ItemId, const FString& Currency, int32 PerItemCost, int32 ItemQuantity);

	/** Records an in-game item was purchased */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordSimpleItemPurchase(const FString& ItemId, int32 ItemQuantity);

	/** Records an in-game item was purchased with attributes */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordSimpleItemPurchaseWithAttributes(const FString& ItemId, int32 ItemQuantity, const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Records an in-game currency was purchased using real-world money */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordSimpleCurrencyPurchase(const FString& GameCurrencyType, int32 GameCurrencyAmount);

	/** Records an in-game currency was purchased using real-world money */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordSimpleCurrencyPurchaseWithAttributes(const FString& GameCurrencyType, int32 GameCurrencyAmount, const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Records an in-game currency was purchased using real-world money */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordCurrencyPurchase(const FString& GameCurrencyType, int32 GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider);

	/** Records an in-game currency was granted by the game with no real-world money being involved */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordCurrencyGiven(const FString& GameCurrencyType, int32 GameCurrencyAmount);

	/** Records an in-game currency was granted by the game with no real-world money being involved */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordCurrencyGivenWithAttributes(const FString& GameCurrencyType, int32 GameCurrencyAmount, const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Builds a struct from the attribute name and value */
	UFUNCTION(BlueprintPure, Category="UTAnalytics")
	static FUTAnalyticsEventAttr MakeEventAttribute(const FString& AttributeName, const FString& AttributeValue);

	/** Gets the current session id from the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static FString GetSessionId();

	/** Sets the session id (if supported) on the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void SetSessionId(const FString& SessionId);

	/** Gets the current user id from the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static FString GetUserId();

	/** Sets the user id (if supported) on the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void SetUserId(const FString& UserId);

	/** Sets the user's age (if supported) on the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void SetAge(int32 Age);

	/** Sets the user's location (if supported) on the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void SetLocation(const FString& Location);

	/** Sets the user's gender (if supported) on the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void SetGender(const FString& Gender);

	/** Sets the game's build info (if supported) on the analytics provider */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void SetBuildInfo(const FString& BuildInfo);

	/** Records an error event has happened with attributes */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordErrorWithAttributes(const FString& Error, const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Records an error event has happened */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordError(const FString& Error);

	/** Records a user progress event has happened with a full list of progress hierarchy names and with attributes */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordProgressWithFullHierarchyAndAttributes(const FString& ProgressType, const TArray<FString>& ProgressNames, const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Records a user progress event has happened with attributes */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordProgressWithAttributes(const FString& ProgressType, const FString& ProgressName, const TArray<FUTAnalyticsEventAttr>& Attributes);

	/** Records a user progress event has happened */
	UFUNCTION(BlueprintCallable, Category="UTAnalytics")
	static void RecordProgress(const FString& ProgressType, const FString& ProgressName);
};
