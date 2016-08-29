// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchServicesPrivatePCH.h"
#include "Core/BlockStructure.h"
#include "AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

DECLARE_LOG_CATEGORY_EXTERN(LogBlockStructureTest, Log, All);
DEFINE_LOG_CATEGORY(LogBlockStructureTest);

// Helper macros for running tests simpler
#define COMBINE_VAR_INNER(A,B) A##B
#define COMBINE_VAR(A,B) COMBINE_VAR_INNER(A,B)
#define CHECKSTRUCTURE_INNER(ArrayName, StructureName, ...) \
	const uint64 ArrayName[] = { __VA_ARGS__ }; \
if (!CheckBlockStructure(StructureName, ArrayName, ARRAY_COUNT(ArrayName) / 2)) \
{ \
	UE_LOG(LogBlockStructureTest, Error, TEXT("Failed test against expected blocks at line %u."), __LINE__); \
	check(false); \
	return false; \
}
#define CHECKSTRUCTURE(...) CHECKSTRUCTURE_INNER(COMBINE_VAR(TestDataArray,__LINE__), BlockStructure, __VA_ARGS__)
#define CHECKOTHERSTRUCTURE(StructureName, ...) CHECKSTRUCTURE_INNER(COMBINE_VAR(TestDataArray,__LINE__), StructureName, __VA_ARGS__)

bool IsStructureLooping(BuildPatchServices::FBlockStructure& Structure)
{
	const BuildPatchServices::FBlockEntry* Slow = Structure.GetHead();
	const BuildPatchServices::FBlockEntry* Fast = Structure.GetHead();
	while (Slow != nullptr && Fast != nullptr && Fast->GetNext() != nullptr)
	{
		Slow = Slow->GetNext();
		Fast = Fast->GetNext()->GetNext();
		if (Slow == Fast)
		{
			return true;
		}
	}
	Slow = Structure.GetFoot();
	Fast = Structure.GetFoot();
	while (Slow != nullptr && Fast != nullptr && Fast->GetPrevious() != nullptr)
	{
		Slow = Slow->GetPrevious();
		Fast = Fast->GetPrevious()->GetPrevious();
		if (Slow == Fast)
		{
			return true;
		}
	}
	return false;
}

