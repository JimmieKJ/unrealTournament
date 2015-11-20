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
	 *		Unfortunately, VC++ will complain about using member functions and fields from deprecated
	 *		class/structs even for class/struct implementation e.g.:
	 *		class DEPRECATED(4.8, "") DeprecatedClass
	 *		{
	 *		public:
	 *			DeprecatedClass() {}
	 *
	 *			float GetMyFloat()
	 *			{
	 *				return MyFloat; // This line will cause warning that deprecated field is used.
	 *			}
	 *		private:
	 *			float MyFloat;
	 *		};
	 *
	 *		To get rid of this warning, place all code not called in class implementation in non-deprecated
	 *		base class and deprecate only derived one. This may force you to change some access specifiers
	 *		from private to protected, e.g.:
	 *
	 *		class DeprecatedClass_Base_DEPRECATED
	 *		{
	 *		protected: // MyFloat is protected now, so DeprecatedClass has access to it.
	 *			float MyFloat;
	 *		};
	 *
	 *		class DEPRECATED(4.8, "") DeprecatedClass : DeprecatedClass_Base_DEPRECATED
	 *		{
	 *		public:
	 *			DeprecatedClass() {}
	 *
	 *			float GetMyFloat()
	 *			{
	 *				return MyFloat;
	 *			}
	 *		};
	 * @param VERSION The release number in which the feature was marked deprecated.
	 * @param MESSAGE A message containing upgrade notes.
	 */
	#define DEPRECATED(VERSION, MESSAGE) __declspec(deprecated(MESSAGE " Please update your code to the new API before upgrading to the next release, otherwise your project will no longer compile."))

	#define PRAGMA_DISABLE_DEPRECATION_WARNINGS \
		__pragma (warning(push)) \
		__pragma (warning(disable:4995)) \
		__pragma (warning(disable:4996))

	#define PRAGMA_ENABLE_DEPRECATION_WARNINGS \
		__pragma (warning(pop))

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
