// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueTest, "System.Core.Misc.Queue", EAutomationTestFlags::ATF_SmokeTest)

bool FQueueTest::RunTest( const FString& Parameters )
{
	// empty queues
	TQueue<int32> q1_1;
	int32 i1_1 = 666;

	TestTrue(TEXT("A new queue must be empty"), q1_1.IsEmpty());

	TestFalse(TEXT("A new queue must not dequeue anything"), q1_1.Dequeue(i1_1));
	TestFalse(TEXT("A new queue must not peek anything"), q1_1.Peek(i1_1));

	// insertion & removal
	TQueue<int32> q2_1;
	int32 i2_1 = 1;
	int32 i2_2 = 2;
	int32 i2_p = 0;

	TestTrue(TEXT("Inserting into a new queue must succeed"), q2_1.Enqueue(i2_1));
	TestTrue(TEXT("Peek must succeed on a queue with one item"), q2_1.Peek(i2_p));
	TestEqual(TEXT("Peek must return the first value"), i2_p, i2_1);

	TestTrue(TEXT("Inserting into a non-empty queue must succeed"), q2_1.Enqueue(i2_2));
	TestTrue(TEXT("Peek must succeed on a queue with two items"), q2_1.Peek(i2_p));
	TestEqual(TEXT("Peek must return the first value"), i2_p, i2_1);

	TestTrue(TEXT("Dequeue must succeed on a queue with two items"), q2_1.Dequeue(i2_p));
	TestEqual(TEXT("Dequeue must return the first value"), i2_p, i2_1);
	TestTrue(TEXT("Dequeue must succeed on a queue with one item"), q2_1.Dequeue(i2_p));
	TestEqual(TEXT("Dequeue must return the second value"), i2_p, i2_2);

	TestTrue(TEXT("After removing all items, the queue must be empty"), q2_1.IsEmpty());

	return true;
}
