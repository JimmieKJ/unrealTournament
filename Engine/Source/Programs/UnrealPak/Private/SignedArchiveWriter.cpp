// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealPak.h"
#include "IPlatformFilePak.h"
#include "SecureHash.h"
#include "BigInt.h"
#include "SignedArchiveWriter.h"

FSignedArchiveWriter::FSignedArchiveWriter(FArchive& InPak, const FString& InPakFilename, const FEncryptionKey& InPublicKey, const FEncryptionKey& InPrivateKey)
: BufferArchive(Buffer)
	, PakWriter(InPak)
	, PakSignaturesFilename(FPaths::ChangeExtension(InPakFilename, TEXT("sig")))
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

void FSignedArchiveWriter::SerializeBufferAndSign()
{
	// Hash the buffer
	FDecryptedSignature SourceSignature;
	
	// Compute a hash for this buffer data
	SourceSignature.Data = FCrc::MemCrc32(&Buffer[0], Buffer.Num());

	// Encrypt the signature
	FEncryptedSignature EncryptedSignature;
	FEncryption::EncryptSignature(SourceSignature, EncryptedSignature, PrivateKey);

	// Flush the buffer
	PakWriter.Serialize(&Buffer[0], Buffer.Num());
	BufferArchive.Seek(0);
	Buffer.Empty(FPakInfo::MaxChunkDataSize);

	// Collect the signature so we can write it out at the end
	ChunkSignatures.Add(EncryptedSignature);

#if 0
	FDecryptedSignature<GPakFileChunkHashSize> TestSignature;
	FEncryption::DecryptBytes(TestSignature.Data, Signature.Data, GPakFileChunkHashSize, PublicKey);
	check(FMemory::Memcmp(TestSignature.Data, Hash, sizeof(Hash)) == 0);
#endif
}

bool FSignedArchiveWriter::Close()
{
	if (BufferArchive.Tell() > 0)
	{
		SerializeBufferAndSign();
	}

	FArchive* SignatureWriter = IFileManager::Get().CreateFileWriter(*PakSignaturesFilename);
	*SignatureWriter << ChunkSignatures;
	delete SignatureWriter;

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
 * Useful code for testing the encryption methods
 */
void TestEncryption()
{
	FEncryptionKey PublicKey;
	FEncryptionKey PrivateKey;
	TEncryptionInt P(TEXT("0x21443BD2DD63E995403"));
	TEncryptionInt Q(TEXT("0x28CBB6E5749AC65749"));
	FEncryption::GenerateKeyPair(P, Q, PublicKey, PrivateKey);

	// Generate random data
	const int32 DataSize = 1024;
	uint8* Data = (uint8*)FMemory::Malloc(DataSize);
	for (int32 Index = 0; Index < DataSize; ++Index)
	{
		Data[Index] = (uint8)(Index % 255);
	}

	// Generate signature
	FDecryptedSignature OriginalSignature;
	FEncryptedSignature EncryptedSignature;
	FDecryptedSignature DecryptedSignature;
	OriginalSignature.Data = FCrc::MemCrc32(Data, DataSize);

	// Encrypt signature with the private key
	FEncryption::EncryptSignature(OriginalSignature, EncryptedSignature, PrivateKey);
	FEncryption::DecryptSignature(EncryptedSignature, DecryptedSignature, PublicKey);

	// Check
	if (OriginalSignature == DecryptedSignature)
	{
		UE_LOG(LogPakFile, Display, TEXT("Keys match"));
	}
	else
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Keys mismatched!"));
	}

	double OverallTime = 0.0;
	double OverallNumTests = 0.0;
	for (int32 TestIndex = 0; TestIndex < 10; ++TestIndex)
	{
		static const int64 NumTests = 500;
		double Timer = FPlatformTime::Seconds();
		{
			for (int64 i = 0; i < NumTests; ++i)
			{
				FEncryption::DecryptSignature(EncryptedSignature, DecryptedSignature, PublicKey);
			}
		}
		Timer = FPlatformTime::Seconds() - Timer;
		OverallTime += Timer;
		OverallNumTests += (double)NumTests;
		UE_LOG(LogPakFile, Display, TEXT("%i signatures decrypted in %.4fs, Avg = %.4fs, OverallAvg = %.4fs"), NumTests, Timer, Timer / (double)NumTests, OverallTime / OverallNumTests);
	}
	
	FMemory::Free(Data);
}