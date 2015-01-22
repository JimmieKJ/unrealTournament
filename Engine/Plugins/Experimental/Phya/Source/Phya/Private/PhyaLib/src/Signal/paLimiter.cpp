//
//  paLimiter.cpp
//
//  Decent brickwall limiting using lookahead.
//  Use to prevents clipping at output points.
//

#include "PhyaPluginPrivatePCH.h"


//#include <math.h>
#include "Signal/paLimiter.hpp"

#include <stdio.h>

/*
#define REPORTSTART		FILE *fp = fopen("output.txt", "a");
#define REPORT			fprintf(fp, "%f	%f	%f\n", ABS(m_bufferIO[i]), m_threshold/m_gain, m_peak);
#define REPORTSTOP		fclose(fp);
*/

#define REPORTSTART
//#define REPORT
#define REPORTSTOP

#define RATEBOOST 2.0f

#define REPORT  checkFloat(out[i],m_threshold);
void checkFloat(paFloat f, paFloat t)
{
	if (f > t)
	{
//	  	printf("limiter fail\n");
	}
}

paLimiter::paLimiter( paFloat attackTime, paFloat holdTime, paFloat releaseTime )
{
	m_input = 0;		// io blocks, user set.
	m_output = 0;
	
	// Make buffer a multiple of blocks, to make buffer read write simpler.
	m_bufferLen = paBlock::nFrames * (1+ (int)(attackTime * paScene::nFramesPerSecond / paBlock::nFrames));
	m_bufferStart = (paFloat*)paCalloc(m_bufferLen, sizeof(paFloat));
	m_bufferEnd = m_bufferStart + m_bufferLen;
	m_bufferIO = m_bufferStart;		// Initial readout/readin point.

	m_state = paLS_DIRECT;
	m_threshold = 20000;  //32767 //! Reduced to prevent glitching - something not quite right.
	m_peak = 0.0f;
	m_gain = 1.0f;
	m_holdLen = (long)(holdTime * paScene::nFramesPerSecond);		// Cannot be smaller than paBlock::nFrames.
	m_releaseLen = (long)(releaseTime * paScene::nFramesPerSecond);

}

paLimiter::~paLimiter()
{
	paFree(m_bufferStart);
}


#ifndef ABS
#define ABS(a)  ((a)>0?(a):-(a))
#endif

paBlock*
paLimiter::tick()
{

	paFloat* in = m_input->getStart();
	paFloat* out = m_output->getStart();
	paFloat t;
	paFloat gainRate;
	paFloat gainTarget;
	paFloat peak;
	int i;

	if (m_bufferIO + paBlock::nFrames >= m_bufferEnd) 
		m_bufferIO = m_bufferStart;

	REPORTSTART

	for(i=0; i < paBlock::nFrames; i++) {

		switch(m_state)
		{
			case paLS_DIRECT :
				while(i < paBlock::nFrames)
				{
					t = in[i];					// Need temp variable in case output = input.
					out[i] = m_bufferIO[i];
REPORT
					m_bufferIO[i] = t;			// Buffer for next time.


					if ( (peak = ABS(t)) > m_threshold )
					{
						m_state = paLS_ATTACK;
						m_peak = peak;
						m_gain  = 1.0f;
						m_gainTarget = m_threshold/(m_peak+0.5f);	// Ensures when target reached output is at threshold.
						// RATEBOOST* gainRate moves kink a bit earlier.. reduces glitch
						m_gainRate = RATEBOOST*(m_gainTarget - m_gain)/m_bufferLen;
						break;
					}
					i++;
				}
				break;


			case paLS_ATTACK :
				while(i < paBlock::nFrames)
				{
					t = in[i];
					m_gain += m_gainRate;
					out[i] = m_bufferIO[i] * m_gain;
REPORT
/*
if(ABS(out[i]) > m_threshold)
{
	int a=1;
}
*/
					m_bufferIO[i] = t;			// Buffer for next time.


					if (m_gain <= m_gainTarget)		// Target reached.
					{
						m_state = paLS_HOLD;
						m_holdCount = m_holdLen;
						m_peak = m_threshold/m_gain;
						break;
					}


					if ( (peak = ABS(t)) > m_threshold )
					{
						gainTarget = m_threshold/(peak+0.5f);	
						gainRate = RATEBOOST*(gainTarget - m_gain)/m_bufferLen;

						if (gainRate < m_gainRate)		// ie faster decrease neccessary to cover new peak.
						{
							m_gainTarget = gainTarget;
							m_gainRate = gainRate;
							m_peak = peak;
						}
						else if (peak > m_peak)			// Current attack extended to cover new peak.
						{
							m_gainTarget = gainTarget;
							m_peak = peak;
						}
						// Otherwise current attack or the following hold will cover new peak.
					}
					
					i++;
				}
				break;

			case paLS_HOLD :
				while(i < paBlock::nFrames)
				{

					t = in[i];
					out[i] = m_bufferIO[i] * m_gain;
REPORT
					m_bufferIO[i] = t;

					m_holdCount--;

					if ( (peak = ABS(t)) > m_threshold )
					{
						if (peak <= m_peak)			// Extend hold to cover new peak.
						{
							if (m_holdCount < m_bufferLen) 
								m_holdCount = m_bufferLen;
						}
						else
						{
							m_state = paLS_ATTACK;					// Start new attack.
							m_peak = peak;
							m_gainTarget = m_threshold/(m_peak+0.5f);	
							m_gainRate = RATEBOOST*(m_gainTarget - m_gain)/m_bufferLen;
							break;
						}
					}

					if (m_holdCount == 0)
					{
						m_state = paLS_RELEASE;
						m_gainRate = (1.0f - m_gain)/m_releaseLen;
						break;
					}

					i++;
				}
				break;

			case paLS_RELEASE :
				while(i < paBlock::nFrames)
				{
					t = in[i];
					m_gain += m_gainRate;
					out[i] = m_bufferIO[i] * m_gain;

REPORT
					m_bufferIO[i] = t;


					if ( (peak = ABS(t)) > m_threshold )
					{
						gainTarget = m_threshold/(peak+0.5f);
						if ((gainTarget - m_gain)/m_bufferLen < m_gainRate)		// ie peak is not currently covered.
						{
							if (gainTarget > m_gain)			// Start new attack.
							{
								m_state = paLS_ATTACK;
								m_peak = peak;
								m_gainTarget = gainTarget;
								m_gainRate = RATEBOOST*(m_gainTarget - m_gain)/m_bufferLen;
								break;
							}
							else
							{									// Start short hold.
								m_state = paLS_HOLD;
								m_peak = peak;
								m_holdCount = m_bufferLen;
								break;
							}
						}
					}

					
					if (m_gain >= 1.0f)
					{
						m_state = paLS_DIRECT;
						break;
					}

					i++;
				}
				break;
		
		}
	}
	REPORTSTOP

	m_bufferIO += paBlock::nFrames;

	return m_output;
}

