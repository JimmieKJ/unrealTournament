/**
 * @file gpg/platform_defines.h
 *
 * @brief Platform-specific preprocessor definitions.
 */

#ifndef GPG_PLATFORM_DEFINES_H_
#define GPG_PLATFORM_DEFINES_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#if __ANDROID__

#define GPG_ANDROID 1

#elif __APPLE__

#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#define GPG_IOS 1

#else
#error Unsupported platform.
#endif

#else

#error Unsupported platform.

#endif

#endif  // GPG_PLATFORM_DEFINES_H_
