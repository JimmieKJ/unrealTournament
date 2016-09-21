// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusPrivatePCH.h"
#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPackage.h"

bool FUserOnlineAccountOculus::GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = AdditionalAuthData.Find(AttrName);
	if (FoundAttr != nullptr)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

bool FUserOnlineAccountOculus::GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const
{
	const FString* FoundAttr = UserAttributes.Find(AttrName);
	if (FoundAttr != nullptr)
	{
		OutAttrValue = *FoundAttr;
		return true;
	}
	return false;
}

bool FUserOnlineAccountOculus::SetUserAttribute(const FString& AttrName, const FString& AttrValue)
{
	UserAttributes[AttrName] = AttrValue;
	return true;
}

bool FOnlineIdentityOculus::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	FString ErrorStr;
	TSharedPtr<FUserOnlineAccountOculus> UserAccountPtr;

	// valid local player index
	// MAX_LOCAL_PLAYERS == 1
	if (LocalUserNum < 0 || LocalUserNum >= MAX_LOCAL_PLAYERS)
	{
		ErrorStr = FString::Printf(TEXT("Invalid LocalUserNum=%d"), LocalUserNum);
	}
	else
	{
		TSharedPtr<const FUniqueNetId>* UserId = UserIds.Find(LocalUserNum);
		if (UserId == nullptr)
		{
			if (ovr_GetLoggedInUserID() == 0)
			{
				ErrorStr = TEXT("Not currently logged into Oculus.  Make sure Oculus is running and you are entitled to the app.");
			}
			else
			{
				OculusSubsystem.AddRequestDelegate(
					ovr_User_GetLoggedInUser(),
					FOculusMessageOnCompleteDelegate::CreateRaw(this, &FOnlineIdentityOculus::OnLoginComplete, LocalUserNum));
				return true;
			}
		}
		else
		{
			const FUniqueNetIdOculus* UniqueIdStr = (FUniqueNetIdOculus*)(UserId->Get());
			TSharedRef<FUserOnlineAccountOculus>* TempPtr = UserAccounts.Find(*UniqueIdStr);
			check(TempPtr);
			UserAccountPtr = *TempPtr;

			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserAccountPtr->GetUserId(), *ErrorStr);
		}
	}

	if (!ErrorStr.IsEmpty())
	{
		UE_LOG_ONLINE(Warning, TEXT("Failed Oculus login. %s"), *ErrorStr);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdOculus(), ErrorStr);
	}

	return false;
}

void FOnlineIdentityOculus::OnLoginComplete(ovrMessageHandle Message, bool bIsError, int32 LocalUserNum)
{
	FString ErrorStr;
	if (bIsError)
	{
		auto Error = ovr_Message_GetError(Message);
		auto ErrorMessage = ovr_Error_GetMessage(Error);
		ErrorStr = FString(ErrorMessage);		
	}
	else
	{
		auto User = ovr_Message_GetUser(Message);
		auto Id = ovr_User_GetID(User);
		FString Name(ovr_User_GetOculusID(User));

		FUniqueNetIdOculus* NewUserId = new FUniqueNetIdOculus(Id);
		if (!NewUserId->IsValid())
		{
			ErrorStr = FString(TEXT("Unable to get a valid ID"));
		}
		else
		{
			TSharedRef<FUserOnlineAccountOculus> UserAccountRef(new FUserOnlineAccountOculus(MakeShareable(NewUserId), *Name));

			// update/add cached entry for user
			UserAccounts.Add(static_cast<FUniqueNetIdOculus>(*UserAccountRef->GetUserId()), UserAccountRef);

			// keep track of user ids for local users
			UserIds.Add(LocalUserNum, UserAccountRef->GetUserId());

			TriggerOnLoginCompleteDelegates(LocalUserNum, true, *UserAccountRef->GetUserId(), *ErrorStr);
			TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, ELoginStatus::LoggedIn, *UserAccountRef->GetUserId());
			return;
		}
	}

	TriggerOnLoginCompleteDelegates(LocalUserNum, false, FUniqueNetIdOculus(), *ErrorStr);
}

bool FOnlineIdentityOculus::Logout(int32 LocalUserNum)
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		// remove cached user account
		UserAccounts.Remove(FUniqueNetIdOculus(*UserId));
		// remove cached user id
		UserIds.Remove(LocalUserNum);
		// not async but should call completion delegate anyway
		TriggerOnLogoutCompleteDelegates(LocalUserNum, true);
		TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::LoggedIn, ELoginStatus::NotLoggedIn, *UserId);
		return true;
	}
	else
	{
		TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	}
	return false;
}

