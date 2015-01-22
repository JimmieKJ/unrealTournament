//
// paMeasureGain.hpp
//
// A tool for measuring the broad-band average gain of a filter, eg a resonator.
// Can be used for prenormalising a filter.
//


#ifndef _paMeasureGain_hpp
#define _paMeasureGain_hpp



class paBlock;
#include "Scene/paAudio.hpp"


paFloat	paMeasureGain( void (*filter)(paBlock* input, paBlock* output) );


#endif
