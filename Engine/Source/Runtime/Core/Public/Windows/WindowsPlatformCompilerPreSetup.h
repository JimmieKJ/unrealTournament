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
	#define DEPRECATED(VERSION, MESSAGE) __declspec(deprecated(MESSAGE " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile."))

	#ifndef EMIT_DEPRECATED_WARNING_MESSAGE_STR
		#define EMIT_DEPRECATED_WARNING_MESSAGE_STR1(x) #x
		#define EMIT_DEPRECATED_WARNING_MESSAGE_STR(x) EMIT_DEPRECATED_WARNING_MESSAGE_STR1(x)
	#endif // EMIT_DEPRECATED_WARNING_MESSAGE_STR

	#define EMIT_DEPRECATED_WARNING_MESSAGE(Msg) __pragma(message(__FILE__ "(" EMIT_DEPRECATED_WARNING_MESSAGE_STR(__LINE__) "): warning C4996: " Msg))

	#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
		__pragma (warning(push)) \
		__pragma (warning(disable:4995)) \
		__pragma (warning(disable:4996))

	#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
		__pragma (warning(push)) \
		__pragma (warning(default:4995)) \
		__pragma (warning(default:4996))

#endif // DISABLE_DEPRECATION

#ifndef PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS
	#define PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS \
		__pragma (warning(push))
#endif // PRAGMA_DISABLE_SHADOW_VARIABLE_WARNINGS

#ifndef PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS
	#define PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS \
		__pragma(warning(pop))
#endif // PRAGMA_ENABLE_SHADOW_VARIABLE_WARNINGS

#ifndef PRAGMA_POP
	#define PRAGMA_POP \
		__pragma(warning(pop))
#endif // PRAGMA_POP

#if _MSC_VER
#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
	__pragma(message(WARNING_LOCATION(Line) ": warning C4996: " Warning))
#elif __clang__
#define EMIT_CUSTOM_WARNING_AT_LINE(Line, Warning) \
	_Pragma(PREPROCESSOR_TO_STRING(message(WARNING_LOCATION(Line) Warning)))
#else
#error Unknown compiler
#endif
