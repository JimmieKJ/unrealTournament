// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnBits.h: Unreal bitstream manipulation classes.
=============================================================================*/

#include "CorePrivatePCH.h" 

// Table.
static const uint8 GShift[8]={0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
static const uint8 GMask [8]={0x00,0x01,0x03,0x07,0x0f,0x1f,0x3f,0x7f};

// Optimized arbitrary bit range memory copy routine.
void appBitsCpy( uint8* Dest, int32 DestBit, uint8* Src, int32 SrcBit, int32 BitCount )
{
	if( BitCount==0 ) return;
	
	// Special case - always at least one bit to copy,
	// a maximum of 2 bytes to read, 2 to write - only touch bytes that are actually used.
	if( BitCount <= 8 ) 
	{
		uint32 DestIndex	   = DestBit/8;
		uint32 SrcIndex	   = SrcBit /8;
		uint32 LastDest	   =( DestBit+BitCount-1 )/8;  
		uint32 LastSrc	   =( SrcBit +BitCount-1 )/8;  
		uint32 ShiftSrc     = SrcBit & 7; 
		uint32 ShiftDest    = DestBit & 7;
		uint32 FirstMask    = 0xFF << ShiftDest;  
		uint32 LastMask     = 0xFE << ((DestBit + BitCount-1) & 7) ; // Pre-shifted left by 1.	
		uint32 Accu;		

		if( SrcIndex == LastSrc )
			Accu = (Src[SrcIndex] >> ShiftSrc); 
		else
			Accu =( (Src[SrcIndex] >> ShiftSrc) | (Src[LastSrc ] << (8-ShiftSrc)) );			

		if( DestIndex == LastDest )
		{
			uint32 MultiMask = FirstMask & ~LastMask;
			Dest[DestIndex] = ( ( Dest[DestIndex] & ~MultiMask ) | ((Accu << ShiftDest) & MultiMask) );		
		}
		else
		{		
			Dest[DestIndex] = (uint8)( ( Dest[DestIndex] & ~FirstMask ) | (( Accu << ShiftDest) & FirstMask) ) ;
			Dest[LastDest ] = (uint8)( ( Dest[LastDest ] & LastMask  )  | (( Accu >> (8-ShiftDest)) & ~LastMask) ) ;
		}

		return;
	}

	// Main copier, uses byte sized shifting. Minimum size is 9 bits, so at least 2 reads and 2 writes.
	uint32 DestIndex		= DestBit/8;
	uint32 FirstSrcMask  = 0xFF << ( DestBit & 7);  
	uint32 LastDest		= ( DestBit+BitCount )/8; 
	uint32 LastSrcMask   = 0xFF << ((DestBit + BitCount) & 7); 
	uint32 SrcIndex		= SrcBit/8;
	uint32 LastSrc		= ( SrcBit+BitCount )/8;  
	int32   ShiftCount    = (DestBit & 7) - (SrcBit & 7); 
	int32   DestLoop      = LastDest-DestIndex; 
	int32   SrcLoop       = LastSrc -SrcIndex;  
	uint32 FullLoop;
	uint32 BitAccu;

	// Lead-in needs to read 1 or 2 source bytes depending on alignment.
	if( ShiftCount>=0 )
	{
		FullLoop  = FMath::Max(DestLoop, SrcLoop);  
		BitAccu   = Src[SrcIndex] << ShiftCount; 
		ShiftCount += 8; //prepare for the inner loop.
	}
	else
	{
		ShiftCount +=8; // turn shifts -7..-1 into +1..+7
		FullLoop  = FMath::Max(DestLoop, SrcLoop-1);  
		BitAccu   = Src[SrcIndex] << ShiftCount; 
		SrcIndex++;		
		ShiftCount += 8; // Prepare for inner loop.  
		BitAccu = ( ( (uint32)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; 
	}

	// Lead-in - first copy.
	Dest[DestIndex] = (uint8) (( BitAccu & FirstSrcMask) | ( Dest[DestIndex] &  ~FirstSrcMask ) );
	SrcIndex++;
	DestIndex++;

	// Fast inner loop. 
	for(; FullLoop>1; FullLoop--) 
	{   // ShiftCount ranges from 8 to 15 - all reads are relevant.
		BitAccu = (( (uint32)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; // Copy in the new, discard the old.
		SrcIndex++;
		Dest[DestIndex] = (uint8) BitAccu;  // Copy low 8 bits.
		DestIndex++;		
	}

	// Lead-out. 
	if( LastSrcMask != 0xFF) 
	{
		if ((uint32)(SrcBit+BitCount-1)/8 == SrcIndex ) // Last legal byte ?
		{
			BitAccu = ( ( (uint32)Src[SrcIndex] << ShiftCount ) + (BitAccu)) >> 8; 
		}
		else
		{
			BitAccu = BitAccu >> 8; 
		}		

		Dest[DestIndex] = (uint8)( ( Dest[DestIndex] & LastSrcMask ) | (BitAccu & ~LastSrcMask) );  		
	}	
}

/*-----------------------------------------------------------------------------
	FBitWriter.
-----------------------------------------------------------------------------*/

/**
 * Constructor using known size the buffer needs to be
 */
FBitWriter::FBitWriter( int64 InMaxBits, bool InAllowResize /*=false*/ )
:   Num			( 0 )
,	Max			( InMaxBits )
{
	Buffer.AddUninitialized( (InMaxBits+7)>>3 );

	AllowResize = InAllowResize;
	FMemory::Memzero(Buffer.GetData(), Buffer.Num());
	ArIsPersistent = ArIsSaving = 1;
	ArNetVer |= 0x80000000;
}

/**
 * Default constructor. Zeros everything.
 */
FBitWriter::FBitWriter(void) : Num(0), Max(0), AllowResize(false)
{
}

/**
 * Resets the bit writer back to its initial state
 */
void FBitWriter::Reset(void)
{
	FArchive::Reset();
	Num = 0;
	FMemory::Memzero(Buffer.GetData(), Buffer.Num());
	ArIsPersistent = ArIsSaving = 1;
	ArNetVer |= 0x80000000;
}

void FBitWriter::SerializeBits( void* Src, int64 LengthBits )
{
	if( AllowAppend(LengthBits) )
	{
		//for( int32 i=0; i<LengthBits; i++,Num++ )
		//	if( ((uint8*)Src)[i>>3] & GShift[i&7] )
		//		Buffer(Num>>3) |= GShift[Num&7];
		if( LengthBits == 1 )
		{
			if( ((uint8*)Src)[0] & 0x01 )
				Buffer[Num>>3] |= GShift[Num&7];
			Num++;
		}
		else
		{
			appBitsCpy(Buffer.GetData(), Num, (uint8*)Src, 0, LengthBits);
			Num += LengthBits;
		}
	}
	else ArIsError = 1;
}
void FBitWriter::Serialize( void* Src, int64 LengthBytes )
{
	//warning: Copied and pasted from FBitWriter::SerializeBits
	int64 LengthBits = LengthBytes*8;
	if( AllowAppend(LengthBits) )
	{
		appBitsCpy(Buffer.GetData(), Num, (uint8*)Src, 0, LengthBits);
		Num += LengthBits;
	}
	else ArIsError = 1;
}
void FBitWriter::SerializeInt( uint32& Value, uint32 ValueMax )
{
	uint32 WriteValue = Value;
	if (WriteValue >= ValueMax)
	{
		UE_LOG(LogSerialization, Error, TEXT("FBitWriter::SerializeInt(): Value out of bounds (Value: %u, ValueMax: %u)"), WriteValue, ValueMax);
		ensureMsgf(false, TEXT("FBitWriter::SerializeInt(): Value out of bounds (Value: %u, ValueMax: %u)"), WriteValue, ValueMax);
		WriteValue = ValueMax-1;
	}
	if (AllowAppend((int32)FMath::CeilLogTwo(ValueMax)))
	{
		uint32 NewValue=0;
		int64 LocalNum = Num;	// Use local var to avoid LHS
		for( uint32 Mask=1; NewValue+Mask<ValueMax && Mask; Mask*=2,LocalNum++ )
		{
			if( WriteValue&Mask )
			{
				Buffer[LocalNum>>3] += GShift[LocalNum&7];
				NewValue += Mask;
			}
		}
		Num = LocalNum;
	} else ArIsError = 1;
}
/** serializes the specified value, but does not bounds check against Max; instead, it will wrap around if the value exceeds Max
 * (this differs from SerializeInt(), which clamps)
 * @param Result - the value to serialize
 * @param Max - maximum value to write; wrap Result if it exceeds this
 */
void FBitWriter::WriteIntWrapped(uint32 Value, uint32 ValueMax)
{
	if( AllowAppend((int32)FMath::CeilLogTwo(ValueMax)) )
	{
		uint32 NewValue=0;
		for( uint32 Mask=1; NewValue+Mask<ValueMax && Mask; Mask*=2,Num++ )
		{
			if( Value&Mask )
			{
				Buffer[Num>>3] += GShift[Num&7];
				NewValue += Mask;
			}
		}
	} else ArIsError = 1;
}
void FBitWriter::WriteBit( uint8 In )
{
	if( AllowAppend(1) )
	{
		if( In )
			Buffer[Num>>3] |= GShift[Num&7];
		Num++;
	}
	else ArIsError = 1;
}

/*-----------------------------------------------------------------------------
	FBitWriterMark.
-----------------------------------------------------------------------------*/

void FBitWriterMark::Pop( FBitWriter& Writer )
{
	checkSlow(Num<=Writer.Num);
	checkSlow(Num<=Writer.Max);

	if( Num&7 )
	{
		Writer.Buffer[Num>>3] &= GMask[Num&7];
	}
	int32 Start = (Num       +7)>>3;
	int32 End   = (Writer.Num+7)>>3;
	if( End != Start )
	{
		checkSlow(Start<Writer.Buffer.Num());
		checkSlow(End<=Writer.Buffer.Num());
		FMemory::Memzero( &Writer.Buffer[Start], End-Start );
	}
	Writer.ArIsError = Overflowed;
	Writer.Num       = Num;
}
/** Copies the last section into a buffer. Does not clear the FBitWriter like ::Pop does */
void FBitWriterMark::Copy( FBitWriter& Writer, TArray<uint8> &Buffer )
{
	checkSlow(Num<=Writer.Num);
	checkSlow(Num<=Writer.Max);

	int32 Bytes = (Writer.Num - Num + 7) >> 3;
	if( Bytes > 0 )
	{
		Buffer.SetNumUninitialized(Bytes);		// This makes room but doesnt zero
		Buffer[Bytes-1] = 0;	// Make sure the last byte is 0 out, because appBitsCpy wont touch the last bits
		appBitsCpy(Buffer.GetData(), 0, Writer.Buffer.GetData(), Num, Writer.Num - Num);
	}
}

/** Pops the BitWriter back to the start but doesn't clear what was written. */
void FBitWriterMark::PopWithoutClear( FBitWriter& Writer )
{
	Writer.Num = Num;
}

/*-----------------------------------------------------------------------------
	FBitReader.
-----------------------------------------------------------------------------*/

//
// Reads bitstreams.
//
FBitReader::FBitReader( uint8* Src, int64 CountBits )
:   Num			( CountBits )
,	Pos			( 0 )
{
	Buffer.AddUninitialized( (CountBits+7)>>3 );
	ArIsPersistent = ArIsLoading = 1;
	ArNetVer |= 0x80000000;
	if( Src )
	{
		FMemory::Memcpy(Buffer.GetData(), Src, (CountBits + 7) >> 3);
	}
}
void FBitReader::SetData( FBitReader& Src, int64 CountBits )
{
	Num        = CountBits;
	Pos        = 0;
	ArIsError  = 0;
	Buffer.Empty();
	Buffer.AddUninitialized( (CountBits+7)>>3 );
	Src.SerializeBits(Buffer.GetData(), CountBits);
}
/** This appends data from another BitReader. It checks that this bit reader is byte-aligned so it can just do a TArray::Append instead of a bitcopy.
 *	It is intended to be used by performance minded code that wants to ensure an appBitCpy is avoided.
 */
void FBitReader::AppendDataFromChecked( FBitReader& Src )
{
	check(Num % 8 == 0);
	Src.AppendTo(Buffer);
	Num += Src.GetNumBits();
}
void FBitReader::AppendDataFromChecked( uint8* Src, uint32 NumBits )
{
	check(Num % 8 == 0);
	uint32 NumBytes = (NumBits+7) >> 3;
	Buffer.AddUninitialized(NumBytes);
	FMemory::Memcpy( &Buffer[Num >> 3], Src, NumBytes );
	Num += NumBits;
}

void FBitReader::AppendTo( TArray<uint8> &DestBuffer )
{
	DestBuffer.Append(Buffer);
}
void FBitReader::EatByteAlign()
{
	// Skip over remaining bits in current byte
	Pos = (Pos+7) & (~0x07);
	if ( Pos > Num )
	{
		UE_LOG( LogNetSerialization, Error, TEXT( "FBitReader::EatByteAlign: Pos > Num" ) );
		SetOverflowed();
	}
}
void FBitReader::SerializeBits( void* Dest, int64 LengthBits )
{
	FMemory::Memzero( Dest, (LengthBits+7)>>3 );

	if ( IsError() )
	{
		return;
	}

	if( Pos+LengthBits<=Num )
	{
		//for( int32 i=0; i<LengthBits; i++,Pos++ )
		//	if( Buffer(Pos>>3) & GShift[Pos&7] )
		//		((uint8*)Dest)[i>>3] |= GShift[i&7];
		if( LengthBits == 1 )
		{
			if( Buffer[Pos>>3] & GShift[Pos&7] )
				((uint8*)Dest)[0] |= 0x01;
			Pos++;
		}
		else
		{
			appBitsCpy((uint8*)Dest, 0, Buffer.GetData(), Pos, LengthBits);
			Pos += LengthBits;
		}
	}
	else 
	{
		SetOverflowed();
		UE_LOG( LogNetSerialization, Error, TEXT( "FBitReader::SerializeBits: Pos + LengthBits > Num" ) );
	}
}
void FBitReader::SerializeInt( uint32& OutValue, uint32 ValueMax )
{
	if ( IsError() )
	{
		return;
	}

	uint32 Value=0; // Use local variable to avoid Load-Hit-Store
	int64 LocalPos = Pos;
	const int64 LocalNum = Num;

	for( uint32 Mask=1; Value+Mask<ValueMax && Mask; Mask*=2,LocalPos++ )
	{
		if( LocalPos>=LocalNum )
		{
			SetOverflowed();
			UE_LOG( LogNetSerialization, Error, TEXT( "FBitReader::SerializeInt: LocalPos >= LocalNum" ) );
			break;
		}
		if( Buffer[LocalPos>>3] & GShift[LocalPos&7] )
		{
			Value |= Mask;
		}
	}
	// Now write back
	Pos = LocalPos;
	OutValue = Value;
}
uint32 FBitReader::ReadInt( uint32 Max )
{
	uint32 Value=0;
	SerializeInt( Value, Max );
	return Value;
}
uint8 FBitReader::ReadBit()
{
	uint8 Bit=0;
	SerializeBits( &Bit, 1 );
	return Bit;
}
void FBitReader::Serialize( void* Dest, int64 LengthBytes )
{
	SerializeBits( Dest, LengthBytes*8 );
}
uint8* FBitReader::GetData()
{
	return Buffer.GetData();
}
uint8* FBitReader::GetDataPosChecked()
{
	check(Pos % 8 == 0);
	return &Buffer[Pos >> 3];
}
uint32 FBitReader::GetBytesLeft()
{
	return ((Num - Pos) + 7) >> 3;
}
uint32 FBitReader::GetBitsLeft()
{
	return (Num - Pos);
}
bool FBitReader::AtEnd()
{
	return ArIsError || Pos>=Num;
}
void FBitReader::SetOverflowed()
{
	UE_LOG( LogNetSerialization, Error, TEXT( "FBitReader::SetOverflowed() called" ) );
	ArIsError = 1;
}
int64 FBitReader::GetNumBytes()
{
	return (Num+7)>>3;
}
int64 FBitReader::GetNumBits()
{
	return Num;
}
int64 FBitReader::GetPosBits()
{
	return Pos;
}

/*-----------------------------------------------------------------------------
	FBitReader.
-----------------------------------------------------------------------------*/

void FBitReaderMark::Copy( FBitReader& Reader, TArray<uint8> &Buffer )
{
	checkSlow(Pos<=Reader.Pos);

	int32 Bytes = (Reader.Pos - Pos + 7) >> 3;
	if( Bytes > 0 )
	{
		Buffer.SetNumUninitialized(Bytes);		// This makes room but doesnt zero
		Buffer[Bytes-1] = 0;	// Make sure the last byte is 0 out, because appBitsCpy wont touch the last bits
		appBitsCpy(Buffer.GetData(), 0, Reader.Buffer.GetData(), Pos, Reader.Pos - Pos);
	}
}
