// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * n-bit integer. @todo: optimize
 * Data is stored as an array of 32-bit words from the least to the most significant
 * Doesn't handle overflows (not a big issue, we can always use a bigger bit count)
 * Minimum sanity checks.
 * Can convert from int64 and back (by truncating the result, this is mostly for testing)
 */
template <int32 NumBits>
class TBigInt
{
	enum
	{
		/** Word size. */
		BitsPerWord = 32,
		NumWords    = NumBits / BitsPerWord
	};

	static_assert(NumBits >= 64, "TBigInt must have at least 64 bits.");

	/** All bits stored as an array of words. */
	uint32 Bits[NumWords];
	
	/**
	 * Makes sure both factors are positive integers and stores their original signs
	 *
	 * @param FactorA first factor
	 * @param SignA sign of the first factor
	 * @param FactorB second factor
	 * @param SignB sign of the second pactor
	 */
	FORCEINLINE static void MakePositiveFactors(TBigInt<NumBits>& FactorA, int32& SignA, TBigInt<NumBits>& FactorB, int32& SignB)
	{
		SignA = FactorA.Sign();
		SignB = FactorB.Sign();
		if (SignA < 0)
		{
			FactorA.Negate();
		}
		if (SignB < 0)
		{
			FactorB.Negate();
		}
	}

	/**
	 * Restores a sign of a result based on the sign of two factors that produced the result.
	 *
	 * @param Result math opration result
	 * @param SignA sign of the first factor
	 * @param SignB sign of the second factor
	 */
	FORCEINLINE static void RestoreSign(TBigInt<NumBits>& Result, int32 SignA, int32 SignB)
	{
		if ((SignA * SignB) < 0)
		{
			Result.Negate();
		}
	}

public:

	/** Sets this integer to 0. */
	FORCEINLINE void Zero()
	{
		FMemory::Memset(Bits, 0, sizeof(Bits));
	}

	/**
	 * Initializes this big int with a 64 bit integer value.
	 *
	 * @param Value The value to set.
	 */
	FORCEINLINE void Set(int64 Value)
	{
		Zero();
		Bits[0] = (Value & 0xffffffff);
		Bits[1] = (Value >> BitsPerWord) & 0xffffffff;
	}

	/** Default constructor. Initializes the number to zero. */
	TBigInt()
	{
		Zero();
	}

	/**
	 * Constructor. Initializes this big int with a 64 bit integer value.
	 *
	 * @param Other The value to set.
	 */
	TBigInt(int64 Other)
	{
		Set(Other);
	}

	/**
	 * Constructor. Initializes this big int with an array of words.
	 */
	explicit TBigInt(const uint32* InBits)
	{
		FMemory::Memcpy(Bits, InBits, sizeof(Bits));
	}

	/**
	 * Constructor. Initializes this big int with a string representing a hex value.
	 */
	explicit TBigInt(const FString& Value)
	{
		Parse(Value);
	}

	/**
	 * Shift left by the specified amount of bits. Does not check if BitCount is valid.
	 *
	 * @param BitCount the number of bits to shift.
	 */
	FORCEINLINE void ShiftLeftInternal(const int32 BitCount)
	{
		checkSlow(BitCount > 0);

		TBigInt<NumBits> Result;
		if (BitCount & (BitsPerWord - 1))
		{
			const int32 LoWordOffset = (BitCount - 1) / BitsPerWord;
			const int32 HiWordOffset = LoWordOffset + 1;
			const int32 LoWordShift  = BitCount - LoWordOffset * BitsPerWord;
			const int32 HiWordShift  = BitsPerWord - LoWordShift;
			Result.Bits[NumWords - 1] |= Bits[NumWords - HiWordOffset] << LoWordShift;
			for (int32 WordIndex = (NumWords - 1) - HiWordOffset; WordIndex >= 0; --WordIndex)
			{
				uint32 Value = Bits[WordIndex];
				Result.Bits[WordIndex + LoWordOffset] |= Value << LoWordShift;
				Result.Bits[WordIndex + HiWordOffset] |= Value >> HiWordShift;
			}
		}
		else
		{
			const int32 ShiftWords = BitCount / BitsPerWord;
			for (int32 WordIndex = NumWords - 1; WordIndex >= ShiftWords; --WordIndex)
			{
				Result.Bits[WordIndex] = Bits[WordIndex - ShiftWords];
			}
		}
		*this = Result;
	}

