// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemIOSPrivatePCH.h"

FOnlineIdentityIOS::FOnlineIdentityIOS() :
	UniqueNetId( NULL )
{
	Login( 0, FOnlineAccountCredentials() );
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityIOS::GetUserAccount(const FUniqueNetId& UserId) const
{
	// not implemented
	TSharedPtr<FUserOnlineAccount> Result;
	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityIOS::GetAllUserAccounts() const
{
	// not implemented
	TArray<TSharedPtr<FUserOnlineAccount> > Result;
	return Result;
}

bool FOnlineIdentityIOS::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	bool bStartedLogin = false;

	// Was the login handled by Game Center
	if( GetLocalGameCenterUser() && 
		GetLocalGameCenterUser().isAuthenticated )
	{
        // Now logged in
		bStartedLogin = true;
        
		const FString PlayerId(GetLocalGameCenterUser().playerID);
		UniqueNetId = MakeShareable( new FUniqueNetIdString( PlayerId ) );
		TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UniqueNetId, TEXT(""));
        
        UE_LOG(LogOnline, Log, TEXT("The user %s has logged into Game Center"), *PlayerId);
	}
	else if([IOSAppDelegate GetDelegate].OSVersion >= 6.0f)
	{
		// Trigger the login event on the main thread.
		bStartedLogin = true;
		dispatch_async(dispatch_get_main_queue(), ^
		{
			[GetLocalGameCenterUser() setAuthenticateHandler:(^(UIViewController* viewcontroller, NSError *error)
			{
				bool bWasSuccessful = false;

				// The login process has completed.
				if (viewcontroller == nil)
				{
					FString ErrorMessage;

					if (GetLocalGameCenterUser().isAuthenticated == YES)
					{
						/* Perform additional tasks for the authenticated player here */
						const FString PlayerId(GetLocalGameCenterUser().playerID);
						UniqueNetId = MakeShareable(new FUniqueNetIdString(PlayerId));

						bWasSuccessful = true;
						UE_LOG(LogOnline, Log, TEXT("The user %s has logged into Game Center"), *PlayerId);
					}
					else
					{
						ErrorMessage = TEXT("The user could not be authenticated by Game Center");
						UE_LOG(LogOnline, Log, TEXT("%s"), *ErrorMessage);
					}

					if (error)
					{
						NSString *errstr = [error localizedDescription];
						UE_LOG(LogOnline, Warning, TEXT("Game Center login has failed. %s]"), *FString(errstr));
					}

					// Report back to the game thread whether this succeeded.
					[FIOSAsyncTask CreateTaskWithBlock : ^ bool(void)
					{
						TSharedPtr<FUniqueNetIdString> UniqueIdForUser = UniqueNetId.IsValid() ? UniqueNetId : MakeShareable(new FUniqueNetIdString());
						TriggerOnLoginCompleteDelegates(LocalUserNum, bWasSuccessful, *UniqueIdForUser, *ErrorMessage);

						return true;
					}];
				}
				else
				{
					// Game Center has provided a view controller for us to login, we present it.
					[[IOSAppDelegate GetDelegate].IOSController 
						presentViewController:viewcontroller animated:YES completion:nil];
				}
			})];
		});
	}
	else
	{
		// User is not currently logged into game center
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdString(), TEXT("IOS version is not compatible with the game center implementation"));
	}
	
	return bStartedLogin;
}

bool FOnlineIdentityIOS::Logout(int32 LocalUserNum)
{
	TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
	return false;
}

bool FOnlineIdentityIOS::AutoLogin(int32 LocalUserNum)
{
	return Login( LocalUserNum, FOnlineAccountCredentials() );
}

ELoginStatus::Type FOnlineIdentityIOS::GetLoginStatus(int32 LocalUserNum) const
{
	ELoginStatus::Type LoginStatus = ELoginStatus::NotLoggedIn;

	if(LocalUserNum < MAX_LOCAL_PLAYERS && GetLocalGameCenterUser() != NULL && GetLocalGameCenterUser().isAuthenticated == YES)
	{
		LoginStatus = ELoginStatus::LoggedIn;
	}

	return LoginStatus;
}

ELoginStatus::Type FOnlineIdentityIOS::GetLoginStatus(const FUniqueNetId& UserId) const 
{
	ELoginStatus::Type LoginStatus = ELoginStatus::NotLoggedIn;

	if(GetLocalGameCenterUser() != NULL && GetLocalGameCenterUser().isAuthenticated == YES)
	{
		LoginStatus = ELoginStatus::LoggedIn;
	}

	return LoginStatus;
}

TSharedPtr<FUniqueNetId> FOnlineIdentityIOS::GetUniquePlayerId(int32 LocalUserNum) const
{
	return UniqueNetId;
}

TSharedPtr<FUniqueNetId> FOnlineIdentityIOS::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if( Bytes && Size == sizeof(uint64) )
	{
		int32 StrLen = FCString::Strlen((TCHAR*)Bytes);
		if (StrLen > 0)
		{
			FString StrId((TCHAR*)Bytes);
			return MakeShareable(new FUniqueNetIdString(StrId));
		}
	}
    
	return NULL;
}

TSharedPtr<FUniqueNetId> FOnlineIdentityIOS::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdString(Str));
}

FString FOnlineIdentityIOS::GetPlayerNickname(int32 LocalUserNum) const
{
	if (LocalUserNum < MAX_LOCAL_PLAYERS && GetLocalGameCenterUser() != NULL)
	{
		NSString* PersonaName = [GetLocalGameCenterUser() alias];
		
        if (PersonaName != nil)
        {
            return FString(PersonaName);
        }
	}

	return FString();
}

FString FOnlineIdentityIOS::GetPlayerNickname(const FUniqueNetId& UserId) const 
{
	if (GetLocalGameCenterUser() != NULL)
	{
		NSString* PersonaName = [GetLocalGameCenterUser() alias];
		
        if (PersonaName != nil)
        {
            return FString(PersonaName);
        }
	}

	return FString();
}

FString FOnlineIdentityIOS::GetAuthToken(int32 LocalUserNum) const
{
	FString ResultToken;
	return ResultToken;
}

void FOnlineIdentityIOS::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	Delegate.ExecuteIfBound(UserId, Privilege, (uint32)EPrivilegeResults::NoFailures);
}

FPlatformUserId FOnlineIdentityIOS::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& InUniqueNetId)
{
	for (int i = 0; i < MAX_LOCAL_PLAYERS; ++i)
	{
		auto CurrentUniqueId = GetUniquePlayerId(i);
		if (CurrentUniqueId.IsValid() && (*CurrentUniqueId == InUniqueNetId))
		{
			return i;
		}
	}

	return PLATFORMUSERID_NONE;
}
