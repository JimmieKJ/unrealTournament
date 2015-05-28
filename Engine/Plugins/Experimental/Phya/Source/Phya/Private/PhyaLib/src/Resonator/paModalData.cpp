//
// paModalData.cpp
//
#include "PhyaPluginPrivatePCH.h"


#include "Resonator/paModalData.hpp"
#include "Resonator/paModalVars.hpp"
#include "System/paMemory.hpp"
#include "Utility/paRnd.hpp"

#include <stdio.h>



paModalData :: paModalData() {

	m_nModes = 1;	// Initially a single test mode.
	m_usingDefaultModes = true;

	//! Allocation of memory for modes should be done after reading number of modes. For now..
	m_freq = paFloatCalloc(paModal::nMaxModes);
	m_damp = paFloatCalloc(paModal::nMaxModes);
	m_amp = paFloatCalloc(paModal::nMaxModes);

	m_freq[0] = 440;	// Handy default.
	m_damp[0] = 1;
	m_amp[0] = 1;

}


paModalData :: ~paModalData() {

	paFree(m_freq);
	paFree(m_damp);
	paFree(m_amp);
}

#if PLATFORM_WINDOWS
#pragma warning (push)
#pragma warning (disable : 4996) // scanf is deprecated, but can't use the safe version as may not be supported on all platforms.
#endif

int
paModalData :: read( const char* data ) {

	int err;
	paFloat freqScale;
	paFloat dampScale;
	paFloat ampScale;

	float f,d,a;

////// Load data and prepare runtime data from it.


	if ( scanf(data, "%f	%f	%f\n", &freqScale, &dampScale, &ampScale) != 3 )
	{
		//fprintf(stderr, "\nBad scaling factors in %s.\n\n", filename_or_data);
		return(-1);
	}

	if (m_usingDefaultModes) {
		m_usingDefaultModes = false;	// Otherwise add new data to existing data.
		m_nModes = 0;
	}

	while( m_nModes < paModal::nMaxModes && (err = scanf(data, "%f %f %f\n", &f, &d, &a)) == 3 ) {
		m_freq[m_nModes] = f * freqScale;
		m_damp[m_nModes] = d * dampScale;
		m_amp[m_nModes] = a * ampScale;

//! Test Risset mode idea.
//m_nModes++;
//m_freq[m_nModes] = m_freq[m_nModes-1] * 1.001;   //+paRnd(int(0), int(50))/10; //  //+ paRnd(paFloat(1.0),paFloat(5.0));
//m_damp[m_nModes] = m_damp[m_nModes-1];
//m_amp[m_nModes] = m_amp[m_nModes-1];

		m_nModes++;
	}

//	if (err == 3 && m_nModes == paModal::nMaxModes)
//		fprintf(stderr, "Total mode limit of %d exceeded: Some modes lost.\n\n", paModal::nMaxModes);

	return(0);
}

#if PLATFORM_WINDOWS
#pragma warning (pop) // scanf is deprecated.
#endif
