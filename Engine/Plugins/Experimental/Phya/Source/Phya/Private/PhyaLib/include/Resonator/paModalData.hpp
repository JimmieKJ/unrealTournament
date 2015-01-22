//
// paModalData.hpp
//
// Contains modal data neccessary for paModalRes operation.
// Can be shared between several resonators. 
//

#if !defined(__paModalData_hpp)
#define __paModalData_hpp

#include "Scene/paAudio.hpp"

struct paModalDataEntry
{
	paFloat m_freq;
	paFloat m_damp;
	paFloat m_amp;
};

class PHYA_API paModalData
{

private:

	bool m_usingDefaultModes;	// Used to ensure default gets wiped when new modes loaded.
	int m_nModes;


public:
	
	paFloat* m_freq;
	paFloat* m_damp;
	paFloat* m_amp;

	paModalData();
	~paModalData();
	
	int read( const char* data );
	int getnModes() { return m_nModes; };

};




#endif

