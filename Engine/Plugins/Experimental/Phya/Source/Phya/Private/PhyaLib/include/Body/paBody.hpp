//
// paBody.hpp
//

#if !defined(__paBody_hpp)
#define __paBody_hpp

#include "Scene/paAudio.hpp"
#include "Utility/paObjectPool.hpp"
#include "Resonator/paRes.hpp"
#include "Surface/paSurface.hpp"



class PHYA_API paBody
{
	friend class paContact;
	friend class paImpact;

private:
	paRes* m_resonator;
	paSurface* m_surface;
	bool m_isEnabled;

	void* m_userData;

public:

	paBody();

	// User interface:

	int setRes(paRes* r) { m_resonator = r; r->zero(); return 0; }
	int setSurface(paSurface* s);

	paRes* getRes() { return m_resonator; }
	paSurface* getSurface() { return m_surface; }


	paFloat m_maxImpulse;


	// Coupling parameters:

	paFloat m_contactAmpMax;
	paFloat m_contactGain;
	paFloat m_contactCrossGain;
	paFloat m_contactDirectGain;

	paFloat m_impactAmpMax;
	paFloat m_impactGain;
	paFloat m_impactCrossGain;
	paFloat m_impactDirectGain;

	int setContactAmpMax(paFloat a) { m_contactAmpMax = a; return 0; }			// Limit the amplitude just before the surface generator.
	int setContactGain(paFloat g) { m_contactGain = g; return 0; }
	int setContactCrossGain(paFloat g) { m_contactCrossGain = g; return 0; }
	int setContactDirectGain(paFloat g) { m_contactDirectGain = g; return 0; }

	int setImpactAmpMax(paFloat g) { m_impactAmpMax = g; return 0; }
	int setImpactGain(paFloat g) { m_impactGain = g; return 0; }
	int setImpactCrossGain(paFloat g) { m_impactCrossGain = g; return 0; }
	int setImpactDirectGain(paFloat g) { m_impactDirectGain = g; return 0; }


	void* getUserData() { return m_userData; }
	int setUserData(void* d) { m_userData = d; return 0; }

	int enable() { m_isEnabled = true; return 0; }
	int disable() { m_isEnabled = false; return 0; }
	bool isEnabled() { return m_isEnabled; }

};









#endif
