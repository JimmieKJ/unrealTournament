// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Engine/GameEngine.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Console.h"
#include "LatentActions.h"
#include "DelayAction.h"
#include "InterpolateComponentToAction.h"
#include "Advertising.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "SlateCore.h"
#include "Engine/StreamableManager.h"
#include "Net/OnlineEngineInterface.h"
#include "UserActivityTracking.h"
#include "PhysicsEngine/PhysicsSettings.h"

//////////////////////////////////////////////////////////////////////////
// UKismetSystemLibrary

UKismetSystemLibrary::UKismetSystemLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UKismetSystemLibrary::StackTraceImpl(const FFrame& StackFrame)
{
	const FString Trace = StackFrame.GetStackTrace();
	UE_LOG(LogBlueprintUserMessages, Log, TEXT("\n%s"), *Trace);
}

bool UKismetSystemLibrary::IsValid(const UObject* Object)
{
	return ::IsValid(Object);
}

bool UKismetSystemLibrary::IsValidClass(UClass* Class)
{
	return ::IsValid(Class);
}

FString UKismetSystemLibrary::GetObjectName(const UObject* Object)
{
	return GetNameSafe(Object);
}

FString UKismetSystemLibrary::GetPathName(const UObject* Object)
{
	return GetPathNameSafe(Object);
}

FString UKismetSystemLibrary::GetDisplayName(const UObject* Object)
{
#if WITH_EDITOR
	if (const AActor* Actor = Cast<const AActor>(Object))
	{
		return Actor->GetActorLabel();
	}
#endif

	if (const UActorComponent* Component = Cast<const UActorComponent>(Object))
	{
		return Component->GetReadableName();
	}

	return Object ? Object->GetName() : FString();
}

FString UKismetSystemLibrary::GetClassDisplayName(UClass* Class)
{
	return Class ? Class->GetName() : FString();
}

FString UKismetSystemLibrary::GetEngineVersion()
{
	return FEngineVersion::Current().ToString();
}

FString UKismetSystemLibrary::GetGameName()
{
	return FString(FApp::GetGameName());
}

FString UKismetSystemLibrary::GetGameBundleId()
{
	return FString(FPlatformProcess::GetGameBundleId());
}

FString UKismetSystemLibrary::GetPlatformUserName()
{
	return FString(FPlatformProcess::UserName());
}

bool UKismetSystemLibrary::DoesImplementInterface(UObject* TestObject, TSubclassOf<UInterface> Interface)
{
	if (Interface != NULL && TestObject != NULL)
	{
		checkf(Interface->IsChildOf(UInterface::StaticClass()), TEXT("Interface parameter %s is not actually an interface."), *Interface->GetName());
		return TestObject->GetClass()->ImplementsInterface(Interface);
	}

	return false;
}

float UKismetSystemLibrary::GetGameTimeInSeconds(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	return World ? World->GetTimeSeconds() : 0.f;
}

bool UKismetSystemLibrary::IsServer(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	return World ? (World->GetNetMode() != NM_Client) : false;
}

bool UKismetSystemLibrary::IsDedicatedServer(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	if (World)
	{
		return (World->GetNetMode() == NM_DedicatedServer);
	}
	return IsRunningDedicatedServer();
}

bool UKismetSystemLibrary::IsPackagedForDistribution()
{
	return FPlatformMisc::IsPackagedForDistribution();
}

FString UKismetSystemLibrary::GetUniqueDeviceId()
{
	return FPlatformMisc::GetUniqueDeviceId();
}

int32 UKismetSystemLibrary::MakeLiteralInt(int32 Value)
{
	return Value;
}

float UKismetSystemLibrary::MakeLiteralFloat(float Value)
{
	return Value;
}

bool UKismetSystemLibrary::MakeLiteralBool(bool Value)
{
	return Value;
}

FName UKismetSystemLibrary::MakeLiteralName(FName Value)
{
	return Value;
}

uint8 UKismetSystemLibrary::MakeLiteralByte(uint8 Value)
{
	return Value;
}

FString UKismetSystemLibrary::MakeLiteralString(const FString& Value)
{
	return Value;
}

FText UKismetSystemLibrary::MakeLiteralText(FText Value)
{
	return Value;
}

UObject* UKismetSystemLibrary::Conv_InterfaceToObject(const FScriptInterface& Interface)
{
	return Interface.GetObject();
}

void UKismetSystemLibrary::PrintString(UObject* WorldContextObject, const FString& InString, bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) // Do not Print in Shipping or Test

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, false);
	FString Prefix;
	if (World)
	{
		if (World->WorldType == EWorldType::PIE)
		{
			switch(World->GetNetMode())
			{
				case NM_Client:
					Prefix = FString::Printf(TEXT("Client %d: "), GPlayInEditorID - 1);
					break;
				case NM_DedicatedServer:
				case NM_ListenServer:
					Prefix = FString::Printf(TEXT("Server: "));
					break;
				case NM_Standalone:
					break;
			}
		}
	}
	
	const FString FinalDisplayString = Prefix + InString;
	FString FinalLogString = FinalDisplayString;

	static const FBoolConfigValueHelper DisplayPrintStringSource(TEXT("Kismet"), TEXT("bLogPrintStringSource"), GEngineIni);
	if (DisplayPrintStringSource)
	{
		const FString SourceObjectPrefix = FString::Printf(TEXT("[%s] "), *GetNameSafe(WorldContextObject));
		FinalLogString = SourceObjectPrefix + FinalLogString;
	}

	if (bPrintToLog)
	{
		UE_LOG(LogBlueprintUserMessages, Log, TEXT("%s"), *FinalLogString);
		
		APlayerController* PC = (WorldContextObject ? UGameplayStatics::GetPlayerController(WorldContextObject, 0) : NULL);
		ULocalPlayer* LocalPlayer = (PC ? Cast<ULocalPlayer>(PC->Player) : NULL);
		if (LocalPlayer && LocalPlayer->ViewportClient && LocalPlayer->ViewportClient->ViewportConsole)
		{
			LocalPlayer->ViewportClient->ViewportConsole->OutputText(FinalDisplayString);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Verbose, TEXT("%s"), *FinalLogString);
	}

	// Also output to the screen, if possible
	if (bPrintToScreen)
	{
		if (GAreScreenMessagesEnabled)
		{
			if (GConfig && Duration < 0)
			{
				GConfig->GetFloat( TEXT("Kismet"), TEXT("PrintStringDuration"), Duration, GEngineIni );
			}
			GEngine->AddOnScreenDebugMessage((uint64)-1, Duration, TextColor.ToFColor(true), FinalDisplayString);
		}
		else
		{
			UE_LOG(LogBlueprint, VeryVerbose, TEXT("Screen messages disabled (!GAreScreenMessagesEnabled).  Cannot print to screen."));
		}
	}
#endif
}

void UKismetSystemLibrary::PrintText(UObject* WorldContextObject, const FText InText, bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) // Do not Print in Shipping or Test
	PrintString(WorldContextObject, InText.ToString(), bPrintToScreen, bPrintToLog, TextColor, Duration);
#endif
}

void UKismetSystemLibrary::PrintWarning(const FString& InString)
{
	PrintString(NULL, InString, true, true);
}

void UKismetSystemLibrary::SetWindowTitle(const FText& Title)
{
	UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
	if (GameEngine != nullptr)
	{
		TSharedPtr<SWindow> GameViewportWindow = GameEngine->GameViewportWindow.Pin();
		if (GameViewportWindow.IsValid())
		{
			GameViewportWindow->SetTitle(Title);
		}
	}
}

void UKismetSystemLibrary::ExecuteConsoleCommand(UObject* WorldContextObject, const FString& Command, APlayerController* Player)
{
	// First, try routing through the primary player
	APlayerController* TargetPC = Player ? Player : UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if( TargetPC )
	{
		TargetPC->ConsoleCommand(Command, true);
	}
}

void UKismetSystemLibrary::QuitGame(UObject* WorldContextObject, class APlayerController* SpecificPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference)
{
	APlayerController* TargetPC = SpecificPlayer ? SpecificPlayer : UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if( TargetPC )
	{
		if ( QuitPreference == EQuitPreference::Background)
		{
			TargetPC->ConsoleCommand("quit background");
		}
		else
		{
			TargetPC->ConsoleCommand("quit");
		}
	}
}

bool UKismetSystemLibrary::K2_IsValidTimerHandle(FTimerHandle TimerHandle)
{
	return TimerHandle.IsValid();
}

FTimerHandle UKismetSystemLibrary::K2_InvalidateTimerHandle(FTimerHandle& TimerHandle)
{
	TimerHandle.Invalidate();
	return TimerHandle;
}

FTimerHandle UKismetSystemLibrary::K2_SetTimer(UObject* Object, FString FunctionName, float Time, bool bLooping)
{
	FName const FunctionFName(*FunctionName);

	if (Object)
	{
		UFunction* const Func = Object->FindFunction(FunctionFName);
		if ( Func && (Func->ParmsSize > 0) )
		{
			// User passed in a valid function, but one that takes parameters
			// FTimerDynamicDelegate expects zero parameters and will choke on execution if it tries
			// to execute a mismatched function
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("SetTimer passed a function (%s) that expects parameters."), *FunctionName);
			return FTimerHandle();
		}
	}

	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, FunctionFName);
	return K2_SetTimerDelegate(Delegate, Time, bLooping);
}