	/**
	 * Shift right by the specified amount of bits. Does not check if BitCount is valid.
	 *
	 * @param BitCount the number of bits to shift.
	 */
	FORCEINLINE void ShiftRightInternal(const int32 BitCount)
	{
		checkSlow(BitCount > 0);

		TBigInt<NumBits> Result;
		if (BitCount & (BitsPerWord - 1))
		{
			const int32 HiWordOffset = (BitCount - 1) / BitsPerWord;
			const int32 LoWordOffset = HiWordOffset + 1;
			const int32 HiWordShift  = BitCount - HiWordOffset * BitsPerWord;
			const int32 LoWordShift  = BitsPerWord - HiWordShift;
			Result.Bits[0] |= Bits[HiWordOffset] >> HiWordShift;
			for (int32 WordIndex = LoWordOffset; WordIndex < NumWords; ++WordIndex)
			{
				uint32 Value = Bits[WordIndex];
				Result.Bits[WordIndex - HiWordOffset] |= Value >> HiWordShift;
				Result.Bits[WordIndex - LoWordOffset] |= Value << LoWordShift;
			}
		}
		else
		{
			const int32 ShiftWords = BitCount / BitsPerWord;
			for (int32 WordIndex = NumWords - 1; WordIndex >= ShiftWords; --WordIndex)
			{
				Result.Bits[WordIndex - ShiftWords] = Bits[WordIndex];
			}
		}
		*this = Result;
	}

