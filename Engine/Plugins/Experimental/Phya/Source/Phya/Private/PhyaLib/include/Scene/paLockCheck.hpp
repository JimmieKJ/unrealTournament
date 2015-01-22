//
// paLockCheck.hpp
//

#include "System/paConfig.h"
#include "Scene/paFlags.hpp"


// Macro to use checking that the thread lock is set when
// trying to access shared data.

#define paLOCKCHECK paAssert( paScene::isLocked || !paScene::audioThreadIsOn && "Use paLock().");
