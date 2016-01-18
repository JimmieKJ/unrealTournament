// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameSubCatalogPCH.h"
#include "McpCatalogHelper.h"
#include "McpQueryResult.h"
#include "McpProfile.h"
#include "JsonObjectConverter.h"
#include "HttpModule.h"
#include "IHttpRequest.h"
#include "IHttpResponse.h"
#include "OnlineHttpRequest.h"
#include "OnlineSubsystemMcp.h"
#include "OnlinePurchaseInterface.h"
#include "OnlineIdentityInterface.h"
#include "OnlineSubsystemNames.h"

#define LOCTEXT_NAMESPACE "FMcpCatalogHelper"

static const FString IN_APP_PURCHASES_STAT(TEXT("in_app_purchases")); // { receipts: [ "", ... ], packages
static const FString DAILY_PURCHASES_STAT(TEXT("daily_purchases")); // { lastInterval: "time", purchaseList: = { "catalogId": Count, ... } }

const FString FMcpCatalogHelper::UserCancelledErrorCode = TEXT("com.epicgames.catalog_helper.user_cancelled");
const FString FMcpCatalogHelper::CatalogOutOfDateErrorCode = TEXT("errors.com.epicgames.modules.gamesubcatalog.catalog_out_of_date");
const FString FMcpCatalogHelper::UnableToParseErrorCode = TEXT("errors.com.epicgames.catalog_helper.unable_to_parse_receipts");

static const int32 DAILY_PURCHASE_INTERVAL_HRS = 24;

DEFINE_LOG_CATEGORY_STATIC(LogCatalogHelper, Log, All);

struct FMcpCatalogHelper::FPurchaseRedemptionState : public TSharedFromThis < FPurchaseRedemptionState >
{
public:
	FPurchaseRedemptionState(FMcpCatalogHelper* OwnerIn)
		: Owner(OwnerIn)
		, State(Initialized)
	{
		check(Owner);
	}

	/** Delegate to fire once RemainingReceipts is empty */
	FMcpQueryComplete OnComplete;

	/** Start the process of validating receipts*/
	void ValidatePreviousPurchases()
	{
		check(State == Initialized);

		UMcpProfile& MainProfile = Owner->GetProfile();
		UMcpProfileGroup& ProfileGroup = Owner->GetProfileGroup();

		// build the request
		TSharedPtr<FOnlineHttpRequest> HttpRequest = MainProfile.GetOnlineSubMcp()->CreateRequest();
		HttpRequest->SetVerb(TEXT("GET"));

		// compute the URL
		FString FinalUrl = ProfileGroup.FormatUrl(Owner->EpicReceiptRoute, ProfileGroup.GetGameAccountId().ToString(), MainProfile.GetProfileId());
		HttpRequest->SetURL(FinalUrl);

		// queue the request
		HttpRequest->OnProcessRequestComplete().BindSP(AsShared(), &FPurchaseRedemptionState::GetEpicReceipts_HttpRequestComplete);
		ProfileGroup.QueueRequest(HttpRequest.ToSharedRef(), FBaseUrlContext::CXC_Client, &MainProfile);
		State = ReadingReceipts;
	}

	/** Validate receipts directly */
	void ValidateNewReceipts(const TArray<FCatalogReceiptInfo>& Receipts)
	{
		check(State == Initialized);
		RemainingReceipts = Receipts;
		LastQueryResult.bSucceeded = true;
		State = PendingValidation;
		ValidateNextReceipt();
	}

private:

	void GetEpicReceipts_HttpRequestComplete(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest, bool bSucceeded)
	{
		check(State == ReadingReceipts);
		FOnlineHttpRequest& HttpRequest = static_cast<FOnlineHttpRequest&>(InHttpRequest.Get());
		const FHttpResponsePtr HttpResponse = HttpRequest.GetResponse();

		// process the response
		TSharedPtr<FJsonValue> JsonPayload = LastQueryResult.Parse(HttpResponse);
		if (LastQueryResult.bSucceeded)
		{
			if (!JsonPayload.IsValid() || JsonPayload->Type != EJson::Array ||
				!FJsonObjectConverter::JsonArrayToUStruct(JsonPayload->AsArray(), &RemainingReceipts, 0, 0))
			{
				LastQueryResult.bSucceeded = false;
				LastQueryResult.ErrorCode = UnableToParseErrorCode;
				LastQueryResult.ErrorMessage = LOCTEXT("ReceiptParseFailed", "Unable to parse receipts from purchasing service.");
			}
		}

		// redeem any receipts we got
		State = PendingValidation;
		ValidateNextReceipt();
	}

