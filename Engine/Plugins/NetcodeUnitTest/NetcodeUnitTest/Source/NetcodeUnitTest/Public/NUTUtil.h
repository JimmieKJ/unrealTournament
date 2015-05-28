// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


// Forward declarations
class UUnitTest;


// @todo JohnB: Convert to multicast delegate

// @todo JohnB: Adjust all of these utility .h files, so that they implement code in .cpp, as these probably slow down compilation

#if !UE_BUILD_SHIPPING
// Process even callback, which passes an extra parameter, for identifying the origin of the hook
typedef bool (*InternalProcessEventCallback)(AActor* /*Actor*/, UFunction* /*Function*/, void* /*Parameters*/, void* /*HookOrigin*/);


/** Track callbacks not by setting the global extern/hook above, but by indirectly handling multiple callbacks locally */
extern TMap<void*, InternalProcessEventCallback> ActiveProcessEventCallbacks;
#endif



/**
 * Output device for allowing quick/dynamic creation of a customized output device, using lambda's passed to delegates
 */

class FDynamicOutputDevice : public FOutputDevice
{
public:
	FDynamicOutputDevice()
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		OnSerialize.Broadcast(V, Verbosity, Category);
	}

	virtual void Flush() override
	{
		OnFlush.Broadcast();
	}

	virtual void TearDown() override
	{
		OnTearDown.Broadcast();
	}

public:
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSerialize, const TCHAR*, ELogVerbosity::Type, const FName&);

	DECLARE_MULTICAST_DELEGATE(FOnFlushOrTearDown);


	FOnSerialize OnSerialize;

	FOnFlushOrTearDown OnFlush;

	FOnFlushOrTearDown OnTearDown;
};


#if !UE_BUILD_SHIPPING
static bool HandleProcessEventCallback(AActor* Actor, UFunction* Function, void* Parameters)
{
	bool bBlockEvent = false;

	for (auto It = ActiveProcessEventCallbacks.CreateConstIterator(); It; ++It)
	{
		InternalProcessEventCallback CurCallback = It->Value;

		// Replace 'Parameters' with the TMap void*, which refers to the object that set the callback
		if (CurCallback(Actor, Function, Parameters, It->Key))
		{
			bBlockEvent = true;
		}
	}

	return bBlockEvent;
}

// HookOrigin should reference the object responsible for the callback, for identification
static void AddProcessEventCallback(void* HookOrigin, InternalProcessEventCallback InCallback)
{
	if (HookOrigin != NULL)
	{
		if (ActiveProcessEventCallbacks.Num() == 0)
		{
			AActor::ProcessEventDelegate.BindStatic(&HandleProcessEventCallback);
		}

		ActiveProcessEventCallbacks.Add(HookOrigin, InCallback);
	}
	else
	{
		UE_LOG(LogUnitTest, Log, TEXT("AddProcessEventCallback: HookOrigin == NULL"));
	}
}

static void RemoveProcessEventCallback(void* HookOrigin, InternalProcessEventCallback InCallback)
{
	if (HookOrigin != NULL)
	{
		ActiveProcessEventCallbacks.Remove(HookOrigin);

		if (ActiveProcessEventCallbacks.Num() == 0)
		{
			AActor::ProcessEventDelegate.Unbind();
		}
	}
}
#endif


// Hack for accessing private members in various Engine classes

// This template, is used to link an arbitrary class member, to the GetPrivate function
template<typename Accessor, typename Accessor::Member Member> struct AccessPrivate
{
	friend typename Accessor::Member GetPrivate(Accessor InAccessor)
	{
		return Member;
	}
};


// Need to define one of these, for every member you want to access (preferably in the .cpp) - for example:
#if 0
// This is used to aid in linking one member in FStackTracker, to the above template
struct FStackTrackerbIsEnabledAccessor
{
	typedef bool FStackTracker::*Member;

	friend Member GetPrivate(FStackTrackerbIsEnabledAccessor);
};

// These combine the structs above, with the template for accessing private members, pointing to the specified member
template struct AccessPrivate<FStackTrackerbIsEnabledAccessor, &FStackTracker::bIsEnabled>;
#endif

// Example for calling private functions
#if 0
// Used, in combination with another template, for accessing private/protected members of classes
struct AShooterCharacterServerEquipWeaponAccessor
{
	typedef void (AShooterCharacter::*Member)(AShooterWeapon* Weapon);

	friend Member GetPrivate(AShooterCharacterServerEquipWeaponAccessor);
};

