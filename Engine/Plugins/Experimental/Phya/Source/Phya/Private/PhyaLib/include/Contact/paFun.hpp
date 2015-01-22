
//
// paFun.hpp
//
// Abstract base class for raw surface profile functions.
//
// Fun's have no collision-time state, only configuration parameters.
// All collision-time state is stored in the function contact generator.
// This simplifies the management of collision resources.


#if !defined(__paFun_hpp)
#define __paFun_hpp

#include "System/paConfig.h"

class paFunContactGen;

class PHYA_API paFun {

public:
	paFun(){}
	virtual ~paFun(){}
	virtual void tick(paFunContactGen*) = 0;
};



#endif
