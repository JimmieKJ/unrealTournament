// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebookPrivatePCH.h"
#include "OnlineIdentityFacebook.h"

// FUserOnlineAccountFacebook

TSharedRef<const FUniqueNetId> FUserOnlineAccountFacebook::GetUserId() const
{
	return UserIdPtr;
}

FString FUserOnlineAccountFacebook::GetRealName() const
{
	//@todo samz - implement
	return FString();
}

FString FUserOnlineAccountFacebook::GetDisplayName(const FString& Platform) const
{
	//@todo samz - implement
	return FString();
}

bool FUserOnlineAccountFacebook::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return false;
}

bool FUserOnlineAccountFacebook::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	return false;
}

FString FUserOnlineAccountFacebook::GetAccessToken() const
{
	return AuthTicket;
}

bool FUserOnlineAccountFacebook::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	return false;
}

// FOnlineIdentityFacebook

/**
 * Sets the needed configuration properties
 */
FOnlineIdentityFacebook::FOnlineIdentityFacebook()
	: LastCheckElapsedTime(0.f)
	, TotalCheckElapsedTime(0.f)
	, MaxCheckElapsedTime(0.f)
	, bHasLoginOutstanding(false)
	, LocalUserNumPendingLogin(0)
{
	if (!GConfig->GetString(TEXT("OnlineSubsystemFacebook.OnlineIdentityFacebook"), TEXT("LoginUrl"), LoginUrl, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing LoginUrl= in [OnlineSubsystemFacebook.OnlineIdentityFacebook] of DefaultEngine.ini"));
	}
	if (!GConfig->GetString(TEXT("OnlineSubsystemFacebook.OnlineIdentityFacebook"), TEXT("LoginRedirectUrl"), LoginRedirectUrl, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing LoginRedirectUrl= in [OnlineSubsystemFacebook.OnlineIdentityFacebook] of DefaultEngine.ini"));
	}
	if (!GConfig->GetString(TEXT("OnlineSubsystemFacebook.OnlineIdentityFacebook"), TEXT("ClientId"), ClientId, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing ClientId= in [OnlineSubsystemFacebook.OnlineIdentityFacebook] of DefaultEngine.ini"));
	}
	if (!GConfig->GetFloat(TEXT("OnlineSubsystemFacebook.OnlineIdentityFacebook"), TEXT("LoginTimeout"), MaxCheckElapsedTime, GEngineIni))
	{
		UE_LOG(LogOnline, Warning, TEXT("Missing LoginTimeout= in [OnlineSubsystemFacebook.OnlineIdentityFacebook] of DefaultEngine.ini"));
		// Default to 30 seconds
		MaxCheckElapsedTime = 30.f;
	}
}

/**
 * Used to do any time based processing of tasks
 *
 * @param DeltaTime the amount of time that has elapsed since the last tick
 */
void FOnlineIdentityFacebook::Tick(float DeltaTime)
{
	// Only tick once per frame
	TickLogin(DeltaTime);
}

/**
 * Ticks the registration process handling timeouts, etc.
 *
 * @param DeltaTime the amount of time that has elapsed since last tick
 */
void FOnlineIdentityFacebook::TickLogin(float DeltaTime)
{
	if (bHasLoginOutstanding)
	{
		LastCheckElapsedTime += DeltaTime;
		TotalCheckElapsedTime += DeltaTime;
		// See if enough time has elapsed in order to check for completion
		if (LastCheckElapsedTime > 1.f ||
			// Do one last check if we're getting ready to time out
			TotalCheckElapsedTime > MaxCheckElapsedTime)
		{
			LastCheckElapsedTime = 0.f;
			FString Title;
			// Find the browser window we spawned which should now be titled with the redirect url
			if (FPlatformMisc::GetWindowTitleMatchingText(*LoginRedirectUrl, Title))
			{
				bHasLoginOutstanding = false;

				// Parse access token from the login redirect url
				FString AccessToken;
				if (FParse::Value(*Title, TEXT("access_token="), AccessToken) &&
					!AccessToken.IsEmpty())
				{
					// strip off any url parameters and just keep the token itself
					FString AccessTokenOnly;
					if (AccessToken.Split(TEXT("&"), &AccessTokenOnly, NULL))
					{
						AccessToken = AccessTokenOnly;
					}
					// kick off http request to get user info with the new token
					TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
					LoginUserRequests.Add(&HttpRequest.Get(), FPendingLoginUser(LocalUserNumPendingLogin, AccessToken));

					FString MeUrl = TEXT("https://graph.facebook.com/me?access_token=`token");					

					HttpRequest->OnProcessRequestComplete().BindRaw(this, &FOnlineIdentityFacebook::MeUser_HttpRequestComplete);
					HttpRequest->SetURL(MeUrl.Replace(TEXT("`token"), *AccessToken, ESearchCase::IgnoreCase));
					HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
					HttpRequest->SetVerb(TEXT("GET"));
					HttpRequest->ProcessRequest();
				}
				else
				{
					TriggerOnLoginCompleteDelegates(LocalUserNumPendingLogin, false, FUniqueNetIdString(TEXT("")), 
						FString(TEXT("RegisterUser() failed to parse the user registration results")));
				}
			}
			// Trigger the delegate if we hit the timeout limit
			else if (TotalCheckElapsedTime > MaxCheckElapsedTime)
			{
				bHasLoginOutstanding = false;
				TriggerOnLoginCompleteDelegates(LocalUserNumPendingLogin, false, FUniqueNetIdString(TEXT("")), 
					FString(TEXT("RegisterUser() timed out without getting the data")));
			}
		}
		// Reset our time trackers if we are done ticking for now
		if (!bHasLoginOutstanding)
		{
			LastCheckElapsedTime = 0.f;
			TotalCheckElapsedTime = 0.f;
		}
	}
}

/**
 * Parses the results into a user account entry
 *
 * @param Results the string returned by the login process
 */
bool FOnlineIdentityFacebook::ParseLoginResults(const FString& Results,FUserOnlineAccountFacebook& Account)
{
	// reset it
	Account = FUserOnlineAccountFacebook();
	// get the access token
	if (FParse::Value(*Results, TEXT("access_token="), Account.AuthTicket))
	{
		return !Account.AuthTicket.IsEmpty();
	}
	return false;
}

/**
 * Obtain online account info for a user that has been registered
 *
 * @param UserId user to search for
 *
 * @return info about the user if found, NULL otherwise
 */
TSharedPtr<FUserOnlineAccount> FOnlineIdentityFacebook::GetUserAccount(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> Result;

	const TSharedRef<FUserOnlineAccountFacebook>* FoundUserAccount = UserAccounts.Find(UserId.ToString());
	if (FoundUserAccount != NULL)
	{
		Result = *FoundUserAccount;
	}

	return Result;
}

/**
 * Obtain list of all known/registered user accounts
 *
 * @param UserId user to search for
 *
 * @return info about the user if found, NULL otherwise
 */
TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityFacebook::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;
	
	for (FUserOnlineAccountFacebookMap::TConstIterator It(UserAccounts); It; ++It)
	{
		Result.Add(It.Value());
	}

	return Result;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityFacebook::GetUniquePlayerId(int32 LocalUserNum) const
{
	const TSharedPtr<const FUniqueNetId>* FoundId = UserIds.Find(LocalUserNum);
	if (FoundId != NULL)
	{
		return *FoundId;
	}
	return NULL;
}

/**
 * Kicks off the browser process to register the user with Facebook
 */
bool FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FString ErrorStr;

	if (bHasLoginOutstanding)
	{
		ErrorStr = FString::Printf(TEXT("Registration already pending for user %d"), 
			LocalUserNumPendingLogin);
	}
	else if (!(LoginUrl.Len() && LoginRedirectUrl.Len() && ClientId.Len()))
	{
		ErrorStr = FString::Printf(TEXT("OnlineSubsystemFacebook is improperly configured in DefaultEngine.ini FacebookEndpoint=%s RedirectUrl=%s ClientId=%s"),
			*LoginUrl, *LoginRedirectUrl, *ClientId);
	}
	else
	{
		// random number to represent client generated state for verification on login
		State = FString::FromInt(FMath::Rand() % 100000);
		// auth url to spawn in browser
		const FString& Command = FString::Printf(TEXT("%s?redirect_uri=%s&client_id=%s&state=%s&response_type=token"), 
			*LoginUrl, *LoginRedirectUrl, *ClientId, *State);
		// This should open the browser with the command as the URL
		if (FPlatformMisc::OsExecute(TEXT("open"), *Command))
		{
			// keep track of local user requesting registration
			LocalUserNumPendingLogin = LocalUserNum;
			bHasLoginOutstanding = true;
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("Failed to execute command %s"),
				*Command);
		}
	}

	if (!ErrorStr.IsEmpty())
	{
		UE_LOG(LogOnline, Error, TEXT("RegisterUser() failed: %s"),
			*ErrorStr);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdString(TEXT("")), ErrorStr);
		return false;
	}
	return true;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityFacebook::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes != NULL && Size > 0)
	{
		FString StrId(Size, (TCHAR*)Bytes);
		return MakeShareable(new FUniqueNetIdString(StrId));
	}
	return NULL;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityFacebook::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdString(Str));
}

