// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DSP/Flanger.h"

namespace Audio
{
	FFlanger::FFlanger()
	{
	}

	FFlanger::~FFlanger()
	{
	}

	void FFlanger::Init(int32 InSampleRate)
	{
		// Call super class init
		FModulatedDelay::Init(InSampleRate);

		// Set delay time values and modulation params for flangers
		MinDelayTimeMsec = 0.0f;
		MaxDelayTimeMsec = 10.0f;
		SetModulationFeedback(0.5f);
		SetWetLevel(0.5f);
	}

	/** 
	* FStereoFlanger
	*/
	FStereoFlanger::FStereoFlanger()
	{
	}

	FStereoFlanger::~FStereoFlanger()
	{
	}

	void FStereoFlanger::Init(int32 InSampleRate)
	{
		Flanger[0].Init(InSampleRate);
		Flanger[1].Init(InSampleRate);

		// Make sure both flangers are in quad phase w/ each other.
		Flanger[0].SetQuadraturePhase(true);
		Flanger[1].SetQuadraturePhase(false);
	}

	void FStereoFlanger::SetLFO(const EWaveTable::Type InLFOType)
	{
		Flanger[0].SetLFO(InLFOType);
		Flanger[1].SetLFO(InLFOType);
	}

	void FStereoFlanger::SetModulationFeedback(const float InModulationFeedback)
	{
		Flanger[0].SetModulationFeedback(InModulationFeedback);
		Flanger[1].SetModulationFeedback(InModulationFeedback);
	}

	void FStereoFlanger::SetModulationMinAndMaxDelayMSec(const float InMinDelayTimeMsec, const float InMaxDelayTimeMsec)
	{
		Flanger[0].SetMinAndMaxDelayMSec(InMinDelayTimeMsec, InMaxDelayTimeMsec);
		Flanger[1].SetMinAndMaxDelayMSec(InMinDelayTimeMsec, InMaxDelayTimeMsec);
	}

	void FStereoFlanger::SetModulationFrequency(const float InModulationFrequency)
	{
		Flanger[0].SetModulationFrequency(InModulationFrequency);
		Flanger[1].SetModulationFrequency(InModulationFrequency);
	}

	void FStereoFlanger::operator()(const float InLeft, const float InRight, float* OutLeft, float* OutRight)
	{
		*OutLeft = Flanger[0](InLeft);
		*OutRight = Flanger[1](InRight);
	}
}
