// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#include "UTAnalytics.h"
#include "UTAnalyticsBlueprintLibrary.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"

DEFINE_LOG_CATEGORY_STATIC(LogUTAnalyticsBPLib, Display, All);

/**
* Converts the UObject accessible array into the native only analytics array type
*/
static inline TArray<FAnalyticsEventAttribute> ConvertAttrs(const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TArray<FAnalyticsEventAttribute> Converted;
	Converted.AddZeroed(Attributes.Num());
	for (int32 Index = 0; Index < Attributes.Num(); Index++)
	{
		Converted[Index].AttrName = Attributes[Index].Name;
		Converted[Index].AttrValue = Attributes[Index].Value;
	}
	return Converted;
}


UUTAnalyticsBlueprintLibrary::UUTAnalyticsBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

bool UUTAnalyticsBlueprintLibrary::StartSession()
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		return Provider->StartSession();
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("StartSession: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
	return false;
}

bool UUTAnalyticsBlueprintLibrary::StartSessionWithAttributes(const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		return Provider->StartSession(ConvertAttrs(Attributes));
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("StartSessionWithAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
	return false;
}

void UUTAnalyticsBlueprintLibrary::EndSession()
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->EndSession();
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("EndSession: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::FlushEvents()
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->FlushEvents();
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("FlushEvents: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordEvent(const FString& EventName)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordEvent(EventName);
	}
	else
	{
		static bool bHasLogged = false;
		if (!bHasLogged)
		{
			UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordEvent: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
		}
	}
}

void UUTAnalyticsBlueprintLibrary::RecordEventWithAttribute(const FString& EventName, const FString& AttributeName, const FString& AttributeValue)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		FAnalyticsEventAttribute Attribute(AttributeName, AttributeValue);
		Provider->RecordEvent(EventName, Attribute);
	}
	else
	{
		static bool bHasLogged = false;
		if (!bHasLogged)
		{
			UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordEventWithAttribute: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
		}
	}
}

void UUTAnalyticsBlueprintLibrary::RecordEventWithAttributes(const FString& EventName, const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordEvent(EventName, ConvertAttrs(Attributes));
	}
	else
	{
		static bool bHasLogged = false;
		if (!bHasLogged)
		{
			UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordEventWithAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
		}
	}
}

void UUTAnalyticsBlueprintLibrary::RecordItemPurchase(const FString& ItemId, const FString& Currency, int32 PerItemCost, int32 ItemQuantity)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordItemPurchase(ItemId, Currency, PerItemCost, ItemQuantity);
	}
	else
	{
		static bool bHasLogged = false;
		if (!bHasLogged)
		{
			UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordItemPurchase: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
		}
	}
}

void UUTAnalyticsBlueprintLibrary::RecordSimpleItemPurchase(const FString& ItemId, int32 ItemQuantity)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordItemPurchase(ItemId, ItemQuantity);
	}
	else
	{
		static bool bHasLogged = false;
		if (!bHasLogged)
		{
			UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordSimpleItemPurchase: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
		}
	}
}

void UUTAnalyticsBlueprintLibrary::RecordSimpleItemPurchaseWithAttributes(const FString& ItemId, int32 ItemQuantity, const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordItemPurchase(ItemId, ItemQuantity, ConvertAttrs(Attributes));
	}
	else
	{
		static bool bHasLogged = false;
		if (!bHasLogged)
		{
			UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordSimpleItemPurchaseWithAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
		}
	}
}

void UUTAnalyticsBlueprintLibrary::RecordSimpleCurrencyPurchase(const FString& GameCurrencyType, int32 GameCurrencyAmount)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordCurrencyPurchase(GameCurrencyType, GameCurrencyAmount);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordSimpleCurrencyPurchase: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordSimpleCurrencyPurchaseWithAttributes(const FString& GameCurrencyType, int32 GameCurrencyAmount, const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordCurrencyPurchase(GameCurrencyType, GameCurrencyAmount, ConvertAttrs(Attributes));
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordSimpleCurrencyPurchaseWithAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordCurrencyPurchase(const FString& GameCurrencyType, int32 GameCurrencyAmount, const FString& RealCurrencyType, float RealMoneyCost, const FString& PaymentProvider)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordCurrencyPurchase(GameCurrencyType, GameCurrencyAmount, RealCurrencyType, RealMoneyCost, PaymentProvider);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordCurrencyPurchase: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordCurrencyGiven(const FString& GameCurrencyType, int32 GameCurrencyAmount)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordCurrencyGiven(GameCurrencyType, GameCurrencyAmount);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordCurrencyGiven: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordCurrencyGivenWithAttributes(const FString& GameCurrencyType, int32 GameCurrencyAmount, const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordCurrencyGiven(GameCurrencyType, GameCurrencyAmount, ConvertAttrs(Attributes));
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordCurrencyGivenWithAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

FString UUTAnalyticsBlueprintLibrary::GetSessionId()
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		return Provider->GetSessionID();
	}
	return FString();
}

void UUTAnalyticsBlueprintLibrary::SetSessionId(const FString& SessionId)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->SetSessionID(SessionId);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("SetSessionId: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

FString UUTAnalyticsBlueprintLibrary::GetUserId()
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		return Provider->GetUserID();
	}
	return FString();
}

void UUTAnalyticsBlueprintLibrary::SetUserId(const FString& UserId)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->SetUserID(UserId);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("SetUserId: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

FUTAnalyticsEventAttr UUTAnalyticsBlueprintLibrary::MakeEventAttribute(const FString& AttributeName, const FString& AttributeValue)
{
	FUTAnalyticsEventAttr EventAttr;
	EventAttr.Name = AttributeName;
	EventAttr.Value = AttributeValue;
	return EventAttr;
}

void UUTAnalyticsBlueprintLibrary::SetAge(int32 Age)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->SetAge(Age);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("SetAge: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::SetLocation(const FString& Location)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->SetLocation(Location);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("SetLocation: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::SetGender(const FString& Gender)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->SetGender(Gender);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("SetGender: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::SetBuildInfo(const FString& BuildInfo)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->SetBuildInfo(BuildInfo);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("SetBuildInfo: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordErrorWithAttributes(const FString& Error, const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordError(Error, ConvertAttrs(Attributes));
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordErrorWithAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordError(const FString& Error)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordError(Error);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordError: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordProgressWithFullHierarchyAndAttributes(const FString& ProgressType, const TArray<FString>& ProgressNames, const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordProgress(ProgressType, ProgressNames, ConvertAttrs(Attributes));
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordProgressWithFullHierarchyAndAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordProgressWithAttributes(const FString& ProgressType, const FString& ProgressName, const TArray<FUTAnalyticsEventAttr>& Attributes)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordProgress(ProgressType, ProgressName, ConvertAttrs(Attributes));
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordProgressWithAttributes: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}

void UUTAnalyticsBlueprintLibrary::RecordProgress(const FString& ProgressType, const FString& ProgressName)
{
	TSharedPtr<IAnalyticsProviderET> Provider = FUTAnalytics::GetProviderPtr();
	if (Provider.IsValid())
	{
		Provider->RecordProgress(ProgressType, ProgressName);
	}
	else
	{
		UE_LOG(LogUTAnalyticsBPLib, Warning, TEXT("RecordProgress: Failed to get the default analytics provider. Double check your [Analytics] configuration in your INI"));
	}
}
