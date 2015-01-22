// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


#ifndef DEPRECATED
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
	 *		struct DEPRECATED(4.6, "Message") MyStruct;
	 *		class DEPRECATED(4.6, "Message") MyClass;
	 *
	 * @param VERSION The release number in which the feature was marked deprecated.
	 * @param MESSAGE A message text containing additional upgrade notes.
	 */
	#define DEPRECATED(VERSION, MESSAGE)
#endif // DEPRECATED

#ifndef PRAGMA_DISABLE_DEPRECATION_WARNINGS
	#define PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif

#ifndef PRAGMA_ENABLE_DEPRECATION_WARNINGS
	#define PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

#ifndef EMIT_DEPRECATED_WARNING_MESSAGE
	#define EMIT_DEPRECATED_WARNING_MESSAGE(Msg)
#endif // EMIT_DEPRECATED_WARNING_MESSAGE

#ifndef EMIT_CUSTOM_WARNING
	#define EMIT_CUSTOM_WARNING(Warning)
#endif // EMIT_CUSTOM_WARNING