bool FOnlineIdentityOculus::AutoLogin(int32 LocalUserNum)
{
	FString LoginStr;
	FString PasswordStr;
	FString TypeStr;

	return Login(LocalUserNum, FOnlineAccountCredentials(TypeStr, LoginStr, PasswordStr));
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityOculus::GetUserAccount(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> Result;

	FUniqueNetIdOculus OculusUserId(UserId);
	const TSharedRef<FUserOnlineAccountOculus>* FoundUserAccount = UserAccounts.Find(OculusUserId);
	if (FoundUserAccount != nullptr)
	{
		Result = *FoundUserAccount;
	}

	return Result;
}

TArray<TSharedPtr<FUserOnlineAccount> > FOnlineIdentityOculus::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount> > Result;

	for (TMap<FUniqueNetIdOculus, TSharedRef<FUserOnlineAccountOculus>>::TConstIterator It(UserAccounts); It; ++It)
	{
		Result.Add(It.Value());
	}

	return Result;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityOculus::GetUniquePlayerId(int32 LocalUserNum) const
{
	const TSharedPtr<const FUniqueNetId>* FoundId = UserIds.Find(LocalUserNum);
	if (FoundId != nullptr)
	{
		return *FoundId;
	}
	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityOculus::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	if (Bytes && Size == sizeof(ovrID))
	{
		uint64* RawUniqueId = (uint64*)Bytes;
		ovrID OculusId(*RawUniqueId);
		return MakeShareable(new FUniqueNetIdOculus(OculusId));
	}
	return nullptr;
}

TSharedPtr<const FUniqueNetId> FOnlineIdentityOculus::CreateUniquePlayerId(const FString& Str)
{
	return MakeShareable(new FUniqueNetIdOculus(Str));
}

ELoginStatus::Type FOnlineIdentityOculus::GetLoginStatus(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UserId = GetUniquePlayerId(LocalUserNum);
	if (UserId.IsValid())
	{
		return GetLoginStatus(*UserId);
	}
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityOculus::GetLoginStatus(const FUniqueNetId& UserId) const
{
	TSharedPtr<FUserOnlineAccount> UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid() &&
		UserAccount->GetUserId()->IsValid())
	{
		return ELoginStatus::LoggedIn;
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityOculus::GetPlayerNickname(int32 LocalUserNum) const
{
	TSharedPtr<const FUniqueNetId> UniqueId = GetUniquePlayerId(LocalUserNum);
	if (UniqueId.IsValid())
	{
		return GetPlayerNickname(*UniqueId);
	}

	return TEXT("OCULUS USER");
}

FString FOnlineIdentityOculus::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	auto UserAccount = GetUserAccount(UserId);
	if (UserAccount.IsValid())
	{
		return UserAccount->GetDisplayName();
	}
	return UserId.ToString();
}

FString FOnlineIdentityOculus::GetAuthToken(int32 LocalUserNum) const
{
	// TODO: implement
	return FString();
}

FOnlineIdentityOculus::FOnlineIdentityOculus(class FOnlineSubsystemOculus& InSubsystem)
	: OculusSubsystem(InSubsystem)
{
	// Auto login the 0-th player
	AutoLogin(0);
}

void FOnlineIdentityOculus::GetUserPrivilege(const FUniqueNetId& UserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate)
{
	// Check for entitlement
	OculusSubsystem.AddRequestDelegate(
		ovr_Entitlement_GetIsViewerEntitled(),
		FOculusMessageOnCompleteDelegate::CreateLambda([&UserId, Privilege, Delegate](ovrMessageHandle Message, bool bIsError)
		{
			uint32 PrivilegeResults = 0;

			// If the user failed the entitlement check, they have no privileges
			if (bIsError)
			{
				auto Error = ovr_Message_GetError(Message);
				FString ErrorMessage(ovr_Error_GetMessage(Error));
				UE_LOG_ONLINE(Error, TEXT("Failed the entitlement check: %s"), *ErrorMessage);
				PrivilegeResults = (uint32)IOnlineIdentity::EPrivilegeResults::UserNotFound;
			}
			else
			{
				UE_LOG_ONLINE(Verbose, TEXT("User is entitled to app"));
				PrivilegeResults = (uint32)IOnlineIdentity::EPrivilegeResults::NoFailures;
			}
			Delegate.ExecuteIfBound(UserId, Privilege, PrivilegeResults);
		}));
}

FPlatformUserId FOnlineIdentityOculus::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId)
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

FString FOnlineIdentityOculus::GetAuthType() const
{
	return TEXT("Oculus");
}
