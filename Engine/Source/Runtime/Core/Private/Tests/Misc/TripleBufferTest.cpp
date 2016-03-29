// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "AutomationTest.h"
#include "TripleBuffer.h"


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTripleBufferTest, "System.Core.Misc.TripleBuffer", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)


bool FTripleBufferTest::RunTest(const FString& Parameters)
{
	// uninitialized buffer
	TTripleBuffer<int32> b1_1(NoInit);

	TestFalse(TEXT("Uninitialized triple buffer must not be dirty"), b1_1.IsDirty());

	// initialized buffer
	TTripleBuffer<int32> b2_1(1);

	TestFalse(TEXT("Initialized triple buffer must not be dirty"), b2_1.IsDirty());
	TestEqual(TEXT("Initialized triple buffer must have correct read buffer value"), b2_1.Read(), 1);

	b2_1.SwapReadBuffers();

	TestEqual(TEXT("Initialized triple buffer must have correct temp buffer value"), b2_1.Read(), 1);

	b2_1.SwapWriteBuffers();

	TestTrue(TEXT("Write buffer swap must set dirty flag"), b2_1.IsDirty());

	b2_1.SwapReadBuffers();

	TestFalse(TEXT("Read buffer swap must clear dirty flag"), b2_1.IsDirty());
	TestEqual(TEXT("Initialized triple buffer must have correct temp buffer value"), b2_1.Read(), 1);

	// Pre-set buffer
	TTripleBuffer<int32> b3_1(1, 2, 3);

	int32 Read = b3_1.Read();
	TestEqual(TEXT("Pre-set triple buffer must have correct read buffer value"), Read, 1);

	b3_1.SwapReadBuffers();

	int32 Temp = b3_1.Read();
	TestEqual(TEXT("Pre-set triple buffer must have correct temp buffer value"), Temp, 3);

	b3_1.SwapWriteBuffers();
	b3_1.SwapReadBuffers();

	int32 Write = b3_1.Read();
	TestEqual(TEXT("Pre-set triple buffer must have correct write buffer value"), Write, 2);

	// operations
	TTripleBuffer<int32> b4_1;

	b4_1.WriteAndSwap(1);
	b4_1.SwapReadBuffers();
	b4_1.WriteAndSwap(2);
	b4_1.Write(3);

	TestEqual(TEXT("Triple buffer must read correct value (1)"), b4_1.Read(), 1);
	TestEqual(TEXT("Triple buffer must read correct value (2)"), b4_1.SwapAndRead(), 2);

	b4_1.SwapWriteBuffers();

	TestEqual(TEXT("Triple buffer must read correct value (3)"), b4_1.SwapAndRead(), 3);

	return true;
}