	void ValidateNextReceipt()
	{
		check(State == PendingValidation);

		while (RemainingReceipts.Num() > 0)
		{
			FCatalogReceiptInfo NextReceipt = RemainingReceipts.Pop();
			if (Owner->CanRedeemReceipt(NextReceipt.AppStore, NextReceipt.ReceiptId))
			{
				// start redeeming this receipt
				FClientUrlContext Context(FMcpQueryComplete::CreateSP(AsShared(), &FPurchaseRedemptionState::VerifyRealMoneyPurchase_Complete));
				Owner->GetProfileGroup().TriggerMcpRpcCommand(TEXT("VerifyRealMoneyPurchase"), FCatalogReceiptInfo::StaticStruct(), &NextReceipt, 0, Context, &Owner->GetProfile());
				return;
			}
		}

		// if we got this far, we're done
		check(RemainingReceipts.Num() <= 0);
		State = Complete;

		// call the delegate
		OnComplete.ExecuteIfBound(LastQueryResult);

		// NOTE: this will delete us, so don't do anything but return after this
		Owner->PendingPurchaseRedemption.Reset();
	}

	void VerifyRealMoneyPurchase_Complete(const FMcpQueryResult& QueryResult)
	{
		LastQueryResult = QueryResult;
		ValidateNextReceipt();
	}

private:
	/** Catalog helper that owns this state machine */
	FMcpCatalogHelper* Owner;

	enum MachineState {
		Initialized,
		ReadingReceipts, // waiting for receipts array to be populated
		PendingValidation, // receipts
		Complete
	};

	/** The state of the purchase redemption process we are in */
	MachineState State;

	/** List of receipts awaiting fulfillment. Removed as we go. */
	TArray<FCatalogReceiptInfo> RemainingReceipts;

	/** most recent query result */
	FMcpQueryResult LastQueryResult;
};

FMcpCatalogHelper::FMcpCatalogHelper(UMcpProfile* OwningProfile, const FString& CatalogRouteOverride, const FString& EpicReceiptRouteOverride)
: Profile(OwningProfile)
, LastDownloadTime(FDateTime::MinValue())
, CacheExpirationTime(FDateTime::MinValue())
, DailyPurchaseCacheVersion(-1)
, DailyPurchaseExpirationTime(FDateTime::MinValue())
{
	check(OwningProfile);
	CatalogRoute = CatalogRouteOverride.IsEmpty() ? TEXT("/api/storefront/v2/catalog") : CatalogRouteOverride;
	EpicReceiptRoute = EpicReceiptRouteOverride.IsEmpty() ? TEXT("/api/game/v2/receipts/`accountId") : EpicReceiptRouteOverride;
}

void FMcpCatalogHelper::FlushCache()
{
	CatalogETag.Empty();
	CatalogDownload.Reset();
	LastDownloadTime = FDateTime::MinValue();
	CacheExpirationTime = FDateTime::MinValue();
	DailyPurchaseCacheVersion = -1;
	DailyPurchaseExpirationTime = FDateTime::MinValue();
	DailyPurchaseCache.Empty();

	if (PendingHttpRequest.IsValid())
	{
		FOnlineSubsystemMcp* OssMcp = GetProfileGroup().GetOnlineSubMcp();
		check(OssMcp);
		TSharedRef<class FOnlineHttpRequest> NonConstRefFix = PendingHttpRequest.ToSharedRef(); // fix not needed if CancelRequest is changed to take a const ref
		OssMcp->CancelRequest(NonConstRefFix);
	}
}

