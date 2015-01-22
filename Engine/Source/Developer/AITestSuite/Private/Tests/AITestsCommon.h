// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AutomationTest.h"

#define ENSURE_FAILED_TESTS 1

class UBehaviorTree;
class UMockAI_BT;

DECLARE_LOG_CATEGORY_EXTERN(LogAITestSuite, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogBehaviorTreeTest, Log, All);

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAITestCommand_WaitSeconds, float, Duration);

class FAITestCommand_WaitOneTick : public IAutomationLatentCommand
{
public: 
	FAITestCommand_WaitOneTick()
		: bAlreadyRun(false)
	{} 
	virtual bool Update() override;
private: 
	bool bAlreadyRun;
};


namespace FAITestHelpers
{
	UWorld* GetWorld();
	static const float TickInterval = 1.f / 30;

	void UpdateFrameCounter();
	uint64 FramesCounter();
}

struct FAITestBase
{
private:
	// internals
	TArray<UObject*> SpawnedObjects;	
	uint32 bTearedDown : 1;
protected:
	FAutomationTestBase* TestRunner;

	FAITestBase() : bTearedDown(false), TestRunner(nullptr)
	{}

	template<typename ClassToSpawn>
	ClassToSpawn* NewAutoDestroyObject()
	{
		ClassToSpawn* ObjectInstance = NewObject<ClassToSpawn>();
		ObjectInstance->AddToRoot();
		SpawnedObjects.Add(ObjectInstance);
		return ObjectInstance;
	}

	UWorld& GetWorld() const
	{
		UWorld* World = FAITestHelpers::GetWorld();
		check(World);
		return *World;
	}

	// loggin helper
	void Test(const FString& Description, bool bValue);

public:

	void SetTestInstance(FAutomationTestBase& AutomationTestInstance) { TestRunner = &AutomationTestInstance; }

	// interface
	virtual ~FAITestBase();
	virtual void SetUp() {}
	virtual bool Update() { return true; }
	virtual void InstantTest() {}
	// must be called!
	virtual void TearDown();
};

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAITestCommand_SetUpTest, FAITestBase*, AITest);
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAITestCommand_PerformTest, FAITestBase*, AITest);
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FAITestCommand_TearDownTest, FAITestBase*, AITest);

// @note that TestClass needs to derive from FAITestBase
#define IMPLEMENT_AI_LATENT_TEST(TestClass, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass##_Runner, PrettyName, (EAutomationTestFlags::ATF_Game | EAutomationTestFlags::ATF_Editor)) \
	bool TestClass##_Runner::RunTest(const FString& Parameters) \
	{ \
		/* spawn test instance. Setup should be done in test's constructor */ \
		TestClass* TestInstance = new TestClass(); \
		TestInstance->SetTestInstance(*this); \
		/* set up */ \
		ADD_LATENT_AUTOMATION_COMMAND(FAITestCommand_SetUpTest(TestInstance)); \
		/* run latent command to update */ \
		ADD_LATENT_AUTOMATION_COMMAND(FAITestCommand_PerformTest(TestInstance)); \
		/* run latent command to tear down */ \
		ADD_LATENT_AUTOMATION_COMMAND(FAITestCommand_TearDownTest(TestInstance)); \
		return true; \
	} 

#define IMPLEMENT_AI_INSTANT_TEST(TestClass, PrettyName) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(TestClass##Runner, PrettyName, (EAutomationTestFlags::ATF_Game | EAutomationTestFlags::ATF_Editor)) \
	bool TestClass##Runner::RunTest(const FString& Parameters) \
	{ \
		/* spawn test instance. Setup should be done in test's constructor */ \
		TestClass* TestInstance = new TestClass(); \
		TestInstance->SetTestInstance(*this); \
		/* set up */ \
		TestInstance->SetUp(); \
		/* call the instant-test code */ \
		TestInstance->InstantTest(); \
		/* tear down */ \
		TestInstance->TearDown(); \
		return true; \
	} 

//----------------------------------------------------------------------//
// Specific test types
//----------------------------------------------------------------------//

struct FAITest_SimpleBT : public FAITestBase
{
	TArray<int32> ExpectedResult;
	UBehaviorTree* BTAsset;
	UMockAI_BT* AIBTUser;
	bool bUseSystemTicking;

	FAITest_SimpleBT();	
	
	virtual void SetUp() override;
	virtual bool Update() override;
	
	virtual void VerifyResults();
};

struct FAITest_SimpleActionsTest : public FAITestBase
{
	FTestLogger<int32> Logger;
	UPawnActionsComponent* ActionsComponent;

	FAITest_SimpleActionsTest();
	virtual ~FAITest_SimpleActionsTest();
	virtual void SetUp() override;
	//virtual bool Update() override;
};