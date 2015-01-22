//
// paFunSurface.hpp
//
// Surface using surface functions, paFun.
// Output is lowpass filtered according to relative body speed at contact.
//


#if !defined(__paFunSurface_hpp)
#define __paFunSurface_hpp




#include "Utility/paObjectPool.hpp"
#include "Surface/paSurface.hpp"
#include "Contact/paFun.hpp"
#include "Scene/paAudio.hpp"


//template class PHYA_API paObjectPool<paFunContactGen>;
//template class PHYA_API paObjectPool<paFunImpactGen>;
class paFunContactGen;
class paFunImpactGen;

class PHYA_API paFunSurface : public paSurface
{
	friend class paFunContactGen;	// Because we are storing the contact gen pools here.
	friend class paFunImpactGen;		// ..and FunContactGen.hpp includes Surface.hpp already.
										// May be better to

protected:

	// Surface properties for contact:
	paFun* m_fun;					// Function which generates the raw surface profile.
	paFloat m_cutoffRate;
	paFloat m_cutoffMin;
	paFloat m_cutoffMax;
	paFloat m_cutoffRate2;			// Secondary filter used to steepen cutoff when rolling.
	paFloat m_cutoffMin2;
	paFloat m_cutoffMax2;
	paFloat m_rateScale;
	paFloat m_rateMin;
	paFloat m_rateMax;
	paFloat m_quietSpeed;
	paFloat m_gainAtRoll;			// Boost when rolling due to increased transfer at low freqs.
	paFloat m_gainBreak;			// Slip speed at which boost starts phasing in.
	paFloat m_gainRate;
	paFloat m_systemAlpha;

	// Surface properties for impact:
	paFloat m_skidImpulseToForceRatio;
	paFloat m_skidGain;				// Gain of contact gen, possibly redundant because of impulseToForceRatio.
	int m_nSkidBlocks;					//! Skid time, change this.
	paFloat m_skidThickness;			// Simulate a loose surface layer.
										// Skidding time = skidThickness / normalSpeed.
	paFloat m_skidMinTime;
	paFloat m_skidMaxTime;

	long m_nFrames[6];		//! num samples could be pre set before loading.
	short* m_start[6];
	int m_nImpactSamples;
	int m_t;

public:

	paFunSurface();
	~paFunSurface();

	// User interface:
	static paObjectPool<paFunContactGen> contactGenPool;
	static paObjectPool<paFunImpactGen> impactGenPool;

	paContactGen* newContactGen();
	int deleteContactGen(paContactGen* gen);

	paImpactGen* newImpactGen();
	int deleteImpactGen(paImpactGen* gen);

	int setFun(paFun* f) { m_fun = f; return 0; }
	paFun* getFun() { return m_fun; return 0; }

	int setHardness(paFloat h) { m_hardness = h; return 0; }

	// Rate is the rate a surface is traversed by a contact, and has different
	// meanings for different kinds of surface. eg
	// Grid - number of bars per second.
	// Rnd - number of bumps per second.
	// Wav - number of wav seconds per second.

	// Rate of contact gen when there is zero contact vel.
	// Useful when contact vel cannot be found easily.
	int setRateMin(paFloat r) {m_rateMin = r; return 0; }	
	int setRateAtSpeed(paFloat r, paFloat s)
	{
		m_rateScale = (r - m_rateMin)/s;
		return 0;
	};
	int setRateConstant(paFloat r)
	{
		m_rateMin = r;
		m_rateScale = 0;
		return 0;
	};
	int setRateScale(paFloat r)
	{
		m_rateScale = r;
		return 0;
	};

	int setRateMax(paFloat r)
	{
		m_rateMax = r;
		return 0;
	};

	// Slip speed / filter cutoff curve.
	// Specify rate of cutoff change with slip speed.
	// Cutoff rises to maximum if given, or sample rate max if that comes first.
	// cutoff 2 is for beefing up the sound when rolling.
	int setCutoffFreqAtRoll(paFloat f) { m_cutoffMin = f; return 0; }
	int setCutoffFreqRate(paFloat r) { m_cutoffRate = r; return 0; }
	int setCutoffFreqMax(paFloat m) { m_cutoffMax = m; return 0; }
	int setCutoffFreq2AtRoll(paFloat f) { m_cutoffMin2 = f; return 0; }
	int setCutoffFreq2Rate(paFloat r) { m_cutoffRate2 = r; return 0; }
	int setCutoffFreq2Max(paFloat m) { m_cutoffMax2 = m; return 0; }

	int setGainBreakSlipSpeed(paFloat s) { m_gainBreak = s; m_gainRate = (s == 0.0f) ? 0.0f : (1-m_gainAtRoll)/m_gainBreak; return 0;}
	int setGainAtRoll(paFloat a) { m_gainAtRoll = a; m_gainRate = (m_gainBreak == 0.0f) ? 0.0f : (1-m_gainAtRoll)/m_gainBreak; return 0; }

	int setSystemDecayAlpha(paFloat a) { m_systemAlpha = a; return 0; }

	int setSkidImpulseToForceRatio(paFloat r) { m_skidImpulseToForceRatio = r;  return 0; }
	int setSkidTime(paFloat seconds);
	int setSkidGain(paFloat g) { m_skidGain = g; return 0; }
	int setSkidThickness(paFloat t) { m_skidThickness = t; return 0; }
	int setSkidMinTime(paFloat t) { m_skidMinTime = t; return 0; }
	int setSkidMaxTime(paFloat t) { m_skidMaxTime = t; return 0; }

	int setQuietSpeed(paFloat s) { m_quietSpeed = s; return 0; }

//	int setnImpactSamples(int n);	//! todo
	int setImpactSample(char* filename);
	int setImpactSample(short* start, long nFrames);
};






#endif