void FMcpCatalogHelper::FreshenCache(const FMcpQueryComplete& Callback)
{
	if (GetProfileGroup().GetServerDateTime() >= CacheExpirationTime)
	{
		Refresh(Callback);
	}
	else
	{
		FOnlineSubsystemMcp* OssMcp = GetProfileGroup().GetOnlineSubMcp();
		check(OssMcp);
		OssMcp->ExecuteDelegateNextTick(FNextTickDelegate::CreateLambda([Callback]() {
			Callback.ExecuteIfBound(FMcpQueryResult(true));
		}));
	}
}

void FMcpCatalogHelper::OnRealMoneyPurchaseComplete(const FUniqueNetId& UserId, bool bWasSuccessful, const TSharedRef<FPurchaseReceipt>& Receipt, const FString& Error, EAppStore::Type AppStore)
{
	// handle failure
	if (!bWasSuccessful)
	{
		UE_LOG(LogCatalogHelper, Log, TEXT("User %s failed purchase from appStore=%s: %s"), *UserId.ToString(), *EAppStore::ToString(AppStore), *Error);

		FMcpQueryResult Result(false);
		Result.ErrorMessage = FText::FromString(Error);
		Result.ErrorRaw = Error;
		if (Error == TEXT("Escape") || Error == TEXT("UserCanceled") || Error == TEXT("UserCancelled"))
		{
			Result.ErrorCode = UserCancelledErrorCode;
		}
		PendingPurchaseRedemption->OnComplete.ExecuteIfBound(Result);
		PendingPurchaseRedemption.Reset();
		return;
	}
	
	// if we succeeded, we now need to verify purchases with the server
	TArray<FCatalogReceiptInfo> CatalogReceipts;
	for (const FPurchaseReceipt::FReceiptOfferEntry& OfferEntry : Receipt->ReceiptOffers)
	{
		for (const FPurchaseReceipt::FLineItemInfo& LineItem : OfferEntry.LineItems)
		{
			FCatalogReceiptInfo ReceiptInfo;
			ReceiptInfo.AppStore = AppStore;
			// this represents the "thing we purchased"
			ReceiptInfo.AppStoreId = LineItem.ItemName;
			// unique ID representing this receipt
			ReceiptInfo.ReceiptId = LineItem.UniqueId;
			// platform specific validation info
			ReceiptInfo.ReceiptInfo = LineItem.ValidationInfo;
			CatalogReceipts.Add(ReceiptInfo);
		}
	}

	// validate any receipts
	PendingPurchaseRedemption->ValidateNewReceipts(CatalogReceipts);
}