// All of the methods below here fail because they aren't supported

bool FOnlineIdentityFacebook::Logout(int32 LocalUserNum)
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		// remove cached user account
		UserAccounts.Remove(UserId->ToString());
		// remove cached user id
		UserIds.Remove(LocalUserNum);
		// not async but should call completion delegate anyway
		TriggerOnLogoutCompleteDelegates(LocalUserNum, true);

		return true;
	}
	else
	{
		UE_LOG(LogOnline, Warning, TEXT("No logged in user found for LocalUserNum=%d."),
			LocalUserNum);
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	}
	return false;
}

bool FOnlineIdentityFacebook::AutoLogin(int32 LocalUserNum)
{
	return false;
}

ELoginStatus::Type FOnlineIdentityFacebook::GetLoginStatus(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetLoginStatus(*UserId);
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityFacebook::GetLoginStatus(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid() &&
		UserAccount->GetUserId()->IsValid() &&
		!UserAccount->GetAccessToken().IsEmpty())
	{
		return ELoginStatus::LoggedIn;
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityFacebook::GetPlayerNickname(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return  GetPlayerNickname(*UserId);
	}
	return TEXT("");
}

FString FOnlineIdentityFacebook::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	const TSharedRef<FUserOnlineAccountFacebook>* FoundUserAccount = UserAccounts.Find(UserId.ToString());
	if (FoundUserAccount != NULL)
	{
		const TSharedRef<FUserOnlineAccountFacebook>& UserAccount = *FoundUserAccount;
		return UserAccount->RealName;
	}
	return TEXT("");
}

