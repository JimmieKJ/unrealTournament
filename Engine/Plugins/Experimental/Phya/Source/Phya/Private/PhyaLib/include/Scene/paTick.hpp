//
// paTick.hpp
//
// Calculates one paBlock of audio output, for the whole acoustic scene.
//


#include "Scene/paAudio.hpp"

class paBlock;


PHYA_API int paTickInit();			// Get resources.
PHYA_API paBlock* paTick();


