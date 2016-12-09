//
// paFlags.cpp
//
//! Should move all global vars and functions into a scene class.
#include "CoreMinimal.h"

namespace paScene {

	bool audioThreadIsOn = false;
	bool outputStreamIsOpen = false;
	bool isLocked = false;
}
