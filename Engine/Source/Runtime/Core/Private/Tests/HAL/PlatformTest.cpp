// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"


float TheCompilerDoesntKnowThisIsAlwaysZero = 0.0f;


struct TestA
{
	virtual ~TestA() {}
	virtual void TestAA() 
	{
		Space[0] = 1;
	}
	uint8 Space[64];
};


struct TestB
{
	virtual ~TestB() {}
	virtual void TestBB() 
	{
		Space[5] = 1;
	}
	uint8 Space[96];
};


struct TestC : public TestA, TestB
{
	int i;
};


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlatformVerificationTest, "System.Core.HAL.Platform Verification", EAutomationTestFlags::ATF_SmokeTest)

bool FPlatformVerificationTest::RunTest (const FString& Parameters)
{
	PTRINT Offset1 = VTABLE_OFFSET(TestC, TestB);
	PTRINT Offset2 = VTABLE_OFFSET(TestC, TestA);
	check(Offset1 == 64 + sizeof(void*));
	check(Offset2 == 0);
	int32 Test = 0x12345678;
#if PLATFORM_LITTLE_ENDIAN
	check(*(uint8*)&Test == 0x78);
#else
	check(*(uint8*)&Test == 0x12);
#endif
	check(FMath::IsNaN(sqrtf(-1.0f)));
	check(!FMath::IsFinite(sqrtf(-1.0f)));
	check(!FMath::IsFinite(-1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!FMath::IsFinite(1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!FMath::IsNaN(-1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!FMath::IsNaN(1.0f/TheCompilerDoesntKnowThisIsAlwaysZero));
	check(!FMath::IsNaN(MAX_FLT));
	check(FMath::IsFinite(MAX_FLT));
	check(!FMath::IsNaN(0.0f));
	check(FMath::IsFinite(0.0f));
	check(!FMath::IsNaN(1.0f));
	check(FMath::IsFinite(1.0f));
	check(!FMath::IsNaN(-1.e37f));
	check(FMath::IsFinite(-1.e37f));
	check(FMath::FloorLog2(0) == 0);
	check(FMath::FloorLog2(1) == 0);
	check(FMath::FloorLog2(2) == 1);
	check(FMath::FloorLog2(12) == 3);
	check(FMath::FloorLog2(16) == 4);

	FGenericPlatformMath::AutoTest();

#if WITH_EDITORONLY_DATA
	check(FPlatformProperties::HasEditorOnlyData());
#else
	check(!FPlatformProperties::HasEditorOnlyData());
#endif

	check(FPlatformProperties::HasEditorOnlyData() != FPlatformProperties::RequiresCookedData());

#if PLATFORM_LITTLE_ENDIAN
	check(FPlatformProperties::IsLittleEndian());
#else
	check(!FPlatformProperties::IsLittleEndian());
#endif
	check(FPlatformProperties::PlatformName());

	check(FString(FPlatformProperties::PlatformName()).Len() > 0); 

	static_assert(ALIGNOF(int32) == 4, "Align of int32 is not 4."); //Hmmm, this would be very strange, ok maybe, but strange

	MS_ALIGN(16) struct FTestAlign
	{
		uint8 Test;
	} GCC_ALIGN(16);

	static_assert(ALIGNOF(FTestAlign) == 16, "Align of FTestAlign is not 16.");

	FName::AutoTest();

	return true;
}
