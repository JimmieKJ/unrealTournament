// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "DebugRenderSceneProxy.h"
#include "EnvironmentQuery/EQSRenderingComponent.h"
#include "EnvironmentQuery/EnvQueryDebugHelpers.h"
#include "EnvironmentQuery/EQSQueryResultSourceInterface.h"

#if USE_EQS_DEBUGGER
void UEnvQueryDebugHelpers::QueryToBlobArray(FEnvQueryInstance& Query, TArray<uint8>& BlobArray, bool bUseCompression)
{
	EQSDebug::FQueryData EQSLocalData;
	UEnvQueryDebugHelpers::QueryToDebugData(Query, EQSLocalData);

	if (!bUseCompression)
	{
		FMemoryWriter ArWriter(BlobArray);
		ArWriter << EQSLocalData;
	}
	else
	{
		TArray<uint8> UncompressedBuffer;
		FMemoryWriter ArWriter(UncompressedBuffer);
		ArWriter << EQSLocalData;

		const int32 UncompressedSize = UncompressedBuffer.Num();
		const int32 HeaderSize = sizeof(int32);
		BlobArray.Init(0, HeaderSize + FMath::TruncToInt(1.1f * UncompressedSize));

		int32 CompressedSize = BlobArray.Num() - HeaderSize;
		uint8* DestBuffer = BlobArray.GetData();
		FMemory::Memcpy(DestBuffer, &UncompressedSize, HeaderSize);
		DestBuffer += HeaderSize;

		FCompression::CompressMemory((ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory), (void*)DestBuffer, CompressedSize, (void*)UncompressedBuffer.GetData(), UncompressedSize);

		BlobArray.SetNum(CompressedSize + HeaderSize, false);
	}
}

void UEnvQueryDebugHelpers::QueryToDebugData(FEnvQueryInstance& Query, EQSDebug::FQueryData& EQSLocalData)
{
	// step 1: data for rendering component
	EQSLocalData.Reset();

	FEQSSceneProxy::CollectEQSData(&Query, &Query, 1.0f, true, EQSLocalData.SolidSpheres, EQSLocalData.Texts, EQSLocalData.RenderDebugHelpers);

	// step 2: detailed scoring data for HUD
	const int32 MaxDetailedItems = 10;
	const int32 FirstItemIndex = 0;

	const int32 NumTests = Query.ItemDetails.IsValidIndex(0) ? Query.ItemDetails[0].TestResults.Num() : 0;
	const int32 NumItems = FMath::Min(MaxDetailedItems, Query.NumValidItems);

	EQSLocalData.Name = Query.QueryName;
	EQSLocalData.Id = Query.QueryID;
	EQSLocalData.Items.Reset(NumItems);
	EQSLocalData.Tests.Reset(NumTests);
	EQSLocalData.NumValidItems = Query.NumValidItems;

	UEnvQueryItemType* ItemCDO = Query.ItemType.GetDefaultObject();
	for (int32 ItemIdx = 0; ItemIdx < NumItems; ItemIdx++)
	{
		EQSDebug::FItemData ItemInfo;
		ItemInfo.ItemIdx = ItemIdx + FirstItemIndex;
		ItemInfo.TotalScore = Query.Items[ItemInfo.ItemIdx].Score;

		const uint8* ItemData = Query.RawData.GetData() + Query.Items[ItemInfo.ItemIdx].DataOffset;
		ItemInfo.Desc = FString::Printf(TEXT("[%d] %s"), ItemInfo.ItemIdx, *ItemCDO->GetDescription(ItemData));

		ItemInfo.TestValues.Reserve(NumTests);
		ItemInfo.TestScores.Reserve(NumTests);
		for (int32 TestIdx = 0; TestIdx < NumTests; TestIdx++)
		{
			const float ScoreN = Query.ItemDetails[ItemInfo.ItemIdx].TestResults[TestIdx];
			const float ScoreW = Query.ItemDetails[ItemInfo.ItemIdx].TestWeightedScores[TestIdx];

			ItemInfo.TestValues.Add(ScoreN);
			ItemInfo.TestScores.Add(ScoreW);
		}

		EQSLocalData.Items.Add(ItemInfo);
	}

	const int32 NumAllTests = Query.Options[Query.OptionIndex].Tests.Num();
	for (int32 TestIdx = 0; TestIdx < NumAllTests; TestIdx++)
	{
		EQSDebug::FTestData TestInfo;

		UEnvQueryTest* TestOb = Query.Options[Query.OptionIndex].Tests[TestIdx];
		TestInfo.ShortName = TestOb->GetDescriptionTitle().ToString();
		TestInfo.Detailed = TestOb->GetDescriptionDetails().ToString().Replace(TEXT("\n"), TEXT(", "));

		EQSLocalData.Tests.Add(TestInfo);
	}

	EQSLocalData.UsedOption = Query.OptionIndex;
	const int32 NumOptions = Query.Options.Num();
	for (int32 OptionIndex = 0; OptionIndex < NumOptions; ++OptionIndex)
	{
		const FString UserName = Query.Options[OptionIndex].Generator->OptionName;
		EQSLocalData.Options.Add(UserName.Len() > 0 ? UserName : Query.Options[OptionIndex].Generator->GetName());
	}
}

