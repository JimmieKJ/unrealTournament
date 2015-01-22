//
// paImpactGen.hpp
//
// Abstract base class for generating sound from a continuous contact.
// Referenced by active paContact objects.
//


#if !defined(__paImpactGen_hpp)
#define __paImpactGen_hpp



#include "Signal/paBlock.hpp"


class paSurface;

class PHYA_API paImpactGen
{


protected:
	paBlock* m_output;

	paSurface* m_otherSurface;
	paFloat m_impactGain;
	paFloat m_limit;

	paFloat m_relTangentSpeedAtImpact;
	paFloat m_relNormalSpeedAtImpact;
	paFloat m_impactImpulse;

	bool m_isNew;

public:

	paImpactGen();
	virtual ~paImpactGen() {}

	virtual paBlock* tick() = 0;

	int m_poolHandle;			// Handle used by the scene contact manager to release the object.

	virtual bool isQuiet() = 0;	// Used to determine when impact should be deleted.

	void setRelTangentSpeedAtImpact(paFloat s) {
		m_relTangentSpeedAtImpact = s; }

	void setRelNormalSpeedAtImpact(paFloat s) {
		m_relNormalSpeedAtImpact = s; }

	void setImpactImpulse(paFloat i) {
		m_impactImpulse = i; }

	virtual void setOutput(paBlock* output) = 0;
	paBlock* getOutput() { return m_output; }

	void setIsNew() { m_isNew = true; }

	void setImpactGain(paFloat g) { m_impactGain = g; }
	void setAmpMax(paFloat l) { m_limit = l; }
	void setOtherSurface(paSurface* s) { m_otherSurface = s; }

};



#endif