void FMcpCatalogHelper::PurchaseCatalogEntry(const FString& GameNamespace, const TSharedRef<FStorefrontOffer>& Offer, int32 Quantity, int32 ExpectedPrice, const FMcpQueryComplete& OnComplete)
{
	// if this is a real-money offer go through the apropriate appstore
	if (Offer->Price.CurrencyType == EStoreCurrencyType::RealMoney)
	{
		FText ErrorMessage;

		// map our platform to the appropriate Purchasing implementation
		IOnlineSubsystem* PlatformOss = nullptr;
		EAppStore::Type AppStoreId = EAppStore::MAX;
		
		// some platforms force store selection
		if (PLATFORM_PS4)
		{
			PlatformOss = IOnlineSubsystem::Get(PS4_SUBSYSTEM);
			AppStoreId = EAppStore::PlayStationStore;
		}
		else if (PLATFORM_IOS)
		{
			PlatformOss = IOnlineSubsystem::Get(IOS_SUBSYSTEM);
			AppStoreId = EAppStore::IOSAppStore;
		}
		else if (PLATFORM_XBOXONE)
		{
			PlatformOss = IOnlineSubsystem::Get(LIVE_SUBSYSTEM);
			AppStoreId = EAppStore::XboxLiveStore;
		}
		else
		{
			// should drive this via config for Windows, Mac, Linux, Android, etc as we may have multiple choices
			// default to Epic for now
			PlatformOss = GetProfileGroup().GetOnlineSubMcp();
			AppStoreId = EAppStore::EpicPurchasingService;
		}

		// did we find a good mapping
		if (AppStoreId != EAppStore::MAX && PlatformOss != nullptr)
		{
			IOnlinePurchasePtr OnlinePurchase = PlatformOss->GetPurchaseInterface();
			if (OnlinePurchase.IsValid())
			{
				// make sure we don't overlap with purchase redemption
				if (!IsPurchaseRedemptionPending())
				{
					// start the pending redemption now so we have it for later
					PendingPurchaseRedemption = MakeShareable(new FPurchaseRedemptionState(this));
					PendingPurchaseRedemption->OnComplete = OnComplete;

					// look up the app store offer id
					FString AppStoreOfferId = Offer->CatalogOffer.GetAppStoreId(AppStoreId);
					UE_LOG(LogCatalogHelper, Log, TEXT("Initiating purchase of offer=%s from appStore=%s"), *AppStoreOfferId, *EAppStore::ToString(AppStoreId));

					// NOTE: we can only support single request purchases right now because validation doesn't give enough information to verify multi-purchases
					FPurchaseCheckoutRequest CheckoutRequest;
					CheckoutRequest.AddPurchaseOffer(GameNamespace, AppStoreOfferId, Quantity);
					OnlinePurchase->Checkout(*GetProfileGroup().GetGameAccountId(), CheckoutRequest, FOnPurchaseCheckoutComplete::CreateSP(this, &FMcpCatalogHelper::OnRealMoneyPurchaseComplete, AppStoreId));
					return;
				}
				else
				{
					ErrorMessage = LOCTEXT("RedemptionPending", "Cannot purchase while redemption is pending");
				}
			}
			else
			{
				ErrorMessage = LOCTEXT("NoOssPurchasing", "Unable to get OnlinePurchase interface from the current platform OSS");
			}
		}
		else
		{
			if (PlatformOss == nullptr)
			{
				ErrorMessage = LOCTEXT("OssPurchasingNotConfigured", "No Purchasing OSS defined for this platform.");
			}
			else
			{
				ErrorMessage = LOCTEXT("NoAppStore", "No AppStore interface enumerated for this platform");
			}
		}

		// failed to initiate purchase, fire the callback
		UE_LOG(LogCatalogHelper, Warning, TEXT("RealMoney purchase failed: %s"), *ErrorMessage.ToString());
		FOnlineSubsystemMcp* OssMcp = GetProfileGroup().GetOnlineSubMcp();
		check(OssMcp);
		OssMcp->ExecuteDelegateNextTick(FNextTickDelegate::CreateLambda([OnComplete, ErrorMessage]() {
			FMcpQueryResult Result(false);
			Result.ErrorMessage = ErrorMessage;
			Result.ErrorRaw = ErrorMessage.ToString();
			OnComplete.ExecuteIfBound(Result);
		}));
	}
	else
	{
		// purchase from MCP for virtual currency offers
		FCatalogPurchaseInfo PurchaseInfo;
		PurchaseInfo.OfferId = Offer->CatalogOffer.OfferId;
		PurchaseInfo.Currency = Offer->Price.CurrencyType;
		PurchaseInfo.CurrencySubType = Offer->Price.CurrencySubType;
		PurchaseInfo.ExpectedPrice = ExpectedPrice;
		PurchaseInfo.PurchaseQuantity = Quantity;
		FClientUrlContext Context(FMcpQueryComplete::CreateSP(this, &FMcpCatalogHelper::OnVirtualCurrencyPurchaseComplete, OnComplete));
		GetProfileGroup().TriggerMcpRpcCommand(TEXT("PurchaseCatalogEntry"), FCatalogPurchaseInfo::StaticStruct(), &PurchaseInfo, 0, Context, &GetProfile());
	}
}

void FMcpCatalogHelper::OnVirtualCurrencyPurchaseComplete(const FMcpQueryResult& Response, FMcpQueryComplete Callback)
{
	UMcpProfileGroup& ProfileGroup = GetProfileGroup();
	UE_LOG(LogCatalogHelper, Display, TEXT("OnPurchaseComplete for %s [%s] bSuccess: %d Response: %s"),
		*ProfileGroup.GetPlayerName(),
		*ProfileGroup.GetGameAccountId().ToString(),
		Response.bSucceeded,
		*Response.ErrorMessage.ToString());
	if (!Response.bSucceeded && Response.ErrorCode == CatalogOutOfDateErrorCode)
	{
		// trigger a refresh (will fire a delegate when it gets updated)
		Refresh(FMcpQueryComplete::CreateSP(AsShared(), &FMcpCatalogHelper::OnCatalogRefreshDone, Callback));
	}
	else
	{
		Callback.ExecuteIfBound(Response);
	}
}

