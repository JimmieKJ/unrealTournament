//
//  paLimiter.hpp
//
//  Decent brickwall limiting using lookahead.
//  Use to prevents clipping at output points.
//



#if !defined (__paLimiter_hpp)
#define __paLimiter_hpp


#include "Signal/paBlock.hpp"

enum paLimiterState
{
	paLS_DIRECT,
	paLS_ATTACK,
	paLS_HOLD,
	paLS_RELEASE,
};

class PHYA_API paLimiter
{
private:

	paBlock* m_input;
	paBlock* m_output;
	int m_bufferLen;			// Used for attack lookahead.
	paFloat* m_bufferStart;
	paFloat* m_bufferEnd;
	paFloat* m_bufferIO;		// Initial readout/readin point.

	paFloat m_threshold;		// Control parameters.
	int m_holdLen;
	int m_releaseLen;

	paFloat m_gain;
	paFloat m_gainRate;
	paFloat m_gainTarget;
	paFloat m_peak;
	long m_holdCount;
	long m_releaseCount;
	paLimiterState m_state;



public:
	paLimiter( paFloat attackTime, paFloat holdTime, paFloat releaseTime );
	~paLimiter();

	void setInput(paBlock* input) { m_input = input; };
	void setOutput(paBlock* output) { m_output = output; };
	void setThreshold(paFloat t) { m_threshold = t; };
	void setHoldTime(paFloat t) { m_holdLen = (long)(t * paScene::nFramesPerSecond); };
	void setReleaseTime(paFloat t) { m_releaseLen = (long)(t * paScene::nFramesPerSecond); };

	paBlock* tick();
	paBlock* tick( paBlock* input, paBlock* output) {
		m_input = input;
		m_output = output;
		return tick();
	}
	paBlock* tick( paBlock* io) {
		m_input = io;
		m_output = io;
		return tick();
	}


	paBlock* getOutput() { return m_output; };
};



#endif
