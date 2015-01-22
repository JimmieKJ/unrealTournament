//
// paFunContactGen.hpp
//



#if !defined(__paFunContactGen_hpp)
#define __paFunContactGen_hpp



#include "Surface/paFunSurface.hpp"
#include "Signal/paLowpass.hpp"
#include "Contact/paContactGen.hpp"

#ifdef FREEPARTICLESURF
#include "Signal/paBiquad.hpp"
#endif


class paFunSurface;

class PHYA_API paFunContactGen : public paContactGen
{

private:

	paFunSurface* m_surface;		// Surface description used by this generator.
	paLowpass m_lowpass;			// Filter used to model variation in relative body slip / roll
	paLowpass m_lowpass2;			// Used to make filter steeper.
	paLowpass m_lowpass3;

	paFloat m_amp;					// Records last amp for isQuiet.
	paFloat m_cfF;					// System decay filtering state.
	paFloat m_ssF;
	paFloat m_scF;

#ifdef FREEPARTICLESURF
	paLowpass m_hitLowpass;			// Shape particle hit rise/decays
	paBiquad m_particleBiquad;		// Particle resonance/filter
#endif

public:

	paFunContactGen();

	void initialize(paFunSurface*);

	void setOutput(paBlock* o) { 
		m_output = o;
		m_lowpass.setIO(o);
		m_lowpass2.setIO(o);
		m_lowpass3.setIO(o);

#ifdef FREEPARTICLESURF
		m_hitLowpass.setIO(o);
		m_particleBiquad.setIO(o);
#endif
	};

	bool isQuiet();
	bool isActive();

	paBlock* tick();

									// State variables accessible to the surface function:
	paFloat m_rate;					// Speed of contact relative to surface.
	paFloat m_x;					// Position of contact in generator (not actual position on surface) Useful for wav / pattern generators.
	paFloat m_y;					// Stores last gen output sample. Used for state and quiet check.
//	paFloat m_y2;					// Could add more here...

	paFloat m_out;					// Last output sample for quiet check.
};



#endif
