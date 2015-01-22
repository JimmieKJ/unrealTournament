//
// paModalRes.hpp
//
// The input block cannot be the same as the output.
//

#if !defined(__paModalRes_hpp)
#define __paModalRes_hpp

#include <math.h>
#include "Resonator/paRes.hpp"
#include "Resonator/paModalData.hpp"



class PHYA_API paModalRes : public paRes
{

private:

	paModalData* m_data;
	int m_nActiveModes;

	//// Runtime coefficients calculated from paModalData.

	paFloat* m_cplus;				// Array of resonator coefficients for different modes.
	paFloat* m_cminus;
	paFloat* m_aa;
	paFloat* m_u;					// End-of-last-block resonator state-variables for each mode,
	paFloat* m_v;					// stored for use at the start of the next block.
	paFloat* m_l0;				// Coefficients used in finding the envelope level of the modes.
	paFloat* m_l1;
	paFloat m_quietLevelSqr;		// Threshold for being quiet.
//	paFloat m_dcBias;				// Used to reduce limit cycles and denormalization.

	int calcCoeffs(void);		// Find runtime coefficients from paModalData and aux adjustments.

	int zero();

public :

	paModalRes();
	paModalRes(paModalData*);
	~paModalRes();

	paBlock* tick();			//  Process 'input', put result in 'm_output'.
	paBlock* tickAdd();			//  Process 'input', add result onto 'm_output'.

	bool isQuiet();

	// User interface :


	int setData(paModalData*);
	int setnActiveModes(int n)
	{
//		paAssert(n <= m_data->getnModes());
		if (!m_data) return -1;
		if (n > m_data->getnModes()) n = m_data->getnModes();
		m_nActiveModes = n;
		return 0;
	};

	paFloat getQuietLevel() const { return sqrtf(m_quietLevelSqr); }
	paFloat getQuietLevelSqrd() const { return m_quietLevelSqr; }
	int setQuietLevel(paFloat l);		// l = 1.0 makes quiet when output is at the level of 1.

	int getTimeCost();

};




#endif