bool CheckBlockStructure(BuildPatchServices::FBlockStructure& Structure, const uint64* Results, int32 Count)
{
	if (IsStructureLooping(Structure))
	{
		return false;
	}
	int32 BlockCount = 0;
	const BuildPatchServices::FBlockEntry* Block = Structure.GetHead();
	while (Block != nullptr)
	{
		if (BlockCount >= Count)
		{
			return false;
		}
		if (Block->GetOffset() != Results[BlockCount * 2])
		{
			return false;
		}
		if (Block->GetSize() != Results[(BlockCount * 2) + 1])
		{
			return false;
		}
		++BlockCount;
		Block = Block->GetNext();
	}
	if (BlockCount != Count)
	{
		return false;
	}
	BlockCount = Count - 1;
	Block = Structure.GetFoot();
	while (Block != nullptr)
	{
		if (BlockCount < 0)
		{
			return false;
		}
		if (Block->GetOffset() != Results[BlockCount * 2])
		{
			return false;
		}
		if (Block->GetSize() != Results[(BlockCount * 2) + 1])
		{
			return false;
		}
		--BlockCount;
		Block = Block->GetPrevious();
	}
	if (BlockCount != -1)
	{
		return false;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlockStructureTest, "BuildPatchServices.BlockStructure", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)
bool FBlockStructureTest::RunTest(const FString& Parameters)
{
	ESearchDir::Type DirTypes[2] = { ESearchDir::FromStart, ESearchDir::FromEnd };
	for (const ESearchDir::Type& Dir : DirTypes)
	{
		BuildPatchServices::FBlockStructure BlockStructure;

		// Adding into blank space.
		BlockStructure.Add(70, 10, Dir); // First block.
		BlockStructure.Add(30, 10, Dir); // New head.
		BlockStructure.Add(0, 10, Dir); // New head.
		BlockStructure.Add(90, 10, Dir); // New foot.
		BlockStructure.Add(50, 10, Dir); // New block.
		CHECKSTRUCTURE(0, 10, 30, 10, 50, 10, 70, 10, 90, 10);

		// Adding into already fully occupied.
		BlockStructure.Add(0, 10, Dir); // Head edge1 to edge2.
		BlockStructure.Add(2, 6, Dir); // Head mid to mid.
		BlockStructure.Add(30, 10, Dir); // Edge1 to edge2.
		BlockStructure.Add(32, 8, Dir); // Mid to edge2.
		BlockStructure.Add(50, 5, Dir); // Edge1 to mid.
		BlockStructure.Add(52, 6, Dir); // Mid to mid.
		BlockStructure.Add(70, 10, Dir); // Edge1 to edge2.
		BlockStructure.Add(72, 6, Dir); // Mid to mid.
		BlockStructure.Add(90, 10, Dir); // Foot edge1 to edge2.
		BlockStructure.Add(92, 6, Dir); // Foot mid to mid.
		CHECKSTRUCTURE(0, 10, 30, 10, 50, 10, 70, 10, 90, 10);

		// Adding into half occupied spaces.
		BlockStructure.Add(25, 5, Dir); // Empty to edge2.
		BlockStructure.Add(45, 10, Dir); // Empty to mid.
		BlockStructure.Add(75, 10, Dir); // Mid to empty.
		BlockStructure.Add(100, 10, Dir); // Foot edge1 to empty.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);

		// Add nothing
		BlockStructure.Add(0, 0, Dir); // Head.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Add(110, 0, Dir); // Foot.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Add(25, 0, Dir); // Edge1.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Add(10, 0, Dir); // Edge2.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Add(50, 0, Dir); // Mid.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Add(69, 0, Dir); // Empty.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);

		// Remove nothing
		BlockStructure.Remove(0, 0, Dir); // Head.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Remove(110, 0, Dir); // Foot.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Remove(25, 0, Dir); // Edge1.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Remove(10, 0, Dir); // Edge2.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Remove(50, 0, Dir); // Mid.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);
		BlockStructure.Remove(69, 0, Dir); // Empty.
		CHECKSTRUCTURE(0, 10, 25, 15, 45, 15, 70, 15, 90, 20);

		// Remove half occupied.
		BlockStructure.Remove(0, 5, Dir); // Head edge1 to mid.
		BlockStructure.Remove(20, 8, Dir); // Empty to mid.
		BlockStructure.Remove(50, 15, Dir); // Mid to empty.
		BlockStructure.Remove(75, 10, Dir); // Mid to edge2.
		CHECKSTRUCTURE(5, 5, 28, 12, 45, 5, 70, 5, 90, 20);

		// Remove unoccupied.
		BlockStructure.Remove(0, 3, Dir); // Head empty to empty.
		BlockStructure.Remove(41, 3, Dir); // Empty to empty.
		BlockStructure.Remove(50, 20, Dir); // Edge2 to edge1.
		BlockStructure.Remove(75, 2, Dir); // Edge2 to empty.
		BlockStructure.Remove(68, 2, Dir); // Empty to edge1.
		BlockStructure.Remove(110, 10, Dir); // Foot edge2 to empty.
		CHECKSTRUCTURE(5, 5, 28, 12, 45, 5, 70, 5, 90, 20);

		// Copy constructor
		BuildPatchServices::FBlockStructure OtherStructure(BlockStructure);
		CHECKOTHERSTRUCTURE(OtherStructure, 5, 5, 28, 12, 45, 5, 70, 5, 90, 20);

		// Remove exact block.
		BlockStructure.Remove(45, 5, Dir); // Edge1 to edge2.
		BlockStructure.Remove(28, 12, Dir); // Edge1 to edge2.
		CHECKSTRUCTURE(5, 5, 70, 5, 90, 20);

		// Remove by engulfing one block.
		BlockStructure.Remove(65, 20, Dir); // Empty to empty.
		CHECKSTRUCTURE(5, 5, 90, 20);

		// Remove by split.
		BlockStructure.Remove(100, 5, Dir); // Foot mid to mid.
		CHECKSTRUCTURE(5, 5, 90, 10, 105, 5);

		// Copy assignment
		OtherStructure = BlockStructure;
		CHECKOTHERSTRUCTURE(OtherStructure, 5, 5, 90, 10, 105, 5);

		// Remove structure
		OtherStructure.Remove(BlockStructure, Dir);
		CHECKOTHERSTRUCTURE(OtherStructure, 0);

		// Add structure
		OtherStructure.Add(BlockStructure, Dir);
		CHECKOTHERSTRUCTURE(OtherStructure, 5, 5, 90, 10, 105, 5);

		// Adding with multiple intersections.
		BlockStructure.Add(28, 12, Dir); // Setup.
		BlockStructure.Add(45, 5, Dir); // Setup.
		CHECKSTRUCTURE(5, 5, 28, 12, 45, 5, 90, 10, 105, 5);
		BlockStructure.Add(20, 35, Dir); // Empty block block empty.
		BlockStructure.Add(95, 15, Dir); // Mid block edge2.
		CHECKSTRUCTURE(5, 5, 20, 35, 90, 20);
		BlockStructure.Add(5, 105, Dir); // Head edge1 to foot edge2.
		CHECKSTRUCTURE(5, 105);
		BlockStructure.Remove(10, 10, Dir); // Setup.
		BlockStructure.Remove(30, 5, Dir); // Setup.
		BlockStructure.Remove(50, 7, Dir); // Setup.
		BlockStructure.Remove(80, 6, Dir); // Setup.
		BlockStructure.Add(83, 1, Dir); // Setup.
		CHECKSTRUCTURE(5, 5, 20, 10, 35, 15, 57, 23, 83, 1, 86, 24);
		BlockStructure.Add(5, 40, Dir); // Head edge1 block mid.
		BlockStructure.Add(60, 50, Dir); // mid block edge2.
		CHECKSTRUCTURE(5, 45, 57, 53);
		BlockStructure.Remove(30, 1, Dir); // Setup.
		BlockStructure.Remove(40, 1, Dir); // Setup.
		BlockStructure.Remove(50, 1, Dir); // Setup.
		BlockStructure.Remove(60, 1, Dir); // Setup.
		BlockStructure.Add(5, 56, Dir);      // Head edge2 blocks foot edge1.
		CHECKSTRUCTURE(5, 105);
		BlockStructure.Remove(30, 1, Dir); // Setup.
		BlockStructure.Remove(40, 1, Dir); // Setup.
		BlockStructure.Remove(50, 1, Dir); // Setup.
		BlockStructure.Remove(60, 1, Dir); // Setup.
		BlockStructure.Add(20, 80, Dir); // Head mid blocks foot mid.
		CHECKSTRUCTURE(5, 105);
		BlockStructure.Remove(30, 1, Dir); // Setup.
		BlockStructure.Remove(40, 1, Dir); // Setup.
		BlockStructure.Remove(50, 1, Dir); // Setup.
		BlockStructure.Remove(60, 1, Dir); // Setup.
		BlockStructure.Add(0, 120, Dir); // Head empty blocks foot empty.
		CHECKSTRUCTURE(0, 120);

		// Removing with multiple intersections
		BlockStructure.Remove(30, 1, Dir); // Setup.
		BlockStructure.Remove(40, 1, Dir); // Setup.
		BlockStructure.Remove(50, 1, Dir); // Setup.
		BlockStructure.Remove(60, 1, Dir); // Setup.
		BlockStructure.Remove(0, 120, Dir); // Head edge1 blocks foot edge2.
		CHECKSTRUCTURE(0);
		BlockStructure.Add(0, 10, Dir); // Setup.
		BlockStructure.Add(20, 10, Dir); // Setup.
		BlockStructure.Add(40, 10, Dir); // Setup.
		BlockStructure.Add(60, 10, Dir); // Setup.
		BlockStructure.Add(80, 10, Dir); // Setup.
		BlockStructure.Remove(25, 40, Dir); // Mid blocks mid.
		CHECKSTRUCTURE(0, 10, 20, 5, 65, 5, 80, 10);
		BlockStructure.Remove(10, 70, Dir); // Head empty blocks foot empty.
		CHECKSTRUCTURE(0, 10, 80, 10);
	}

	return true;
}

#undef COMBINE_VAR_INNER
#undef COMBINE_VAR
#undef CHECKSTRUCTURE_INNER
#undef CHECKSTRUCTURE

#endif //WITH_DEV_AUTOMATION_TESTS
