//
// paUserFun.hpp
//
//


#if !defined(__paUserFun_hpp)
#define __paUserFun_hpp

#include "Surface/paFun.hpp"


class PHYA_API paUserFun : public paFun
{
private:

public:
	paUserFun(){}	// Initially even mark/space.

	void tick(paFunContactGen*);

};



#endif