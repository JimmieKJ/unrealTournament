// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


// Module includes
#include "OnlineSubsystemFacebookPrivatePCH.h"
#include "OnlineIdentityFacebook.h"

// Other UE4 includes
#include "IOSAppDelegate.h"


///////////////////////////////////////////////////////////////////////////////////////
// FUserOnlineAccountFacebook implementation


FUserOnlineAccountFacebook::FUserOnlineAccountFacebook(const FString& InUserId, const FString& InAuthTicket) 
		: AuthTicket(InAuthTicket)
		, UserId(new FUniqueNetIdString(InUserId))
{
}

TSharedRef<FUniqueNetId> FUserOnlineAccountFacebook::GetUserId() const
{
	return UserId;
}

FString FUserOnlineAccountFacebook::GetRealName() const
{
	//@todo samz - implement
	return FString();
}

FString FUserOnlineAccountFacebook::GetDisplayName() const
{
	//@todo samz - implement
	return FString();
}

bool FUserOnlineAccountFacebook::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
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

///////////////////////////////////////////////////////////////////////////////////////
// FOnlineIdentityFacebook implementation


FOnlineIdentityFacebook::FOnlineIdentityFacebook()
	: UserAccount( MakeShareable(new FUserOnlineAccountFacebook()) )
	, LoginStatus( ELoginStatus::NotLoggedIn )
{

}


TSharedPtr<FUserOnlineAccount> FOnlineIdentityFacebook::GetUserAccount(const FUniqueNetId& UserId) const
{
	return UserAccount;
}


TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityFacebook::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;

	Result.Add( UserAccount );

	return Result;
}


TSharedPtr<FUniqueNetId> FOnlineIdentityFacebook::GetUniquePlayerId(int32 LocalUserNum) const
{
	return UserAccount->GetUserId();
}


bool FOnlineIdentityFacebook::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	bool bTriggeredLogin = true;

	dispatch_async(dispatch_get_main_queue(),^ 
		{
			[FBSession openActiveSessionWithReadPermissions:nil allowLoginUI:YES 
				completionHandler:^(FBSession *session, FBSessionState status, NSError *error) 
				{
					UE_LOG(LogOnline, Display, TEXT("[FBSession openActiveSessionWithReadPermissions]"));

					// Check we have logged in correctly
					bool bSuccessfullyLoggedIn = error == nil && status != FBSessionStateClosedLoginFailed && status != FBSessionStateCreatedOpening;
					if( bSuccessfullyLoggedIn )
					{
						UE_LOG(LogOnline, Display, TEXT("[FBSession bSuccessfullyLoggedIn = true]"));

						FString Token([session accessTokenData].accessToken);
						UserAccount->AuthTicket = Token;
						UE_LOG(LogOnline, Display, TEXT("Got signin token: %s"), *Token );

						[[FBRequest requestForMe] startWithCompletionHandler:^(FBRequestConnection *connection, NSDictionary<FBGraphUser> *user, NSError *error2)
							{
								if (!error2)
								{
									const FString UserId([user objectForKey:@"id"]);
									UserAccount->UserId = MakeShareable(new FUniqueNetIdString(UserId));
									UE_LOG(LogOnline, Display, TEXT("got user profile for id: %s"), *UserId );
								}
								else
								{
									UE_LOG(LogOnline, Display, TEXT("[FBRequest requestForMe] startWithCompletionHandler: Error: %d"), [error2 code] );
								}
							}
						];
						TriggerOnLoginCompleteDelegates(LocalUserNum, bSuccessfullyLoggedIn, UserAccount->UserId.Get(), TEXT(""));

						//@todo samz - login status change delegate
					}
					else
					{
						UE_LOG(LogOnline, Display, TEXT("[FBSession bSuccessfullyLoggedIn = false[%d]]"), [error code]);
						[FBSession.activeSession closeAndClearTokenInformation];
						[FBSession setActiveSession:nil];
						
						FUniqueNetIdString TempId;
						TriggerOnLoginCompleteDelegates(LocalUserNum, bSuccessfullyLoggedIn, TempId, FString(TEXT("Failed to create a valid FBSession")));
					}
					LoginStatus = bSuccessfullyLoggedIn ? ELoginStatus::LoggedIn : ELoginStatus::NotLoggedIn;

					UE_LOG(LogOnline, Display, TEXT("Facebook login was successful? - %d"), bSuccessfullyLoggedIn);
				}
			];
		}
	);

	return bTriggeredLogin;	
}


TSharedPtr<FUniqueNetId> FOnlineIdentityFacebook::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes != NULL && Size > 0)
	{
		FString StrId(Size, (TCHAR*)Bytes);
		return MakeShareable(new FUniqueNetIdString(StrId));
	}
	return NULL;
}


TSharedPtr<FUniqueNetId> FOnlineIdentityFacebook::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdString(Str));
}


bool FOnlineIdentityFacebook::Logout(int32 LocalUserNum)
{
	//@todo samz - login status change delegate

	dispatch_async(dispatch_get_main_queue(),^ 
		{
			if( [FBSession.activeSession isOpen] )
			{
				[FBSession.activeSession closeAndClearTokenInformation];
				[FBSession setActiveSession:nil];
			}
		}
	);

	return true;
}


bool FOnlineIdentityFacebook::AutoLogin(int32 LocalUserNum)
{
	return false;
}


ELoginStatus::Type FOnlineIdentityFacebook::GetLoginStatus(int32 LocalUserNum) const
{
	return LoginStatus;
}

ELoginStatus::Type FOnlineIdentityFacebook::GetLoginStatus(const FUniqueNetId& UserId) const 
{
	return LoginStatus;
}


FString FOnlineIdentityFacebook::GetPlayerNickname(int32 LocalUserNum) const
{
	return TEXT("FacebookUser");
}

FString FOnlineIdentityFacebook::GetPlayerNickname(const FUniqueNetId& UserId) const 
{
	return TEXT("FacebookUser");
}


FString FOnlineIdentityFacebook::GetAuthToken(int32 LocalUserNum) const
{
	return UserAccount->GetAccessToken();
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