	/**
	 * Adds two integers.
	 */
	FORCEINLINE void Add(const TBigInt<NumBits>& Other)
	{
		int64 CarryOver = 0;
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			int64 WordSum = (int64)Other.Bits[WordIndex] + (int64)Bits[WordIndex] + CarryOver;
			CarryOver = WordSum >> BitsPerWord;
			WordSum &= 0xffffffff;
			Bits[WordIndex] = (uint32)WordSum;
		}
	}

	/**
	 * Subtracts two integers.
	 */
	FORCEINLINE void Subtract(const TBigInt<NumBits>& Other)
	{
		TBigInt<NumBits> NegativeOther(Other);
		NegativeOther.Negate();
		Add(NegativeOther);
	}

	/**
	 * Checks if this integer is negative.
	 */
	FORCEINLINE bool IsNegative() const
	{
		return !!(Bits[NumWords - 1] & (1U << (BitsPerWord - 1)));
	}

	/**
	 * Returns the sign of this integer.
	 */
	FORCEINLINE int32 Sign() const
	{
		return IsNegative() ? -1 : 1;
	}

	/**
	 * Negates this integer. value = -value
	 */
	void Negate()
	{
		static const TBigInt<NumBits> One(1LL);
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			Bits[WordIndex] = ~Bits[WordIndex];
		}
		Add(One);
	}


	/**
	 * Multiplies two integers.
	 */
	void Multiply(const TBigInt<NumBits>& Factor)
	{
		TBigInt<NumBits> Result;
		TBigInt<NumBits> Temp = *this;
		TBigInt<NumBits> Other = Factor;
		
		int32 ThisSign;
		int32 OtherSign;
		MakePositiveFactors(Temp, ThisSign, Other, OtherSign);

		int32 ShiftCount = 0;
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{			
			if (Other.Bits[WordIndex])
			{
				for (int32 BitIndex = 0; BitIndex < BitsPerWord; ++BitIndex)
				{					
					if (!!(Other.Bits[WordIndex] & (1 << BitIndex)))
					{
						if (ShiftCount)
						{
							Temp.ShiftLeftInternal(ShiftCount);
							ShiftCount = 0;
						}
						Result += Temp;
					}
					ShiftCount++;
				}
			}
			else
			{
				ShiftCount += BitsPerWord;
			}
		}
		// Restore the sign if necessary		
		RestoreSign(Result, OtherSign, ThisSign);
		*this = Result;		
	}

	/**
	 * Divides two integers with remainder.
	 */
	void DivideWithRemainder(const TBigInt<NumBits>& Divisor, TBigInt<NumBits>& Remainder) 
	{ 
		static const TBigInt<NumBits> One(1LL);
		TBigInt<NumBits> Denominator(Divisor);
		TBigInt<NumBits> Dividend(*this);		
		
		int32 DenominatorSign;
		int32 DividendSign;
		MakePositiveFactors(Denominator, DenominatorSign, Dividend, DividendSign);

		if (Denominator > Dividend)
		{
			Remainder = *this;
			Zero();
			return;
		}
		if (Denominator == Dividend)
		{
			Remainder.Zero();
			*this = One;
			RestoreSign(*this, DenominatorSign, DividendSign);
			return;
		}

		TBigInt<NumBits> Current(One);
		TBigInt<NumBits> Quotient;

		while (Denominator <= Dividend) 
		{
			Denominator.ShiftLeftInternal(1);	
			Current.ShiftLeftInternal(1);
		}
    Denominator.ShiftRightInternal(1);
    Current.ShiftRightInternal(1);

		while (!Current.IsZero()) 
		{
			if (Dividend >= Denominator) 
			{
				Dividend -= Denominator;
				Quotient |= Current;
			}
			Current.ShiftRightInternal(1);
			Denominator.ShiftRightInternal(1);
		}    
		RestoreSign(Quotient, DenominatorSign, DividendSign);
		Remainder = Dividend;
		*this = Quotient;
	}

	/**
	 * Divides two integers.
	 */
	void Divide(const TBigInt<NumBits>& Divisor) 
	{
		TBigInt<NumBits> Remainder;
		DivideWithRemainder(Divisor, Remainder);
	}

	/**
	 * Performs modulo operation on this integer.
	 */
	void Modulo(const TBigInt<NumBits>& Modulus)
	{
		// a mod b = a - floor(a/b)*b
		check(!IsNegative());
		TBigInt<NumBits> Dividend(*this);
		Dividend.Divide(Modulus);
		Dividend.Multiply(Modulus);
		Subtract(Dividend);
	}

	/**
	 * Calculates square root of this integer.
	 */
	void Sqrt() 
	{
		TBigInt<NumBits> Number(*this);
    TBigInt<NumBits> Result;

    TBigInt<NumBits> Bit(1);
		Bit.ShiftLeftInternal(NumBits - 2);
    while (Bit > Number)
		{
			Bit.ShiftRightInternal(2);
		}
 
		TBigInt<NumBits> Temp;
    while (!Bit.IsZero()) 
		{
			Temp = Result;
			Temp.Add(Bit);
			if (Number >= Temp) 
			{
				Number -= Temp;
				Result.ShiftRightInternal(1);
				Result += Bit;
			}
			else
			{
				Result.ShiftRightInternal(1);
			}
			Bit.ShiftRightInternal(2);
    }
    *this = Result;
	}

	/**
	 * Assignment operator for int64 values.
	 */
	FORCEINLINE TBigInt& operator=(int64 Other)
	{
		Set(Other);
		return *this;
	}

	/**
	 * Returns the index of the highest word that is not zero. -1 if no such word exists.
	 */
	FORCEINLINE int32 GetHighestNonZeroWord() const
	{
		int32 WordIndex;
		for (WordIndex = NumWords - 1; WordIndex >= 0 && Bits[WordIndex] == 0; --WordIndex);
		return WordIndex;
	}

	/**
	 * Returns the index of the highest non-zero bit. -1 if no such bit exists.
	 */
	FORCEINLINE int32 GetHighestNonZeroBit() const
	{
		for (int32 WordIndex = NumWords - 1; WordIndex >= 0; --WordIndex)
		{
			if (!!Bits[WordIndex])
			{
				int32 BitIndex;
				for (BitIndex = BitsPerWord - 1; BitIndex >= 0 && !(Bits[WordIndex] & (1 << BitIndex)); --BitIndex);
				return BitIndex + WordIndex * BitsPerWord;
			}
		}
		return -1;
	}

	/**
	 * Returns a bit value as an integer value (0 or 1).
	 */
	FORCEINLINE int32 GetBit(int32 BitIndex) const
	{
		const int32 WordIndex = BitIndex / BitsPerWord;
		BitIndex -= WordIndex * BitsPerWord;
		return (Bits[WordIndex] & (1 << BitIndex)) ? 1 : 0;
	}

	/**
	 * Sets a bit value.
	 */
	FORCEINLINE void SetBit(int32 BitIndex, int32 Value)
	{
		const int32 WordIndex = BitIndex / BitsPerWord;
		BitIndex -= WordIndex * BitsPerWord;
		if (!!Value)
		{
			Bits[WordIndex] |= (1 << BitIndex);
		}
		else
		{
			Bits[WordIndex] &= ~(1 << BitIndex);
		}
	}

	/**
	 * Shift left by the specified amount of bits.
	 *
	 * @param BitCount the number of bits to shift.
	 */
	void ShiftLeft(const int32 BitCount)
	{
		// Early out in the trivial cases
		if (BitCount == 0)
		{
			return;
		}
		else if (BitCount < 0)
		{
			ShiftRight(-BitCount);
			return;
		}
		else if (BitCount >= NumBits)
		{
			Zero();
			return;
		}
		ShiftLeftInternal(BitCount);
	}

	/**
	 * Shift right by the specified amount of bits.
	 *
	 * @param BitCount the number of bits to shift.
	 */
	void ShiftRight(const int32 BitCount)
	{
		// Early out in the trivial cases
		if (BitCount == 0)
		{
			return;
		}
		else if (BitCount < 0)
		{
			ShiftLeft(-BitCount);
			return;
		}
		else if (BitCount >= NumBits)
		{
			Zero();
			return;
		}
		ShiftRightInternal(BitCount);
	}

	/**
	 * Bitwise 'or'
	 */
	FORCEINLINE void BitwiseOr(const TBigInt<NumBits>& Other)
	{
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			Bits[WordIndex] |= Other.Bits[WordIndex];
		}
	}

	/**
	 * Bitwise 'and'
	 */
	FORCEINLINE void BitwiseAnd(const TBigInt<NumBits>& Other)
	{
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			Bits[WordIndex] &= Other.Bits[WordIndex];
		}
	}

	/**
	 * Bitwise 'not'
	 */
	FORCEINLINE void BitwiseNot()
	{
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			Bits[WordIndex] = ~Bits[WordIndex];
		}
	}

	/**
	 * Checks if two integers are equal.
	 */
	FORCEINLINE bool IsEqual(const TBigInt<NumBits>& Other) const
	{
		for (int32 WordIndex = 0; WordIndex < NumWords; ++WordIndex)
		{
			if (Bits[WordIndex] != Other.Bits[WordIndex])
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * this < Other
	 */
	bool IsLess(const TBigInt<NumBits>& Other) const
	{
		if (IsNegative())
		{
			if (!Other.IsNegative())
			{
				return true;
			}
			else
			{
				return IsGreater(Other);
			}
		}
		int32 WordIndex;
		for (WordIndex = NumWords - 1; WordIndex >= 0 && Other.Bits[WordIndex] == Bits[WordIndex]; --WordIndex);
		return WordIndex >= 0 && Bits[WordIndex] < Other.Bits[WordIndex];
	}

	/**
	 * this <= Other
	 */
	bool IsLessOrEqual(const TBigInt<NumBits>& Other) const
	{
		if (IsNegative())
		{
			if (!Other.IsNegative())
			{
				return true;
			}
			else
			{
				return IsGreaterOrEqual(Other);
			}
		}
		int32 WordIndex;
		for (WordIndex = NumWords - 1; WordIndex >= 0 && Other.Bits[WordIndex] == Bits[WordIndex]; --WordIndex);
		return WordIndex < 0 || Bits[WordIndex] < Other.Bits[WordIndex];
	}

	/**
	 * this > Other
	 */
	bool IsGreater(const TBigInt<NumBits>& Other) const
	{
		if (IsNegative())
		{
			if (!Other.IsNegative())
			{
				return false;
			}
			else
			{
				return IsLess(Other);
			}
		}
		int32 WordIndex;
		for (WordIndex = NumWords - 1; WordIndex >= 0 && Other.Bits[WordIndex] == Bits[WordIndex]; --WordIndex);
		return WordIndex >= 0 && Bits[WordIndex] > Other.Bits[WordIndex];
	}

	/**
	 * this >= Other
	 */
	bool IsGreaterOrEqual(const TBigInt<NumBits>& Other) const
	{
		if (IsNegative())
		{
			if (Other.IsNegative())
			{
				return false;
			}
			else
			{
				return IsLessOrEqual(Other);
			}
		}
		int32 WordIndex;
		for (WordIndex = NumWords - 1; WordIndex >= 0 && Other.Bits[WordIndex] == Bits[WordIndex]; --WordIndex);
		return WordIndex < 0 || Bits[WordIndex] > Other.Bits[WordIndex];
	}

	/**
	 * this == 0
	 */
	FORCEINLINE bool IsZero() const
	{
		int32 WordIndex;
		for (WordIndex = NumWords - 1; WordIndex >= 0 && !Bits[WordIndex]; --WordIndex);
		return WordIndex < 0;
	}

	/**
	 * this > 0
	 */
	FORCEINLINE bool IsGreaterThanZero() const
	{
		return !IsNegative() && !IsZero();
	}

	/**
	 * this < 0
	 */
	FORCEINLINE bool IsLessThanZero() const
	{
		return IsNegative() && !IsZero();
	}

	/**
	 * Bit indexing operator
	 */
	FORCEINLINE bool operator[](int32 BitIndex) const
	{
		return GetBit(BitIndex);
	}

	// Begin operator overloads
	FORCEINLINE TBigInt<NumBits> operator>>(int32 Count) const
	{
		TBigInt<NumBits> Result = *this;
		Result.ShiftRight(Count);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator>>=(int32 Count)
	{
		ShiftRight(Count);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator<<(int32 Count) const
	{
		TBigInt<NumBits> Result = *this;
		Result.ShiftLeft(Count);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator<<=(int32 Count)
	{
		ShiftLeft(Count);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator+(const TBigInt<NumBits>& Other) const
	{
		TBigInt<NumBits> Result(*this);
		Result.Add(Other);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator++()
	{
		static const TBigInt<NumBits> One(1);
		Add(One);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits>& operator+=(const TBigInt<NumBits>& Other)
	{
		Add(Other);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator-(const TBigInt<NumBits>& Other) const
	{
		TBigInt<NumBits> Result(*this);
		Result.Subtract(Other);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator--()
	{
		static const TBigInt<NumBits> One(1);
		Subtract(One);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits>& operator-=(const TBigInt<NumBits>& Other)
	{
		Subtract(Other);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator*(const TBigInt<NumBits>& Other) const
	{
		TBigInt<NumBits> Result(*this);
		Result.Multiply(Other);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator*=(const TBigInt<NumBits>& Other)
	{
		Multiply(Other);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator/(const TBigInt<NumBits>& Divider) const
	{
		TBigInt<NumBits> Result(*this);
		Result.Divide(Divider);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator/=(const TBigInt<NumBits>& Divider)
	{
		Divide(Divider);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator%(const TBigInt<NumBits>& Modulus) const
	{
		TBigInt<NumBits> Result(*this);
		Result.Modulo(Modulus);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator%=(const TBigInt<NumBits>& Modulus)
	{
		Modulo(Modulus);
		return *this;
	}

	FORCEINLINE bool operator<(const TBigInt<NumBits>& Other) const
	{
		return IsLess(Other);
	}

	FORCEINLINE bool operator<=(const TBigInt<NumBits>& Other) const
	{
		return IsLessOrEqual(Other);
	}

	FORCEINLINE bool operator>(const TBigInt<NumBits>& Other) const
	{
		return IsGreater(Other);
	}

	FORCEINLINE bool operator>=(const TBigInt<NumBits>& Other) const
	{
		return IsGreaterOrEqual(Other);
	}

	FORCEINLINE bool operator==(const TBigInt<NumBits>& Other) const
	{
		return IsEqual(Other);
	}

	FORCEINLINE bool operator!=(const TBigInt<NumBits>& Other) const
	{
		return !IsEqual(Other);
	}

	FORCEINLINE TBigInt<NumBits> operator&(const TBigInt<NumBits>& Other) const
	{
		TBigInt<NumBits> Result(*this);
		Result.BitwiseAnd(Other);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator&=(const TBigInt<NumBits>& Other)
	{
		BitwiseAnd(Other);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator|(const TBigInt<NumBits>& Other) const
	{
		TBigInt<NumBits> Result(*this);
		Result.BitwiseOr(Other);
		return Result;
	}

	FORCEINLINE TBigInt<NumBits>& operator|=(const TBigInt<NumBits>& Other)
	{
		BitwiseOr(Other);
		return *this;
	}

	FORCEINLINE TBigInt<NumBits> operator~() const
	{
		TBigInt<NumBits> Result(*this);
		Result.BitwiseNot();
		return Result;
	}
	// End operator overloads

	/**
	 * Returns the value of this big int as a 64-bit integer. If the value is greater, the higher bits are truncated.
	 */
	int64 ToInt() const
	{
		int64 Result;
		if (!IsNegative())
		{
			Result = (int64)Bits[0] + ((int64)Bits[1] << BitsPerWord);
		}
		else
		{
			TBigInt<NumBits> Positive(*this);
			Positive.Negate();
			Result = (int64)Positive.Bits[0] + ((int64)Positive.Bits[1] << BitsPerWord);
		}
		return (int64)Bits[0] + ((int64)Bits[1] << BitsPerWord);
	}

	/**
	 * Returns this big int as a string.
	 */
	FString ToString() const
	{
		FString Text(TEXT("0x"));
		int32 WordIndex;
		for (WordIndex = NumWords - 1; WordIndex > 0; --WordIndex)
		{
			if (Bits[WordIndex])
			{
				break;
			}
		}
		for (; WordIndex >= 0; --WordIndex)
		{
			Text += FString::Printf(TEXT("%08x"), Bits[WordIndex]);
		}
		return Text;
	}

	/**
	 * Parses a string representing a hex value
	 */
	void Parse(const FString& Value)
	{
		check(Value.Len() <= NumBits / 8);
		Zero();
		int32 ValueStartIndex = 0;
		if (Value.Len() >= 2 && Value[0] == '0' && FChar::ToUpper(Value[1]) == 'X')
		{
			ValueStartIndex = 2;
		}
		const int32 BytesPerWord = BitsPerWord / 8;
		for (int32 CharIndex = Value.Len() - 1; CharIndex >= ValueStartIndex; --CharIndex)
		{
			const TCHAR ByteChar = Value[CharIndex];
			const int32 ByteIndex = (Value.Len() - CharIndex - 1);
			const int32 WordIndex = ByteIndex / BytesPerWord;			
			const int32 ByteValue = ByteChar > '9' ? (FChar::ToUpper(ByteChar) - 'A' + 10) : (ByteChar - '0');
			check(ByteValue >= 0 && ByteValue <= 15);
			Bits[WordIndex] |= (ByteValue << (ByteIndex - WordIndex * 4) * 4);
		}
	}

	/**
	 * Serialization.
	 */
	friend FArchive& operator<<(FArchive& Ar, TBigInt<NumBits>& Value)
	{
		for (int32 Index = 0; Index < NumWords; ++Index)
		{
			Ar << Value.Bits[Index];
		}
		return Ar;
	}
};

// Predefined big int types
typedef TBigInt<256> int256;
typedef TBigInt<512> int512;

/**
 * Encyption key - exponent and modulus pair
 */
template <typename IntType>
struct TEncryptionKey
{
	IntType Exponent;
	IntType Modulus;
};
typedef TEncryptionKey<int256> FEncryptionKey;

/**
 * Math utils for encryption.
 */
struct FEncryption
{
	/**
	 * Greatest common divisor of ValueA and ValueB.
	 */
	template <typename IntType>
	static IntType CalculateGCD(IntType ValueA, IntType ValueB)
	{ 
		// Early out in obvious cases
		if (ValueA == 0)
		{
			return ValueA;
		}
		if (ValueB == 0) 
		{
			return ValueB;
		}
		
		// Shift is log(n) where n is the greatest power of 2 dividing both A and B.
		int32 Shift;
		for (Shift = 0; ((ValueA | ValueB) & 1) == 0; ++Shift) 
		{
			ValueA >>= 1;
			ValueB >>= 1;
		}
 
		while ((ValueA & 1) == 0)
		{
			ValueA >>= 1;
		}
 
		do 
		{
			// Remove all factors of 2 in B.
			while ((ValueB & 1) == 0)
			{
				ValueB >>= 1;
			}
 
			// Make sure A <= B
			if (ValueA > ValueB) 
			{
				Swap(ValueA, ValueB);
			}
			ValueB = ValueB - ValueA;
		} 
		while (ValueB != 0);
 
		// Restore common factors of 2.
		return ValueA << Shift;
	}

	/** 
	 * Multiplicative inverse of exponent using extended GCD algorithm.
	 *
	 * Extended gcd: ax + by = gcd(a, b), where a = exponent, b = fi(n), gcd(a, b) = gcd(e, fi(n)) = 1, fi(n) is the Euler's totient function of n
	 * We only care to find d = x, which is our multiplicatve inverse of e (a).
	 */
	template <typename IntType>
	static IntType CalculateMultiplicativeInverseOfExponent(IntType Exponent, IntType Totient)
	{
		IntType x0 = 1;
		IntType y0 = 0;
		IntType x1 = 0;
		IntType y1 = 1;
		IntType a0 = Exponent;
		IntType b0 = Totient;
		while (b0 != 0)
		{
			// Quotient = Exponent / Totient
			IntType Quotient = a0 / b0;

			// (Exponent, Totient) = (Totient, Exponent mod Totient)
			IntType b1 = a0 % b0;
			a0 = b0;
			b0 = b1;

			// (x, lastx) = (lastx - Quotient*x, x)
			IntType x2 = x0 - Quotient * x1;
			x0 = x1;
			x1 = x2;

			// (y, lasty) = (lasty - Quotient*y, y)
			IntType y2 = y0 - Quotient * y1;
			y0 = y1;
			y1 = y2;
		}
		// If x0 is negative, find the corresponding positive element in (mod Totient)
		while (x0 < 0)
		{
			x0 += Totient;
		}
		return x0;
	}

	/**
	 * Generate Key Pair for encryption and decryption.
	 */
	template <typename IntType>
	static void GenerateKeyPair(const IntType& P, const IntType& Q, FEncryptionKey& PublicKey, FEncryptionKey& PrivateKey)
	{
		const IntType Modulus = P * Q;
		const IntType Fi = (P - 1) * (Q - 1);

		IntType EncodeExponent = Fi;
		IntType DecodeExponent = 0;
		do
		{
			for ( --EncodeExponent; EncodeExponent > 1 && CalculateGCD(EncodeExponent, Fi) > 1; --EncodeExponent);
			DecodeExponent = CalculateMultiplicativeInverseOfExponent(EncodeExponent, Fi);
		}
		while (DecodeExponent == EncodeExponent);

		PublicKey.Exponent = DecodeExponent;
		PublicKey.Modulus = Modulus;

		PrivateKey.Exponent = EncodeExponent;
		PrivateKey.Modulus = Modulus;
	}

	/** 
	 * Raise Base to power of Exponent in mod Modulus.
	 */
	template <typename IntType>
	FORCEINLINE static IntType ModularPow(IntType Base, IntType Exponent, IntType Modulus)
	{
		const IntType One(1);
		IntType Result(One);
		while (Exponent > 0)
		{
			if ((Exponent & One) != 0)
			{
				Result = (Result * Base) % Modulus;
			}
			Exponent >>= 1;
			Base = (Base * Base) % Modulus;
		}
		return Result;
	}
};

/**
 * Specialization for int256 (performance). Avoids using temporary results and most of the operations are inplace.
 */
template <>
FORCEINLINE int256 FEncryption::ModularPow(int256 Base, int256 Exponent, int256 Modulus)
{
	static const int256 One(1);
	int256 Result(One);
	while (Exponent.IsGreaterThanZero())
	{
		if (!(Exponent & One).IsZero())
		{
			Result.Multiply(Base);
			Result.Modulo(Modulus);
		}
		Exponent.ShiftRightInternal(1);
		Base.Multiply(Base);
		Base.Modulo(Modulus);
	}
	return Result;
}

struct FSignature
{
	int256 Data[20];

	void Serialize(FArchive& Ar)
	{
		for (int32 Index = 0; Index < ARRAY_COUNT(Data); ++Index)
		{
			Ar << Data[Index];
		}
	}

	static int64 Size()
	{
		return sizeof(int256) * 20;
	}
};