// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "LogWidgetCommands.h"
#include "SLogWidget.h"

#include "INetcodeUnitTest.h"
#include "NUTUtil.h"
#include "UnitTestEnvironment.h"


/**
 * Globals
 */

UNetConnection* GActiveReceiveUnitConnection = NULL;
bool GIsInitializingActorChan = false;
bool GSentBunch = false;

ELogType GActiveLogTypeFlags = ELogType::None;

#if !UE_BUILD_SHIPPING
TMap<void*, InternalProcessEventCallback> ActiveProcessEventCallbacks;
#endif


/**
 * Definitions/implementations
 */

DEFINE_LOG_CATEGORY(LogUnitTest);
DEFINE_LOG_CATEGORY(None);


/**
 * Module implementation
 */
class FNetcodeUnitTest : public INetcodeUnitTest
{
private:
	static FWorldDelegates::FWorldInitializationEvent::FDelegate OnWorldCreatedDelegate;

#ifdef DELEGATE_DEPRECATED
	static FDelegateHandle OnWorldCreatedDelegateHandle;
#endif

public:
	/**
	 * Called upon loading of the NetcodeUnitTest library
	 */
	virtual void StartupModule() override
	{
		static bool bSetDelegate = false;

		if (!bSetDelegate)
		{
			OnWorldCreatedDelegate = FWorldDelegates::FWorldInitializationEvent::FDelegate::CreateStatic(
										&FNetcodeUnitTest::OnWorldCreated);

#ifdef DELEGATE_DEPRECATED
			OnWorldCreatedDelegateHandle = FWorldDelegates::OnPreWorldInitialization.Add(OnWorldCreatedDelegate);
#else
			FWorldDelegates::OnPreWorldInitialization.Add(OnWorldCreatedDelegate);
#endif

			bSetDelegate = true;
		}

		FLogWidgetCommands::Register();
		FUnitTestEnvironment::Register();
	}

	/**
	 * Called immediately prior to unloading of the NetcodeUnitTest library
	 */
	virtual void ShutdownModule() override
	{
		FLogWidgetCommands::Unregister();
		FUnitTestEnvironment::Unregister();
	}


	/**
	 * Delegate implementation, for adding NUTActor to ServerActors, early on in engine startup
	 */
	static void OnWorldCreated(UWorld* UnrealWorld, const UWorld::InitializationValues IVS)
	{
		// If NUTActor isn't already in RuntimeServerActors, add it now
		if (GEngine != NULL)
		{
			bool bNUTActorPresent = GEngine->RuntimeServerActors.ContainsByPredicate(
				[](const FString& CurServerActor)
				{
					return CurServerActor == TEXT("/Script/NetcodeUnitTest.NUTActor");
				});

			if (!bNUTActorPresent)
			{
				UE_LOG(LogUnitTest, Log, TEXT("NUTActor not present in RuntimeServerActors - adding this"));

				GEngine->RuntimeServerActors.Add(TEXT("/Script/NetcodeUnitTest.NUTActor"));
			}
		}

		// Now remove it, so it's only called once
#ifdef DELEGATE_DEPRECATED
		FWorldDelegates::OnPreWorldInitialization.Remove(OnWorldCreatedDelegateHandle);
#else
		FWorldDelegates::OnPreWorldInitialization.Remove(OnWorldCreatedDelegate);
#endif
	}
};

FWorldDelegates::FWorldInitializationEvent::FDelegate FNetcodeUnitTest::OnWorldCreatedDelegate = NULL;

#ifdef DELEGATE_DEPRECATED
FDelegateHandle FNetcodeUnitTest::OnWorldCreatedDelegateHandle;
#endif


// Essential for getting the .dll to compile, and for the package to be loadable
IMPLEMENT_MODULE(FNetcodeUnitTest, NetcodeUnitTest);
