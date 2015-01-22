//
// paModalRes.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "System/paConfig.h"
#include <math.h>

#include "Resonator/paModalRes.hpp"
#include "Resonator/paModalVars.hpp"
#include "System/paMemory.hpp"

#define twopi 6.2853


int
paModalRes :: setData(paModalData* d)
{
	if (d)	// Otherwise initialize default mode
	{
		m_data = d;
		m_nActiveModes = m_data->getnModes();
	}

	zero();
	m_paramsChanged = false;

	return(0);
}


int
paModalRes :: zero()
{
	int i;

	calcCoeffs();

	for(i=0; i<paModal::nMaxModes; i++)		// Set initial states, consistent with DC bias.
	{
		m_u[i] = 0; //m_dcBias / (1-m_cminus[i] + 1/(1-m_cplus[i]));
		m_v[i] = 0; //m_u[i]/(1-m_cplus[i]);
	}

	return 0;
}


paModalRes :: paModalRes()
{
	m_nActiveModes = 0;

	//! Mallocs should be managed dynamically in file load etc.
	m_cplus = paFloatCalloc(paModal::nMaxModes);
	m_cminus = paFloatCalloc(paModal::nMaxModes);

	m_aa = paFloatCalloc(paModal::nMaxModes);

	m_u = paFloatCalloc(paModal::nMaxModes);
	m_v = paFloatCalloc(paModal::nMaxModes);

	m_l0 = paFloatCalloc(paModal::nMaxModes);
	m_l1 = paFloatCalloc(paModal::nMaxModes);

	m_data = NULL;

	m_auxFreqScale = 1.0;
	m_auxAmpScale = 1.0;
	m_auxDampScale = 1.0;

	m_contactDampingOld = 1.0;
	resetContactDamping();

	setQuietLevel(1.0f);
	m_levelSqr = 0.0;

	m_paramsChanged = true;

//	m_dcBias = 0.0f;
}


paModalRes :: ~paModalRes()
{
	paFree(m_cplus);
	paFree(m_cminus);
	paFree(m_aa);
	paFree(m_u);
	paFree(m_v);
	paFree(m_l0);
	paFree(m_l1);

}



// Calculate resonator filter coefficients.
//
// This is done once when a resonator is first initialized,
// and then whenever changes are made to the modes,
// eg when auxScaleFactors are changed.
//
// Frequent Frequency Update For Deformable Body Effects and Similar:
//
// The calculations here look fairly heavy, but small fequency jumps aren't
// very noticeable, so we can get away with update rates of as low as 30Hz for typical frequency
// variations. At this rate the relative cost of calculating coefficients
// is low compared to the resonator calculation.

// Frequent Amplitude Update For Continuous Contact:
//
// Approximating continuous variations of coupling amplitudes with discrete jumps is
// much more noticeable than for frequency variation, but luckily the cost of updating
// the aa parameters can be much lower if simple interpolation is applied between updates.
// Continuous amplitude coupling variation corresponds to moving a contact point over
// a surface. However we can just set the coupling on initial contact and not bother to
// vary it, without noticing much difference - Variation of coupling with impacts is
// more important to model. For this reason coupling interpolation is not implemented,
// although it could be added transparently as a user option for each input.


int
paModalRes :: calcCoeffs()
{
	paFloat t1;
	paFloat t2;
	paFloat t3;

	paFloat creal;
	paFloat cimg;
	paFloat ds = m_contactDamping * m_auxDampScale / paScene::nFramesPerSecond;
	paFloat fs = m_auxFreqScale / paScene::nFramesPerSecond;
	paAssert(m_data && "Modal data undefined in resonator.");
	paAssert(!(m_data && (!m_data->m_amp || !m_data->m_damp || !m_data->m_freq)) && "Modal data defined in resonator, but not valid");

	paFloat* d = m_data->m_damp;					// Resolve indirection before loop.
	paFloat* f = m_data->m_freq;
	paFloat* a = m_data->m_amp;

	paFloat fTemp;
//	paFloat u, v, lsqr;
 
	int m;

	for(m = 0; m < m_nActiveModes; m++) { 

//		u = m_u[m];
//		v = m_v[m];
//		lsqr = m_l1[m] * ( (u+v)*(u+v) + m_l0[m] * u*v);
//		fTemp	= (paFloat)f[m] * fs * (1.0 + 0.005 * sqrt(m_levelSqr) /f[m]);  //!

		fTemp	= (paFloat)	f[m] * fs;
		if (fTemp > .5) m_aa[m] = 0.0;				// Remove mode if it would frequency-alias.
		else{
			t1		= (paFloat)	exp(-d[m] * ds);
			t2		= (paFloat)	twopi * fTemp;
			creal	= (paFloat)	cos(t2) * t1;		//! Replace with cheaper approximation?
			cimg	= (paFloat)	sin(t2) * t1;

			t3		    = (paFloat)	sqrt(1 - cimg*cimg);
			m_cplus[m]  = creal+t3;
			m_cminus[m] = creal-t3;
			m_aa[m]     = a[m] * cimg * m_auxAmpScale;

			// Find constants used by isQuiet() to find level of mode[0]
			m_l0[m] = (paFloat)(2.0* (t3 - (paFloat)1.0));
//			m_l0[m] = (paFloat)(2.0* (sqrt(1 - cimg*cimg) - (paFloat)1.0));
			m_l1[m] = 1/cimg/cimg;

		}
	}

	return 0;
}


