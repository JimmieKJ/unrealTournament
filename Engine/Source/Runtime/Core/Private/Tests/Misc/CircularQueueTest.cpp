// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "Containers/CircularQueue.h"
#include "Misc/AutomationTest.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCircularQueueTest, "System.Core.Misc.CircularQueue", EAutomationTestFlags::ATF_SmokeTest)

bool FCircularQueueTest::RunTest( const FString& Parameters )
{
	const uint32 QueueSize = 8;

	// empty queue
	{
		TCircularQueue<int32> Queue(QueueSize);

		TestTrue(TEXT("Newly created queues must be empty"), Queue.IsEmpty());
		TestFalse(TEXT("Newly created queues must not be full"), Queue.IsFull());
	}

	// partially filled
	{
		TCircularQueue<int32> Queue(QueueSize);
		int32 Value = 0;
	
		TestTrue(TEXT("Adding to an empty queue must succeed"), Queue.Enqueue(666));
		TestFalse(TEXT("Partially filled queues must not be empty"), Queue.IsEmpty());
		TestFalse(TEXT("Partially filled queues must not be full"), Queue.IsFull());
		TestTrue(TEXT("Peeking at a partially filled queue must succeed"), Queue.Peek(Value));
	}

	// full queue
	{
		TCircularQueue<int32> Queue(QueueSize);

		for (int32 Index = 0; Index < QueueSize - 1; ++Index)
		{
			TestTrue(TEXT("Adding to non-full queue must succeed"), Queue.Enqueue(Index));
		}

		TestFalse(TEXT("Full queues must not be empty"), Queue.IsEmpty());
		TestTrue(TEXT("Full queues must be full"), Queue.IsFull());
		TestFalse(TEXT("Adding to full queue must fail"), Queue.Enqueue(666));

		int32 Value = 0;

		for (int32 Index = 0; Index < QueueSize - 1; ++Index)
		{
			TestTrue(TEXT("Peeking at a none-empty queue must succeed"), Queue.Peek(Value));
			TestEqual(TEXT("The peeked at value must be correct"), Value, Index);
			TestTrue(TEXT("Removing from a non-empty queue must succeed"), Queue.Dequeue(Value));
			TestEqual(TEXT("The removed value must be correct"), Value, Index);
		}

		TestTrue(TEXT("A queue that had all items removed must be empty"), Queue.IsEmpty());
		TestFalse(TEXT("A queue that had all items removed must not be full"), Queue.IsFull());
	}

	return true;
}
