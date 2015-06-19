// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsModulePrivatePCH.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayTagTest, "System.GameplayTags.GameplayTag", EAutomationTestFlags::ATF_Editor)

#if WITH_EDITOR
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
#endif //WITH_EDITOR 

bool GameplayTagTest_SimpleTest()
{
	FName TagName = FName(TEXT("GameplayTagTest.Test2"));
	FGameplayTag Tag = IGameplayTagsModule::Get().GetGameplayTagsManager().RequestGameplayTag(TagName);
	return Tag.GetTagName() == TagName;
}

bool FGameplayTagTest::RunTest(const FString& Parameters)
{
#if WITH_EDITOR
	// Create Test Data 
	UDataTable* DataTable = CreateGameplayDataTable();

	IGameplayTagsModule::Get().GetGameplayTagsManager().PopulateTreeFromDataTable(DataTable);

	// Run Tests
	bool bSuccess = true;
	bSuccess &= GameplayTagTest_SimpleTest();
	// Add more tests here... 

	return bSuccess;
#else
	return true;
#endif //WITH_EDITOR
}
