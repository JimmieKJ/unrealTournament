//
// paWhiteFun.cpp
//
// White noise, unaltered by rate. ie scale-free
//


#if !defined(__paWhiteFun_hpp)
#define __paWhiteFun_hpp

#include "Contact/paFun.hpp"


class PHYA_API paWhiteFun : public paFun
{
public:
	paWhiteFun(){}
	void tick(paFunContactGen*);
};



#endif