// Combines the above struct, with the template used for accessing private/protected members
template struct AccessPrivate<AShooterCharacterServerEquipWeaponAccessor, &AShooterCharacter::ServerEquipWeapon>;

// To call private function:
//	(GET_PRIVATE(AShooterCharacter, CurChar, ServerEquipWeapon))(AShooterWeapon::StaticClass());
#endif

/**
 * Defines a class and template specialization, for a variable, needed for use with the GET_PRIVATE hook below
 *
 * @param InClass		The class being accessed (not a string, just the class, i.e. FStackTracker)
 * @param VarName		Name of the variable being accessed (again, not a string)
 * @param VarType		The type of the variable being accessed
 */
#define IMPLEMENT_GET_PRIVATE_VAR(InClass, VarName, VarType) \
	struct InClass##VarName##Accessor \
	{ \
		typedef VarType InClass::*Member; \
		\
		friend Member GetPrivate(InClass##VarName##Accessor); \
	}; \
	\
	template struct AccessPrivate<InClass##VarName##Accessor, &InClass::VarName>;

/**
 * Defines a class and template specialization, for a function, needed for use with the GET_PRIVATE hook below
 *
 * @param InClass		The class being accessed (not a string, just the class, i.e. FStackTracker)
 * @param FuncName		Name of the function being accessed (again, not a string)
 * @param FuncRet		The return type of the function
 * @param FuncParms		The function parameters
 * @param FuncModifier	(Optional) Modifier placed after the function - usually 'const'
 */
#define IMPLEMENT_GET_PRIVATE_FUNC_CONST(InClass, FuncName, FuncRet, FuncParms, FuncModifier) \
	struct InClass##FuncName##Accessor \
	{ \
		typedef FuncRet (InClass::*Member)(FuncParms) FuncModifier; \
		\
		friend Member GetPrivate(InClass##FuncName##Accessor); \
	}; \
	\
	template struct AccessPrivate<InClass##FuncName##Accessor, &InClass::FuncName>;

#define IMPLEMENT_GET_PRIVATE_FUNC(InClass, FuncName, FuncRet, FuncParms) \
	IMPLEMENT_GET_PRIVATE_FUNC_CONST(InClass, FuncName, FuncRet, FuncParms, ;)


/**
 * A macro for tidying up accessing of private members, through the above code
 *
 * @param InClass		The class being accessed (not a string, just the class, i.e. FStackTracker)
 * @param InObj			Pointer to an instance of the specified class
 * @param MemberName	Name of the member being accessed (again, not a string)
 * @return				The value of the member 
 */
#define GET_PRIVATE(InClass, InObj, MemberName) (*InObj).*GetPrivate(InClass##MemberName##Accessor())

// Version of above, for calling private functions
#define CALL_PRIVATE(InClass, InObj, MemberName) (GET_PRIVATE(InClass, InObj, MemberName))


/**
 * General utility functions
 */
struct NUTUtil
{
	/**
	 * Returns the currently active net driver (either pending or one for current level)
	 *
	 * @return	The active net driver
	 */
	static inline UNetDriver* GetActiveNetDriver(UWorld* InWorld)
	{
		UNetDriver* ReturnVal = NULL;
		FWorldContext* WorldContext = &GEngine->GetWorldContextFromWorldChecked(InWorld);

		if (WorldContext != NULL && WorldContext->PendingNetGame != NULL && WorldContext->PendingNetGame->NetDriver != NULL)
		{
			ReturnVal = WorldContext->PendingNetGame->NetDriver;
		}
		else if (InWorld != NULL)
		{
			ReturnVal = InWorld->NetDriver;
		}

		return ReturnVal;
	}

	static inline UWorld* GetPrimaryWorld()
	{
		UWorld* ReturnVal = NULL;

		if (GEngine != NULL)
		{
			for (auto It=GEngine->GetWorldContexts().CreateConstIterator(); It; ++It)
			{
				const FWorldContext& Context = *It;

				if ((Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && Context.World())
				{
					ReturnVal = Context.World();
					break;
				}
			}
		}

		return ReturnVal;
	}

	/**
	 * Outputs a full list of valid unit test class defaults, representing all unit tests which can be executed
	 *
	 * @param OutUnitTestClassDefaults	The array that unit test class defaults should be output to
	 */
	static void GetUnitTestClassDefList(TArray<UUnitTest*>& OutUnitTestClassDefaults);

	/**
	 * Takes a list of unit test class defaults, and reorders them in a more readable way, based on type and implementation date
	 *
	 * @param InUnitTestClassDefaults	The array of unit test class defaults to reorder
	 */
	static void SortUnitTestClassDefList(TArray<UUnitTest*>& InUnitTestClassDefaults);


	/**
	 * Overridden parse function, for supporting escaped quotes, e.g: -UnitTestServerParms="-LogCmds=\"LogNet all\""
	 */
	static bool ParseValue(const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, int32 MaxLen, bool bShouldStopOnComma=true)
	{
		bool bReturnVal = false;
		const TCHAR* Found = FCString::Strfind(Stream, Match);

		if (Found != NULL)
		{
			const TCHAR* Start = Found + FCString::Strlen(Match);

			// Check for quoted arguments' string with spaces
			// -Option="Value1 Value2"
			//         ^~~~Start
			bool bArgumentsQuoted = *Start == '"';

			// Number of characters we can look back from found looking for first parenthesis.
			uint32 AllowedBacktraceCharactersCount = Found - Stream;

			// Check for fully quoted string with spaces
			bool bFullyQuoted = 
				// "Option=Value1 Value2"
				//  ^~~~Found
				(AllowedBacktraceCharactersCount > 0 && (*(Found - 1) == '"'))
				// "-Option=Value1 Value2"
				//   ^~~~Found
				|| (AllowedBacktraceCharactersCount > 1 && ((*(Found - 1) == '-') && (*(Found - 2) == '"')));

			if (bArgumentsQuoted || bFullyQuoted)
			{
				// Skip quote character if only params were quoted.
				int32 QuoteCharactersToSkip = bArgumentsQuoted ? 1 : 0;
				FCString::Strncpy(Value, Start + QuoteCharactersToSkip, MaxLen);

				Value[MaxLen-1] = 0;

				TCHAR* EndQuote = FCString::Strstr(Value, TEXT("\x22"));
				TCHAR* EscapedQuote = FCString::Strstr(Value, TEXT("\\\x22"));
				bool bContainsEscapedQuotes = EscapedQuote != NULL;

				// JohnB: Keep iterating until we've moved past all escaped quotes
				while (EndQuote != NULL && EscapedQuote != NULL && EscapedQuote < EndQuote)
				{
					TCHAR* Temp = EscapedQuote + 2;

					EndQuote = FCString::Strstr(Temp, TEXT("\x22"));
					EscapedQuote = FCString::Strstr(Temp, TEXT("\\\x22"));
				}

				if (EndQuote != NULL)
				{
					*EndQuote = 0;
				}

				// JohnB: Fixup all escaped quotes
				if (bContainsEscapedQuotes)
				{
					EscapedQuote = FCString::Strstr(Value, TEXT("\\\x22"));

					while (EscapedQuote != NULL)
					{
						TCHAR* Temp = EscapedQuote;

						while (true)
						{
							Temp[0] = Temp[1];

							if (*(++Temp) == '\0')
							{
								break;
							}
						}

						EscapedQuote = FCString::Strstr(Value, TEXT("\\\x22"));
					}
				}
			}
			else
			{
				// Non-quoted string without spaces.
				FCString::Strncpy(Value, Start, MaxLen);

				Value[MaxLen-1] = 0;

				TCHAR* Temp = FCString::Strstr(Value, TEXT(" "));
				
				if (Temp != NULL)
				{
					*Temp = 0;
				}

				Temp = FCString::Strstr(Value, TEXT("\r"));
				
				if (Temp != NULL)
				{
					*Temp = 0;
				}

				Temp = FCString::Strstr(Value, TEXT("\n"));
				
				if (Temp != NULL)
				{
					*Temp = 0;
				}

				Temp = FCString::Strstr(Value, TEXT("\t"));
				
				if (Temp != NULL)
				{
					*Temp = 0;
				}


				if (bShouldStopOnComma)
				{
					Temp = FCString::Strstr(Value, TEXT(","));

					if (Temp != NULL)
					{
						*Temp = 0;
					}
				}
			}

			bReturnVal = true;
		}

		return bReturnVal;
	}

	/**
	 * Overridden parse function, for supporting escaped quotes
	 */
	static bool ParseValue(const TCHAR* Stream, const TCHAR* Match, FString& Value, bool bShouldStopOnComma=true)
	{
		bool bReturnVal = false;
		TCHAR Temp[4096] = TEXT("");

		if (ParseValue(Stream, Match, Temp, ARRAY_COUNT(Temp), bShouldStopOnComma))
		{
			Value = Temp;
			bReturnVal = true;
		}

		return bReturnVal;
	}
};