void FMcpCatalogHelper::OnCatalogRefreshDone(const FMcpQueryResult& Result, FMcpQueryComplete Callback)
{
	if (!Result.bSucceeded)
	{
		UE_LOG(LogCatalogHelper, Error, TEXT("Unable to update the catalog: %d %s - %s"), Result.HttpResult, *Result.ErrorCode, *Result.ErrorMessage.ToString());
		Callback.ExecuteIfBound(Result);
	}
	else
	{
		FMcpQueryResult PurchaseResult(false);
		PurchaseResult.ErrorCode = CatalogOutOfDateErrorCode;
		//Result.ErrorMessage = ;
		Callback.ExecuteIfBound(PurchaseResult);
	}
}

bool FMcpCatalogHelper::RedeemPurchases(const FMcpQueryComplete& OnComplete)
{
	if (IsPurchaseRedemptionPending())
	{
		return false;
	}

	// start the redemption state machine
	PendingPurchaseRedemption = MakeShareable(new FPurchaseRedemptionState(this));
	PendingPurchaseRedemption->OnComplete = OnComplete;
	PendingPurchaseRedemption->ValidatePreviousPurchases();
	return true;
}

FTimespan FMcpCatalogHelper::GetTimeUntilDailyLimitReset() const
{
	FDateTime Now = GetProfileGroup().GetServerDateTime();
	if (Now >= DailyPurchaseExpirationTime)
	{
		return FTimespan::Zero();
	}
	return DailyPurchaseExpirationTime - Now;
}

FTimespan FMcpCatalogHelper::GetTimeUntilMarketplaceRefresh() const
{
	FDateTime Now = GetProfileGroup().GetServerDateTime();
	if (Now >= CacheExpirationTime)
	{
		return FTimespan::Zero();
	}
	return CacheExpirationTime - Now;
}

FTimespan FMcpCatalogHelper::GetCatalogCacheAge() const
{
	return GetProfileGroup().GetServerDateTime() - LastDownloadTime;
}

inline UMcpProfileGroup& FMcpCatalogHelper::GetProfileGroup() const
{
	return GetProfile().GetProfileGroup();
}

void FMcpCatalogHelper::Refresh(const FMcpQueryComplete& Callback)
{
	// add the callback if bound
	if (Callback.IsBound())
	{
		OnRefreshCompleted.Add(Callback);
	}

	// do a refresh if necessary
	if (!PendingHttpRequest.IsValid())
	{
		// build the request
		PendingHttpRequest = GetProfile().GetOnlineSubMcp()->CreateRequest();
		PendingHttpRequest->SetVerb(TEXT("GET"));
		
		// etag header
		if (!CatalogETag.IsEmpty())
		{
			PendingHttpRequest->SetHeader(TEXT("If-None-Match"), CatalogETag);
		}
		
		// language header
		FCultureRef CurrentCulture = FInternationalization::Get().GetCurrentCulture();
		TArray<FString> CultureChain = CurrentCulture->GetPrioritizedParentCultureNames();
		FString LanguageHeader = FString::Join(CultureChain, TEXT(" "));
		PendingHttpRequest->SetHeader(TEXT("X-EpicGames-Language"), LanguageHeader);

		// compute the URL
		FString FinalUrl = GetProfileGroup().FormatUrl(CatalogRoute, GetProfileGroup().GetGameAccountId().ToString(), GetProfile().GetProfileId());
		PendingHttpRequest->SetURL(FinalUrl);

		// queue the request
		PendingHttpRequest->OnProcessRequestComplete().BindSP(this, &FMcpCatalogHelper::RefreshCatalog_HttpRequestComplete);
		GetProfileGroup().QueueRequest(PendingHttpRequest.ToSharedRef(), FBaseUrlContext::CXC_Client, &GetProfile());
	}
}

