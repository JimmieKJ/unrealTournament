// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	/** The various types of wave tables */
	namespace EWaveTable
	{
		enum Type
		{
			SineWaveTable,
			SawWaveTable,
			TriangleWaveTable,
			SquareWaveTable,
			BandLimitedSawWaveTable,
			BandLimitedTriangleWaveTable,
			BandLimitedSquareWaveTable,
			UserCreated
		};
	}


	/** Base class for a wave table oscillator. */
	class FWaveTableOsc
	{
	public:
		FWaveTableOsc();
		virtual ~FWaveTableOsc();

		/** Initializes the wave table oscillator */
		void Init(const int32 InSampleRate, const int32 InTableSize = 1024);

		/** Set the frequency of the oscillator in hertz */
		void SetFrequency(const float InFrequencyHz);

		/** Sets the polarity of the wave table oscillator. True makes it unipolar. False makes it bipolar. */
		void SetPolarity(const bool bInIsUnipolar);

		/** Function which returns the current in-phase and quad phase sample values. */
		void operator()(float* OutSample, float* OutQuadSample);

		/** Returns the type of the wave table this is. */
		virtual EWaveTable::Type GetType() const = 0;

	protected:
		/** Function to generate the wave table. */
		virtual void Generate(const int32 InTableSize) = 0;

		/** Audio for the wave table. */
		TArray<float> WaveTable;

	private:

		/** Sample rate of the oscillator. */
		int32 SampleRate;

		/** Read index of the wave table oscillator */
		float ReadIndex;

		/** Quadrature read index of the wave table oscillator */
		float QuadPhaseReadIndex;

		/** The amount to increment to the read indices whenever read from the oscillator */
		float ReadDelta;

		/** What polarity the wave table generator works in. */
		bool bIsUnipolar;
	};

	class FSineWaveTable : public FWaveTableOsc
	{
	public:
		EWaveTable::Type GetType() const { return EWaveTable::SineWaveTable; }

	protected:
		virtual void Generate(const int32 InTableSize) override;
		
	};

	class FSawWaveTable : public FWaveTableOsc
	{
	public:
		EWaveTable::Type GetType() const { return EWaveTable::SawWaveTable; }

	protected:
		virtual void Generate(const int32 InTableSize) override;
	};

	class FBandLimitedSawWaveTable : public FWaveTableOsc
	{
	public:
		EWaveTable::Type GetType() const { return EWaveTable::BandLimitedSawWaveTable; }

	protected:
		virtual void Generate(const int32 InTableSize) override;
	};

	class FTriangleWaveTable : public FWaveTableOsc
	{
	public:
		EWaveTable::Type GetType() const { return EWaveTable::TriangleWaveTable; }

	protected:
		virtual void Generate(const int32 InTableSize) override;
	};

	class FBandLimitedTriangleWaveTable : public FWaveTableOsc
	{
	public:
		EWaveTable::Type GetType() const { return EWaveTable::BandLimitedTriangleWaveTable; }

	protected:
		virtual void Generate(const int32 InTableSize) override;
	};

	class FSquareWaveTable : public FWaveTableOsc
	{
	public:
		EWaveTable::Type GetType() const { return EWaveTable::SquareWaveTable; }

	protected:
		virtual void Generate(const int32 InTableSize) override;
	};

	class FBandLimitedSquareWaveTable : public FWaveTableOsc
	{
	public:
		EWaveTable::Type GetType() const { return EWaveTable::BandLimitedSquareWaveTable; }

	protected:
		virtual void Generate(const int32 InTableSize) override;
	};

}
