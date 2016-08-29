// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/LatentActionManager.h"
#include "BlueprintFunctionLibrary.h"
#include "Engine/CollisionProfile.h"
#include "AssetPtr.h"
#include "KismetSystemLibrary.generated.h"

UENUM(BlueprintType)
namespace EDrawDebugTrace
{
	enum Type
	{
		None, 
		ForOneFrame, 
		ForDuration, 
		Persistent
	};
}

/** Enum used to indicate desired behavior for MoveComponentTo latent function. */
UENUM()
namespace EMoveComponentAction
{
	enum Type
	{
		/** Move to target over specified time. */
		Move, 
		/** If currently moving, stop. */
		Stop,
		/** If currently moving, return to where you started, over the time elapsed so far. */
		Return
	};
}

UENUM()
namespace EQuitPreference
{
	enum Type
	{
		/** Exit the game completely. */
		Quit,
		/** Move the application to the background. */
		Background,
	};
}

USTRUCT()
struct FGenericStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 Data;
};

UCLASS()
class ENGINE_API UKismetSystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	// --- Globally useful functions ------------------------------
	/** Prints a stack trace to the log, so you can see how a blueprint got to this node */
	UFUNCTION(BlueprintCallable, CustomThunk, Category="Development|Editor", meta=(Keywords = "ScriptTrace"))
	static void StackTrace();
	static void StackTraceImpl(const FFrame& StackFrame);
	DECLARE_FUNCTION(execStackTrace)
	{
		P_FINISH;
		StackTraceImpl(Stack);
	}

	// Return true if the object is usable : non-null and not pending kill
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static bool IsValid(const UObject* Object);

	// Return true if the class is usable : non-null and not pending kill
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static bool IsValidClass(UClass* Class);

	// Returns the actual object name.
	UFUNCTION(BlueprintPure, Category = "Utilities")
	static FString GetObjectName(const UObject* Object);

	// Returns the full path to the specified object.
	UFUNCTION(BlueprintPure, Category="Utilities")
	static FString GetPathName(const UObject* Object);

	// Returns the display name (or actor label), for displaying as a debugging aid.
	// Note: In editor builds, this is the actor label.  In non-editor builds, this is the actual object name.  This function should not be used to uniquely identify actors!
	// It is not localized and should not be used for display to an end user of a game.
	UFUNCTION(BlueprintPure, Category="Utilities")
	static FString GetDisplayName(const UObject* Object);

	// Returns the display name of a class
	UFUNCTION(BlueprintPure, Category = "Utilities", meta = (DisplayName = "Get Display Name"))
	static FString GetClassDisplayName(UClass* Class);

	// Engine build number, for displaying to end users.
	UFUNCTION(BlueprintPure, Category="Development")
	static FString GetEngineVersion();

	/** Get the name of the current game  */
	UFUNCTION(BlueprintPure, Category="Game")
	static FString GetGameName();

	/**
	 * Retrieves the game's platform-specific bundle identifier or package name of the game
	 *
	 * @return The game's bundle identifier or package name.
	 */
	UFUNCTION(BlueprintPure, Category="Game", meta=(Keywords = "bundle id package name"))
	static FString GetGameBundleId();

	/** Get the current user name from the OS */
	UFUNCTION(BlueprintPure, Category="Utilities|Platform")
	static FString GetPlatformUserName();

	UFUNCTION(BlueprintPure, Category="Utilities")
	static bool DoesImplementInterface(UObject* TestObject, TSubclassOf<UInterface> Interface);

	/** 
	 * Get the current game time, in seconds. This stops when the game is paused and is affected by slomo. 
	 * 
	 * @param WorldContextObject	World context
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Time", meta=(WorldContext="WorldContextObject") )
	static float GetGameTimeInSeconds(UObject* WorldContextObject);

	/** Returns whether the world this object is in is the host or not */
	UFUNCTION(BlueprintPure, Category="Networking", meta=(WorldContext="WorldContextObject") )
	static bool IsServer(UObject* WorldContextObject);

	/** Returns whether this is running on a dedicated server */
	UFUNCTION(BlueprintPure, Category="Networking", meta=(WorldContext="WorldContextObject"))
	static bool IsDedicatedServer(UObject* WorldContextObject);

	/** Returns whether this is a build that is packaged for distribution */
	UFUNCTION(BlueprintPure, Category="Development")
	static bool IsPackagedForDistribution();

	/** Returns the platform specific unique device id */
	UFUNCTION(BlueprintPure, Category="Utilities|Platform")
	static FString GetUniqueDeviceId();

	UFUNCTION(BlueprintPure, meta=(DisplayName = "ToObject (interface)", CompactNodeTitle = "->"), Category="Utilities")
	static UObject* Conv_InterfaceToObject(const FScriptInterface& Interface); 

	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "true"), Category = "Utilities")
	static UObject* Conv_AssetToObject(const TAssetPtr<UObject>& Asset);

	UFUNCTION(BlueprintPure, meta = (BlueprintInternalUseOnly = "true"), Category = "Utilities")
	static TSubclassOf<UObject> Conv_AssetClassToClass(const TAssetSubclassOf<UObject>& AssetClass);

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAssetLoaded, class UObject*, Loaded);

	UFUNCTION(BlueprintCallable, meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "Utilities")
	static void LoadAsset(UObject* WorldContextObject, const TAssetPtr<UObject>& Asset, FOnAssetLoaded OnLoaded, FLatentActionInfo LatentInfo);

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAssetClassLoaded, TSubclassOf<UObject>, Loaded);

	UFUNCTION(BlueprintCallable, meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"), Category = "Utilities")
	static void LoadAssetClass(UObject* WorldContextObject, const TAssetSubclassOf<UObject>& AssetClass, FOnAssetClassLoaded OnLoaded, FLatentActionInfo LatentInfo);


	/**
	 * Creates a literal integer
	 * @param	Value	value to set the integer to
	 * @return	The literal integer
	 */
	UFUNCTION(BlueprintPure, Category="Math|Integer")
	static int32 MakeLiteralInt(int32 Value);

	/**
	 * Creates a literal float
	 * @param	Value	value to set the float to
	 * @return	The literal float
	 */
	UFUNCTION(BlueprintPure, Category="Math|Float")
	static float MakeLiteralFloat(float Value);

	/**
	 * Creates a literal bool
	 * @param	Value	value to set the bool to
	 * @return	The literal bool
	 */
	UFUNCTION(BlueprintPure, Category="Math|Boolean")
	static bool MakeLiteralBool(bool Value);

	/**
	 * Creates a literal name
	 * @param	Value	value to set the name to
	 * @return	The literal name
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Name")
	static FName MakeLiteralName(FName Value);

	/**
	 * Creates a literal byte
	 * @param	Value	value to set the byte to
	 * @return	The literal byte
	 */
	UFUNCTION(BlueprintPure, Category="Math|Byte")
	static uint8 MakeLiteralByte(uint8 Value);

	/**
	 * Creates a literal string
	 * @param	Value	value to set the string to
	 * @return	The literal string
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|String")
	static FString MakeLiteralString(const FString& Value);

	/**
	 * Creates a literal FText
	 * @param	Value	value to set the FText to
	 * @return	The literal FText
	 */
	UFUNCTION(BlueprintPure, Category="Utilities|Text")
	static FText MakeLiteralText(FText Value);

	/**
	 * Prints a string to the log, and optionally, to the screen
	 * If Print To Log is true, it will be visible in the Output Log window.  Otherwise it will be logged only as 'Verbose', so it generally won't show up.
	 *
	 * @param	InString		The string to log out
	 * @param	bPrintToScreen	Whether or not to print the output to the screen
	 * @param	bPrintToLog		Whether or not to print the output to the log
	 * @param	bPrintToConsole	Whether or not to print the output to the console
	 * @param	TextColor		Whether or not to print the output to the console
	 * @param	Duration		The display duration (if Print to Screen is True). Using negative number will result in loading the duration time from the config.
	 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", CallableWithoutWorldContext, Keywords = "log print", AdvancedDisplay = "2", DevelopmentOnly), Category="Utilities|String")
	static void PrintString(UObject* WorldContextObject, const FString& InString = FString(TEXT("Hello")), bool bPrintToScreen = true, bool bPrintToLog = true, FLinearColor TextColor = FLinearColor(0.0, 0.66, 1.0), float Duration = 2.f);

	/**
	 * Prints text to the log, and optionally, to the screen
	 * If Print To Log is true, it will be visible in the Output Log window.  Otherwise it will be logged only as 'Verbose', so it generally won't show up.
	 *
	 * @param	InText			The text to log out
	 * @param	bPrintToScreen	Whether or not to print the output to the screen
	 * @param	bPrintToLog		Whether or not to print the output to the log
	 * @param	bPrintToConsole	Whether or not to print the output to the console
	 * @param	TextColor		Whether or not to print the output to the console
	 * @param	Duration		The display duration (if Print to Screen is True). Using negative number will result in loading the duration time from the config.
	 */
	UFUNCTION(BlueprintCallable, meta=(WorldContext="WorldContextObject", CallableWithoutWorldContext, Keywords = "log", AdvancedDisplay = "2", DevelopmentOnly), Category="Utilities|Text")
	static void PrintText(UObject* WorldContextObject, const FText InText = FText::FromString(TEXT("Hello")), bool bPrintToScreen = true, bool bPrintToLog = true, FLinearColor TextColor = FLinearColor(0.0, 0.66, 1.0), float Duration = 2.f);

	/**
	 * Prints a warning string to the log and the screen. Meant to be used as a way to inform the user that they misused the node.
	 *
	 * WARNING!! Don't change the signature of this function without fixing up all nodes using it in the compiler
	 *
	 * @param	InString		The string to log out
	 */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "TRUE"))
	static void PrintWarning(const FString& InString);

	/** Sets the game window title */
	UFUNCTION(BlueprintCallable, Category = "Utilities")
	static void SetWindowTitle(const FText& Title);

	/**
	 * Executes a console command, optionally on a specific controller
	 * 
	 * @param	Command			Command to send to the console
	 * @param	SpecificPlayer	If specified, the console command will be routed through the specified player
	 */
	UFUNCTION(BlueprintCallable, Category="Development",meta=(WorldContext="WorldContextObject"))
	static void ExecuteConsoleCommand(UObject* WorldContextObject, const FString& Command, class APlayerController* SpecificPlayer = NULL );

	/** 
	 *	Exit the current game 
	 * @param	SpecificPlayer	The specific player to quit the game. If not specified, player 0 will quit.
	 */
	UFUNCTION(BlueprintCallable, Category="Game",meta=(WorldContext="WorldContextObject"))
	static void QuitGame(UObject* WorldContextObject, class APlayerController* SpecificPlayer, TEnumAsByte<EQuitPreference::Type> QuitPreference);

	//=============================================================================
	// Latent Actions

	/** 
	 * Perform a latent action with a delay (specified in seconds).  Calling again while it is counting down will be ignored.
	 * 
	 * @param WorldContext	World context.
	 * @param Duration 		length of delay (in seconds).
	 * @param LatentInfo 	The latent action.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities|FlowControl", meta=(Latent, WorldContext="WorldContextObject", LatentInfo="LatentInfo", Duration="0.2", Keywords="sleep"))
	static void	Delay(UObject* WorldContextObject, float Duration, struct FLatentActionInfo LatentInfo );

	/** 
	 * Perform a latent action with a retriggerable delay (specified in seconds).  Calling again while it is counting down will reset the countdown to Duration.
	 * 
	 * @param WorldContext	World context.
	 * @param Duration 		length of delay (in seconds).
	 * @param LatentInfo 	The latent action.
	 */
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject", Duration="0.2", Keywords="sleep"), Category="Utilities|FlowControl")
	static void RetriggerableDelay(UObject* WorldContextObject, float Duration, FLatentActionInfo LatentInfo);

	/*
	 * Interpolate a component to the specified relative location and rotation over the course of OverTime seconds. 
	 * @param Component						Component to interpolate
	 * @param TargetRelativeLocation		Relative target location
	 * @param TargetRelativeRotation		Relative target rotation
	 * @param bEaseOut						if true we will ease out (ie end slowly) during interpolation
	 * @param bEaseIn						if true we will ease in (ie start slowly) during interpolation
	 * @param OverTime						duration of interpolation
	 * @param bForceShortestRotationPath	if true we will always use the shortest path for rotation
	 * @param MoveAction					required movement behavior @see EMoveComponentAction
	 * @param LatentInfo					The latent action
	 */
	UFUNCTION(BlueprintCallable, meta=(Latent, LatentInfo="LatentInfo", WorldContext="WorldContextObject", ExpandEnumAsExecs="MoveAction", OverTime="0.2"), Category="Components")
	static void MoveComponentTo(USceneComponent* Component, FVector TargetRelativeLocation, FRotator TargetRelativeRotation, bool bEaseOut, bool bEaseIn, float OverTime, bool bForceShortestRotationPath, TEnumAsByte<EMoveComponentAction::Type> MoveAction, FLatentActionInfo LatentInfo);

	// --- Timer functions with delegate input ----------

	/**
	 * Set a timer to execute delegate. Setting an existing timer will reset that timer with updated parameters.
	 * @param Event			Event. Can be a K2 function or a Custom Event.
	 * @param Time			How long to wait before executing the delegate, in seconds. Setting a timer to <= 0 seconds will clear it if it is set.
	 * @param bLooping		True to keep executing the delegate every Time seconds, false to execute delegate only once.
	 * @return				The timer handle to pass to other timer functions to manipulate this timer.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Set Timer by Event"), Category="Utilities|Time")
	static FTimerHandle K2_SetTimerDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate, float Time, bool bLooping);

	/**
	 * Clears a set timer.
	 * @param Event  Can be a K2 function or a Custom Event.
	 */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use Clear Timer by Handle", DisplayName = "Clear Timer by Event"), Category="Utilities|Time")
	static void K2_ClearTimerDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/**
	 * Pauses a set timer at its current elapsed time.
	 * @param Event  Can be a K2 function or a Custom Event.
	 */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use Pause Timer by Handle", DisplayName = "Pause Timer by Event"), Category="Utilities|Time")
	static void K2_PauseTimerDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/**
	 * Resumes a paused timer from its current elapsed time.
	 * @param Event  Can be a K2 function or a Custom Event.
	 */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage = "Use Unpause Timer by Handle", DisplayName = "Unpause Timer by Event"), Category="Utilities|Time")
	static void K2_UnPauseTimerDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/**
	 * Returns true if a timer exists and is active for the given delegate, false otherwise.
	 * @param Event  Can be a K2 function or a Custom Event.
	 * @return				True if the timer exists and is active.
	 */
	UFUNCTION(BlueprintPure, meta=(DeprecatedFunction, DeprecationMessage = "Use Is Timer Active by Handle", DisplayName = "Is Timer Active by Event"), Category="Utilities|Time")
	static bool K2_IsTimerActiveDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/**
	 * Returns true if a timer exists and is paused for the given delegate, false otherwise.
	 * @param Event  Can be a K2 function or a Custom Event.
	 * @return				True if the timer exists and is paused.
	 */
	UFUNCTION(BlueprintPure, meta=(DeprecatedFunction, DeprecationMessage = "Use Is Timer Paused by Handle", DisplayName = "Is Timer Paused by Event"), Category = "Utilities|Time")
	static bool K2_IsTimerPausedDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/**
	 * Returns true is a timer for the given delegate exists, false otherwise.
	 * @param Event  Can be a K2 function or a Custom Event.
	 * @return				True if the timer exists.
	 */
	UFUNCTION(BlueprintPure, meta=(DeprecatedFunction, DeprecationMessage = "Use Does Timer Exist by Handle", DisplayName = "Does Timer Exist by Event"), Category = "Utilities|Time")
	static bool K2_TimerExistsDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);
	
	/**
	 * Returns elapsed time for the given delegate (time since current countdown iteration began).
	 * @param Event  Can be a K2 function or a Custom Event.
	 * @return				How long has elapsed since the current iteration of the timer began.
	 */
	UFUNCTION(BlueprintPure, meta=(DeprecatedFunction, DeprecationMessage = "Use Get Timer Elapsed Time by Handle", DisplayName = "Get Timer Elapsed Time by Event"), Category="Utilities|Time")
	static float K2_GetTimerElapsedTimeDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	/**
	 * Returns time until the timer will next execute its delegate.
	 * @param Event  Can be a K2 function or a Custom Event.
	 * @return				How long is remaining in the current iteration of the timer.
	 */
	UFUNCTION(BlueprintPure, meta=(DeprecatedFunction, DeprecationMessage = "Use Get Timer Remaining Time by Handle", DisplayName = "Get Timer Remaining Time by Event"), Category="Utilities|Time")
	static float K2_GetTimerRemainingTimeDelegate(UPARAM(DisplayName="Event") FTimerDynamicDelegate Delegate);

	// --- Timer functions with handle input ----------

	/**
	 * Returns whether the timer handle is valid. This does not indicate that there is an active timer that this handle references, but rather that it once referenced a valid timer.
	 * @param Handle		The handle of the timer to check validity of.
	 * @return				Whether the timer handle is valid.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Valid"), Category="Utilities|Time")
	static bool K2_IsValidTimerHandle(FTimerHandle Handle);

	/**
	 * Returns whether the timer handle is valid. This does not indicate that there is an active timer that this handle references, but rather that it once referenced a valid timer.
	 * @param Handle		The handle of the timer to check validity of.
	 * @return				Return the invalidated timer handle for convenience.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Invalidate"), Category="Utilities|Time")
	static FTimerHandle K2_InvalidateTimerHandle(UPARAM(ref) FTimerHandle& Handle);

	/**
	 * Clears a set timer.
	 * @param Handle		The handle of the timer to clear.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Clear Timer by Handle", WorldContext="WorldContextObject", DeprecatedFunction, DeprecationMessage = "Use Clear and Invalidate Timer by Handle. Note: you no longer need to reset your handle yourself after switching to the new function."), Category="Utilities|Time")
	static void K2_ClearTimerHandle(UObject* WorldContextObject, FTimerHandle Handle);

	/**
	 * Clears a set timer.
	 * @param Handle		The handle of the timer to clear.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Clear and Invalidate Timer by Handle", WorldContext="WorldContextObject"), Category="Utilities|Time")
	static void K2_ClearAndInvalidateTimerHandle(UObject* WorldContextObject, UPARAM(ref) FTimerHandle& Handle);

	/**
	 * Pauses a set timer at its current elapsed time.
	 * @param Handle		The handle of the timer to pause.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Pause Timer by Handle", WorldContext="WorldContextObject"), Category="Utilities|Time")
	static void K2_PauseTimerHandle(UObject* WorldContextObject, FTimerHandle Handle);

	/**
	 * Resumes a paused timer from its current elapsed time.
	 * @param Handle		The handle of the timer to unpause.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Unpause Timer by Handle", WorldContext="WorldContextObject"), Category="Utilities|Time")
	static void K2_UnPauseTimerHandle(UObject* WorldContextObject, FTimerHandle Handle);

	/**
	 * Returns true if a timer exists and is active for the given handle, false otherwise.
	 * @param Handle		The handle of the timer to check whether it is active.
	 * @return				True if the timer exists and is active.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Timer Active by Handle", WorldContext="WorldContextObject"), Category="Utilities|Time")
	static bool K2_IsTimerActiveHandle(UObject* WorldContextObject, FTimerHandle Handle);

	/**
	 * Returns true if a timer exists and is paused for the given handle, false otherwise.
	 * @param Handle		The handle of the timer to check whether it is paused.
	 * @return				True if the timer exists and is paused.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Timer Paused by Handle", WorldContext="WorldContextObject"), Category = "Utilities|Time")
	static bool K2_IsTimerPausedHandle(UObject* WorldContextObject, FTimerHandle Handle);

	/**
	 * Returns true is a timer for the given handle exists, false otherwise.
	 * @param Handle		The handle to check whether it exists.
 	 * @return				True if the timer exists.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Does Timer Exist by Handle", WorldContext="WorldContextObject"), Category = "Utilities|Time")
	static bool K2_TimerExistsHandle(UObject* WorldContextObject, FTimerHandle Handle);
	
	/**
	 * Returns elapsed time for the given handle (time since current countdown iteration began).
	 * @param Handle		The handle of the timer to get the elapsed time of.
	 * @return				How long has elapsed since the current iteration of the timer began.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Timer Elapsed Time by Handle", WorldContext="WorldContextObject"), Category="Utilities|Time")
	static float K2_GetTimerElapsedTimeHandle(UObject* WorldContextObject, FTimerHandle Handle);

	/**
	 * Returns time until the timer will next execute its handle.
	 * @param Handle		The handle of the timer to time remaining of.
	 * @return				How long is remaining in the current iteration of the timer.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Timer Remaining Time by Handle", WorldContext="WorldContextObject"), Category="Utilities|Time")
	static float K2_GetTimerRemainingTimeHandle(UObject* WorldContextObject, FTimerHandle Handle);

	// --- Timer functions ------------------------------

	/**
	 * Set a timer to execute delegate. Setting an existing timer will reset that timer with updated parameters.
	 * @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	 * @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	 * @param Time			How long to wait before executing the delegate, in seconds. Setting a timer to <= 0 seconds will clear it if it is set.
	 * @param bLooping		true to keep executing the delegate every Time seconds, false to execute delegate only once.
	 * @return				The timer handle to pass to other timer functions to manipulate this timer.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Set Timer by Function Name", DefaultToSelf = "Object"), Category="Utilities|Time")
	static FTimerHandle K2_SetTimer(UObject* Object, FString FunctionName, float Time, bool bLooping);

	/**
	 * Clears a set timer.
	 * @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	 * @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Clear Timer by Function Name", DefaultToSelf = "Object"), Category="Utilities|Time")
	static void K2_ClearTimer(UObject* Object, FString FunctionName);

	/**
	 * Pauses a set timer at its current elapsed time.
	 * @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	 * @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Pause Timer by Function Name", DefaultToSelf = "Object"), Category="Utilities|Time")
	static void K2_PauseTimer(UObject* Object, FString FunctionName);

	/**
	 * Resumes a paused timer from its current elapsed time.
	 * @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	 * @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "Unpause Timer by Function Name", DefaultToSelf = "Object"), Category="Utilities|Time")
	static void K2_UnPauseTimer(UObject* Object, FString FunctionName);

	/**
	 * Returns true if a timer exists and is active for the given delegate, false otherwise.
	 * @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	 * @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	 * @return				True if the timer exists and is active.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Timer Active by Function Name", DefaultToSelf = "Object"), Category="Utilities|Time")
	static bool K2_IsTimerActive(UObject* Object, FString FunctionName);

	/**
	* Returns true if a timer exists and is paused for the given delegate, false otherwise.
	* @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	* @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	* @return				True if the timer exists and is paused.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Is Timer Paused by Function Name", DefaultToSelf = "Object"), Category = "Utilities|Time")
	static bool K2_IsTimerPaused(UObject* Object, FString FunctionName);

	/**
	* Returns true is a timer for the given delegate exists, false otherwise.
	* @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	* @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	* @return				True if the timer exists.
	*/
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Does Timer Exist by Function Name", DefaultToSelf = "Object"), Category = "Utilities|Time")
	static bool K2_TimerExists(UObject* Object, FString FunctionName);
	
	/**
	 * Returns elapsed time for the given delegate (time since current countdown iteration began).
	 * @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	 * @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	 * @return				How long has elapsed since the current iteration of the timer began.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Timer Elapsed Time by Function Name", DefaultToSelf = "Object"), Category="Utilities|Time")
	static float K2_GetTimerElapsedTime(UObject* Object, FString FunctionName);

	/**
	 * Returns time until the timer will next execute its delegate.
	 * @param Object		Object that implements the delegate function. Defaults to self (this blueprint)
	 * @param FunctionName	Delegate function name. Can be a K2 function or a Custom Event.
	 * @return				How long is remaining in the current iteration of the timer.
	 */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Timer Remaining Time by Function Name", DefaultToSelf = "Object"), Category="Utilities|Time")
	static float K2_GetTimerRemainingTime(UObject* Object, FString FunctionName);


	// --- 'Set property by name' functions ------------------------------

	/** Set an int32 property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true"))
	static void SetIntPropertyByName(UObject* Object, FName PropertyName, int32 Value);

	/** Set an uint8 or enum property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true"))
	static void SetBytePropertyByName(UObject* Object, FName PropertyName, uint8 Value);

	/** Set a float property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true"))
	static void SetFloatPropertyByName(UObject* Object, FName PropertyName, float Value);

	/** Set a bool property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true"))
	static void SetBoolPropertyByName(UObject* Object, FName PropertyName, bool Value);

	/** Set an OBJECT property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true"))
	static void SetObjectPropertyByName(UObject* Object, FName PropertyName, UObject* Value);

	/** Set a CLASS property by name */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static void SetClassPropertyByName(UObject* Object, FName PropertyName, TSubclassOf<UObject> Value);

	/** Set an INTERFACE property by name */
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (BlueprintInternalUseOnly = "true"))
	static void SetInterfacePropertyByName(UObject* Object, FName PropertyName, const FScriptInterface& Value);

	/** Set a NAME property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value" ))
	static void SetNamePropertyByName(UObject* Object, FName PropertyName, const FName& Value);

	/** Set a ASSET property by name */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value"))
	static void SetAssetPropertyByName(UObject* Object, FName PropertyName, const TAssetPtr<UObject>& Value);

	/** Set a ASSETCLASS property by name */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value"))
	static void SetAssetClassPropertyByName(UObject* Object, FName PropertyName, const TAssetSubclassOf<UObject>& Value);

	/** Set a STRING property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value" ))
	static void SetStringPropertyByName(UObject* Object, FName PropertyName, const FString& Value);

	/** Set a TEXT property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value" ))
	static void SetTextPropertyByName(UObject* Object, FName PropertyName, const FText& Value);

	/** Set a VECTOR property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value" ))
	static void SetVectorPropertyByName(UObject* Object, FName PropertyName, const FVector& Value);

	/** Set a ROTATOR property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value" ))
	static void SetRotatorPropertyByName(UObject* Object, FName PropertyName, const FRotator& Value);

	/** Set a LINEAR COLOR property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value" ))
	static void SetLinearColorPropertyByName(UObject* Object, FName PropertyName, const FLinearColor& Value);

	/** Set a TRANSFORM property by name */
	UFUNCTION(BlueprintCallable, meta=(BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value" ))
	static void SetTransformPropertyByName(UObject* Object, FName PropertyName, const FTransform& Value);

	/** Set a CollisionProfileName property by name */
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", AutoCreateRefTerm = "Value"))
	static void SetCollisionProfileNameProperty(UObject* Object, FName PropertyName, const FCollisionProfileName& Value);

	DECLARE_FUNCTION(execSetCollisionProfileNameProperty)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, StructPropertyName);

		Stack.StepCompiledIn<UStructProperty>(NULL);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;

		P_FINISH;
		P_NATIVE_BEGIN;
		Generic_SetStructurePropertyByName(OwnerObject, StructPropertyName, SrcStructAddr);
		P_NATIVE_END;
	}

	/** Set a custom structure property by name */
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void SetStructurePropertyByName(UObject* Object, FName PropertyName, const FGenericStruct& Value);

	static void Generic_SetStructurePropertyByName(UObject* OwnerObject, FName StructPropertyName, const void* SrcStructAddr);

	/** Based on UKismetArrayLibrary::execSetArrayPropertyByName */
	DECLARE_FUNCTION(execSetStructurePropertyByName)
	{
		P_GET_OBJECT(UObject, OwnerObject);
		P_GET_PROPERTY(UNameProperty, StructPropertyName);

		Stack.StepCompiledIn<UStructProperty>(NULL);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;

		P_FINISH;
		P_NATIVE_BEGIN;
		Generic_SetStructurePropertyByName(OwnerObject, StructPropertyName, SrcStructAddr);
		P_NATIVE_END;
	}

	// --- Collision functions ------------------------------

	// @NOTE!!!!! please remove _NEW when we can remove all _DEPRECATED functions and create redirect in BaseEngine.INI

	/**
	 * Returns an array of actors that overlap the given sphere.
	 * @param WorldContext	World context
	 * @param SpherePos		Center of sphere.
	 * @param SphereRadius	Size of sphere.
	 * @param Filter		Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter	If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors		Returned array of actors. Unsorted.
	 * @return				true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "SphereOverlapActors"))
	static bool SphereOverlapActors_NEW(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SphereOverlapActors", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore"))
	static bool SphereOverlapActors_DEPRECATED(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);

	/**
	 * Returns an array of components that overlap the given sphere.
	 * @param WorldContext	World context
	 * @param SpherePos		Center of sphere.
	 * @param SphereRadius	Size of sphere.
	 * @param Filter		Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter	If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors		Returned array of actors. Unsorted.
	 * @return				true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName="SphereOverlapComponents"))
	static bool SphereOverlapComponents_NEW(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SphereOverlapComponents", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore"))
	static bool SphereOverlapComponents_DEPRECATED(UObject* WorldContextObject, const FVector SpherePos, float SphereRadius, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);
	

	/**
	 * Returns an array of actors that overlap the given axis-aligned box.
	 * @param WorldContext	World context
	 * @param BoxPos		Center of box.
	 * @param BoxExtent		Extents of box.
	 * @param Filter		Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter	If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors		Returned array of actors. Unsorted.
	 * @return				true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName="BoxOverlapActors"))
	static bool BoxOverlapActors_NEW(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new BoxOverlapActors", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore"))
	static bool BoxOverlapActors_DEPRECATED(UObject* WorldContextObject, const FVector BoxPos, FVector BoxExtent, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);

	/**
	 * Returns an array of components that overlap the given axis-aligned box.
	 * @param WorldContext	World context
	 * @param BoxPos		Center of box.
	 * @param BoxExtent		Extents of box.
	 * @param Filter		Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter	If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors		Returned array of actors. Unsorted.
	 * @return				true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName="BoxOverlapComponents"))
	static bool BoxOverlapComponents_NEW(UObject* WorldContextObject, const FVector BoxPos, FVector Extent, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new BoxOverlapComponents", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore"))
	static bool BoxOverlapComponents_DEPRECATED(UObject* WorldContextObject, const FVector BoxPos, FVector Extent, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);


	/**
	 * Returns an array of actors that overlap the given capsule.
	 * @param WorldContext	World context
	 * @param CapsulePos	Center of the capsule.
	 * @param Radius		Radius of capsule hemispheres and radius of center cylinder portion.
	 * @param HalfHeight	Half-height of the capsule (from center of capsule to tip of hemisphere.
	 * @param Filter		Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter	If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors		Returned array of actors. Unsorted.
	 * @return				true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName="CapsuleOverlapActors"))
	static bool CapsuleOverlapActors_NEW(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new CapsuleOverlapActors", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore"))
	static bool CapsuleOverlapActors_DEPRECATED(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);
	/**
	 * Returns an array of components that overlap the given capsule.
	 * @param WorldContext	World context
	 * @param CapsulePos	Center of the capsule.
	 * @param Radius		Radius of capsule hemispheres and radius of center cylinder portion.
	 * @param HalfHeight	Half-height of the capsule (from center of capsule to tip of hemisphere.
	 * @param Filter		Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter	If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors		Returned array of actors. Unsorted.
	 * @return				true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName="CapsuleOverlapComponents") )
	static bool CapsuleOverlapComponents_NEW(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new CapsuleOverlapComponents", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore") )
	static bool CapsuleOverlapComponents_DEPRECATED(UObject* WorldContextObject, const FVector CapsulePos, float Radius, float HalfHeight, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);
	

	/**
	 * Returns an array of actors that overlap the given component.
	 * @param Component				Component to test with.
	 * @param ComponentTransform	Defines where to place the component for overlap testing.
	 * @param Filter				Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter			If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors				Returned array of actors. Unsorted.
	 * @return						true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(AutoCreateRefTerm="ActorsToIgnore", DisplayName="ComponentOverlapActors"))
	static bool ComponentOverlapActors_NEW(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new ComponentOverlapActors", AutoCreateRefTerm="ActorsToIgnore"))
	static bool ComponentOverlapActors_DEPRECATED(UPrimitiveComponent* Component, const FTransform& ComponentTransform, EOverlapFilterOption Filter, UClass* ActorClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class AActor*>& OutActors);
	
	/**
	 * Returns an array of components that overlap the given component.
	 * @param Component				Component to test with.
	 * @param ComponentTransform	Defines where to place the component for overlap testing.
	 * @param Filter				Option to restrict results to only static or only dynamic.  For efficiency.
	 * @param ClassFilter			If set, will only return results of this class or subclasses of it.
	 * @param ActorsToIgnore		Ignore these actors in the list
	 * @param OutActors				Returned array of actors. Unsorted.
	 * @return						true if there was an overlap that passed the filters, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(AutoCreateRefTerm="ActorsToIgnore", DisplayName="ComponentOverlapComponents"))
	static bool ComponentOverlapComponents_NEW(UPrimitiveComponent* Component, const FTransform& ComponentTransform, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);

	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new ComponentOverlapComponents", AutoCreateRefTerm="ActorsToIgnore"))
	static bool ComponentOverlapComponents_DEPRECATED(UPrimitiveComponent* Component, const FTransform& ComponentTransform, EOverlapFilterOption Filter, UClass* ComponentClassFilter, const TArray<AActor*>& ActorsToIgnore, TArray<class UPrimitiveComponent*>& OutComponents);

	/**
	 * Does a collision trace along the given line and returns the first blocking hit encountered.
	 * This trace finds the objects that RESPONDS to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName="LineTraceByChannel", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", Keywords="raycast"))
	static bool LineTraceSingle_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);
	
	/**
	 * Does a collision trace along the given line and returns all hits encountered up to and including the first blocking hit.
	 * This trace finds the objects that RESPOND to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param TraceChannel	The channel to trace
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a blocking hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiLineTraceByChannel", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", Keywords="raycast"))
	static bool LineTraceMulti_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	/**
	 * Sweeps a sphere along the given line and returns the first blocking hit encountered.
	 * This trace finds the objects that RESPONDS to the given TraceChannel
	 * 
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "SphereTraceByChannel", Keywords="sweep"))
	static bool SphereTraceSingle_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a sphere along the given line and returns all hits encountered up to and including the first blocking hit.
	 * This trace finds the objects that RESPOND to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a blocking hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiSphereTraceByChannel", Keywords="sweep"))
	static bool SphereTraceMulti_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);

	/**
	* Sweeps a box along the given line and returns the first blocking hit encountered.
	* This trace finds the objects that RESPONDS to the given TraceChannel
	*
	* @param Start			Start of line segment.
	* @param End			End of line segment.
	* @param HalfSize	    Distance from the center of box along each axis
	* @param Orientation	Orientation of the box
	* @param TraceChannel
	* @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	* @param OutHit			Properties of the trace hit.
	* @return				True if there was a hit, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (bIgnoreSelf = "true", WorldContext="WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore", DisplayName = "BoxTraceByChannel", Keywords="sweep"))
	static bool BoxTraceSingle(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	* Sweeps a box along the given line and returns all hits encountered.
	* This trace finds the objects that RESPONDS to the given TraceChannel
	*
	* @param Start			Start of line segment.
	* @param End			End of line segment.
	* @param HalfSize	    Distance from the center of box along each axis
	* @param Orientation	Orientation of the box
	* @param TraceChannel
	* @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	* @param OutHits		A list of hits, sorted along the trace from start to finish. The blocking hit will be the last hit, if there was one.
	* @return				True if there was a blocking hit, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (bIgnoreSelf = "true", WorldContext="WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore", DisplayName = "MultiBoxTraceByChannel", Keywords="sweep"))
	static bool BoxTraceMulti(UObject* WorldContextObject, const FVector Start, const FVector End, FVector HalfSize, const FRotator Orientation, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);

	
	/**
	 * Sweeps a capsule along the given line and returns the first blocking hit encountered.
	 * This trace finds the objects that RESPOND to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "CapsuleTraceByChannel", Keywords="sweep"))
	static bool CapsuleTraceSingle_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a capsule along the given line and returns all hits encountered up to and including the first blocking hit.
	 * This trace finds the objects that RESPOND to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a blocking hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiCapsuleTraceByChannel", Keywords="sweep"))
	static bool CapsuleTraceMulti_NEW(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ETraceTypeQuery TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);

	/**
	 * Does a collision trace along the given line and returns the first hit encountered.
	 * This only finds objects that are of a type specified by ObjectTypes.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param ObjectTypes	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "LineTraceForObjects", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", Keywords="raycast"))
	static bool LineTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f );
	
	/**
	 * Does a collision trace along the given line and returns all hits encountered.
	 * This only finds objects that are of a type specified by ObjectTypes.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param ObjectTypes	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiLineTraceForObjects", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", Keywords="raycast"))
	static bool LineTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	/**
	 * Sweeps a sphere along the given line and returns the first hit encountered.
	 * This only finds objects that are of a type specified by ObjectTypes.
	 * 
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param ObjectTypes	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "SphereTraceForObjects", Keywords="sweep"))
	static bool SphereTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a sphere along the given line and returns all hits encountered.
	 * This only finds objects that are of a type specified by ObjectTypes.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param ObjectTypes	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiSphereTraceForObjects", Keywords="sweep"))
	static bool SphereTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);
	

	/**
	* Sweeps a box along the given line and returns the first hit encountered.
	* This only finds objects that are of a type specified by ObjectTypes.
	*
	* @param Start			Start of line segment.
	* @param End			End of line segment.
	* @param Orientation	
	* @param HalfSize		Radius of the sphere to sweep
	* @param ObjectTypes	Array of Object Types to trace
	* @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	* @param OutHit			Properties of the trace hit.
	* @return				True if there was a hit, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (bIgnoreSelf = "true", WorldContext="WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore", DisplayName = "BoxTraceForObjects", Keywords="sweep"))
	static bool BoxTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);


	/**
	* Sweeps a box along the given line and returns all hits encountered.
	* This only finds objects that are of a type specified by ObjectTypes.
	*
	* @param Start			Start of line segment.
	* @param End			End of line segment.
	* @param Orientation
	* @param HalfSize		Radius of the sphere to sweep
	* @param ObjectTypes	Array of Object Types to trace
	* @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	* @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	* @return				True if there was a hit, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (bIgnoreSelf = "true", WorldContext="WorldContextObject", AutoCreateRefTerm = "ActorsToIgnore", DisplayName = "MultiBoxTraceForObjects", Keywords="sweep"))
	static bool BoxTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, const FVector HalfSize, const FRotator Orientation, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);

	/**
	 * Sweeps a capsule along the given line and returns the first hit encountered.
	 * This only finds objects that are of a type specified by ObjectTypes.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param ObjectTypes	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "CapsuleTraceForObjects", Keywords="sweep"))
	static bool CapsuleTraceSingleForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a capsule along the given line and returns all hits encountered.
	 * This only finds objects that are of a type specified by ObjectTypes.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param ObjectTypes	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiCapsuleTraceForObjects", Keywords="sweep"))
	static bool CapsuleTraceMultiForObjects(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<EObjectTypeQuery> > & ObjectTypes, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);

	/**
	 * Does a collision trace along the given line and returns the first blocking hit encountered.
	 * This trace finds the objects that RESPOND to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SingleLineTraceByChannel", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", DisplayName = "SingleLineTraceByChannelDeprecated"))
	static bool LineTraceSingle_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);
	
	/**
	 * Does a collision trace along the given line and returns all hits encountered up to and including the first blocking hit.
	 * This trace finds the objects that RESPOND to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new MultiLineTraceByChannel", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", DisplayName = "MultiLineTraceByChannelDeprecated"))
	static bool LineTraceMulti_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	/**
	 * Sweeps a sphere along the given line and returns the first blocking hit encountered.
	 * This trace finds the objects that RESPONDS to the given TraceChannel
	 * 
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SingleSphereTraceByChannel", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "SingleSphereTraceByChannelDeprecated"))
	static bool SphereTraceSingle_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a sphere along the given line and returns all hits encountered up to and including the first blocking hit.
	 * This trace finds the objects that RESPONDS to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new MultiSphereTraceByChannel", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiSphereTraceByChannelDeprecated"))
	static bool SphereTraceMulti_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);
	
	/**
	 * Sweeps a capsule along the given line and returns the first blocking hit encountered.
	 * This trace finds the objects that RESPONDS to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SingleCapsuleTraceByChannel", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "SingleCapsuleTraceByChannelDeprecated"))
	static bool CapsuleTraceSingle_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a capsule along the given line and returns all hits encountered up to and including the first blocking hit.
	 * This trace finds the objects that RESPONDS to the given TraceChannel
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param TraceChannel	
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new MultiCapsuleTraceByChannel", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiCapsuleTraceByChannelDeprecated"))
	static bool CapsuleTraceMulti_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, ECollisionChannel TraceChannel, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);

	/**
	 * Does a collision trace along the given line and returns the first hit encountered.
	 * This finds objects belonging to the channels specified in the ObjectsToTrace input.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param ObjectsToTrace	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SingleLineTraceForObjects", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", DisplayName = "SingleLineTraceByObjectDeprecated"))
	static bool LineTraceSingleByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);
	
	/**
	 * Does a collision trace along the given line and returns all hits encountered.
	 * This finds objects belonging to the channels specified in the ObjectsToTrace input.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param ObjectsToTrace	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new MultiLineTraceForObjects", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", AdvancedDisplay="TraceColor,TraceHitColor,DrawTime", DisplayName = "MultiLineTraceByObjectDeprecated"))
	static bool LineTraceMultiByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf, FLinearColor TraceColor = FLinearColor::Red, FLinearColor TraceHitColor = FLinearColor::Green, float DrawTime = 5.0f);

	/**
	 * Sweeps a sphere along the given line and returns the first hit encountered.
	 * This finds objects belonging to the channels specified in the ObjectsToTrace input.
	 * 
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param ObjectsToTrace	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SingleSphereTraceForObjects", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "SingleSphereTraceByObjectDeprecated"))
	static bool SphereTraceSingleByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a sphere along the given line and returns all hits encountered.
	 * This finds objects belonging to the channels specified in the ObjectsToTrace input.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the sphere to sweep
	 * @param ObjectsToTrace	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new MultiSphereTraceForObjects", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiSphereTraceByObjectDeprecated"))
	static bool SphereTraceMultiByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);
	
	/**
	 * Sweeps a capsule along the given line and returns the first hit encountered.
	 * This finds objects belonging to the channels specified in the ObjectsToTrace input.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param ObjectsToTrace	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHit		Properties of the trace hit.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new SingleCapsuleTraceForObjects", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "SingleCapsuleTraceByObjectDeprecated"))
	static bool CapsuleTraceSingleByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, FHitResult& OutHit, bool bIgnoreSelf);

	/**
	 * Sweeps a capsule along the given line and returns all hits encountered.
	 * This finds objects belonging to the channels specified in the ObjectsToTrace input.
	 * 
	 * @param WorldContext	World context
	 * @param Start			Start of line segment.
	 * @param End			End of line segment.
	 * @param Radius		Radius of the capsule to sweep
	 * @param HalfHeight	Distance from center of capsule to tip of hemisphere endcap.
	 * @param ObjectsToTrace	Array of Object Types to trace 
	 * @param bTraceComplex	True to test against complex collision, false to test against simplified collision.
	 * @param OutHits		A list of hits, sorted along the trace from start to finish.  The blocking hit will be the last hit, if there was one.
	 * @return				True if there was a hit, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DeprecatedFunction, DeprecationMessage = "Use new MultiCapsuleTraceForObjects", bIgnoreSelf="true", WorldContext="WorldContextObject", AutoCreateRefTerm="ActorsToIgnore", DisplayName = "MultiCapsuleTraceByObjectDeprecated"))
	static bool CapsuleTraceMultiByObject_DEPRECATED(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius, float HalfHeight, const TArray<TEnumAsByte<ECollisionChannel> > & ObjectsToTrace, bool bTraceComplex, const TArray<AActor*>& ActorsToIgnore, EDrawDebugTrace::Type DrawDebugType, TArray<FHitResult>& OutHits, bool bIgnoreSelf);

	/**
	 * Returns an array of unique actors represented by the given list of components.
	 * @param ComponentList		List of components.
	 * @param ClassFilter		If set, will only return results of this class or subclasses of it.
	 * @param OutActorList		Start of line segment.
	 */
	UFUNCTION(BlueprintCallable, Category="Utilities")
	static void GetActorListFromComponentList(const TArray<class UPrimitiveComponent*>& ComponentList, UClass* ActorClassFilter, TArray<class AActor*>& OutActorList);


	// --- Debug drawing functions ------------------------------

	/** Draw a debug line */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugLine(UObject* WorldContextObject, const FVector LineStart, const FVector LineEnd, FLinearColor LineColor, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug circle! */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugCircle(UObject* WorldContextObject, FVector Center, float Radius, int32 NumSegments=12, FLinearColor LineColor = FLinearColor::White, float Duration=0.f, float Thickness=0.f, FVector YAxis=FVector(0.f,1.f,0.f),FVector ZAxis=FVector(0.f,0.f,1.f), bool bDrawAxis=false);
	
	/** Draw a debug point */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugPoint(UObject* WorldContextObject, const FVector Position, float Size, FLinearColor PointColor, float Duration=0.f);

	/** Draw directional arrow, pointing from LineStart to LineEnd. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugArrow(UObject* WorldContextObject, const FVector LineStart, const FVector LineEnd, float ArrowSize, FLinearColor LineColor, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug box */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugBox(UObject* WorldContextObject, const FVector Center, FVector Extent, FLinearColor LineColor, const FRotator Rotation=FRotator::ZeroRotator, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug coordinate system. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugCoordinateSystem(UObject* WorldContextObject, const FVector AxisLoc, const FRotator AxisRot, float Scale=1.f, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug sphere */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugSphere(UObject* WorldContextObject, const FVector Center, float Radius=100.f, int32 Segments=12, FLinearColor LineColor = FLinearColor::White, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug cylinder */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugCylinder(UObject* WorldContextObject, const FVector Start, const FVector End, float Radius=100.f, int32 Segments=12, FLinearColor LineColor = FLinearColor::White, float Duration=0.f, float Thickness = 0.f);
	
	/** Draw a debug cone */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject", DeprecatedFunction, DeprecationMessage="DrawDebugCone has been changed to use degrees for angles instead of radians. Place a new DrawDebugCone node and pass your angles as degrees."))
	static void DrawDebugCone(UObject* WorldContextObject, const FVector Origin, const FVector Direction, float Length, float AngleWidth, float AngleHeight, int32 NumSides, FLinearColor LineColor, float Duration = 0.f, float Thickness = 0.f);

	/** 
	 * Draw a debug cone 
	 * Angles are specified in degrees
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject", DisplayName="DrawDebugCone"))
	static void DrawDebugConeInDegrees(UObject* WorldContextObject, const FVector Origin, const FVector Direction, float Length=100.f, float AngleWidth=45.f, float AngleHeight=45.f, int32 NumSides = 12, FLinearColor LineColor = FLinearColor::White, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug capsule */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugCapsule(UObject* WorldContextObject, const FVector Center, float HalfHeight, float Radius, const FRotator Rotation, FLinearColor LineColor = FLinearColor::White, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug string at a 3d world location. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugString(UObject* WorldContextObject, const FVector TextLocation, const FString& Text, class AActor* TestBaseActor = NULL, FLinearColor TextColor = FLinearColor::White, float Duration=0.f);
	/** 
	 * Removes all debug strings. 
	 *
	 * @param WorldContext	World context
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void FlushDebugStrings(UObject* WorldContextObject);

	/** Draws a debug plane. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugPlane(UObject* WorldContextObject, const FPlane& PlaneCoordinates, const FVector Location, float Size, FLinearColor PlaneColor = FLinearColor::White, float Duration=0.f);

	/** 
	 * Flush all persistent debug lines and shapes.
	 *
	 * @param WorldContext	World context
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void FlushPersistentDebugLines(UObject* WorldContextObject);

	/** Draws a debug frustum. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugFrustum(UObject* WorldContextObject, const FTransform& FrustumTransform, FLinearColor FrustumColor = FLinearColor::White, float Duration=0.f, float Thickness = 0.f);

	/** Draw a debug camera shape. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Debug")
	static void DrawDebugCamera(const ACameraActor* CameraActor, FLinearColor CameraColor = FLinearColor::White, float Duration=0.f);

	/* Draws a 2D Histogram of size 'DrawSize' based FDebugFloatHistory struct, using DrawTransform for the position in the world. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugFloatHistoryTransform(UObject* WorldContextObject, const FDebugFloatHistory& FloatHistory, const FTransform& DrawTransform, FVector2D DrawSize, FLinearColor DrawColor = FLinearColor::White, float Duration = 0.f);

	/* Draws a 2D Histogram of size 'DrawSize' based FDebugFloatHistory struct, using DrawLocation for the location in the world, rotation will face camera of first player. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Debug", meta=(WorldContext="WorldContextObject"))
	static void DrawDebugFloatHistoryLocation(UObject* WorldContextObject, const FDebugFloatHistory& FloatHistory, FVector DrawLocation, FVector2D DrawSize, FLinearColor DrawColor = FLinearColor::White, float Duration = 0.f);

	UFUNCTION(BlueprintPure, Category = "Rendering|Debug")
	static FDebugFloatHistory AddFloatHistorySample(float Value, const FDebugFloatHistory& FloatHistory);
	
	/** Mark as modified. */
	UFUNCTION(BlueprintCallable, Category="Development|Editor")
	static void CreateCopyForUndoBuffer(UObject* ObjectToModify);

	/** Get bounds */
	UFUNCTION(BlueprintPure, Category="Collision")
	static void GetComponentBounds(const USceneComponent* Component, FVector& Origin, FVector& BoxExtent, float& SphereRadius);

	UFUNCTION(BlueprintPure, Category="Collision", meta=(DeprecatedFunction))
	static void GetActorBounds(const AActor* Actor, FVector& Origin, FVector& BoxExtent);

	/**
	 * Get the clamped state of r.DetailMode, see console variable help (allows for scalability, cannot be used in construction scripts)
	 * 0: low, show only object with DetailMode low or higher
	 * 1: medium, show all object with DetailMode medium or higher
	 * 2: high, show all objects
	 */
	UFUNCTION(BlueprintPure, Category="Rendering", meta=(UnsafeDuringActorConstruction = "true"))
	static int32 GetRenderingDetailMode();

	/**
	 * Get the clamped state of r.MaterialQualityLevel, see console variable help (allows for scalability, cannot be used in construction scripts)
	 * 0: low
	 * 1: high
	 * 2: medium
	 */
	UFUNCTION(BlueprintPure, Category="Rendering|Material", meta=(UnsafeDuringActorConstruction = "true"))
	static int32 GetRenderingMaterialQualityLevel();

	/**
	 * Gets the list of support fullscreen resolutions.
	 * @return true if successfully queried the device for available resolutions.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	static bool GetSupportedFullscreenResolutions(TArray<FIntPoint>& Resolutions);

	/**
	* Gets the list of windowed resolutions which are convenient for the current primary display size.
	* @return true if successfully queried the device for available resolutions.
	*/
	UFUNCTION(BlueprintCallable, Category="Rendering")
	static bool GetConvenientWindowedResolutions(TArray<FIntPoint>& Resolutions);

	/**
	 * Gets the smallest Y resolution we want to support in the UI, clamped within reasons
	 * @return value in pixels
	 */
	UFUNCTION(BlueprintPure, Category="Rendering", meta=(UnsafeDuringActorConstruction = "true"))
	static int32 GetMinYResolutionForUI();

	/**
	* Gets the smallest Y resolution we want to support in the 3D view, clamped within reasons
	* @return value in pixels
	*/
	UFUNCTION(BlueprintPure, Category = "Rendering", meta = (UnsafeDuringActorConstruction = "true"))
	static int32 GetMinYResolutionFor3DView();

	// Opens the specified URL in the platform's web browser of choice
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void LaunchURL(const FString& URL);

	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static bool CanLaunchURL(const FString& URL);

	// Deletes all unreferenced objects, keeping only referenced objects (this command will be queued and happen at the end of the frame)
	// Note: This can be a slow operation, and should only be performed where a hitch would be acceptable
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void CollectGarbage();

	/**
	 * Will show an ad banner (iAd on iOS, or AdMob on Android) on the top or bottom of screen, on top of the GL view (doesn't resize the view)
	 * (iOS and Android only)
	 *
	 * @param AdIdIndex The index of the ID to select for the add to show
	 * @param bShowOnBottomOfScreen If true, the iAd will be shown at the bottom of the screen, top otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void ShowAdBanner(int32 AdIdIndex, bool bShowOnBottomOfScreen);

	/**
	* Retrieves the total number of Ad IDs that can be selected between
	*/
	UFUNCTION(BlueprintPure, Category = "Utilities|Platform", meta = (DisplayName = "Get Ad ID Count"))
	static int32 GetAdIDCount();

	/**
	 * Hides the ad banner (iAd on iOS, or AdMob on Android). Will force close the ad if it's open
	 * (iOS and Android only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void HideAdBanner();

	/**
	 * Forces closed any displayed ad. Can lead to loss of revenue
	 * (iOS and Android only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void ForceCloseAdBanner();

	/**
	 * Displays the built-in leaderboard GUI (iOS and Android only; this function may be renamed or moved in a future release)
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void ShowPlatformSpecificLeaderboardScreen(const FString& CategoryName);

	/**
	 * Displays the built-in achievements GUI (iOS and Android only; this function may be renamed or moved in a future release)
	 *
	 * @param SpecificPlayer Specific player's achievements to show. May not be supported on all platforms. If null, defaults to the player with ControllerId 0
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void ShowPlatformSpecificAchievementsScreen(class APlayerController* SpecificPlayer);

	/**
	 * Returns whether the player is logged in to the currently active online subsystem.
	 *
	 * @param Player Specific player's login status to get. May not be supported on all platforms. If null, defaults to the player with ControllerId 0.
	 */
	UFUNCTION(BlueprintPure, Category = "Online")
	static bool IsLoggedIn(APlayerController* SpecificPlayer);

	/**
	 * Allows or inhibits screensaver
	 * @param	bAllowScreenSaver		If false, don't allow screensaver if possible, otherwise allow default behavior
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void ControlScreensaver(bool bAllowScreenSaver);

	/**
	 * Allows or inhibits system default handling of volume up and volume down buttons (Android only)
	 * @param	bEnabled				If true, allow Android to handle volume up and down events
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void SetVolumeButtonsHandledBySystem(bool bEnabled);

	/**
	 * Returns true if system default handling of volume up and volume down buttons enabled (Android only)
	 */
	UFUNCTION(BlueprintPure, Category = "Utilities|Platform")
	static bool GetVolumeButtonsHandledBySystem();

	/**
	 * Resets the gamepad to player controller id assignments (Android only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void ResetGamepadAssignments();

	/*
	 * Resets the gamepad assignment to player controller id (Android only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void ResetGamepadAssignmentToController(int32 ControllerId);

	/**
	 * Returns true if controller id assigned to a gamepad (Android only)
	 */
	UFUNCTION(BlueprintPure, Category = "Utilities|Platform")
	static bool IsControllerAssignedToGamepad(int32 ControllerId);

	/**
	 * Sets the state of the transition message rendered by the viewport. (The blue text displayed when the game is paused and so forth.)
	 *
	 * @param WorldContextObject	World context
	 * @param State					set true to suppress transition message
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities", meta = (WorldContext="WorldContextObject"))
	static void SetSuppressViewportTransitionMessage(UObject* WorldContextObject, bool bState);

	/**
	 * Returns an array of the user's preferred languages in order of preference
	 * @return An array of language IDs ordered from most preferred to least
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static TArray<FString> GetPreferredLanguages();

	/**
	* Returns the currency code associated with the device's locale
	* @return the currency code associated with the device's locale
	*/
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static FString GetLocalCurrencyCode();

	/**
	* Returns the currency symbol associated with the device's locale
	* @return the currency symbol associated with the device's locale
	*/
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static FString GetLocalCurrencySymbol();

	/**
	 * Requests permission to send remote notifications to the user's device.
	 * (iOS only)
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities|Platform")
	static void RegisterForRemoteNotifications();

	/**
	 * Tells the engine what the user is doing for debug, analytics, etc.
	 */
	UFUNCTION(BlueprintCallable, Category = "Utilities")
	static void SetUserActivity(const FUserActivity& UserActivity);
};
