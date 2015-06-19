// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Build.h"


/*----------------------------------------------------------------------------
	Check, verify, etc macros
----------------------------------------------------------------------------*/

//
// "check" expressions are only evaluated if enabled.
// "verify" expressions are always evaluated, but only cause an error if enabled.
//

#if !UE_BUILD_SHIPPING
#define _DebugBreakAndPromptForRemote() \
	if (!FPlatformMisc::IsDebuggerPresent()) { FPlatformMisc::PromptForRemoteDebugging(false); } FPlatformMisc::DebugBreak();
#else
	#define _DebugBreakAndPromptForRemote()
#endif // !UE_BUILD_SHIPPING
#if DO_CHECK
	#define checkCode( Code )		do { Code } while ( false );
	#define verify(expr)			{ if(!(expr)) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); CA_ASSUME(expr); } }
	#define check(expr)				{ if(!(expr)) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed( #expr, __FILE__, __LINE__ ); CA_ASSUME(expr); } }
	
	/**
	 * verifyf, checkf: Same as verify, check but with printf style additional parameters
	 * Read about __VA_ARGS__ (variadic macros) on http://gcc.gnu.org/onlinedocs/gcc-3.4.4/cpp.pdf.
	 */
	#define verifyf(expr, format,  ...)		{ if(!(expr)) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__, format, ##__VA_ARGS__); CA_ASSUME(expr); } }
	#define checkf(expr, format,  ...)		{ if(!(expr)) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__, format, ##__VA_ARGS__); CA_ASSUME(expr); } }
	/**
	 * Denotes code paths that should never be reached.
	 */
	#define checkNoEntry()       { FDebug::LogAssertFailedMessage( "Enclosing block should never be called", __FILE__, __LINE__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed("Enclosing block should never be called", __FILE__, __LINE__ ); }

	/**
	 * Denotes code paths that should not be executed more than once.
	 */
	#define checkNoReentry()     { static bool s_beenHere##__LINE__ = false;                                         \
	                               checkf( !s_beenHere##__LINE__, TEXT("Enclosing block was called more than once") );   \
								   s_beenHere##__LINE__ = true; }

	class FRecursionScopeMarker
	{
	public: 
		FRecursionScopeMarker(uint16 &InCounter) : Counter( InCounter ) { ++Counter; }
		~FRecursionScopeMarker() { --Counter; }
	private:
		uint16& Counter;
	};

	/**
	 * Denotes code paths that should never be called recursively.
	 */
	#define checkNoRecursion()  static uint16 RecursionCounter##__LINE__ = 0;                                            \
	                            checkf( RecursionCounter##__LINE__ == 0, TEXT("Enclosing block was entered recursively") );  \
	                            const FRecursionScopeMarker ScopeMarker##__LINE__( RecursionCounter##__LINE__ )

	#define unimplemented()       { FDebug::LogAssertFailedMessage( "Unimplemented function called", __FILE__, __LINE__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed("Unimplemented function called", __FILE__, __LINE__); }

#else
	#define checkCode(...)
	#define check(expr)
	#define checkf(expr, format, ...)
	#define checkNoEntry()
	#define checkNoReentry()
	#define checkNoRecursion()
	#define verify(expr)				{ if(!(expr)){} }
	#define verifyf(expr, format, ...)	{ if(!(expr)){} }
	#define unimplemented()
#endif

//
// Check for development only.
//
#if DO_GUARD_SLOW
	#define checkSlow(expr)					{ if(!(expr)) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__); CA_ASSUME(expr); } }
	#define checkfSlow(expr, format, ...)	{ if(!(expr)) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed( #expr, __FILE__, __LINE__, format, ##__VA_ARGS__ ); CA_ASSUME(expr); } }
	#define verifySlow(expr)				{ if(!(expr)) { FDebug::LogAssertFailedMessage( #expr, __FILE__, __LINE__ ); _DebugBreakAndPromptForRemote(); FDebug::AssertFailed(#expr, __FILE__, __LINE__); } }
#else
	#define checkSlow(expr)
	#define checkfSlow(expr, format, ...)
	#define verifySlow(expr)  if(expr){}
#endif

/**
 * ensure() can be used to test for *non-fatal* errors at runtime
 *
 * Rather than crashing, an error report (with a full call stack) will be logged and submitted to the crash server. 
 * This is useful when you want runtime code verification but you're handling the error case anyway.
 *
 * Note: ensure() can be nested within conditionals!
 *
 * Example:
 *
 *		if( ensure( InObject != NULL ) )
 *		{
 *			InObject->Modify();
 *		}
 *
 * This code is safe to execute as the pointer dereference is wrapped in a non-NULL conditional block, but
 * you still want to find out if this ever happens so you can avoid side effects.  Using ensure() here will
 * force a crash report to be generated without crashing the application (and potentially causing editor
 * users to lose unsaved work.)
 *
 * ensure() resolves to a regular assertion (crash) in shipping or test builds.
 */

#if DO_CHECK && !USING_CODE_ANALYSIS // The Visual Studio 2013 analyzer doesn't understand these complex conditionals

	namespace UE4Asserts_Private
	{
		// This is used by ensureOnce to generate a bool per instance
		// by passing a lambda which will uniquely instantiate the template.
		template <typename Type>
		bool TrueOnFirstCallOnly(const Type&)
		{
			static bool bValue = true;
			bool Result = bValue;
			bValue = false;
			return Result;
		}

		FORCEINLINE bool OptionallyDebugBreakAndPromptForRemoteReturningFalse(bool bBreak)
		{
			if (bBreak)
			{
				FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse();
			}
			return false;
		}
	}

	#define ensure(         InExpression                ) ((InExpression) != 0 || FDebug::EnsureNotFalse_OptionallyLogFormattedEnsureMessageReturningFalse(true,                                          #InExpression, __FILE__, __LINE__, TEXT("")               ) || FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse())
	#define ensureMsg(      InExpression, InMsg         ) ((InExpression) != 0 || FDebug::EnsureNotFalse_OptionallyLogFormattedEnsureMessageReturningFalse(true,                                          #InExpression, __FILE__, __LINE__, InMsg                  ) || FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse())
	#define ensureMsgf(     InExpression, InFormat, ... ) ((InExpression) != 0 || FDebug::EnsureNotFalse_OptionallyLogFormattedEnsureMessageReturningFalse(true,                                          #InExpression, __FILE__, __LINE__, InFormat, ##__VA_ARGS__) || FPlatformMisc::DebugBreakAndPromptForRemoteReturningFalse())
	#define ensureOnce(     InExpression                ) ((InExpression) != 0 || FDebug::EnsureNotFalse_OptionallyLogFormattedEnsureMessageReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{}), #InExpression, __FILE__, __LINE__, TEXT("")               ) || UE4Asserts_Private::OptionallyDebugBreakAndPromptForRemoteReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{})))
	#define ensureOnceMsgf( InExpression, InFormat, ... ) ((InExpression) != 0 || FDebug::EnsureNotFalse_OptionallyLogFormattedEnsureMessageReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{}), #InExpression, __FILE__, __LINE__, InFormat, ##__VA_ARGS__) || UE4Asserts_Private::OptionallyDebugBreakAndPromptForRemoteReturningFalse(UE4Asserts_Private::TrueOnFirstCallOnly([]{})))

#else	// DO_CHECK

	#define ensure(         InExpression                ) ((InExpression) != 0)
	#define ensureMsg(      InExpression, InMsg         ) ((InExpression) != 0)
	#define ensureMsgf(     InExpression, InFormat, ... ) ((InExpression) != 0)
	#define ensureOnce(     InExpression                ) ((InExpression) != 0)
	#define ensureOnceMsgf( InExpression, InFormat, ... ) ((InExpression) != 0)

#endif	// DO_CHECK

// These are only for use as part of the GET_MEMBER_NAME_CHECKED macro
template <typename T>
bool JunkFunc(const T&);

template <typename T>
bool JunkFunc(const volatile T&);

// Returns FName(TEXT("MemberName")), while statically verifying that the member exists in ClassName
#define GET_MEMBER_NAME_CHECKED(ClassName, MemberName) \
	((void)sizeof(JunkFunc(((ClassName*)0)->MemberName)), FName(TEXT(#MemberName)))

#define GET_MEMBER_NAME_STRING_CHECKED(ClassName, MemberName) \
	((void)sizeof(JunkFunc(((ClassName*)0)->MemberName)), TEXT(#MemberName))

// Returns FName(TEXT("FunctionName")), while statically verifying that the function exists in ClassName
// &((ClassName*)0)->FunctionName will generate 'error: cannot create a non-constant pointer to member function' on non-MSVC compilers
#if PLATFORM_WINDOWS && !defined(__clang__)
	#define GET_FUNCTION_NAME_CHECKED(ClassName, FunctionName) \
		((void)sizeof(&((ClassName*)0)->FunctionName), FName(TEXT(#FunctionName)))
#else
	#define GET_FUNCTION_NAME_CHECKED(ClassName, FunctionName) \
		FName(TEXT(#FunctionName))
#endif

/*----------------------------------------------------------------------------
	Stats helpers
----------------------------------------------------------------------------*/

#if STATS
	#define STAT(x) x
#else
	#define STAT(x)
#endif

/*----------------------------------------------------------------------------
	Logging
----------------------------------------------------------------------------*/

// Define NO_LOGGING to strip out all writing to log files, OutputDebugString(), etc.
// This is needed for consoles that require no logging (Xbox, Xenon)

struct FTCharArrayTester
{
	template <uint32 N>
	static char (&Func(const TCHAR(&)[N]))[2];
	static char (&Func(...))[1];
};

#define IS_TCHAR_ARRAY(expr) (sizeof(FTCharArrayTester::Func(expr)) == 2)

#define LowLevelFatalError(Format, ...) \
	{ \
		static_assert(IS_TCHAR_ARRAY(Format), "Formatting string must be a TCHAR array."); \
		FError::LowLevelFatal(__FILE__, __LINE__, Format, ##__VA_ARGS__); \
		_DebugBreakAndPromptForRemote(); \
		FDebug::AssertFailed("", __FILE__, __LINE__, Format, ##__VA_ARGS__); \
	}


/*----------------------------------------------------------------------------
	Logging suppression
----------------------------------------------------------------------------*/

#ifndef COMPILED_IN_MINIMUM_VERBOSITY
	#define COMPILED_IN_MINIMUM_VERBOSITY VeryVerbose
#else
	#if !IS_MONOLITHIC
		#error COMPILED_IN_MINIMUM_VERBOSITY can only be defined in monolithic builds.
	#endif
#endif

#if NO_LOGGING

	struct FNoLoggingCategory {};
	
	// This will only log Fatal errors
	#define UE_LOG(CategoryName, Verbosity, Format, ...) \
	{ \
		static_assert(IS_TCHAR_ARRAY(Format), "Formatting string must be a TCHAR array."); \
		if (ELogVerbosity::Verbosity == ELogVerbosity::Fatal) \
		{ \
			FError::LowLevelFatal(__FILE__, __LINE__, Format, ##__VA_ARGS__); \
			_DebugBreakAndPromptForRemote(); \
			FDebug::AssertFailed("", __FILE__, __LINE__, Format, ##__VA_ARGS__); \
		} \
	}
	// Conditional logging (fatal errors only).
	#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
	{ \
		static_assert(IS_TCHAR_ARRAY(Format), "Formatting string must be a TCHAR array."); \
		if (ELogVerbosity::Verbosity == ELogVerbosity::Fatal) \
		{ \
			if (Condition) \
			{ \
				FError::LowLevelFatal(__FILE__, __LINE__, Format, ##__VA_ARGS__); \
				_DebugBreakAndPromptForRemote(); \
				FDebug::AssertFailed("", __FILE__, __LINE__, Format, ##__VA_ARGS__); \
			} \
		} \
	}

	#define UE_LOG_ACTIVE(...)				(0)
	#define UE_LOG_ANY_ACTIVE(...)			(0)
	#define UE_SUPPRESS(...) {}
	#define UE_SET_LOG_VERBOSITY(...)
	#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity) extern FNoLoggingCategory CategoryName
	#define DEFINE_LOG_CATEGORY(...)
	#define DEFINE_LOG_CATEGORY_STATIC(...)
	#define DECLARE_LOG_CATEGORY_CLASS(...)
	#define DEFINE_LOG_CATEGORY_CLASS(...)
	#define LOG_SCOPE_VERBOSITY_OVERRIDE(...)

#else

	/** 
	 * A predicate that returns true if the given logging category is active (logging) at a given verbosity level
	 * using compile time settings only. 
	 * @param CategoryName name of the logging category
	 * @param Verbosity, verbosity level to test against
	**/
	#define UE_LOG_CHECK_COMPILEDIN_VERBOSITY(CategoryName, Verbosity) \
		((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) <= FLogCategory##CategoryName::CompileTimeVerbosity && \
		(ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) <= ELogVerbosity::COMPILED_IN_MINIMUM_VERBOSITY)

	/** 
	 * A predicate that returns true if the given logging category is active (logging) at a given verbosity level 
	 * @param CategoryName name of the logging category
	 * @param Verbosity, verbosity level to test against
	**/
	#define UE_LOG_ACTIVE(CategoryName, Verbosity) \
		(UE_LOG_CHECK_COMPILEDIN_VERBOSITY(CategoryName, Verbosity) && \
		!CategoryName.IsSuppressed(ELogVerbosity::Verbosity))

	#define UE_SET_LOG_VERBOSITY(CategoryName, Verbosity) \
		CategoryName.SetVerbosity(ELogVerbosity::Verbosity);

	/** 
	 * A  macro that outputs a formatted message to log if a given logging category is active at a given verbosity level
	 * @param CategoryName name of the logging category
	 * @param Verbosity, verbosity level to test against
	 * @param Format, format text
	 ***/
	#if USING_CODE_ANALYSIS
		// Code analysis warnings will fire for every log statement due to comparisons against compile-time constant values, so we disable it
		#define UE_LOG(CategoryName, Verbosity, Format, ...)
		#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...)
	#else
		#define UE_LOG(CategoryName, Verbosity, Format, ...) \
		{ \
			static_assert(IS_TCHAR_ARRAY(Format), "Formatting string must be a TCHAR array."); \
			static_assert((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity && ELogVerbosity::Verbosity > 0, "Verbosity must be constant and in range."); \
			if (UE_LOG_CHECK_COMPILEDIN_VERBOSITY(CategoryName, Verbosity)) \
			{ \
				if (!CategoryName.IsSuppressed(ELogVerbosity::Verbosity)) \
				{ \
					FMsg::Logf_Internal(__FILE__, __LINE__, CategoryName.GetCategoryName(), ELogVerbosity::Verbosity, Format, ##__VA_ARGS__); \
					if (ELogVerbosity::Verbosity == ELogVerbosity::Fatal) \
					{\
						_DebugBreakAndPromptForRemote(); \
						FDebug::AssertFailed("", __FILE__, __LINE__, Format, ##__VA_ARGS__); \
					}\
				} \
			} \
		}

		// Conditional logging. Will only log if Condition is met.
		#define UE_CLOG(Condition, CategoryName, Verbosity, Format, ...) \
		{ \
			static_assert(IS_TCHAR_ARRAY(Format), "Formatting string must be a TCHAR array."); \
			static_assert((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity && ELogVerbosity::Verbosity > 0, "Verbosity must be constant and in range."); \
			if (UE_LOG_CHECK_COMPILEDIN_VERBOSITY(CategoryName, Verbosity)) \
			{ \
				if (!CategoryName.IsSuppressed(ELogVerbosity::Verbosity)) \
				{ \
					if (Condition) \
					{ \
						FMsg::Logf_Internal(__FILE__, __LINE__, CategoryName.GetCategoryName(), ELogVerbosity::Verbosity, Format, ##__VA_ARGS__); \
						if (ELogVerbosity::Verbosity == ELogVerbosity::Fatal) \
						{\
							_DebugBreakAndPromptForRemote(); \
							FDebug::AssertFailed("", __FILE__, __LINE__, Format, ##__VA_ARGS__); \
						}\
					} \
				} \
			} \
		}
	#endif

	/** 
	 * A macro that executes some code within a scope if a given logging category is active at a given verbosity level
	 * Also, withing the scope of the execution, the default category and verbosity is set up for the low level logging 
	 * functions.
	 * @param CategoryName name of the logging category
	 * @param Verbosity, verbosity level to test against
	 * @param ExecuteIfUnsuppressed, code to execute if the verbosity level for this category is being displayed
	 ***/
	#define UE_SUPPRESS(CategoryName, Verbosity, ExecuteIfUnsuppressed) \
	{ \
		static_assert((ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask) < ELogVerbosity::NumVerbosity && ELogVerbosity::Verbosity > 0, "Verbosity must be constant and in range."); \
		if (UE_LOG_CHECK_COMPILEDIN_VERBOSITY(CategoryName, Verbosity)) \
		{ \
			if (!CategoryName.IsSuppressed(ELogVerbosity::Verbosity)) \
			{ \
				FScopedCategoryAndVerbosityOverride TEMP__##CategoryName(CategoryName.GetCategoryName(), ELogVerbosity::Type(ELogVerbosity::Verbosity & ELogVerbosity::VerbosityMask)); \
				ExecuteIfUnsuppressed; \
				CategoryName.PostTrigger(ELogVerbosity::Verbosity); \
			} \
		} \
	}

	/** 
	 * A macro to declare a logging category as a C++ "extern" 
	 * @param CategoryName, category to declare
	 * @param DefaultVerbosity, default run time verbosity
	 * @param CompileTimeVerbosity, maximum verbosity to compile into the code
	 **/
	#define DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultVerbosity, CompileTimeVerbosity) \
		extern struct FLogCategory##CategoryName : public FLogCategory<ELogVerbosity::DefaultVerbosity, ELogVerbosity::CompileTimeVerbosity> \
		{ \
			FORCEINLINE FLogCategory##CategoryName() : FLogCategory(TEXT(#CategoryName)) {} \
		} CategoryName;

	/** 
	 * A macro to define a logging category, usually paired with DECLARE_LOG_CATEGORY_EXTERN from the header.
	 * @param CategoryName, category to define
	**/
	#define DEFINE_LOG_CATEGORY(CategoryName) FLogCategory##CategoryName CategoryName;

	/** 
	 * A macro to define a logging category as a C++ "static"
	 * @param CategoryName, category to declare
	 * @param DefaultVerbosity, default run time verbosity
	 * @param CompileTimeVerbosity, maximum verbosity to compile into the code
	**/
	#define DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity) \
		static struct FLogCategory##CategoryName : public FLogCategory<ELogVerbosity::DefaultVerbosity, ELogVerbosity::CompileTimeVerbosity> \
		{ \
			FORCEINLINE FLogCategory##CategoryName() : FLogCategory(TEXT(#CategoryName)) {} \
		} CategoryName;

	/** 
	 * A macro to declare a logging category as a C++ "class static" 
	 * @param CategoryName, category to declare
	 * @param DefaultVerbosity, default run time verbosity
	 * @param CompileTimeVerbosity, maximum verbosity to compile into the code
	**/
	#define DECLARE_LOG_CATEGORY_CLASS(CategoryName, DefaultVerbosity, CompileTimeVerbosity) \
		DEFINE_LOG_CATEGORY_STATIC(CategoryName, DefaultVerbosity, CompileTimeVerbosity)

	/** 
	 * A macro to define a logging category, usually paired with DECLARE_LOG_CATEGORY_CLASS from the header.
	 * @param CategoryName, category to definee
	**/
	#define DEFINE_LOG_CATEGORY_CLASS(Class, CategoryName) Class::FLogCategory##CategoryName Class::CategoryName;

	/** 
	 * A macro to override Verbosity of the Category within the scope
	 * @param CategoryName, category to declare
	 * @param ScopeVerbosity, verbosity to override
	 **/
	#define LOG_SCOPE_VERBOSITY_OVERRIDE(CategoryName, ScopeVerbosity) FLogScopedVerbosityOverride LogCategory##CategoryName##Override(&CategoryName, ScopeVerbosity)

#endif // NO_LOGGING
#if PLATFORM_HTML5
#include "HTML5/HTML5AssertionMacros.h"
#endif 
