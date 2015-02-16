#pragma once

#ifdef PI
# undef PI
# include "Leap.h"
# define PI (3.1415926535897932f)
#else
# include "Leap.h"
#endif
