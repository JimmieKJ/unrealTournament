//
// paRndFun.hpp
//
// Simple, efficient random surface functions.
// These are stochastic rather than consistently repeating when position reoccurs.
//


#if !defined(__paRndFun_hpp)
#define __paRndFun_hpp

#include "System/paFloat.h"
#include "Contact/paFun.hpp"


class PHYA_API paRndFun : public paFun
{
private:
	paFloat m_zeroRate;
	bool m_fastZero;
	paFloat m_min;
	paFloat m_zr;
public:
	paRndFun(){
		m_zeroRate = 0.0f;
		m_fastZero = false;
		m_min = -1.0f;
	}
	// ZeroRate determines the probability of the surface profile returning to zero.
	// A higher value gives a shorter average 'bump'.
	// It is relative to the main rate, so that as speed changes,
	// the duration of the bumps follows this.
	// -1 forces the fastest possible bump.

	void setZeroRate(paFloat p){
		m_zeroRate = p;
		if (p == -1) m_fastZero = true;  else m_fastZero = false;
	}

	//void setAbsZeroRate(paFloat p){
	//	m_zeroRate = p;
	//	if (p == -1) m_fastZero = true;  else m_fastZero = false;
	//}

	void setFastZero(){
		m_fastZero = true;
	}

	void tick(paFunContactGen*);

	void setMin(paFloat m){ m_min = m; };
};



#endif
