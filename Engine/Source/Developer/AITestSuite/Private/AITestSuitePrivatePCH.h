// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Core.h"
#include "CoreUObject.h"
#include "InputCore.h"
#include "EngineDefines.h"
#include "EngineSettings.h"
#include "EngineStats.h"
#include "EngineLogs.h"
#include "EngineGlobals.h"

#include "Engine/EngineBaseTypes.h"
#include "Engine/EngineTypes.h"
#include "Engine/Engine.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include "AITestSuite.h"

#include "TestLogger.h"

#include "AITypes.h"

#include "BehaviorTree/Blackboard/BlackboardKeyAllTypes.h"
#include "BehaviorTree/BlackboardData.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Perception/AIPerceptionComponent.h"
#include "Actions/PawnActionsComponent.h"

#include "BTBuilder.h"

#include "Tests/AITestsCommon.h"

#include "Actions/TestPawnAction_Log.h"

#include "MockAI.h"
#include "MockAI_BT.h"
#include "MockGameplayTasks.h"

