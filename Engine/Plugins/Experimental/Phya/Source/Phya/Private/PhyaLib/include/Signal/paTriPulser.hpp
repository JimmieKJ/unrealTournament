//
// paTriPulser.hpp
//
// Generates a triangular pulse with controllable width and amp.
// NB the output will be slightly aliased because of the highfreq content due to the non-smooth points.
//
//


#ifndef _paTrianglePulser_hpp
#define _paTrianglePulser_hpp



#include "Signal/paBlock.hpp"

class PHYA_API paTriPulser
{
protected:
	int m_widthSamps;
	paFloat m_widthSecs;
	paFloat m_amp;
	bool m_beenHit;
	bool m_isQuiet;

	paBlock* m_output;

public:

	paTriPulser();
	~paTriPulser();
	int hit(paFloat amp);		// The pulse is scaled so that total energy similar to a wave segment of amplitude 'amp' and duration 1 second.
	int setWidthSeconds(paFloat width);
	bool isQuiet() { return m_isQuiet; };
	paBlock* tick(void);
	void setOutput( paBlock* output ) { m_output = output; };
	paBlock* getOutput() { return m_output; };
};


#endif