void FMcpCatalogHelper::RefreshCatalog_HttpRequestComplete(TSharedRef<FHttpRetrySystem::FRequest>& InHttpRequest, bool bSucceeded)
{
	FOnlineHttpRequest& HttpRequest = static_cast<FOnlineHttpRequest&>(InHttpRequest.Get());
	const FHttpResponsePtr HttpResponse = HttpRequest.GetResponse();

	// process the response
	FMcpQueryResult QueryResult;
	TSharedPtr<FJsonValue> JsonPayload = QueryResult.Parse(HttpResponse);
	if (QueryResult.bSucceeded)
	{
		// update the catalog from JSON
		TSharedPtr<FCatalogDownload> JsonStoreCatalog = MakeShareable(new FCatalogDownload());
		if (JsonPayload.IsValid() && JsonPayload->Type == EJson::Object &&
			FJsonObjectConverter::JsonObjectToUStruct(JsonPayload->AsObject().ToSharedRef(), JsonStoreCatalog.Get(), 0, 0))
		{
			// save the catalog
			CatalogDownload = JsonStoreCatalog;
			CatalogETag = HttpResponse->GetHeader("ETag");

			// update the timeouts
			LastDownloadTime = GetProfileGroup().GetServerDateTime();
			FDateTime IntervalStart;
			GetProfileGroup().GetRefreshInterval(IntervalStart, CacheExpirationTime, LastDownloadTime, CatalogDownload->RefreshIntervalHrs);
		}

		// fire the multicast delegate
		OnCatalogChanged.Broadcast();
	}
	else
	{
		// treat not-modified as a success (no need to update the catalog though)
		if (QueryResult.HttpResult == EHttpResponseCodes::NotModified)
		{
			// update the timeouts
			LastDownloadTime = GetProfileGroup().GetServerDateTime();
			FDateTime IntervalStart;
			GetProfileGroup().GetRefreshInterval(IntervalStart, CacheExpirationTime, LastDownloadTime, CatalogDownload->RefreshIntervalHrs);

			// mark this as a success
			QueryResult.bSucceeded = true;
		}
	}

	// remove the refresh pending flag
	check(PendingHttpRequest.IsValid());
	PendingHttpRequest.Reset();
	
	// fire all refresh callbacks
	if (OnRefreshCompleted.Num() > 0)
	{
		TArray<FMcpQueryComplete> Callbacks = OnRefreshCompleted;
		OnRefreshCompleted.Empty();
		for (const FMcpQueryComplete& Cb : Callbacks)
		{
			Cb.ExecuteIfBound(QueryResult);
		}
	}
}

TArray<FString> FMcpCatalogHelper::DebugGetStorefrontNames() const
{
	TArray<FString> StorefrontNames;
	if (CatalogDownload.IsValid())
	{
		for (const FStorefront& Storefront : CatalogDownload->Storefronts)
		{
			StorefrontNames.Add(Storefront.Name);
		}
	}
	return StorefrontNames;
}

const FStorefront* FMcpCatalogHelper::GetStorefront(const FString& StorefrontName) const
{
	if (CatalogDownload.IsValid())
	{
		for (const FStorefront& Storefront : CatalogDownload->Storefronts)
		{
			if (Storefront.Name == StorefrontName)
			{
				return &Storefront;
			}
		}
	}
	return nullptr;
}

FString FMcpCatalogHelper::GetUserCountryCode() const
{
	FString UserCountryCode;

	// Get the currency code from User Online Account
	const auto& UserId = GetProfileGroup().GetGameAccountId();
	if (UserId.IsValid())
	{
		FOnlineSubsystemMcp* OssMcp = GetProfileGroup().GetOnlineSubMcp();
		IOnlineIdentityPtr OnlineIdentity = OssMcp ? OssMcp->GetIdentityInterface() : IOnlineIdentityPtr();
		TSharedPtr<FUserOnlineAccount> UserAccount = OnlineIdentity.IsValid() ? OnlineIdentity->GetUserAccount(*UserId) : TSharedPtr<FUserOnlineAccount>();
		if (UserAccount.IsValid())
		{
			UserAccount->GetUserAttribute(TEXT("country"), UserCountryCode);
			UserCountryCode = UserCountryCode.ToUpper();
		}
	}

	// handle missing/empty data
	if (UserCountryCode.IsEmpty())
	{
		UE_LOG(LogCatalogHelper, Error, TEXT("Unable to get user's country code. Defaulting to US"));
		UserCountryCode = TEXT("US");
	}

	return UserCountryCode;
}

