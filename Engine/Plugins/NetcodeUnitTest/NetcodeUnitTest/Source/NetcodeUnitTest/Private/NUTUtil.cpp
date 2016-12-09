// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NUTUtil.h"


#include "UnitTest.h"
#include "ProcessUnitTest.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ClientUnitTest.h"


/**
 * NUTUtil
 */

void NUTUtil::GetUnitTestClassDefList(TArray<UUnitTest*>& OutUnitTestClassDefaults)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		// @todo #JohnBRefactor: Move all 'abstract class' checks to a central location, as you do this in multiple locations
		if (It->IsChildOf(UUnitTest::StaticClass()) && *It != UUnitTest::StaticClass() && *It != UClientUnitTest::StaticClass() &&
			*It != UProcessUnitTest::StaticClass())
		{
			UUnitTest* CurDefault = Cast<UUnitTest>(It->GetDefaultObject());

			if (CurDefault != NULL)
			{
				OutUnitTestClassDefaults.Add(CurDefault);
			}
		}
	}
}

void NUTUtil::SortUnitTestClassDefList(TArray<UUnitTest*>& InUnitTestClassDefaults)
{
	// @todo #JohnBRefactorLambda: Convert these inline sort functions to lambda's now
	struct FUnitTestDateSort
	{
		FORCEINLINE bool operator()(const UUnitTest& A, const UUnitTest& B) const
		{
			return (A.GetUnitTestDate() < B.GetUnitTestDate());
		}
	};

	struct FUnitTestTypeDateSort
	{
		FUnitTestTypeDateSort(TArray<FString>& InTypeOrder)
			: TypeOrder(InTypeOrder)
		{
		}

		FORCEINLINE bool operator()(const UUnitTest& A, const UUnitTest& B) const
		{
			bool bReturnVal = false;

			if (TypeOrder.IndexOfByKey(A.GetUnitTestType()) < TypeOrder.IndexOfByKey(B.GetUnitTestType()))
			{
				bReturnVal = true;
			}
			else if (TypeOrder.IndexOfByKey(A.GetUnitTestType()) == TypeOrder.IndexOfByKey(B.GetUnitTestType()) &&
						A.GetUnitTestDate() < B.GetUnitTestDate())
			{
				bReturnVal = true;
			}

			return bReturnVal;
		}

		/** The order with which the prioritize types */
		TArray<FString> TypeOrder;
	};


	// First reorder the unit test classes by date, then grab the unit test types by date, then group them by type/date
	TArray<FString> ListTypes;

	InUnitTestClassDefaults.Sort(FUnitTestDateSort());

	for (int i=0; i<InUnitTestClassDefaults.Num(); i++)
	{
		ListTypes.AddUnique(InUnitTestClassDefaults[i]->GetUnitTestType());
	}

	// Now sort again, based both on type and date
	InUnitTestClassDefaults.Sort(FUnitTestTypeDateSort(ListTypes));
}


