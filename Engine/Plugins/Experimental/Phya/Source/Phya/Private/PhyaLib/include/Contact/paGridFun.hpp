//
// paGridFun.hpp
//
// Simple, efficient random surface functions.
// These are stochastic rather than consistently repeating when position reoccurs.
//


#if !defined(__paGridFun_hpp)
#define __paGridFun_hpp

#include "Contact/paFun.hpp"


class PHYA_API paGridFun : public paFun
{
private:
	paFloat m_cut;					// Determines mark/space ratio, or 'grid bar width'

public:
	paGridFun(){ m_cut = -0.5f; }	// Initially even mark/space.

	void tick(paFunContactGen*);

	void setMark(paFloat m) { m_cut = m - 1.0f; }		// Set mark as a fraction of 1.
};



#endif
