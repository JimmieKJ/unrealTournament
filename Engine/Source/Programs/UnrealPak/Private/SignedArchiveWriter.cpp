// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealPak.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "BigInt.h"
#include "SignedArchiveWriter.h"

FSignedArchiveWriter::FSignedArchiveWriter(FArchive& Pak, const FEncryptionKey& InPublicKey, const FEncryptionKey& InPrivateKey)
	: BufferArchive(Buffer)
	, PakWriter(Pak)
	, SizeOnDisk(0)
	, PakSize(0)
	, PublicKey(InPublicKey)
	, PrivateKey(InPrivateKey)
{
	Buffer.Reserve(FPakInfo::MaxChunkDataSize);
}

FSignedArchiveWriter::~FSignedArchiveWriter()
{
	if (BufferArchive.Tell() > 0)
	{
		SerializeBufferAndSign();
	}
	delete &PakWriter;
}

void FSignedArchiveWriter::Encrypt(int256* EncryptedData, const uint8* Data, const int64 DataLength, const FEncryptionKey EncryptionKey)
{
	for (int Index = 0; Index < DataLength; ++Index)
	{
		EncryptedData[Index] = FEncryption::ModularPow(int256(Data[Index]), EncryptionKey.Exponent, EncryptionKey.Modulus);
	}
}

void FSignedArchiveWriter::SerializeBufferAndSign()
{
	// Hash the buffer
	uint8 Hash[GPakFileChunkHashSize];
	
	// Compute a hash for this buffer data
	ComputePakChunkHash(&Buffer[0], Buffer.Num(), Hash);

	// Encrypt the signature
	FEncryptedSignature<GPakFileChunkHashSize> Signature;
	Encrypt(Signature.Data, Hash, sizeof(Hash), PrivateKey);

	// Flush the buffer
	PakWriter.Serialize(&Buffer[0], Buffer.Num());
	BufferArchive.Seek(0);
	Buffer.Empty(FPakInfo::MaxChunkDataSize);

	// Collect the signature so we can write it out at the end
	ChunkSignatures.Add(Signature);
}

bool FSignedArchiveWriter::Close()
{
	if (BufferArchive.Tell() > 0)
	{
		SerializeBufferAndSign();
	}

	// Write out all the chunk signatures
	for (FEncryptedSignature<GPakFileChunkHashSize>& Signature : ChunkSignatures)
	{
		Signature.Serialize(PakWriter);
	}

	int64 NumSignatures = ChunkSignatures.Num();
	PakWriter << NumSignatures;

	return FArchive::Close();
}

void FSignedArchiveWriter::Serialize(void* Data, int64 Length)
{
	// Serialize data to a buffer. When the max buffer size is reached, the buffer is signed and
	// serialized to disk with its signature
	uint8* DataToWrite = (uint8*)Data;
	int64 RemainingSize = Length;
	while (RemainingSize > 0)
	{
		int64 BufferPos = BufferArchive.Tell();
		int64 SizeToWrite = RemainingSize;
		if (BufferPos + SizeToWrite > FPakInfo::MaxChunkDataSize)
		{
			SizeToWrite = FPakInfo::MaxChunkDataSize - BufferPos;
		}

		BufferArchive.Serialize(DataToWrite, SizeToWrite);
		if (BufferArchive.Tell() == FPakInfo::MaxChunkDataSize)
		{
			SerializeBufferAndSign();
		}
			
		SizeOnDisk += SizeToWrite;
		PakSize += SizeToWrite;

		RemainingSize -= SizeToWrite;
		DataToWrite += SizeToWrite;
	}
}

int64 FSignedArchiveWriter::Tell()
{
	return PakSize;
}

int64 FSignedArchiveWriter::TotalSize()
{
	return PakSize;
}

void FSignedArchiveWriter::Seek(int64 InPos)
{
	UE_LOG(LogPakFile, Fatal, TEXT("Seek is not supported in FSignedArchiveWriter."));
}

/**
	* Encrypt a buffer
	*/
static int256* Encrypt(const uint8* Data, const int64 DataLength, const FEncryptionKey EncryptionKey)
{
	int256* EncryptedData = (int256*)FMemory::Malloc(DataLength * sizeof(uint64));
	for (int Index = 0; Index < DataLength; ++Index)
	{
		EncryptedData[Index] = FEncryption::ModularPow(int256(Data[Index]), EncryptionKey.Exponent, EncryptionKey.Modulus);
	}
	return EncryptedData;
}

/**
	* Decrypt a buffer
	*/
static uint8* Decrypt(const int256* Data, const int64 DataLength, const FEncryptionKey& DecryptionKey)
{
	uint8* DecryptedData = (uint8*)FMemory::Malloc(DataLength);
	for (int64 Index = 0; Index < DataLength; ++Index)
	{
		int256 DecryptedByte = FEncryption::ModularPow(Data[Index], DecryptionKey.Exponent, DecryptionKey.Modulus);
		DecryptedData[Index] = (uint8)DecryptedByte.ToInt();
	}
	return DecryptedData;
}

/**
 * Useful code for testing the encryption methods
 */
void TestEncryption()
{
	FEncryptionKey PublicKey;
	FEncryptionKey PrivateKey;
	int256 P = 61;
	int256 Q = 53;
	FEncryption::GenerateKeyPair(P, Q, PublicKey, PrivateKey);

	// Generate random data
	const int32 DataSize = 1024;
	uint8* Data = (uint8*)FMemory::Malloc(DataSize);
	for (int32 Index = 0; Index < DataSize; ++Index)
	{
		Data[Index] = (uint8)(Index % 255);
	}

	// Generate signature
	uint8 Signature[20];
	FSHA1::HashBuffer(Data, DataSize, Signature);

	// Encrypt signature with the private key
	int256* EncryptedSignature = Encrypt(Signature, sizeof(Signature), PrivateKey);
	uint8* DecryptedSignature = Decrypt(EncryptedSignature, sizeof(Signature), PublicKey);

	// Check
	bool Identical = FMemory::Memcmp(DecryptedSignature, Signature, sizeof(Signature)) == 0;
	if (Identical)
	{
		UE_LOG(LogPakFile, Display, TEXT("Keys match"));
	}
	else
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Keys mismatched!"));
	}

	FMemory::Free(Data);
	FMemory::Free(EncryptedSignature);
	FMemory::Free(DecryptedSignature);
}