//
// paBiquad.hpp
//
// Biquad filter, 2 pole, 2 zero.
// Non-interpolating, yet.
// Note we can have the input and output pointing to the same paBlock
//


#if !defined (__paBiquad_hpp)
#define __paBiquad_hpp


#include "Signal/paBlock.hpp"



class PHYA_API paBiquad
{
private:

	paBlock* m_input;
	paBlock* m_output;

	// Buffered 'set' variables, for thread-safe operation, and efficiency.

	paFloat m_a0;		// Filter coefficients.
	paFloat m_a1;
	paFloat m_a2;
	paFloat m_b1;
	paFloat m_b2;

	paFloat m_x1;		// Last inputs and outputs.
	paFloat m_x2;
	paFloat m_y1;
	paFloat m_y2;

public:
	paBiquad();
	~paBiquad();

	void setInput(paBlock* input) { m_input = input; }
	void setOutput(paBlock* output) { m_output = output; }  // Output can be set to input.
	void setIO(paBlock* buffer) { m_input = buffer; m_output = buffer; }

	void setCoeffs(paFloat a0, paFloat a1, paFloat a2, paFloat b1, paFloat b2) {
		m_a0 = a0;
		m_a1 = a1;
		m_a2 = a2;
		m_b1 = b1;
		m_b2 = b2;
	}

	void reset() {
		m_x1 = 0.0f;  // Reset internal filter state!!
		m_x2 = 0.0f;
		m_y1 = 0.0f;
		m_y2 = 0.0f;
	};


	// Filter one paBlock.
	paBlock* tick();
	paBlock* tick( paBlock* input, paBlock* output) {
		m_input = input;
		m_output = output;
		return tick();
	}

	paBlock* getOutput() { return m_output; };
};



#endif


