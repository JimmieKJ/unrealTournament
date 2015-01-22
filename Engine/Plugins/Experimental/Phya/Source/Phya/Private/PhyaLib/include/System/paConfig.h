/*
    paConfig.h
*/

#ifndef _PA_CONFIG_H
#define _PA_CONFIG_H

#ifndef PHYA_API
#   define PHYA_API
#endif

#ifndef USING_AIO
#   define USING_AIO 0
#endif

#ifndef _PA_ASSERTS
#   define _PA_ASSERTS 1
#endif

#ifndef _PA_THREADS
#   define _PA_THREADS 0
#endif


#if _PA_ASSERTS
#include <assert.h>
#define paAssert( COND ) assert( COND )
#endif // _PA_ASSERTS

#define USING_PA_TERM 0
#define USING_USER_FUN 0

#endif // _PA_CONFIG_H
