// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

#if WITH_DEV_AUTOMATION_TESTS

#if WITH_EDITOR

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayTagTest, "System.GameplayTags.GameplayTag", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

static UDataTable* CreateGameplayDataTable()
{
	FString CSV(TEXT(",Tag,CategoryText,\r\n0,GameplayTagTest.Test1\r\n1,GameplayTagTest.Test2"));

	auto DataTable = NewObject<UDataTable>(GetTransientPackage(), FName(TEXT("TempDataTable")));
	DataTable->RowStruct = FGameplayTagTableRow::StaticStruct();
	DataTable->CreateTableFromCSVString(CSV);

	FGameplayTagTableRow * Row = (FGameplayTagTableRow*)DataTable->RowMap["0"];
	if (Row)
	{
		check(Row->Tag == TEXT("GameplayTagTest.Test1"));
	}
	return DataTable;
}

bool GameplayTagTest_SimpleTest()
{
	FName TagName = FName(TEXT("GameplayTagTest.Test2"));
	FGameplayTag Tag = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTag(TagName);
	return Tag.GetTagName() == TagName;
}

bool FGameplayTagTest::RunTest(const FString& Parameters)
{
	// Create Test Data 
	UDataTable* DataTable = CreateGameplayDataTable();

	IGameplayTagsModule::Get().GetGameplayTagsManager().PopulateTreeFromDataTable(DataTable);

	// Run Tests
	bool bSuccess = true;
	bSuccess &= GameplayTagTest_SimpleTest();
	// Add more tests here... 

	return bSuccess;
}

#endif //WITH_EDITOR 

#endif //WITH_DEV_AUTOMATION_TESTS