FString FOnlineIdentityFacebook::GetAuthToken(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(*UserId);
		if (UserAccount.IsValid())
		{
			return UserAccount->GetAccessToken();
		}
	}
	return FString();
}

void FOnlineIdentityFacebook::MeUser_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	bool bResult = false;
	FString ResponseStr, ErrorStr;
	FUserOnlineAccountFacebook User;

	FPendingLoginUser PendingRegisterUser = LoginUserRequests.FindRef(HttpRequest.Get());
	// Remove the request from list of pending entries
	LoginUserRequests.Remove(HttpRequest.Get());

	if (bSucceeded &&
		HttpResponse.IsValid())
	{
		ResponseStr = HttpResponse->GetContentAsString();
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			UE_LOG(LogOnline, Verbose, TEXT("RegisterUser request complete. url=%s code=%d response=%s"), 
				*HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *ResponseStr);

			if (User.FromJson(ResponseStr))
			{
				if (!User.UserId.IsEmpty())
				{
					// copy and construct the unique id
					TSharedRef<FUserOnlineAccountFacebook> UserRef(new FUserOnlineAccountFacebook(User));
					UserRef->UserIdPtr = MakeShareable(new FUniqueNetIdString(User.UserId));
					// update/add cached entry for user
					UserAccounts.Add(User.UserId, UserRef);
					// update the access token
					UserRef->AuthTicket = PendingRegisterUser.AccessToken;
					// keep track of user ids for local users
					UserIds.Add(PendingRegisterUser.LocalUserNum, UserRef->GetUserId());

					bResult = true;
				}
				else
				{
					ErrorStr = FString::Printf(TEXT("Missing user id. payload=%s"),
						*ResponseStr);
				}
			}
			else
			{
				ErrorStr = FString::Printf(TEXT("Invalid response payload=%s"),
					*ResponseStr);
			}
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("Invalid response. code=%d error=%s"),
				HttpResponse->GetResponseCode(), *ResponseStr);
		}
	}
	else
	{
		ErrorStr = TEXT("No response");
	}
	if (!ErrorStr.IsEmpty())
	{
		UE_LOG(LogOnline, Warning, TEXT("RegisterUser request failed. %s"), *ErrorStr);
	}

	TriggerOnLoginCompleteDelegates(PendingRegisterUser.LocalUserNum, bResult, FUniqueNetIdString(User.UserId), ErrorStr);
}

void FOnlineIdentityFacebook::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}	

FPlatformUserId FOnlineIdentityFacebook::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId)
{
	for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
	{
		auto CurrentUniqueId = GetUniquePlayerId(i);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == UniqueNetId))
		{
			return i;
		}
	}

	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityFacebook::GetAuthType() const
{
	return TEXT("");
}

