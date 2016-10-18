// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"
#include "Algo/Copy.h"
#include "Algo/Transform.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAlgosTest, "System.Core.Misc.Algos", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

#define NUM_TEST_OBJECTS      32



bool FAlgosTest::RunTest(const FString& Parameters)
{
	// setup
	TArray<int> TestData;
	for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
	{
		TestData.Add(i);
	}
	TArray<int> TestData2;
	for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
	{
		TestData2.Add(FMath::Rand());
	}

	// copy
	{
		TArray<int> TestArray;
		// empty array
		Algo::Copy(TestData, TestArray);
		check(TestArray == TestData);
		// existing data
		Algo::Copy(TestData2, TestArray);
		check(TestArray.Num() == NUM_TEST_OBJECTS * 2);
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			check(TestArray[i] == TestData[i]);
		}
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			check(TestArray[i + NUM_TEST_OBJECTS] == TestData2[i]);
		}
	}

	// copy if
	{
		TArray<int> TestArray;
		// empty array
		Algo::CopyIf(TestData, TestArray, [](int i) { return (i % 2) == 0; });
		int j = 0;
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == TestData[i]);
				++j;
			}
		}
		// existing data
		Algo::CopyIf(TestData2, TestArray, [](int i) { return (i % 2) == 0; });
		j = 0;
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == TestData[i]);
				++j;
			}
		}
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData2[i] % 2 == 0)
			{
				check(TestArray[j] == TestData2[i]);
				++j;
			}
		}
		check(j == TestArray.Num())
	}

	// transform
	{
		TArray<float> TestArray;
		// empty array
		Algo::Transform(TestData, TestArray, [](int i) { return FMath::DegreesToRadians(i); });
		check(TestArray.Num() == NUM_TEST_OBJECTS);
		for (int i = 0; i < TestArray.Num(); ++i)
		{
			check(TestArray[i] == FMath::DegreesToRadians(TestData[i]));
		}
		// existing data
		Algo::Transform(TestData2, TestArray, [](int i) { return FMath::DegreesToRadians(i); });
		check(TestArray.Num() == NUM_TEST_OBJECTS * 2);
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			check(TestArray[i] == FMath::DegreesToRadians(TestData[i]));
		}
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			check(TestArray[i + NUM_TEST_OBJECTS] == FMath::DegreesToRadians(TestData2[i]));
		}
	}

	// transform if
	{
		TArray<float> TestArray;
		// empty array
		Algo::TransformIf(TestData, TestArray, [](int i) { return (i % 2) == 0; }, [](int i) { return FMath::DegreesToRadians(i); });
		int j = 0;
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == FMath::DegreesToRadians(TestData[i]));
				++j;
			}
		}

		// existing data
		Algo::TransformIf(TestData2, TestArray, [](int i) { return (i % 2) == 0; }, [](int i) { return FMath::DegreesToRadians(i); });
		j = 0;
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData[i] % 2 == 0)
			{
				check(TestArray[j] == FMath::DegreesToRadians(TestData[i]));
				++j;
			}
		}
		for (int i = 0; i < NUM_TEST_OBJECTS; ++i)
		{
			if (TestData2[i] % 2 == 0)
			{
				check(TestArray[j] == FMath::DegreesToRadians(TestData2[i]));
				++j;
			}
		}
		check(j == TestArray.Num())
	}

	return true;
}
