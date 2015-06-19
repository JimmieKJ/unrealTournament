#include "OVRVersion.h"

#define STRINGIZE( x )			#x
#define STRINGIZE_VALUE( x )	STRINGIZE( x )

const char OVR_VERSION_STRING[] = STRINGIZE_VALUE( OVR_MAJOR_VERSION )  \
							"." STRINGIZE_VALUE( OVR_MINOR_VERSION )	\
							"." STRINGIZE_VALUE( OVR_BUILD_VERSION )	\
							"." STRINGIZE_VALUE( OVR_REV_VERSION )		\
							" " __DATE__ " " __TIME__ ;