FTimerHandle UKismetSystemLibrary::K2_SetTimerDelegate(FTimerDynamicDelegate Delegate, float Time, bool bLooping)
{
	FTimerHandle Handle;
	if (Delegate.IsBound())
	{
		const UWorld* const World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.SetTimer(Handle, Delegate, Time, bLooping);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("SetTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}

	return Handle;
}

void UKismetSystemLibrary::K2_ClearTimer(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	K2_ClearTimerDelegate(Delegate);
}

void UKismetSystemLibrary::K2_ClearTimerDelegate(FTimerDynamicDelegate Delegate)
{
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if (World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.ClearTimer(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("ClearTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
}

void UKismetSystemLibrary::K2_ClearTimerHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
}

void UKismetSystemLibrary::K2_ClearAndInvalidateTimerHandle(UObject* WorldContextObject, FTimerHandle& Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
		if (World)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
}

void UKismetSystemLibrary::K2_PauseTimer(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	K2_PauseTimerDelegate(Delegate);
}

void UKismetSystemLibrary::K2_PauseTimerDelegate(FTimerDynamicDelegate Delegate)
{
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.PauseTimer(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("PauseTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
}

void UKismetSystemLibrary::K2_PauseTimerHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			World->GetTimerManager().PauseTimer(Handle);
		}
	}
}

void UKismetSystemLibrary::K2_UnPauseTimer(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	K2_UnPauseTimerDelegate(Delegate);
}

void UKismetSystemLibrary::K2_UnPauseTimerDelegate(FTimerDynamicDelegate Delegate)
{
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			TimerManager.UnPauseTimer(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning,
			TEXT("UnPauseTimer passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
}

void UKismetSystemLibrary::K2_UnPauseTimerHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			World->GetTimerManager().UnPauseTimer(Handle);
		}
	}
}

bool UKismetSystemLibrary::K2_IsTimerActive(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_IsTimerActiveDelegate(Delegate);
}

bool UKismetSystemLibrary::K2_IsTimerActiveDelegate(FTimerDynamicDelegate Delegate)
{
	bool bIsActive = false;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			bIsActive = TimerManager.IsTimerActive(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("IsTimerActive passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}

	return bIsActive;
}

bool UKismetSystemLibrary::K2_IsTimerActiveHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	bool bIsActive = false;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			bIsActive = World->GetTimerManager().IsTimerActive(Handle);
		}
	}

	return bIsActive;
}

bool UKismetSystemLibrary::K2_IsTimerPaused(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_IsTimerPausedDelegate(Delegate);
}

bool UKismetSystemLibrary::K2_IsTimerPausedDelegate(FTimerDynamicDelegate Delegate)
{
	bool bIsPaused = false;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			bIsPaused = TimerManager.IsTimerPaused(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("IsTimerPaused passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return bIsPaused;
}

bool UKismetSystemLibrary::K2_IsTimerPausedHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	bool bIsPaused = false;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			bIsPaused = World->GetTimerManager().IsTimerPaused(Handle);
		}
	}

	return bIsPaused;
}

bool UKismetSystemLibrary::K2_TimerExists(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_TimerExistsDelegate(Delegate);
}

bool UKismetSystemLibrary::K2_TimerExistsDelegate(FTimerDynamicDelegate Delegate)
{
	bool bTimerExists = false;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			bTimerExists = TimerManager.TimerExists(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning,
			TEXT("TimerExists passed a bad function (%s) or object (%s)"),
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return bTimerExists;
}

bool UKismetSystemLibrary::K2_TimerExistsHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	bool bTimerExists = false;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			bTimerExists = World->GetTimerManager().TimerExists(Handle);
		}
	}

	return bTimerExists;
}

float UKismetSystemLibrary::K2_GetTimerElapsedTime(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_GetTimerElapsedTimeDelegate(Delegate);
}

float UKismetSystemLibrary::K2_GetTimerElapsedTimeDelegate(FTimerDynamicDelegate Delegate)
{
	float ElapsedTime = 0.f;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			ElapsedTime = TimerManager.GetTimerElapsed(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("GetTimerElapsedTime passed a bad function (%s) or object (%s)"), 
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return ElapsedTime;
}

float UKismetSystemLibrary::K2_GetTimerElapsedTimeHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	float ElapsedTime = 0.f;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			ElapsedTime = World->GetTimerManager().GetTimerElapsed(Handle);
		}
	}

	return ElapsedTime;
}

float UKismetSystemLibrary::K2_GetTimerRemainingTime(UObject* Object, FString FunctionName)
{
	FTimerDynamicDelegate Delegate;
	Delegate.BindUFunction(Object, *FunctionName);

	return K2_GetTimerRemainingTimeDelegate(Delegate);
}

float UKismetSystemLibrary::K2_GetTimerRemainingTimeDelegate(FTimerDynamicDelegate Delegate)
{
	float RemainingTime = 0.f;
	if (Delegate.IsBound())
	{
		UWorld* World = GEngine->GetWorldFromContextObject(Delegate.GetUObject());
		if(World)
		{
			FTimerManager& TimerManager = World->GetTimerManager();
			FTimerHandle Handle = TimerManager.K2_FindDynamicTimerHandle(Delegate);
			RemainingTime = TimerManager.GetTimerRemaining(Handle);
		}
	}
	else
	{
		UE_LOG(LogBlueprintUserMessages, Warning, 
			TEXT("GetTimerRemainingTime passed a bad function (%s) or object (%s)"), 
			*Delegate.GetFunctionName().ToString(), *GetNameSafe(Delegate.GetUObject()));
	}
	return RemainingTime;
}

float UKismetSystemLibrary::K2_GetTimerRemainingTimeHandle(UObject* WorldContextObject, FTimerHandle Handle)
{
	float RemainingTime = 0.f;
	if (Handle.IsValid())
	{
		UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
		if (World)
		{
			RemainingTime = World->GetTimerManager().GetTimerRemaining(Handle);
		}
	}

	return RemainingTime;
}

