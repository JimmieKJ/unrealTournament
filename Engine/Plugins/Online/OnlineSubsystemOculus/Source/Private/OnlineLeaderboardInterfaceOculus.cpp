// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusPrivatePCH.h"
#include "OnlineLeaderboardInterfaceOculus.h"
#include "OnlineIdentityOculus.h"
#include "OnlineSubsystemOculusPackage.h"

FOnlineLeaderboardOculus::FOnlineLeaderboardOculus(class FOnlineSubsystemOculus& InSubsystem)
: OculusSubsystem(InSubsystem)
{
}

bool FOnlineLeaderboardOculus::ReadLeaderboards(const TArray< TSharedRef<const FUniqueNetId> >& Players, FOnlineLeaderboardReadRef& ReadObject)
{
	// Not supported
	return false;
};

bool FOnlineLeaderboardOculus::ReadLeaderboardsForFriends(int32 LocalUserNum, FOnlineLeaderboardReadRef& ReadObject)
{
	ReadObject->ReadState = EOnlineAsyncTaskState::InProgress;
	OculusSubsystem.AddRequestDelegate(
		ovr_Leaderboard_GetEntries(TCHAR_TO_ANSI(*ReadObject->LeaderboardName.ToString()), 100, ovrLeaderboard_FilterFriends, ovrLeaderboard_StartAtTop),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, ReadObject](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			ReadObject->ReadState = EOnlineAsyncTaskState::Failed;
			TriggerOnLeaderboardReadCompleteDelegates(false);
			return;
		}

		auto LeaderboardArray = ovr_Message_GetLeaderboardEntryArray(Message);
		auto LeaderboardArraySize = ovr_LeaderboardEntryArray_GetSize(LeaderboardArray);

		for (size_t i = 0; i < LeaderboardArraySize; i++)
		{
			auto LeaderboardEntry = ovr_LeaderboardEntryArray_GetElement(LeaderboardArray, i);
			auto User = ovr_LeaderboardEntry_GetUser(LeaderboardEntry);
			auto NickName = FString(ovr_User_GetOculusID(User));
			auto UserID = ovr_User_GetID(User);
			auto Rank = ovr_LeaderboardEntry_GetRank(LeaderboardEntry);
			auto Score = ovr_LeaderboardEntry_GetScore(LeaderboardEntry);

			auto Row = FOnlineStatsRow(NickName, MakeShareable(new FUniqueNetIdOculus(UserID)));
			Row.Rank = Rank;
			Row.Columns.Add(ReadObject->SortedColumn, FVariantData(Score));
			ReadObject->Rows.Add(Row);
		}

		auto ScoreColumnMetaData = new FColumnMetaData(ReadObject->SortedColumn, EOnlineKeyValuePairDataType::Double);

		ReadObject->ColumnMetadata.Add(*ScoreColumnMetaData);

		ReadObject->ReadState = EOnlineAsyncTaskState::Done;
		TriggerOnLeaderboardReadCompleteDelegates(true);
	}));
	return true;
}

void FOnlineLeaderboardOculus::FreeStats(FOnlineLeaderboardRead& ReadObject)
{
	// no-op
}

bool FOnlineLeaderboardOculus::WriteLeaderboards(const FName& SessionName, const FUniqueNetId& Player, FOnlineLeaderboardWrite& WriteObject)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!(LoggedInPlayerId.IsValid() && Player == *LoggedInPlayerId))
	{
		UE_LOG_ONLINE(Error, TEXT("Can only write to leaderboards for logged in player id"));
		return false;
	}

	auto StatData = WriteObject.FindStatByName(WriteObject.RatedStat);

	if (StatData == nullptr)
	{
		UE_LOG_ONLINE(Error, TEXT("Could not find RatedStat: %s"), *WriteObject.RatedStat.ToString());
		return false;
	}

	double Score;

	switch (StatData->GetType())
	{
		case EOnlineKeyValuePairDataType::Int32:
		{
			int32 Value;
			StatData->GetValue(Value);
			Score = (double)Value;
			break;
		}
		case EOnlineKeyValuePairDataType::UInt32:
		{
			uint32 Value;
			StatData->GetValue(Value);
			Score = (double)Value;
			break;
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			int64 Value;
			StatData->GetValue(Value);
			Score = (double)Value;
			break;
		}
		case EOnlineKeyValuePairDataType::UInt64:
		{
			uint64 Value;
			StatData->GetValue(Value);
			Score = (double)Value;
			break;
		}
		case EOnlineKeyValuePairDataType::Double:
		{
			StatData->GetValue(Score);
			break;
		}
		case EOnlineKeyValuePairDataType::Float:
		{
			float Value;
			StatData->GetValue(Value);
			Score = (double)Value;
			break;
		}
		default:
		{
			UE_LOG_ONLINE(Error, TEXT("Invalid Stat type to save to the leaderboard: %s"), EOnlineKeyValuePairDataType::ToString(StatData->GetType()));
			return false;
		}
	}

	for (const auto& LeaderboardName : WriteObject.LeaderboardNames)
	{
		ovr_Leaderboard_WriteEntry(TCHAR_TO_ANSI(*LeaderboardName.ToString()), Score, /* extra_data */ nullptr, 0, (WriteObject.UpdateMethod == ELeaderboardUpdateMethod::Force));
	}

	return true;
};

bool FOnlineLeaderboardOculus::FlushLeaderboards(const FName& SessionName)
{
	TriggerOnLeaderboardFlushCompleteDelegates(SessionName, true);
	return true;
};

bool FOnlineLeaderboardOculus::WriteOnlinePlayerRatings(const FName& SessionName, int32 LeaderboardId, const TArray<FOnlinePlayerScore>& PlayerScores)
{
	// Not supported
	return false;
};