/*
void
paModalRes :: processControlInput()
{
	// ContactDamping accumulation check.
	if (m_contactDamping > m_maxContactDamping)
		m_contactDamping = m_maxContactDamping;
	if (m_contactDamping != m_contactDampingOld)
	{
		m_contactDampingOld = m_contactDamping;
		m_paramsChanged = true;
	}


	if (m_paramsChanged) {
		calcCoeffs();
		m_paramsChanged = false;
		return;
	}

	// Amp coupling update for separate inputs goes here..
}
*/

int
paModalRes :: setQuietLevel(paFloat l)
{
//	l *= 0.1f;		// Makes up for inaccuracy in level detector.
	m_quietLevelSqr = l*l;
	return 0;
};


paBlock*
paModalRes :: tick()
{

	m_output->zero();	// Could be made faster by overwriting output on mode 0,
						// but this won't make much difference because num modes not small.
	tickAdd();
	return m_output;
}


paBlock*
paModalRes :: tickAdd()
{
// Modal resonator, based on 2nd order variant optimized for speed by Kees van den Doel.

	int nFrames = paBlock::nFrames;
	int mode;
	int i;
	paFloat uPrev;
	paFloat u = 0;
	paFloat v;
	paFloat cp, cm, aaa;

	//long double uPrev;
	//long double u;
	//long double v;
	//long double cp, cm, aaa;

	paAssert(m_input);
	paAssert(m_output);
	paAssert(m_input != m_output && "res->tickAdd() cannot use an input and output block the same.");

	paFloat* out = m_output->getStart();			// Output samples. 'output' is const. 'start' isn't.
	paFloat* in = m_input->getStart();

//	paBlock* tempBlock = paBlock::newBlock();  //t
	int nLowModes = -1;  //t

	updateContactDamping();

	if (m_paramsChanged) { calcCoeffs(); m_paramsChanged = false; }

	for(mode=0; mode < m_nActiveModes; mode++) {
//	for(mode=10; mode <30; mode++) {

		//if (mode == nLowModes) {
		//	tempBlock->copy(m_output);	// Save output so far for exciting high modes.  //t
		//	paFloat* in = tempBlock->getStart();
		//	for(i=0; i<nFrames; i++)
		//	{ 
		//		in[i] = 0; //abs(in[i]);		// Non-linear distortion will excite higher modes.
		//	}
		//	//tempBlock->add(m_input);	// Retain external excitation for higher modes.
		//}

		uPrev = m_u[mode];		// Load resonator state for this mode from end of previous block.
		v = m_v[mode];
		cp = m_cplus[mode];
		cm = m_cminus[mode];

//		dcOffset = m_dcBias / (1-cm + 1/(1-cp));	// Experimental anti-limit cycle offset

		// Run the resonator for this mode for one block.

		// The impulse response of each mode has the form sin(wt)exp(lambda t), t>0
		// This is the physical displacement generated by a force impulse.
		// Note the standard 2-pole resonator is slightly different having the response
		// cos(wt)exp(lambda t)

		//! If you hear residual noise, its probably due to your crappy DAC.

		aaa = m_aa[mode];

		for(i=0; i< nFrames; i++)
		{
//			if (mode==1) in[i] = 0;
			if (mode==1 && in[i] != 0) {
				int a=1;
			}
			u = cm * uPrev - v + aaa * in[i] + (paFloat)DENORMALISATION_EPSILON;	// Prevents denormalization.
			v = cp * v + uPrev;
			uPrev = u;
			out[i] += v;
		}


		m_u[mode] = u;
		m_v[mode] = v;
	}


//	paBlock::deleteBlock(tempBlock);	//t
	return	m_output;	// paBlock pointer

}	// tickAdd()




bool
paModalRes :: isQuiet()
{
	if (m_makeQuiet) {
		m_makeQuiet = false;
		return true;
	}

	paFloat u;
	paFloat v;
	int mode;
	m_levelSqr = 0;

	for(mode=0; mode < m_nActiveModes; mode++) {		//! Maybe just use the first 5 or so?

		// Use state variables to find mode levels and estimated total level (squared)
		// (total level is sqrt energy. energy = sum of mode energies = sum of mode levels squared)

		u = m_u[mode];
		v = m_v[mode];

		m_levelSqr += m_l1[mode] * ( (u+v)*(u+v) + m_l0[mode] * u*v);
	}

	//! Could maybe quieten modes seperately if they are barely excited.
	//! Need to distinguish between impact and contact cases though, because
	//! impacts can be faded from a fairly high level, whereas contacts will
	//! sound poor if repeatedly faded and rewoken: Use 'inContact' flag in resonators.

// setAuxFreqScale( 1.0 + sqrt(lsqr) * 0.00001 );   //!!

	return ( m_levelSqr < m_quietLevelSqr );
}


int
paModalRes :: getTimeCost()
{
	// For now a rough estimate of time cost, on an arbitrary scale.
	return m_nActiveModes*10;
}




