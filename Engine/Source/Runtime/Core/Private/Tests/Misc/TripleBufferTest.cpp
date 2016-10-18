// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"
#include "TripleBuffer.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTripleBufferTest, "System.Core.Misc.TripleBuffer", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)


bool FTripleBufferTest::RunTest(const FString& Parameters)
{
	// uninitialized buffer
	{
		TTripleBuffer<int32> Buffer(NoInit);

		TestFalse(TEXT("Uninitialized triple buffer must not be dirty"), Buffer.IsDirty());
	}

	// initialized buffer
	{
		TTripleBuffer<int32> Buffer(1);

		TestFalse(TEXT("Initialized triple buffer must not be dirty"), Buffer.IsDirty());
		TestEqual(TEXT("Initialized triple buffer must have correct read buffer value"), Buffer.Read(), 1);

		Buffer.SwapReadBuffers();

		TestEqual(TEXT("Initialized triple buffer must have correct temp buffer value"), Buffer.Read(), 1);

		Buffer.SwapWriteBuffers();

		TestTrue(TEXT("Write buffer swap must set dirty flag"), Buffer.IsDirty());

		Buffer.SwapReadBuffers();

		TestFalse(TEXT("Read buffer swap must clear dirty flag"), Buffer.IsDirty());
		TestEqual(TEXT("Initialized triple buffer must have correct temp buffer value"), Buffer.Read(), 1);
	}

	// pre-set buffer
	{
		int32 Array[3] = { 1, 2, 3 };
		TTripleBuffer<int32> Buffer(Array);

		int32 Read = Buffer.Read();
		TestEqual(TEXT("Pre-set triple buffer must have correct Read buffer value"), Read, 3);

		Buffer.SwapReadBuffers();

		int32 Temp = Buffer.Read();
		TestEqual(TEXT("Pre-set triple buffer must have correct Temp buffer value"), Temp, 1);

		Buffer.SwapWriteBuffers();
		Buffer.SwapReadBuffers();

		int32 Write = Buffer.Read();
		TestEqual(TEXT("Pre-set triple buffer must have correct Write buffer value"), Write, 2);
	}

	// operations
	{
		TTripleBuffer<int32> Buffer;

		Buffer.WriteAndSwap(1);
		Buffer.SwapReadBuffers();
		Buffer.WriteAndSwap(2);
		Buffer.Write(3);

		TestEqual(TEXT("Triple buffer must read correct value (1)"), Buffer.Read(), 1);
		TestEqual(TEXT("Triple buffer must read correct value (2)"), Buffer.SwapAndRead(), 2);

		Buffer.SwapWriteBuffers();

		TestEqual(TEXT("Triple buffer must read correct value (3)"), Buffer.SwapAndRead(), 3);
	}

	return true;
}