TArray< TSharedRef<FStorefrontOffer> > FMcpCatalogHelper::GetStorefrontOffers(const FString& StorefrontName) const
{
	TArray< TSharedRef<FStorefrontOffer> > Result;
	const FStorefront* Storefront = GetStorefront(StorefrontName);
	if (Storefront)
	{
		// find the max priority for each catalog group
		TMap<FString,int32> MaxPriority;
		for (const FCatalogOffer& Entry : Storefront->CatalogEntries)
		{
			if (!Entry.CatalogGroup.IsEmpty())
			{
				int32& LastMax = MaxPriority.Add(Entry.CatalogGroup);
				if (LastMax < Entry.CatalogGroupPriority)
				{
					LastMax = Entry.CatalogGroupPriority;
				}
			}
		}

		// add entries that are >= max priority
		for (const FCatalogOffer& Entry : Storefront->CatalogEntries)
		{
			if (Entry.CatalogGroup.IsEmpty() || Entry.CatalogGroupPriority >= MaxPriority.FindOrAdd(Entry.CatalogGroup))
			{
				// add an entry for each price option
				for (const FCatalogItemPrice& Price : Entry.Prices)
				{
					Result.Add(MakeShareable(new FStorefrontOffer(CatalogDownload, Entry, Price)));
				}
			}
		}

		// filter for real money offers not matching the user's currency
		const FString UserCurrency = GetUserCountryCode();
		Result = Result.FilterByPredicate([&](const TSharedRef<FStorefrontOffer>& Offer) {

			// check for real money prices
			if (Offer->Price.CurrencyType == EStoreCurrencyType::RealMoney)
			{
				const FString& PriceType = Offer->Price.CurrencySubType;

				// is this a country-specific price
				int32 ColonIndex = INDEX_NONE;
				if (PriceType.FindChar(':', ColonIndex))
				{
					if (ColonIndex != UserCurrency.Len() || !PriceType.StartsWith(UserCurrency))
					{
						// price is for a different country
						return false;
					}
				}
			}

			// check for single purchase offers
			if (Offer->CatalogOffer.SinglePurchaseOnly)
			{
				if (HasPurchasedItem(Offer->CatalogOffer.FulfillmentId))
				{
					// they've already bought this
					return false;
				}
			}

			// check for required purchases
			for (const FString& Required : Offer->CatalogOffer.RequiredFulfillments)
			{
				if (!HasPurchasedItem(Required))
				{
					// they have not purchased a required fulfillment
					return false;
				}
			}

			return true;
		});

		// sort the final result list
		Result.Sort([](const TSharedRef<FStorefrontOffer>& A, const TSharedRef<FStorefrontOffer>& B) {
			int32 PriorityA = A->CatalogOffer.SortPriority;
			int32 PriorityB = B->CatalogOffer.SortPriority;
			if (PriorityA == PriorityB)
			{
				// use alphabetized title as fallback to make this sort stable (in a given language)
				return A->CatalogOffer.Title.ToString() < B->CatalogOffer.Title.ToString();
			}
			return PriorityA > PriorityB; // higher priority items come first
		});
	}
	return Result;
}

const FCatalogItemPrice* FMcpCatalogHelper::GetServicePrice(const FString& ServiceName) const
{
	if (CatalogDownload.IsValid())
	{
		for (const FServicePrice& Price : CatalogDownload->ServicePricing)
		{
			if (Price.ServiceName == ServiceName)
			{
				return &Price.Price;
			}
		}
	}
	return nullptr;
}

FDateTime FMcpCatalogHelper::GetDailyLimitRefreshTime() const
{
	FDateTime Start, End;
	GetProfileGroup().GetRefreshInterval(Start, End, GetProfileGroup().GetServerDateTime(), DAILY_PURCHASE_INTERVAL_HRS);
	return End;
}

