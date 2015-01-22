//
// paFunImpactGen.hpp
//
// Note this includes a paFunContactGen for doing skidding noise.
//

#if !defined(__paFunImpactGen_hpp)
#define __paFunImpactGen_hpp


#include "Impact/paImpactGen.hpp"
#include "Contact/paFunContactGen.hpp"
#include "Signal/paTriPulser.hpp"


class PHYA_API paFunImpactGen : public paImpactGen
{
private:

	paFunSurface* m_surface;			// Surface description used by this generator.
	paTriPulser m_pulser;
	paFunContactGen m_contactGen;       //!! Only used for skids, too wasteful.

	paLowpass m_lowpass;			// Filter impact sample  //! This is not generally needed, should be dynamically allocated / in seperate impact generator.


	bool m_isQuiet;
	bool m_isSkidding;
	int m_skidCount;
	paFloat m_amp;
	paFloat m_impulseToHardnessBreakpoint;
	paFloat m_impulseToHardnessScale;


	long m_x;	// Sample pointer
	bool m_endOfSample;
	int m_samplePlaying;

public:

	paFunImpactGen();

	void setOutput(paBlock* o) {
		m_pulser.setOutput(o);
		m_lowpass.setIO(o);
		m_output = o;
	};

	void initialize(paFunSurface*);

	paBlock* tick();

	bool isQuiet();

};



#endif
