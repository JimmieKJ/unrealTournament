//
// paCoolSurface.hpp
//


#if !defined(__paCoolSurface_hpp)
#define __paCoolSurface_hpp

#include "System/paFloat.h"

class paContactGen;
class paImpactGen;

class PHYA_API paCoolSurface
{
private:

public:

	paCoolSurface();

//	friend class paContactGen;

	virtual paContactGen* newContactGen() = 0;			// Used in main tick() to create new contacts
	virtual int deleteContactGen(paContactGen*) = 0;		// of the correct surface type.

	virtual paImpactGen* newImpactGen() = 0;
	virtual int deleteImpactGen(paImpactGen*) = 0;


	paFloat m_maxImpulse;	// For impacts.
	paFloat m_maxForce;		// For contacts.
	paFloat m_hardness;							// Used to calculate 'combined hardness' of a collision.
												// From this the parameters of each impact generator are derived.
												// Generally m_hardness = 1/(timescale_of_impact).
												// Could possible be used in contact calculations.
	paFloat m_impulseToHardnessBreakpoint;		// Hardness is increased by the scale times the
	paFloat m_impulseToHardnessScale;			// amount the impulse exceeds the breakpoint.
												// Models nonlinearity.

	paFloat m_contactDamping;					// Damping added to resonators that become attached
												// to this surface.

	// Coupling parameters:
	paFloat m_contactGain;
	paFloat m_contactAmpMax;
	paFloat m_contactCrossGain;
	paFloat m_contactDirectGain;

	paFloat m_impactGain;
	paFloat m_impactAmpMax;
	paFloat m_impactCrossGain;
	paFloat m_impactDirectGain;

	int setContactGain(paFloat g) { m_contactGain = g; return 0; }
	int setContactAmpMax(paFloat a) { m_contactAmpMax = a; return 0; }
	int setContactCrossGain(paFloat g) { m_contactCrossGain = g; return 0; }
	int setContactDirectGain(paFloat g) { m_contactDirectGain = g; return 0; }

	int setImpactGain(paFloat g) { m_impactGain = g; return 0; }
	int setImpactAmpMax(paFloat g) { m_impactAmpMax = g; return 0; }
	int setImpactCrossGain(paFloat g) { m_impactCrossGain = g; return 0; }
	int setImpactDirectGain(paFloat g) { m_impactDirectGain = g; return 0; }

	// Hardness and contact damping are often related, but here they are
	// independently controllable.
	int setHardness(paFloat h) { m_hardness = h; return 0; }
	int setImpulseToHardnessBreakpoint(paFloat b) { m_impulseToHardnessBreakpoint = b; return 0; }
	int setImpulseToHardnessScale(paFloat s) { m_impulseToHardnessScale = s; return 0; }
	int setContactDamping(paFloat d) { m_contactDamping = d; return 0; }

};






#endif

