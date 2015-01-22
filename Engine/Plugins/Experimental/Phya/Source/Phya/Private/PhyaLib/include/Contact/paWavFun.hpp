//
// paWavFun.hpp
//
// Simple, efficient random surface functions.
// These are stochastic rather than consistently repeating when position reoccurs.
//


#if !defined(__paWavFun_hpp)
#define __paWavFun_hpp

#include "Contact/paFun.hpp"


class PHYA_API paWavFun : public paFun
{
private:
	short* m_start;
	long m_nFrames;
	int m_interp;

public:
	paWavFun() { m_start = 0; m_interp = 1; }
	virtual ~paWavFun() { freeWav(); }

	int readWav(char* filename);

	void freeWav();

	void setInterpOn() { m_interp = 1; };
	void setInterpOff() { m_interp = 0; };

	void tick(paFunContactGen*);
};



#endif