void UKismetSystemLibrary::SetIntPropertyByName(UObject* Object, FName PropertyName, int32 Value)
{
	if(Object != NULL)
	{
		UIntProperty* IntProp = FindField<UIntProperty>(Object->GetClass(), PropertyName);
		if(IntProp != NULL)
		{
			IntProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetBytePropertyByName(UObject* Object, FName PropertyName, uint8 Value)
{
	if(Object != NULL)
	{
		UByteProperty* ByteProp = FindField<UByteProperty>(Object->GetClass(), PropertyName);
		if(ByteProp != NULL)
		{
			ByteProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetFloatPropertyByName(UObject* Object, FName PropertyName, float Value)
{
	if(Object != NULL)
	{
		UFloatProperty* FloatProp = FindField<UFloatProperty>(Object->GetClass(), PropertyName);
		if(FloatProp != NULL)
		{
			FloatProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetBoolPropertyByName(UObject* Object, FName PropertyName, bool Value)
{
	if(Object != NULL)
	{
		UBoolProperty* BoolProp = FindField<UBoolProperty>(Object->GetClass(), PropertyName);
		if(BoolProp != NULL)
		{
			BoolProp->SetPropertyValue_InContainer(Object, Value );
		}
	}
}

void UKismetSystemLibrary::SetObjectPropertyByName(UObject* Object, FName PropertyName, UObject* Value)
{
	if(Object != NULL && Value != NULL)
	{
		UObjectPropertyBase* ObjectProp = FindField<UObjectPropertyBase>(Object->GetClass(), PropertyName);
		if(ObjectProp != NULL && Value->IsA(ObjectProp->PropertyClass)) // check it's the right type
		{
			ObjectProp->SetObjectPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetClassPropertyByName(UObject* Object, FName PropertyName, TSubclassOf<UObject> Value)
{
	if (Object && *Value)
	{
		UClassProperty* ClassProp = FindField<UClassProperty>(Object->GetClass(), PropertyName);
		if (ClassProp != NULL && Value->IsChildOf(ClassProp->MetaClass)) // check it's the right type
		{
			ClassProp->SetObjectPropertyValue_InContainer(Object, *Value);
		}
	}
}

void UKismetSystemLibrary::SetInterfacePropertyByName(UObject* Object, FName PropertyName, const FScriptInterface& Value)
{
	if (Object)
	{
		UInterfaceProperty* InterfaceProp = FindField<UInterfaceProperty>(Object->GetClass(), PropertyName);
		if (InterfaceProp != NULL && Value.GetObject()->GetClass()->ImplementsInterface(InterfaceProp->InterfaceClass)) // check it's the right type
		{
			InterfaceProp->SetPropertyValue_InContainer(Object, Value);
		}
	}
}

void UKismetSystemLibrary::SetStringPropertyByName(UObject* Object, FName PropertyName, const FString& Value)
{
	if(Object != NULL)
	{
		UStrProperty* StringProp = FindField<UStrProperty>(Object->GetClass(), PropertyName);
		if(StringProp != NULL)
		{
			StringProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetNamePropertyByName(UObject* Object, FName PropertyName, const FName& Value)
{
	if(Object != NULL)
	{
		UNameProperty* NameProp = FindField<UNameProperty>(Object->GetClass(), PropertyName);
		if(NameProp != NULL)
		{
			NameProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetAssetPropertyByName(UObject* Object, FName PropertyName, const TAssetPtr<UObject>& Value)
{
	if (Object != NULL)
	{
		UAssetObjectProperty* ObjectProp = FindField<UAssetObjectProperty>(Object->GetClass(), PropertyName);
		const FAssetPtr* AssetPtr = (const FAssetPtr*)(&Value);
		ObjectProp->SetPropertyValue_InContainer(Object, *AssetPtr);
	}
}

void UKismetSystemLibrary::SetAssetClassPropertyByName(UObject* Object, FName PropertyName, const TAssetSubclassOf<UObject>& Value)
{
	if (Object != NULL)
	{
		UAssetClassProperty* ObjectProp = FindField<UAssetClassProperty>(Object->GetClass(), PropertyName);
		const FAssetPtr* AssetPtr = (const FAssetPtr*)(&Value);
		ObjectProp->SetPropertyValue_InContainer(Object, *AssetPtr);
	}
}

UObject* UKismetSystemLibrary::Conv_AssetToObject(const TAssetPtr<UObject>& Asset)
{
	return ((const FAssetPtr*)&Asset)->Get();
}

TSubclassOf<UObject> UKismetSystemLibrary::Conv_AssetClassToClass(const TAssetSubclassOf<UObject>& AssetClass)
{
	return Cast<UClass>(((const FAssetPtr*)&AssetClass)->Get());
}

void UKismetSystemLibrary::SetTextPropertyByName(UObject* Object, FName PropertyName, const FText& Value)
{
	if(Object != NULL)
	{
		UTextProperty* TextProp = FindField<UTextProperty>(Object->GetClass(), PropertyName);
		if(TextProp != NULL)
		{
			TextProp->SetPropertyValue_InContainer(Object, Value);
		}		
	}
}

void UKismetSystemLibrary::SetVectorPropertyByName(UObject* Object, FName PropertyName, const FVector& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* VectorStruct = TBaseStructure<FVector>::Get();
		UStructProperty* VectorProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(VectorProp != NULL && VectorProp->Struct == VectorStruct)
		{
			*VectorProp->ContainerPtrToValuePtr<FVector>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetRotatorPropertyByName(UObject* Object, FName PropertyName, const FRotator& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* RotatorStruct = TBaseStructure<FRotator>::Get();
		UStructProperty* RotatorProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(RotatorProp != NULL && RotatorProp->Struct == RotatorStruct)
		{
			*RotatorProp->ContainerPtrToValuePtr<FRotator>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetLinearColorPropertyByName(UObject* Object, FName PropertyName, const FLinearColor& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* ColorStruct = TBaseStructure<FLinearColor>::Get();
		UStructProperty* ColorProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(ColorProp != NULL && ColorProp->Struct == ColorStruct)
		{
			*ColorProp->ContainerPtrToValuePtr<FLinearColor>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetTransformPropertyByName(UObject* Object, FName PropertyName, const FTransform& Value)
{
	if(Object != NULL)
	{
		UScriptStruct* TransformStruct = TBaseStructure<FTransform>::Get();
		UStructProperty* TransformProp = FindField<UStructProperty>(Object->GetClass(), PropertyName);
		if(TransformProp != NULL && TransformProp->Struct == TransformStruct)
		{
			*TransformProp->ContainerPtrToValuePtr<FTransform>(Object) = Value;
		}		
	}
}

void UKismetSystemLibrary::SetCollisionProfileNameProperty(UObject* Object, FName PropertyName, const FCollisionProfileName& Value)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.
	check(0);
}

void UKismetSystemLibrary::Generic_SetStructurePropertyByName(UObject* OwnerObject, FName StructPropertyName, const void* SrcStructAddr)
{
	if (OwnerObject != NULL)
	{
		UStructProperty* StructProp = FindField<UStructProperty>(OwnerObject->GetClass(), StructPropertyName);
		if (StructProp != NULL)
		{
			void* Dest = StructProp->ContainerPtrToValuePtr<void>(OwnerObject);
			StructProp->CopyValuesInternal(Dest, SrcStructAddr, 1);
		}
	}
}

void UKismetSystemLibrary::GetActorListFromComponentList(const TArray<UPrimitiveComponent*>& ComponentList, UClass* ActorClassFilter, TArray<class AActor*>& OutActorList)
{
	OutActorList.Empty();
	for (int32 CompIdx=0; CompIdx<ComponentList.Num(); ++CompIdx)
	{
		UPrimitiveComponent* const C = ComponentList[CompIdx];
		if (C)
		{
			AActor* const Owner = C->GetOwner();
			if (Owner)
			{
				if ( !ActorClassFilter || Owner->IsA(ActorClassFilter) )
				{
					OutActorList.AddUnique(Owner);
				}
			}
		}
	}
}

bool UKismetSystemLibrary::SphereOverlapActors_DEPRECATED(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = SphereOverlapComponents_DEPRECATED(WorldContextObject, SpherePos, SphereRadius, Filter, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::SphereOverlapComponents_DEPRECATED(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	static FName SphereOverlapComponentsName(TEXT("SphereOverlapComponents"));
	FCollisionQueryParams Params(SphereOverlapComponentsName, false);
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams::InitType InitType = FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter(Filter);
	
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	World->OverlapMultiByObjectType(Overlaps, SpherePos, FQuat::Identity, FCollisionObjectQueryParams(InitType), FCollisionShape::MakeSphere(SphereRadius), Params);
	
	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}



bool UKismetSystemLibrary::BoxOverlapActors_DEPRECATED(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = BoxOverlapComponents_DEPRECATED(WorldContextObject, BoxPos, BoxExtent, Filter, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::BoxOverlapComponents_DEPRECATED(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	static FName BoxOverlapComponentsName(TEXT("BoxOverlapComponents"));
	FCollisionQueryParams Params(BoxOverlapComponentsName, false);
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams::InitType InitType = FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter(Filter);
	
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	World->OverlapMultiByObjectType(Overlaps, BoxPos, FQuat::Identity, FCollisionObjectQueryParams(InitType), FCollisionShape::MakeBox(BoxExtent), Params);

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::CapsuleOverlapActors_DEPRECATED(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = CapsuleOverlapComponents_DEPRECATED(WorldContextObject, CapsulePos, Radius, HalfHeight, Filter, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::CapsuleOverlapComponents_DEPRECATED(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	static FName CapsuleOverlapComponentsName(TEXT("CapsuleOverlapComponents"));
	FCollisionQueryParams Params(CapsuleOverlapComponentsName, false);
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;
	
	FCollisionObjectQueryParams::InitType InitType = FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter(Filter);

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );	
	World->OverlapMultiByObjectType(Overlaps, CapsulePos, FQuat::Identity, FCollisionObjectQueryParams(InitType), FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::ComponentOverlapActors_DEPRECATED(UPrimitiveComponent* Component, const FTransform& ComponentTransform, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = ComponentOverlapComponents_DEPRECATED(Component, ComponentTransform, Filter, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::ComponentOverlapComponents_DEPRECATED(UPrimitiveComponent* Component, const FTransform& ComponentTransform, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	static FName ComponentOverlapComponentsName(TEXT("ComponentOverlapComponents"));
	FComponentQueryParams Params(ComponentOverlapComponentsName);	
	Params.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams::InitType InitType = FCollisionObjectQueryParams::GetCollisionChannelFromOverlapFilter(Filter);
	check( Component->GetWorld());
	Component->GetWorld()->ComponentOverlapMulti(Overlaps, Component, ComponentTransform.GetTranslation(), ComponentTransform.GetRotation(), Params, FCollisionObjectQueryParams(InitType));

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::SphereOverlapActors_NEW(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = SphereOverlapComponents_NEW(WorldContextObject, SpherePos, SphereRadius, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::SphereOverlapComponents_NEW(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	static FName SphereOverlapComponentsName(TEXT("SphereOverlapComponents"));
	FCollisionQueryParams Params(SphereOverlapComponentsName, false);
	Params.AddIgnoredActors(ActorsToIgnore);
	Params.bTraceAsyncScene = true;
	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
		ObjectParams.AddObjectTypesToQuery(Channel);
	}


	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	if(World != nullptr)
	{
		World->OverlapMultiByObjectType(Overlaps, SpherePos, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(SphereRadius), Params);
	}

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}



bool UKismetSystemLibrary::BoxOverlapActors_NEW(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = BoxOverlapComponents_NEW(WorldContextObject, BoxPos, BoxExtent, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::BoxOverlapComponents_NEW(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	static FName BoxOverlapComponentsName(TEXT("BoxOverlapComponents"));
	FCollisionQueryParams Params(BoxOverlapComponentsName, false);
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
		ObjectParams.AddObjectTypesToQuery(Channel);
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	if (World != nullptr)
	{
		World->OverlapMultiByObjectType(Overlaps, BoxPos, FQuat::Identity, ObjectParams, FCollisionShape::MakeBox(BoxExtent), Params);
	}

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::CapsuleOverlapActors_NEW(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = CapsuleOverlapComponents_NEW(WorldContextObject, CapsulePos, Radius, HalfHeight, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::CapsuleOverlapComponents_NEW(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	static FName CapsuleOverlapComponentsName(TEXT("CapsuleOverlapComponents"));
	FCollisionQueryParams Params(CapsuleOverlapComponentsName, false);
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
		ObjectParams.AddObjectTypesToQuery(Channel);
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );	
	if (World != nullptr)
	{
		World->OverlapMultiByObjectType(Overlaps, CapsulePos, FQuat::Identity, ObjectParams, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
	}

	for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
	{
		FOverlapResult const& O = Overlaps[OverlapIdx];
		if (O.Component.IsValid())
		{ 
			if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
			{
				OutComponents.Add(O.Component.Get());
			}
		}
	}

	return (OutComponents.Num() > 0);
}


bool UKismetSystemLibrary::ComponentOverlapActors_NEW(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	TArray<UPrimitiveComponent*> OverlapComponents;
	bool bOverlapped = ComponentOverlapComponents_NEW(Component, ComponentTransform, ObjectTypes, NULL, ActorsToIgnore, OverlapComponents);
	if (bOverlapped)
	{
		GetActorListFromComponentList(OverlapComponents, ActorClassFilter, OutActors);
	}

	return (OutActors.Num() > 0);
}

bool UKismetSystemLibrary::ComponentOverlapComponents_NEW(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<UPrimitiveComponent*>& OutComponents)
{
	OutComponents.Empty();

	if(Component != nullptr)
	{
		static FName ComponentOverlapComponentsName(TEXT("ComponentOverlapComponents"));
		FComponentQueryParams Params(ComponentOverlapComponentsName);	
		Params.bTraceAsyncScene = true;
		Params.AddIgnoredActors(ActorsToIgnore);

		TArray<FOverlapResult> Overlaps;

		FCollisionObjectQueryParams ObjectParams;
		for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
		{
			const ECollisionChannel & Channel = UCollisionProfile::Get()->ConvertToCollisionChannel(false, *Iter);
			ObjectParams.AddObjectTypesToQuery(Channel);
		}

		check( Component->GetWorld());
		Component->GetWorld()->ComponentOverlapMulti(Overlaps, Component, ComponentTransform.GetTranslation(), ComponentTransform.GetRotation(), Params, ObjectParams);

		for (int32 OverlapIdx=0; OverlapIdx<Overlaps.Num(); ++OverlapIdx)
		{
			FOverlapResult const& O = Overlaps[OverlapIdx];
			if (O.Component.IsValid())
			{ 
				if ( !ComponentClassFilter || O.Component.Get()->IsA(ComponentClassFilter) )
				{
					OutComponents.Add(O.Component.Get());
				}
			}
		}
	}

	return (OutComponents.Num() > 0);
}
static const float KISMET_TRACE_DEBUG_DRAW_DURATION = 5.f;
static const float KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE = 16.f;

bool UKismetSystemLibrary::LineTraceSingle_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	return LineTraceSingle_DEPRECATED(WorldContextObject, Start, End, UEngineTypes::ConvertToCollisionChannel(TraceChannel), bTraceComplex, ActorsToIgnore, DrawDebugType, OutHit, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime);
}

bool UKismetSystemLibrary::LineTraceSingle_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceSingleName(TEXT("LineTraceSingle"));

	FCollisionQueryParams Params(LineTraceSingleName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? DrawTime : 0.f;

		// @fixme, draw line with thickness = 2.f?
		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugLine(World, Start, OutHit.ImpactPoint, TraceColor.ToFColor(true), bPersistent, LifeTime);
			::DrawDebugLine(World, OutHit.ImpactPoint, End, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
			::DrawDebugPoint(World, OutHit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::LineTraceMulti_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	return LineTraceMulti_DEPRECATED(WorldContextObject, Start, End, UEngineTypes::ConvertToCollisionChannel(TraceChannel), bTraceComplex, ActorsToIgnore, DrawDebugType, OutHits, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime);
}

bool UKismetSystemLibrary::LineTraceMulti_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceMultiName(TEXT("LineTraceMulti"));

	FCollisionQueryParams Params(LineTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );	
	bool const bHit = World->LineTraceMultiByChannel(OutHits, Start, End, TraceChannel, Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? DrawTime : 0.f;

		// @fixme, draw line with thickness = 2.f?
		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().ImpactPoint;
			::DrawDebugLine(World, Start, BlockingHitPoint , TraceColor.ToFColor(true), bPersistent, LifeTime);
			::DrawDebugLine(World, BlockingHitPoint, End, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx=0; HitIdx<OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? TraceColor.ToFColor(true) : TraceHitColor.ToFColor(true)), bPersistent, LifeTime);
		}
	}

	return bHit;
}

void DrawDebugSweptSphere(const UWorld* InWorld, FVector const& Start, FVector const& End, float Radius, FColor const& Color, bool bPersistentLines=false, float LifeTime=-1.f, uint8 DepthPriority=0)
{
	FVector const TraceVec = End - Start;
	float const Dist = TraceVec.Size();

	FVector const Center = Start + TraceVec * 0.5f;
	float const HalfHeight = (Dist * 0.5f) + Radius;

	FQuat const CapsuleRot = FRotationMatrix::MakeFromZ(TraceVec).ToQuat();
	::DrawDebugCapsule(InWorld, Center, HalfHeight, Radius, CapsuleRot, Color, bPersistentLines, LifeTime, DepthPriority );
}

void DrawDebugSweptBox(const UWorld* InWorld, FVector const& Start, FVector const& End, FRotator const & Orientation, FVector const & HalfSize, FColor const& Color, bool bPersistentLines = false, float LifeTime = -1.f, uint8 DepthPriority = 0)
{
	FVector const TraceVec = End - Start;
	float const Dist = TraceVec.Size();

	FVector const Center = Start + TraceVec * 0.5f;

	FQuat const CapsuleRot = Orientation.Quaternion();
	::DrawDebugBox(InWorld, Start, HalfSize, CapsuleRot, Color, bPersistentLines, LifeTime, DepthPriority);
	
	//now draw lines from vertices
	FVector Vertices[8];
	Vertices[0] = Start + CapsuleRot.RotateVector(FVector(-HalfSize.X, -HalfSize.Y, -HalfSize.Z));	//flt
	Vertices[1] = Start + CapsuleRot.RotateVector(FVector(-HalfSize.X, HalfSize.Y, -HalfSize.Z));	//frt
	Vertices[2] = Start + CapsuleRot.RotateVector(FVector(-HalfSize.X, -HalfSize.Y, HalfSize.Z));	//flb
	Vertices[3] = Start + CapsuleRot.RotateVector(FVector(-HalfSize.X, HalfSize.Y, HalfSize.Z));	//frb
	Vertices[4] = Start + CapsuleRot.RotateVector(FVector(HalfSize.X, -HalfSize.Y, -HalfSize.Z));	//blt
	Vertices[5] = Start + CapsuleRot.RotateVector(FVector(HalfSize.X, HalfSize.Y, -HalfSize.Z));	//brt
	Vertices[6] = Start + CapsuleRot.RotateVector(FVector(HalfSize.X, -HalfSize.Y, HalfSize.Z));	//blb
	Vertices[7] = Start + CapsuleRot.RotateVector(FVector(HalfSize.X, HalfSize.Y, HalfSize.Z));		//brb
	for (int32 VertexIdx = 0; VertexIdx < 8; ++VertexIdx)
	{
		::DrawDebugLine(InWorld, Vertices[VertexIdx], Vertices[VertexIdx] + TraceVec, Color, bPersistentLines, LifeTime, DepthPriority);
	}

	::DrawDebugBox(InWorld, End, HalfSize, CapsuleRot, Color, bPersistentLines, LifeTime, DepthPriority);
}


bool UKismetSystemLibrary::BoxTraceSingle(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	static const FName BoxTraceSingleName(TEXT("BoxTraceSingle"));

	FCollisionQueryParams Params(BoxTraceSingleName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	bool const bHit = World ? World->SweepSingleByChannel(OutHit, Start, End, Orientation.Quaternion(), UEngineTypes::ConvertToCollisionChannel(TraceChannel), FCollisionShape::MakeBox(HalfSize), Params) : false;

	if (DrawDebugType != EDrawDebugTrace::None && (World != nullptr))
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration) ? KISMET_TRACE_DEBUG_DRAW_DURATION : 0.f;

		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugSweptBox(World, Start, OutHit.Location, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptBox(World, OutHit.Location, End, Orientation, HalfSize, FColor::Green, bPersistent, LifeTime);
			::DrawDebugPoint(World, OutHit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, FColor::Red, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptBox(World, Start, End, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceMulti(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	static const FName BoxTraceMultiName(TEXT("BoxTraceMulti"));

	FCollisionQueryParams Params(BoxTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	bool const bHit = World ? World->SweepMultiByChannel(OutHits, Start, End, Orientation.Quaternion(), UEngineTypes::ConvertToCollisionChannel(TraceChannel), FCollisionShape::MakeBox(HalfSize), Params) : false;

	if (DrawDebugType != EDrawDebugTrace::None && (World != nullptr))
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration) ? KISMET_TRACE_DEBUG_DRAW_DURATION : 0.f;

		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().Location;
			::DrawDebugSweptBox(World, Start, BlockingHitPoint, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptBox(World, BlockingHitPoint, End, Orientation, HalfSize, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptBox(World, Start, End, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx = 0; HitIdx < OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? FColor::Red : FColor::Green), bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceSingle_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	return SphereTraceSingle_DEPRECATED(WorldContextObject, Start, End, Radius, UEngineTypes::ConvertToCollisionChannel(TraceChannel), bTraceComplex, ActorsToIgnore, DrawDebugType, OutHit, bIgnoreSelf);
}

bool UKismetSystemLibrary::SphereTraceSingle_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	static const FName SphereTraceSingleName(TEXT("SphereTraceSingle"));

	FCollisionQueryParams Params(SphereTraceSingleName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(Radius), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugSweptSphere(World, Start, OutHit.Location, Radius, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptSphere(World, OutHit.Location, End, Radius, FColor::Green, bPersistent, LifeTime);
			::DrawDebugPoint(World, OutHit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, FColor::Red, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptSphere(World, Start, End, Radius, FColor::Red, bPersistent, LifeTime);
		}
	}
	
	return bHit;
}

bool UKismetSystemLibrary::SphereTraceMulti_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	return SphereTraceMulti_DEPRECATED(WorldContextObject, Start, End, Radius, UEngineTypes::ConvertToCollisionChannel(TraceChannel), bTraceComplex, ActorsToIgnore, DrawDebugType, OutHits, bIgnoreSelf);
}

bool UKismetSystemLibrary::SphereTraceMulti_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	static const FName SphereTraceMultiName(TEXT("SphereTraceMulti"));

	FCollisionQueryParams Params(SphereTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepMultiByChannel(OutHits, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(Radius), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().Location;
			::DrawDebugSweptSphere(World, Start, BlockingHitPoint, Radius, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptSphere(World, BlockingHitPoint, End, Radius, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptSphere(World, Start, End, Radius, FColor::Red, bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx=0; HitIdx<OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? FColor::Red : FColor::Green), bPersistent, LifeTime);
		}
	}
	
	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceSingle_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	return CapsuleTraceSingle_DEPRECATED(WorldContextObject, Start, End, Radius, HalfHeight, UEngineTypes::ConvertToCollisionChannel(TraceChannel), bTraceComplex, ActorsToIgnore, DrawDebugType, OutHit, bIgnoreSelf);
}

bool UKismetSystemLibrary::CapsuleTraceSingle_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	static const FName CapsuleTraceSingleName(TEXT("CapsuleTraceSingle"));

	FCollisionQueryParams Params(CapsuleTraceSingleName, bTraceComplex);
	Params.AddIgnoredActors(ActorsToIgnore);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, OutHit.Location, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, OutHit.Location, FColor::Red, bPersistent, LifeTime);
			::DrawDebugPoint(World, OutHit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, FColor::Red, bPersistent, LifeTime);

			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Green, bPersistent, LifeTime);
			::DrawDebugLine(World, OutHit.Location, End, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, End, FColor::Red, bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceMulti_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	return CapsuleTraceMulti_DEPRECATED(WorldContextObject, Start, End, Radius, HalfHeight, UEngineTypes::ConvertToCollisionChannel(TraceChannel), bTraceComplex, ActorsToIgnore, DrawDebugType, OutHits, bIgnoreSelf);
}

bool UKismetSystemLibrary::CapsuleTraceMulti_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	static const FName CapsuleTraceMultiName(TEXT("CapsuleTraceMulti"));

	FCollisionQueryParams Params(CapsuleTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepMultiByChannel(OutHits, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().Location;
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, BlockingHitPoint, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, BlockingHitPoint, FColor::Red, bPersistent, LifeTime);

			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Green, bPersistent, LifeTime);
			::DrawDebugLine(World, BlockingHitPoint, End, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, End, FColor::Red, bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx=0; HitIdx<OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? FColor::Red : FColor::Green), bPersistent, LifeTime);
		}
	}

	return bHit;
}

/** Object Query functions **/
bool UKismetSystemLibrary::LineTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{

	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	return LineTraceSingleByObject_DEPRECATED(WorldContextObject, Start, End, CollisionObjectTraces, bTraceComplex, ActorsToIgnore, DrawDebugType, OutHit, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime);
}

bool UKismetSystemLibrary::LineTraceSingleByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceSingleName(TEXT("LineTraceSingle"));

	FCollisionQueryParams Params(LineTraceSingleName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectsToTrace.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? DrawTime : 0.f;

		// @fixme, draw line with thickness = 2.f?
		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugLine(World, Start, OutHit.ImpactPoint, TraceColor.ToFColor(true), bPersistent, LifeTime);
			::DrawDebugLine(World, OutHit.ImpactPoint, End, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
			::DrawDebugPoint(World, OutHit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::LineTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	return LineTraceMultiByObject_DEPRECATED(WorldContextObject, Start, End, CollisionObjectTraces, bTraceComplex, ActorsToIgnore, DrawDebugType, OutHits, bIgnoreSelf, TraceColor, TraceHitColor, DrawTime);
}

bool UKismetSystemLibrary::LineTraceMultiByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor, FLinearColor TraceHitColor, float DrawTime)
{
	static const FName LineTraceMultiName(TEXT("LineTraceMulti"));

	FCollisionQueryParams Params(LineTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectsToTrace.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );	
	bool const bHit = World->LineTraceMultiByObjectType(OutHits, Start, End, ObjectParams, Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? DrawTime : 0.f;

		// @fixme, draw line with thickness = 2.f?
		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().ImpactPoint;
			::DrawDebugLine(World, Start, BlockingHitPoint , TraceColor.ToFColor(true), bPersistent, LifeTime);
			::DrawDebugLine(World, BlockingHitPoint, End, TraceHitColor.ToFColor(true), bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugLine(World, Start, End, TraceColor.ToFColor(true), bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx=0; HitIdx<OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? TraceColor.ToFColor(true) : TraceHitColor.ToFColor(true)), bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	return SphereTraceSingleByObject_DEPRECATED(WorldContextObject, Start, End, Radius, CollisionObjectTraces, bTraceComplex, ActorsToIgnore, DrawDebugType, OutHit, bIgnoreSelf);
}

bool UKismetSystemLibrary::SphereTraceSingleByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	static const FName SphereTraceSingleName(TEXT("SphereTraceSingle"));

	FCollisionQueryParams Params(SphereTraceSingleName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectsToTrace.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugSweptSphere(World, Start, OutHit.Location, Radius, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptSphere(World, OutHit.Location, End, Radius, FColor::Green, bPersistent, LifeTime);
			::DrawDebugPoint(World, OutHit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, FColor::Red, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptSphere(World, Start, End, Radius, FColor::Red, bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::SphereTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	return SphereTraceMultiByObject_DEPRECATED(WorldContextObject, Start, End, Radius, CollisionObjectTraces, bTraceComplex, ActorsToIgnore, DrawDebugType, OutHits, bIgnoreSelf);
}

bool UKismetSystemLibrary::SphereTraceMultiByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	static const FName SphereTraceMultiName(TEXT("SphereTraceMulti"));

	FCollisionQueryParams Params(SphereTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectsToTrace.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepMultiByObjectType(OutHits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().Location;
			::DrawDebugSweptSphere(World, Start, BlockingHitPoint, Radius, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptSphere(World, BlockingHitPoint, End, Radius, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptSphere(World, Start, End, Radius, FColor::Red, bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx=0; HitIdx<OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? FColor::Red : FColor::Green), bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	static const FName BoxTraceSingleName(TEXT("BoxTraceSingle"));

	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	FCollisionQueryParams Params(BoxTraceSingleName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = CollisionObjectTraces.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	bool const bHit = World ? World->SweepSingleByObjectType(OutHit, Start, End, Orientation.Quaternion(), ObjectParams, FCollisionShape::MakeBox(HalfSize), Params) : false;

	if (DrawDebugType != EDrawDebugTrace::None && (World != nullptr))
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration) ? KISMET_TRACE_DEBUG_DRAW_DURATION : 0.f;

		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHit.Location;
			::DrawDebugSweptBox(World, Start, BlockingHitPoint, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptBox(World, BlockingHitPoint, End, Orientation, HalfSize, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptBox(World, Start, End, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::BoxTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, const TArray<TEnumAsByte<	EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	static const FName BoxTraceMultiName(TEXT("BoxTraceMulti"));

	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	FCollisionQueryParams Params(BoxTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = CollisionObjectTraces.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	bool const bHit = World ? World->SweepMultiByObjectType(OutHits, Start, End, Orientation.Quaternion(), ObjectParams, FCollisionShape::MakeBox(HalfSize), Params) : false;

	if (DrawDebugType != EDrawDebugTrace::None && (World != nullptr))
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration) ? KISMET_TRACE_DEBUG_DRAW_DURATION : 0.f;

		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().Location;
			::DrawDebugSweptBox(World, Start, BlockingHitPoint, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
			::DrawDebugSweptBox(World, BlockingHitPoint, End, Orientation, HalfSize, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugSweptBox(World, Start, End, Orientation, HalfSize, FColor::Red, bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx = 0; HitIdx < OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? FColor::Red : FColor::Green), bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}

	return CapsuleTraceSingleByObject_DEPRECATED(WorldContextObject, Start, End, Radius, HalfHeight, CollisionObjectTraces, bTraceComplex, ActorsToIgnore, DrawDebugType, OutHit, bIgnoreSelf);
}

bool UKismetSystemLibrary::CapsuleTraceSingleByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf)
{
	static const FName CapsuleTraceSingleName(TEXT("CapsuleTraceSingle"));

	FCollisionQueryParams Params(CapsuleTraceSingleName, bTraceComplex);
	Params.AddIgnoredActors(ActorsToIgnore);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectsToTrace.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHit.bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, OutHit.Location, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, OutHit.Location, FColor::Red, bPersistent, LifeTime);
			::DrawDebugPoint(World, OutHit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, FColor::Red, bPersistent, LifeTime);

			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Green, bPersistent, LifeTime);
			::DrawDebugLine(World, OutHit.Location, End, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, End, FColor::Red, bPersistent, LifeTime);
		}
	}

	return bHit;
}

bool UKismetSystemLibrary::CapsuleTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	TArray<TEnumAsByte<ECollisionChannel>> CollisionObjectTraces;
	CollisionObjectTraces.AddUninitialized(ObjectTypes.Num());

	for (auto Iter = ObjectTypes.CreateConstIterator(); Iter; ++Iter)
	{
		CollisionObjectTraces[Iter.GetIndex()] = UEngineTypes::ConvertToCollisionChannel(*Iter);
	}
	return CapsuleTraceMultiByObject_DEPRECATED(WorldContextObject, Start, End, Radius, HalfHeight, CollisionObjectTraces, bTraceComplex, ActorsToIgnore, DrawDebugType, OutHits, bIgnoreSelf);
}

bool UKismetSystemLibrary::CapsuleTraceMultiByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf)
{
	static const FName CapsuleTraceMultiName(TEXT("CapsuleTraceMulti"));

	FCollisionQueryParams Params(CapsuleTraceMultiName, bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bReturnFaceIndex = !UPhysicsSettings::Get()->bSuppressFaceRemapTable; // Ask for face index, as long as we didn't disable globally
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);
	if (bIgnoreSelf)
	{
		AActor* IgnoreActor = Cast<AActor>(WorldContextObject);
		if (IgnoreActor)
		{
			Params.AddIgnoredActor(IgnoreActor);
		}
		else
		{
			// find owner
			UObject* CurrentObject = WorldContextObject;
			while (CurrentObject)
			{
				CurrentObject = CurrentObject->GetOuter();
				IgnoreActor = Cast<AActor>(CurrentObject);
				if (IgnoreActor)
				{
					Params.AddIgnoredActor(IgnoreActor);
					break;
				}
			}
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (auto Iter = ObjectsToTrace.CreateConstIterator(); Iter; ++Iter)
	{
		const ECollisionChannel & Channel = (*Iter);
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
		else
		{
			UE_LOG(LogBlueprintUserMessages, Warning, TEXT("%d isn't valid object type"), (int32)Channel);
		}
	}

	if (ObjectParams.IsValid() == false)
	{
		UE_LOG(LogBlueprintUserMessages, Warning, TEXT("Invalid object types"));
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	bool const bHit = World->SweepMultiByObjectType(OutHits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);

	if (DrawDebugType != EDrawDebugTrace::None)
	{
		bool bPersistent = DrawDebugType == EDrawDebugTrace::Persistent;
		float LifeTime = (DrawDebugType == EDrawDebugTrace::ForDuration)? KISMET_TRACE_DEBUG_DRAW_DURATION: 0.f;

		if (bHit && OutHits.Last().bBlockingHit)
		{
			// Red up to the blocking hit, green thereafter
			FVector const BlockingHitPoint = OutHits.Last().Location;
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, BlockingHitPoint, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, BlockingHitPoint, FColor::Red, bPersistent, LifeTime);

			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Green, bPersistent, LifeTime);
			::DrawDebugLine(World, BlockingHitPoint, End, FColor::Green, bPersistent, LifeTime);
		}
		else
		{
			// no hit means all red
			::DrawDebugCapsule(World, Start, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugCapsule(World, End, HalfHeight, Radius, FQuat::Identity, FColor::Red, bPersistent, LifeTime);
			::DrawDebugLine(World, Start, End, FColor::Red, bPersistent, LifeTime);
		}

		// draw hits
		for (int32 HitIdx=0; HitIdx<OutHits.Num(); ++HitIdx)
		{
			FHitResult const& Hit = OutHits[HitIdx];
			::DrawDebugPoint(World, Hit.ImpactPoint, KISMET_TRACE_DEBUG_IMPACTPOINT_SIZE, (Hit.bBlockingHit ? FColor::Red : FColor::Green), bPersistent, LifeTime);
		}
	}

	return bHit;
}

/** Draw a debug line */
void UKismetSystemLibrary::DrawDebugLine(UObject* WorldContextObject, FVector const LineStart, FVector const LineEnd, FLinearColor Color, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(World != nullptr)
	{
		::DrawDebugLine(World, LineStart, LineEnd, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
}

/** Draw a debug circle */
void UKismetSystemLibrary::DrawDebugCircle(UObject* WorldContextObject,FVector Center, float Radius, int32 NumSegments, FLinearColor LineColor, float LifeTime, float Thickness, FVector YAxis, FVector ZAxis, bool bDrawAxis)
{ 
	::DrawDebugCircle(GEngine->GetWorldFromContextObject(WorldContextObject),Center, Radius, NumSegments, LineColor.ToFColor(true), false, LifeTime, SDPG_World, Thickness, YAxis, ZAxis, bDrawAxis);
}

/** Draw a debug point */
void UKismetSystemLibrary::DrawDebugPoint(UObject* WorldContextObject, FVector const Position, float Size, FLinearColor PointColor, float LifeTime)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugPoint(World, Position, Size, PointColor.ToFColor(true), false, LifeTime, SDPG_World);
	}
}

/** Draw directional arrow, pointing from LineStart to LineEnd. */
void UKismetSystemLibrary::DrawDebugArrow(UObject* WorldContextObject, FVector const LineStart, FVector const LineEnd, float ArrowSize, FLinearColor Color, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugDirectionalArrow(World, LineStart, LineEnd, ArrowSize, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
}

/** Draw a debug box */
void UKismetSystemLibrary::DrawDebugBox(UObject* WorldContextObject, FVector const Center, FVector Extent, FLinearColor Color, const FRotator Rotation, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if(World != nullptr)
	{
		if (Rotation == FRotator::ZeroRotator)
		{
			::DrawDebugBox(World, Center, Extent, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
		}
		else
		{
			::DrawDebugBox(World, Center, Extent, Rotation.Quaternion(), Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
		}
	}
}

/** Draw a debug coordinate system. */
void UKismetSystemLibrary::DrawDebugCoordinateSystem(UObject* WorldContextObject, FVector const AxisLoc, FRotator const AxisRot, float Scale, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugCoordinateSystem(World, AxisLoc, AxisRot, Scale, false, LifeTime, SDPG_World, Thickness);
	}
}

/** Draw a debug sphere */
void UKismetSystemLibrary::DrawDebugSphere(UObject* WorldContextObject, FVector const Center, float Radius, int32 Segments, FLinearColor Color, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugSphere(World, Center, Radius, Segments, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
}

/** Draw a debug cylinder */
void UKismetSystemLibrary::DrawDebugCylinder(UObject* WorldContextObject, FVector const Start, FVector const End, float Radius, int32 Segments, FLinearColor Color, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugCylinder(World, Start, End, Radius, Segments, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
}

/** Draw a debug cone */
void UKismetSystemLibrary::DrawDebugCone(UObject* WorldContextObject, FVector const Origin, FVector const Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FLinearColor Color, float Duration, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugCone(World, Origin, Direction, Length, AngleWidth, AngleHeight, NumSides, Color.ToFColor(true), false, Duration, SDPG_World, Thickness);
	}
}

void UKismetSystemLibrary::DrawDebugConeInDegrees(UObject* WorldContextObject, FVector const Origin, FVector const Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FLinearColor Color, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugCone(World, Origin, Direction, Length, FMath::DegreesToRadians(AngleWidth), FMath::DegreesToRadians(AngleHeight), NumSides, Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
}

/** Draw a debug capsule */
void UKismetSystemLibrary::DrawDebugCapsule(UObject* WorldContextObject, FVector const Center, float HalfHeight, float Radius, const FRotator Rotation, FLinearColor Color, float LifeTime, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugCapsule(World, Center, HalfHeight, Radius, Rotation.Quaternion(), Color.ToFColor(true), false, LifeTime, SDPG_World, Thickness);
	}
}

/** Draw a debug string at a 3d world location. */
void UKismetSystemLibrary::DrawDebugString(UObject* WorldContextObject, FVector const TextLocation, const FString& Text, class AActor* TestBaseActor, FLinearColor TextColor, float Duration)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugString(World, TextLocation, Text, TestBaseActor, TextColor.ToFColor(true), Duration);
	}
}

/** Removes all debug strings. */
void UKismetSystemLibrary::FlushDebugStrings( UObject* WorldContextObject )
{
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	if(World != nullptr)
	{
		::FlushDebugStrings( World );
	}
}

/** Draws a debug plane. */
void UKismetSystemLibrary::DrawDebugPlane(UObject* WorldContextObject, FPlane const& P, FVector const Loc, float Size, FLinearColor Color, float LifeTime)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		::DrawDebugSolidPlane(World, P, Loc, Size, Color.ToFColor(true), false, LifeTime, SDPG_World);
	}
}

/** Flush all persistent debug lines and shapes */
void UKismetSystemLibrary::FlushPersistentDebugLines(UObject* WorldContextObject)
{
	UWorld* World = GEngine->GetWorldFromContextObject( WorldContextObject );
	if(World != nullptr)
	{
		::FlushPersistentDebugLines( World );
	}
}

/** Draws a debug frustum. */
void UKismetSystemLibrary::DrawDebugFrustum(UObject* WorldContextObject, const FTransform& FrustumTransform, FLinearColor FrustumColor, float Duration, float Thickness)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if( World != nullptr && FrustumTransform.IsRotationNormalized() )
	{
		FMatrix FrustumToWorld =  FrustumTransform.ToMatrixWithScale();
		::DrawDebugFrustum(World, FrustumToWorld, FrustumColor.ToFColor(true), false, Duration, SDPG_World, Thickness);
	}
}

/** Draw a debug camera shape. */
void UKismetSystemLibrary::DrawDebugCamera(const ACameraActor* CameraActor, FLinearColor CameraColor, float Duration)
{
	if(CameraActor)
	{
		FVector CamLoc = CameraActor->GetActorLocation();
		FRotator CamRot = CameraActor->GetActorRotation();
		::DrawDebugCamera(CameraActor->GetWorld(), CameraActor->GetActorLocation(), CameraActor->GetActorRotation(), CameraActor->GetCameraComponent()->FieldOfView, 1.0f, CameraColor.ToFColor(true), false, Duration, SDPG_World);
	}
}

void UKismetSystemLibrary::DrawDebugFloatHistoryTransform(UObject* WorldContextObject, const FDebugFloatHistory& FloatHistory, const FTransform& DrawTransform, FVector2D DrawSize, FLinearColor DrawColor, float LifeTime)
{
	UWorld * World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World)
	{
		::DrawDebugFloatHistory(*World, FloatHistory, DrawTransform, DrawSize, DrawColor.ToFColor(true), false, LifeTime);
	}
}

void UKismetSystemLibrary::DrawDebugFloatHistoryLocation(UObject* WorldContextObject, const FDebugFloatHistory& FloatHistory, FVector DrawLocation, FVector2D DrawSize, FLinearColor DrawColor, float LifeTime)
{
	UWorld * World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World)
	{
		::DrawDebugFloatHistory(*World, FloatHistory, DrawLocation, DrawSize, DrawColor.ToFColor(true), false, LifeTime);
	}
}

FDebugFloatHistory UKismetSystemLibrary::AddFloatHistorySample(float Value, const FDebugFloatHistory& FloatHistory)
{
	FDebugFloatHistory NewDebugFloatHistory = FloatHistory;
	NewDebugFloatHistory.AddSample(Value);
	return NewDebugFloatHistory;
}

/** Mark as modified. */
void UKismetSystemLibrary::CreateCopyForUndoBuffer(UObject* ObjectToModify)
{
	if (ObjectToModify != NULL)
	{
		ObjectToModify->Modify();
	}
}

void UKismetSystemLibrary::GetComponentBounds(const USceneComponent* Component, FVector& Origin, FVector& BoxExtent, float& SphereRadius)
{
	if (Component != NULL)
	{
		Origin = Component->Bounds.Origin;
		BoxExtent = Component->Bounds.BoxExtent;
		SphereRadius = Component->Bounds.SphereRadius;
	}
	else
	{
		Origin = FVector::ZeroVector;
		BoxExtent = FVector::ZeroVector;
		SphereRadius = 0.0f;
		UE_LOG(LogBlueprintUserMessages, Verbose, TEXT("GetComponentBounds passed a bad component"));
	}
}

void UKismetSystemLibrary::GetActorBounds(const AActor* Actor, FVector& Origin, FVector& BoxExtent)
{
	if (Actor != NULL)
	{
		const FBox Bounds = Actor->GetComponentsBoundingBox(true);

		// To keep consistency with the other GetBounds functions, transform our result into an origin / extent formatting
		Bounds.GetCenterAndExtents(Origin, BoxExtent);
	}
	else
	{
		Origin = FVector::ZeroVector;
		BoxExtent = FVector::ZeroVector;
		UE_LOG(LogBlueprintUserMessages, Verbose, TEXT("GetActorBounds passed a bad actor"));
	}
}


void UKismetSystemLibrary::Delay(UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo )
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if (LatentActionManager.FindExistingAction<FDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == NULL)
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayAction(Duration, LatentInfo));
		}
	}
}

// Delay execution by Duration seconds; Calling again before the delay has expired will reset the countdown to Duration.
void UKismetSystemLibrary::RetriggerableDelay(UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FDelayAction* Action = LatentActionManager.FindExistingAction<FDelayAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
		if (Action == NULL)
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FDelayAction(Duration, LatentInfo));
		}
		else
		{
			// Reset the existing delay to the new duration
			Action->TimeRemaining = Duration;
		}
	}
}

void UKismetSystemLibrary::MoveComponentTo(USceneComponent* Component, FVector TargetRelativeLocation, FRotator TargetRelativeRotation, bool bEaseOut, bool bEaseIn, float OverTime, bool bForceShortestRotationPath, TEnumAsByte<EMoveComponentAction::Type> MoveAction, FLatentActionInfo LatentInfo)
{
	if (UWorld* World = ((Component != NULL) ? Component->GetWorld() : NULL))
	{
		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		FInterpolateComponentToAction* Action = LatentActionManager.FindExistingAction<FInterpolateComponentToAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);

		const FVector ComponentLocation = (Component != NULL) ? Component->RelativeLocation : FVector::ZeroVector;
		const FRotator ComponentRotation = (Component != NULL) ? Component->RelativeRotation : FRotator::ZeroRotator;

		// If not currently running
		if (Action == NULL)
		{
			if (MoveAction == EMoveComponentAction::Move)
			{
				// Only act on a 'move' input if not running
				Action = new FInterpolateComponentToAction(OverTime, LatentInfo, Component, bEaseOut, bEaseIn, bForceShortestRotationPath);

				Action->TargetLocation = TargetRelativeLocation;
				Action->TargetRotation = TargetRelativeRotation;

				Action->InitialLocation = ComponentLocation;
				Action->InitialRotation = ComponentRotation;

				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, Action);
			}
		}
		else
		{
			if (MoveAction == EMoveComponentAction::Move)
			{
				// A 'Move' action while moving restarts interpolation
				Action->TotalTime = OverTime;
				Action->TimeElapsed = 0.f;

				Action->TargetLocation = TargetRelativeLocation;
				Action->TargetRotation = TargetRelativeRotation;

				Action->InitialLocation = ComponentLocation;
				Action->InitialRotation = ComponentRotation;
			}
			else if (MoveAction == EMoveComponentAction::Stop)
			{
				// 'Stop' just stops the interpolation where it is
				Action->bInterpolating = false;
			}
			else if (MoveAction == EMoveComponentAction::Return)
			{
				// Return moves back to the beginning
				Action->TotalTime = Action->TimeElapsed;
				Action->TimeElapsed = 0.f;

				// Set our target to be our initial, and set the new initial to be the current position
				Action->TargetLocation = Action->InitialLocation;
				Action->TargetRotation = Action->InitialRotation;

				Action->InitialLocation = ComponentLocation;
				Action->InitialRotation = ComponentRotation;
			}
		}
	}
}

int32 UKismetSystemLibrary::GetRenderingDetailMode()
{
	static const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DetailMode"));

	// clamp range
	int32 Ret = FMath::Clamp(CVar->GetInt(), 0, 2);

	return Ret;
}

int32 UKismetSystemLibrary::GetRenderingMaterialQualityLevel()
{
	static const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MaterialQualityLevel"));

	// clamp range
	int32 Ret = FMath::Clamp(CVar->GetInt(), 0, (int32)EMaterialQualityLevel::Num - 1);

	return Ret;
}

bool UKismetSystemLibrary::GetSupportedFullscreenResolutions(TArray<FIntPoint>& Resolutions)
{
	uint32 MinYResolution = UKismetSystemLibrary::GetMinYResolutionForUI();

	FScreenResolutionArray SupportedResolutions;
	if ( RHIGetAvailableResolutions(SupportedResolutions, true) )
	{
		uint32 LargestY = 0;
		for ( const FScreenResolutionRHI& SupportedResolution : SupportedResolutions )
		{
			LargestY = FMath::Max(LargestY, SupportedResolution.Height);
			if(SupportedResolution.Height >= MinYResolution)
			{
				FIntPoint Resolution;
				Resolution.X = SupportedResolution.Width;
				Resolution.Y = SupportedResolution.Height;

				Resolutions.Add(Resolution);
			}
		}
		if(!Resolutions.Num())
		{
			// if we don't have any resolution, we take the largest one(s)
			for ( const FScreenResolutionRHI& SupportedResolution : SupportedResolutions )
			{
				if(SupportedResolution.Height == LargestY)
				{
					FIntPoint Resolution;
					Resolution.X = SupportedResolution.Width;
					Resolution.Y = SupportedResolution.Height;

					Resolutions.Add(Resolution);
				}
			}
		}

		return true;
	}

	return false;
}

bool UKismetSystemLibrary::GetConvenientWindowedResolutions(TArray<FIntPoint>& Resolutions)
{
	FDisplayMetrics DisplayMetrics;
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
	}
	else
	{
		FDisplayMetrics::GetDisplayMetrics(DisplayMetrics);
	}

	GenerateConvenientWindowedResolutions(DisplayMetrics, Resolutions);

	return true;
}

static TAutoConsoleVariable<int32> CVarMinYResolutionForUI(
	TEXT("r.MinYResolutionForUI"),
	720,
	TEXT("Defines the smallest Y resolution we want to support in the UI (default is 720)"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarMinYResolutionFor3DView(
	TEXT("r.MinYResolutionFor3DView"),
	360,
	TEXT("Defines the smallest Y resolution we want to support in the 3D view"),
	ECVF_RenderThreadSafe
	);

int32 UKismetSystemLibrary::GetMinYResolutionForUI()
{
	int32 Value = FMath::Clamp(CVarMinYResolutionForUI.GetValueOnGameThread(), 200, 8 * 1024);

	return Value;
}

int32 UKismetSystemLibrary::GetMinYResolutionFor3DView()
{
	int32 Value = FMath::Clamp(CVarMinYResolutionFor3DView.GetValueOnGameThread(), 200, 8 * 1024);

	return Value;
}

void UKismetSystemLibrary::LaunchURL(const FString& URL)
{
	if (!URL.IsEmpty())
	{
		FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
	}
}

bool UKismetSystemLibrary::CanLaunchURL(const FString& URL)
{
	if (!URL.IsEmpty())
	{
		return FPlatformProcess::CanLaunchURL(*URL);
	}

	return false;
}
void UKismetSystemLibrary::CollectGarbage()
{
	GEngine->DeferredCommands.Add(TEXT("obj gc"));
}

void UKismetSystemLibrary::ShowAdBanner(int32 AdIdIndex, bool bShowOnBottomOfScreen)
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->ShowAdBanner(bShowOnBottomOfScreen, AdIdIndex);
	}
}

int32 UKismetSystemLibrary::GetAdIDCount()
{
	uint32 AdIDCount = 0;
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		AdIDCount = Provider->GetAdIDCount();
	}

	return AdIDCount;
}

void UKismetSystemLibrary::HideAdBanner()
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->HideAdBanner();
	}
}

void UKismetSystemLibrary::ForceCloseAdBanner()
{
	if (IAdvertisingProvider* Provider = FAdvertising::Get().GetDefaultProvider())
	{
		Provider->CloseAdBanner();
	}
}

void UKismetSystemLibrary::ShowPlatformSpecificLeaderboardScreen(const FString& CategoryName)
{
	// not PIE safe, doesn't have world context
	UOnlineEngineInterface::Get()->ShowLeaderboardUI(nullptr, CategoryName);
}

void UKismetSystemLibrary::ShowPlatformSpecificAchievementsScreen(APlayerController* SpecificPlayer)
{
	UWorld* World = SpecificPlayer ? SpecificPlayer->GetWorld() : nullptr;

	// Get the controller id from the player
	int LocalUserNum = 0;
	if (SpecificPlayer)
	{
		ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(SpecificPlayer->Player);
		if (LocalPlayer)
		{
			LocalUserNum = LocalPlayer->GetControllerId();
		}
	}

	UOnlineEngineInterface::Get()->ShowAchievementsUI(World, LocalUserNum);
}

bool UKismetSystemLibrary::IsLoggedIn(APlayerController* SpecificPlayer)
{
	UWorld* World = SpecificPlayer ? SpecificPlayer->GetWorld() : nullptr;

	int LocalUserNum = 0;
	if (SpecificPlayer != nullptr)
	{
		ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(SpecificPlayer->Player);
		if(LocalPlayer)
		{
			LocalUserNum = LocalPlayer->GetControllerId();
		}
	}

	return UOnlineEngineInterface::Get()->IsLoggedIn(World, LocalUserNum);
}

void UKismetSystemLibrary::SetStructurePropertyByName(UObject* Object, FName PropertyName, const FGenericStruct& Value)
{
	// We should never hit these!  They're stubs to avoid NoExport on the class.
	check(0);
}

void UKismetSystemLibrary::ControlScreensaver(bool bAllowScreenSaver)
{
	FPlatformMisc::ControlScreensaver(bAllowScreenSaver ? FPlatformMisc::EScreenSaverAction::Enable : FPlatformMisc::EScreenSaverAction::Disable);
}

void UKismetSystemLibrary::SetVolumeButtonsHandledBySystem(bool bEnabled)
{
	FPlatformMisc::SetVolumeButtonsHandledBySystem(bEnabled);
}

bool UKismetSystemLibrary::GetVolumeButtonsHandledBySystem()
{
	return FPlatformMisc::GetVolumeButtonsHandledBySystem();
}

void UKismetSystemLibrary::ResetGamepadAssignments()
{
	FPlatformMisc::ResetGamepadAssignments();
}

void UKismetSystemLibrary::ResetGamepadAssignmentToController(int32 ControllerId)
{
	FPlatformMisc::ResetGamepadAssignmentToController(ControllerId);
}

bool UKismetSystemLibrary::IsControllerAssignedToGamepad(int32 ControllerId)
{
	return FPlatformMisc::IsControllerAssignedToGamepad(ControllerId);
}

void UKismetSystemLibrary::SetSuppressViewportTransitionMessage(UObject* WorldContextObject, bool bState)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World && World->GetFirstLocalPlayerFromController() != nullptr && World->GetFirstLocalPlayerFromController()->ViewportClient != nullptr )
	{
		World->GetFirstLocalPlayerFromController()->ViewportClient->SetSuppressTransitionMessage(bState);
	}
}

TArray<FString> UKismetSystemLibrary::GetPreferredLanguages()
{
	return FPlatformMisc::GetPreferredLanguages();
}

FString UKismetSystemLibrary::GetLocalCurrencyCode()
{
	return FPlatformMisc::GetLocalCurrencyCode();
}

FString UKismetSystemLibrary::GetLocalCurrencySymbol()
{
	return FPlatformMisc::GetLocalCurrencySymbol();
}

struct FLoadAssetActionBase : public FPendingLatentAction, public FGCObject
{
	// @TODO: it would be good to have static/global manager? 

public:
	FStringAssetReference AssetReference;
	FStreamableManager StreamableManager;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	virtual void OnLoaded() PURE_VIRTUAL(FLoadAssetActionBase::OnLoaded, );

	FLoadAssetActionBase(const FStringAssetReference& InAssetReference, const FLatentActionInfo& InLatentInfo)
		: AssetReference(InAssetReference)
		, ExecutionFunction(InLatentInfo.ExecutionFunction)
		, OutputLink(InLatentInfo.Linkage)
		, CallbackTarget(InLatentInfo.CallbackTarget)
	{
		StreamableManager.SimpleAsyncLoad(AssetReference);
	}

	virtual ~FLoadAssetActionBase()
	{
		StreamableManager.Unload(AssetReference);
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		const bool bLoaded = StreamableManager.IsAsyncLoadComplete(AssetReference);
		if (bLoaded)
		{
			OnLoaded();
		}
		Response.FinishAndTriggerIf(bLoaded, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	virtual FString GetDescription() const override
	{
		return FString::Printf(TEXT("Load Asset Action Base: %s"), *AssetReference.ToString());
	}
#endif

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		StreamableManager.AddStructReferencedObjects(Collector);
	}
};

void UKismetSystemLibrary::LoadAsset(UObject* WorldContextObject, const TAssetPtr<UObject>& Asset, UKismetSystemLibrary::FOnAssetLoaded OnLoaded, FLatentActionInfo LatentInfo)
{
	struct FLoadAssetAction : public FLoadAssetActionBase
	{
	public:
		UKismetSystemLibrary::FOnAssetLoaded OnLoadedCallback;

		FLoadAssetAction(const FStringAssetReference& InAssetReference, UKismetSystemLibrary::FOnAssetLoaded InOnLoadedCallback, const FLatentActionInfo& InLatentInfo)
			: FLoadAssetActionBase(InAssetReference, InLatentInfo)
			, OnLoadedCallback(InOnLoadedCallback)
		{}

		virtual void OnLoaded() override
		{
			UObject* LoadedObject = AssetReference.ResolveObject();
			OnLoadedCallback.ExecuteIfBound(LoadedObject);
		}
	};

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FLoadAssetAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FLoadAssetAction* NewAction = new FLoadAssetAction(Asset.ToStringReference(), OnLoaded, LatentInfo);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

void UKismetSystemLibrary::LoadAssetClass(UObject* WorldContextObject, const TAssetSubclassOf<UObject>& AssetClass, UKismetSystemLibrary::FOnAssetClassLoaded OnLoaded, FLatentActionInfo LatentInfo)
{
	struct FLoadAssetClassAction : public FLoadAssetActionBase
	{
	public:
		UKismetSystemLibrary::FOnAssetClassLoaded OnLoadedCallback;

		FLoadAssetClassAction(const FStringAssetReference& InAssetReference, UKismetSystemLibrary::FOnAssetClassLoaded InOnLoadedCallback, const FLatentActionInfo& InLatentInfo)
			: FLoadAssetActionBase(InAssetReference, InLatentInfo)
			, OnLoadedCallback(InOnLoadedCallback)
		{}

		virtual void OnLoaded() override
		{
			UClass* LoadedObject = Cast<UClass>(AssetReference.ResolveObject());
			OnLoadedCallback.ExecuteIfBound(LoadedObject);
		}
	};

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
	if (World != nullptr)
	{
		FLatentActionManager& LatentManager = World->GetLatentActionManager();
		if (LatentManager.FindExistingAction<FLoadAssetClassAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr)
		{
			FLoadAssetClassAction* NewAction = new FLoadAssetClassAction(AssetClass.ToStringReference(), OnLoaded, LatentInfo);
			LatentManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
		}
	}
}

void UKismetSystemLibrary::RegisterForRemoteNotifications()
{
	FPlatformMisc::RegisterForRemoteNotifications();
}

void UKismetSystemLibrary::SetUserActivity(const FUserActivity& UserActivity)
{
	FUserActivityTracking::SetActivity(UserActivity);
}