int32 FMcpCatalogHelper::GetAmountLeftInStock(const FCatalogOffer& CatalogOffer) const
{
	// check if there is a limit at all
	if (CatalogOffer.DailyLimit < 0)
	{
		return -1;
	}

	// figure out how many they've purchased today
	const TMap<FString, int32>& Purchases = GetDailyPurchases();
	const int32* PurchaseEntry = Purchases.Find(CatalogOffer.OfferId);
	int32 NumPurchased = PurchaseEntry ? *PurchaseEntry : 0;

	// return the amount remaining
	return FMath::Max(0, CatalogOffer.DailyLimit - NumPurchased);
}

const TMap<FString, int32>& FMcpCatalogHelper::GetDailyPurchases() const
{
	int64 ProfileRev = GetProfile().GetProfileRevision();
	FDateTime Now = GetProfileGroup().GetServerDateTime();
	if (ProfileRev != DailyPurchaseCacheVersion || Now > DailyPurchaseExpirationTime)
	{
		// clear the cache
		DailyPurchaseCacheVersion = ProfileRev;
		DailyPurchaseCache.Empty();

		// rebuild it from the profile
		TSharedPtr<const FJsonValue> Attribute = GetProfile().GetStat(DAILY_PURCHASES_STAT);
		if (Attribute.IsValid() && Attribute->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject>& Obj = Attribute->AsObject();
			FString LastIntervalStr;
			FDateTime LastInterval = FDateTime::MinValue();;

			// see if these purchases are from today
			if (Obj->TryGetStringField(TEXT("lastInterval"), LastIntervalStr) &&
				FDateTime::ParseIso8601(*LastIntervalStr, LastInterval) &&
				GetProfileGroup().IsCurrentInterval(LastInterval, DAILY_PURCHASE_INTERVAL_HRS, &DailyPurchaseExpirationTime))
			{
				// rebuild the cache from the JSON
				const TSharedPtr<FJsonObject>& PurchaseMapJson = Obj->GetObjectField(TEXT("purchaseList"));
				for (const auto& It : PurchaseMapJson->Values)
				{
					if (It.Value.IsValid() && It.Value->Type == EJson::Number)
					{
						int32 Count = FMath::TruncToInt(It.Value->AsNumber());
						DailyPurchaseCache.Add(It.Key, Count);
					}
				}
			}
			else
			{
				FDateTime StartTime;
				GetProfileGroup().GetRefreshInterval(StartTime, DailyPurchaseExpirationTime, Now, DAILY_PURCHASE_INTERVAL_HRS);
			}
		}
	}
	return DailyPurchaseCache;
}

bool FMcpCatalogHelper::CanRedeemReceipt(EAppStore::Type AppStore, const FString& ReceiptId) const
{
	TSharedPtr<const FJsonValue> IapStat = GetProfile().GetStat(IN_APP_PURCHASES_STAT);
	if (!IapStat.IsValid() || IapStat->Type != EJson::Object)
	{
		// couldn't find IAP stat
		return false;
	}

	// build the receipt tracker string
	FString CheckString = FString::Printf(TEXT("%s:%s"), EAppStore::ToReceiptPrefix(AppStore), *ReceiptId);

	// see if it's in the receipts array
	const TArray< TSharedPtr<FJsonValue> >& Receipts = IapStat->AsObject()->GetArrayField(TEXT("receipts"));
	for (const TSharedPtr<FJsonValue>& Receipt : Receipts)
	{
		if (Receipt.IsValid() && Receipt->Type == EJson::String)
		{
			if (Receipt->AsString().Equals(CheckString))
			{
				// receipt redeemed already
				return false;
			}
		}
	}

	// receipt not redeemed yet (assuming it's valid)
	return true;
}

bool FMcpCatalogHelper::HasPurchasedItem(const FString& FulfillmentId) const
{
	TSharedPtr<const FJsonValue> IapStat = GetProfile().GetStat(IN_APP_PURCHASES_STAT);
	if (IapStat.IsValid() && IapStat->Type == EJson::Object)
	{
		const TArray< TSharedPtr<FJsonValue> >& Packages = IapStat->AsObject()->GetArrayField(TEXT("fulfillments"));
		for (const TSharedPtr<FJsonValue>& Package : Packages)
		{
			if (Package.IsValid() && Package->Type == EJson::String)
			{
				if (Package->AsString().Equals(FulfillmentId))
				{
					return true;
				}
			}
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