void  UEnvQueryDebugHelpers::BlobArrayToDebugData(const TArray<uint8>& BlobArray, EQSDebug::FQueryData& EQSLocalData, bool bUseCompression)
{
	if (!bUseCompression)
	{
		FMemoryReader ArReader(BlobArray);
		ArReader << EQSLocalData;
	}
	else
	{
		TArray<uint8> UncompressedBuffer;
		int32 UncompressedSize = 0;
		const int32 HeaderSize = sizeof(int32);
		uint8* SrcBuffer = (uint8*)BlobArray.GetData();
		FMemory::Memcpy(&UncompressedSize, SrcBuffer, HeaderSize);
		SrcBuffer += HeaderSize;
		const int32 CompressedSize = BlobArray.Num() - HeaderSize;

		UncompressedBuffer.AddZeroed(UncompressedSize);

		FCompression::UncompressMemory((ECompressionFlags)(COMPRESS_ZLIB), (void*)UncompressedBuffer.GetData(), UncompressedSize, SrcBuffer, CompressedSize);
		FMemoryReader ArReader(UncompressedBuffer);

		ArReader << EQSLocalData;
	}
}

#endif //ENABLE_VISUAL_LOG

#if ENABLE_VISUAL_LOG && USE_EQS_DEBUGGER
void UEnvQueryDebugHelpers::LogQueryInternal(FEnvQueryInstance& Query, const FLogCategoryBase& Category, ELogVerbosity::Type Verbosity, float TimeSeconds, FVisualLogEntry *CurrentEntry)
{
	const int32 UniqueId = FVisualLogger::Get().GetUniqueId(TimeSeconds);
	TArray<uint8> BlobArray;
	UEnvQueryDebugHelpers::QueryToBlobArray(Query, BlobArray);
	const FString AdditionalLogInfo = FString::Printf(TEXT("Executed EQS: \n - Name: '%s' (id=%d, option=%d),\n - All Items: %d,\n - ValidItems: %d"), *Query.QueryName, Query.QueryID, Query.OptionIndex, Query.ItemDetails.Num(), Query.NumValidItems);
	FVisualLogLine Line(Category.GetCategoryName(), Verbosity, AdditionalLogInfo, Query.QueryID);
	Line.TagName = *EVisLogTags::TAG_EQS;
	Line.UniqueId = UniqueId;

	CurrentEntry->LogLines.Add(Line);
	CurrentEntry->AddDataBlock(EVisLogTags::TAG_EQS, BlobArray, Category.GetCategoryName(), Verbosity).UniqueId = UniqueId;
}
#endif //ENABLE_VISUAL_LOG && USE_EQS_DEBUGGER
