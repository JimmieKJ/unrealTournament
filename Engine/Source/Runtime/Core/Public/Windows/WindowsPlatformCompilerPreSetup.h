// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#ifndef DISABLE_DEPRECATION
	/**
	 * Macro for marking up deprecated code, functions and types.
	 *
	 * Features that are marked as deprecated are scheduled to be removed from the code base
	 * in a future release. If you are using a deprecated feature in your code, you should
	 * replace it before upgrading to the next release. See the Upgrade Notes in the release
	 * notes for the release in which the feature was marked deprecated.
	 *
	 * Sample usage (note the slightly different syntax for classes and structures):
	 *
	 *		DEPRECATED(4.6, "Message")
	 *		void Function();
	 *
	 *		struct DEPRECATED(4.6, "Message") MODULE_API MyStruct
	 *		{
	 *			// StructImplementation
	 *		};
	 *		class DEPRECATED(4.6, "Message") MODULE_API MyClass
	 *		{
	 *			// ClassImplementation
	 *		};
	 *
	 * @param VERSION The release number in which the feature was marked deprecated.
	 * @param MESSAGE A message containing upgrade notes.
	 */
	#if defined(__clang__)
		#define DEPRECATED(VERSION, MESSAGE) __attribute__((deprecated(MESSAGE " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile.")))

		#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
				_Pragma ("clang diagnostic push") \
				_Pragma ("clang diagnostic ignored \"-Wdeprecated-declarations\"")

		#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
				_Pragma ("clang diagnostic pop")
	#else
		#define DEPRECATED(VERSION, MESSAGE) __declspec(deprecated(MESSAGE " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile."))

		#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
			__pragma (warning(push)) \
			__pragma (warning(disable:4995)) \
			__pragma (warning(disable:4996))

		#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
			__pragma (warning(pop))
	#endif

#endif // DISABLE_DEPRECATION

#if defined(__clang__)
	// Clang compiler on Windows
	#ifndef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
				_Pragma ("clang diagnostic push") \
				_Pragma ("clang diagnostic ignored \"-Wshadow\"")
	#endif // PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS \
			_Pragma("clang diagnostic pop")
	#endif // PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_POP
		#define PRAGMA_POP \
			_Pragma("clang diagnostic pop")
	#endif // PRAGMA_POP
#else
	// VC++
	// 4456 - declaration of 'LocalVariable' hides previous local declaration
	// 4457 - declaration of 'LocalVariable' hides function parameter
	// 4458 - declaration of 'LocalVariable' hides class member
	// 4459 - declaration of 'LocalVariable' hides global declaration
	#ifndef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
			__pragma (warning(push)) \
			__pragma (warning(disable:4456)) \
			__pragma (warning(disable:4457)) \
			__pragma (warning(disable:4458)) \
			__pragma (warning(disable:4459))
	#endif // PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
		#define PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS \
			__pragma(warning(pop))
	#endif // PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

	#ifndef PRAGMA_POP
		#define PRAGMA_POP \
			__pragma(warning(pop))
	#endif // PRAGMA_POP
#endif

#if defined(__clang__)
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
		_Pragma(PREPROCESSOR_TO_STRING(message(WARNING_LOCATION(Line) Warning)))
#else
	#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
		__pragma(message(WARNING_LOCATION(Line) ": warning C4996: " Warning))
#endif

#if defined(__clang__)
	// Make certain warnings always be warnings, even despite -Werror.
	// Rationale: we don't want to suppress those as there are plans to address them (e.g. UE-12341), but breaking builds due to these warnings is very expensive
	// since they cannot be caught by all compilers that we support. They are deemed to be relatively safe to be ignored, at least until all SDKs/toolchains start supporting them.
	#pragma clang diagnostic warning "-Wreorder"
#endif

