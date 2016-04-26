// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "RSAEncryptionHandlerComponent.h"

DEFINE_LOG_CATEGORY(PacketHandlerLog);
IMPLEMENT_MODULE(FRSAEncryptorHandlerComponentModuleInterface, RSAEncryptionHandlerComponent);

RSAEncryptionHandlerComponent::RSAEncryptionHandlerComponent(int32 InKeySizeInBits)
{
	KeySizeInBits = InKeySizeInBits;
	SetActive(true);
}

void RSAEncryptionHandlerComponent::Initialize()
{
	Params.GenerateRandomWithKeySize(Rng, KeySizeInBits);
	PublicKey = CryptoPP::RSA::PublicKey(Params);
	PrivateKey = CryptoPP::RSA::PrivateKey(Params);

	PrivateEncryptor = CryptoPP::RSAES_OAEP_SHA_Encryptor(PrivateKey);
	PrivateDecryptor = CryptoPP::RSAES_OAEP_SHA_Decryptor(PrivateKey);

	PrivateKeyMaxPlaintextLength = static_cast<uint32>(PrivateEncryptor.FixedMaxPlaintextLength());
	PrivateKeyFixedCiphertextLength = static_cast<uint32>(PrivateEncryptor.FixedCiphertextLength());

	SetState(ERSAEncryptionHandler::State::InitializedLocalKeys);
}

bool RSAEncryptionHandlerComponent::IsValid() const
{
	return PrivateKeyMaxPlaintextLength > 0;
}

void RSAEncryptionHandlerComponent::SetState(ERSAEncryptionHandler::State InState)
{
	State = InState;
}

void RSAEncryptionHandlerComponent::Outgoing(FBitWriter& Packet)
{
	switch (State)
	{
		case ERSAEncryptionHandler::State::InitializedLocalKeys:
		{
			PackLocalKey(Packet);
			SetState(ERSAEncryptionHandler::State::InitializedLocalKeysSentLocal);
			break;
		}
		case ERSAEncryptionHandler::State::InitializedLocalRemoteKeys:
		{
			PackLocalKey(Packet);
			SetState(ERSAEncryptionHandler::State::InitializedLocalRemoteKeysSentLocal);
			break;
		}
		case ERSAEncryptionHandler::State::Initialized:
		{
			if (Packet.GetNumBytes() > 0)
			{
				Encrypt(Packet);
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void RSAEncryptionHandlerComponent::Incoming(FBitReader& Packet)
{
	switch (State)
	{
		case ERSAEncryptionHandler::State::InitializedLocalKeys:
		{
			UnPackRemoteKey(Packet);
			SetState(ERSAEncryptionHandler::State::InitializedLocalRemoteKeys);
			break;
		}
		case ERSAEncryptionHandler::State::InitializedLocalKeysSentLocal:
		{
			UnPackRemoteKey(Packet);
			Initialized();
			SetState(ERSAEncryptionHandler::State::Initialized);
			break;
		}
		case ERSAEncryptionHandler::State::InitializedLocalRemoteKeysSentLocal:
		{
			Initialized();
			SetState(ERSAEncryptionHandler::State::Initialized);
		}
		case ERSAEncryptionHandler::State::Initialized:
		{
			Decrypt(Packet);
			break;
		}
		default:
		{
			break;
		}
	}
}

void RSAEncryptionHandlerComponent::PackLocalKey(FBitWriter& Packet)
{
	FBitWriter Local;
	Local.AllowAppend(true);
	Local.SetAllowResize(true);

	// Save remote public key information
	TArray<byte> ModulusArray;
	TArray<byte> ExponentArray;
	SavePublicKeyModulus(ModulusArray);
	SavePublicKeyExponent(ExponentArray);

	// Pack public key info
	Local << ModulusArray;
	Local << ExponentArray;
	Local.Serialize(Packet.GetData(), Packet.GetNumBytes());
	Packet = Local;
}

void RSAEncryptionHandlerComponent::UnPackRemoteKey(FBitReader& Packet)
{
	// Save remote public key
	TArray<byte> ModulusArray;
	TArray<byte> ExponentArray;

	Packet << ModulusArray;
	Packet << ExponentArray;

	CryptoPP::Integer Modulus;
	CryptoPP::Integer Exponent;

	for (int32 i = 0; i < ModulusArray.Num(); ++i)
	{
		Modulus.SetByte(i, ModulusArray[i]);
	}

	for (int32 i = 0; i < ExponentArray.Num(); ++i)
	{
		Exponent.SetByte(i, ExponentArray[i]);
	}

	RemotePublicKey.SetModulus(Modulus);
	RemotePublicKey.SetPublicExponent(Exponent);
	RemotePublicEncryptor = CryptoPP::RSAES_OAEP_SHA_Encryptor(RemotePublicKey);
	RemotePublicKeyMaxPlaintextLength = static_cast<uint32>(RemotePublicEncryptor.FixedMaxPlaintextLength());
	RemotePublicKeyFixedCiphertextLength = static_cast<uint32>(RemotePublicEncryptor.FixedCiphertextLength());
}

void RSAEncryptionHandlerComponent::Encrypt(FBitWriter& Packet)
{
	// Serialize size of plain text data
	uint32 NumberOfBytesInPlaintext = static_cast<uint32>(Packet.GetNumBytes());

	TArray<byte> PlainText;

	// Copy byte stream to PlainText from Packet
	for (int32 i = 0; i < Packet.GetNumBytes(); ++i)
	{
		PlainText.Add(Packet.GetData()[i]);
	}

	Packet.Reset();

	if (NumberOfBytesInPlaintext == 0 || NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
	{
		if (NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
		{
			UE_LOG(PacketHandlerLog, Warning, TEXT("RSA Encryption skipped as plain text size is too large for this key size. Increase key size or send smaller packets."));
		}

		uint32 NoBytesEncrypted = 0;
		Packet.SerializeIntPacked(NoBytesEncrypted);
		Packet.Serialize(PlainText.GetData(), PlainText.Num());
		return;
	}

	// Serialize invalid amount of bytes
	Packet.SerializeIntPacked(NumberOfBytesInPlaintext);

	TArray<byte> CipherText;

	// The number of bytes in the plaintext is too large for the cipher text
	EncryptWithRemotePublic(PlainText, CipherText);

	// Copy byte stream to Packet from CipherText
	for (int i = 0; i < CipherText.Num(); ++i)
	{
		Packet << CipherText[i];
	}

	UE_LOG(PacketHandlerLog, Log, TEXT("RSA Encrypted"));
}

void RSAEncryptionHandlerComponent::Decrypt(FBitReader& Packet)
{
	// Serialize size of plain text data
	uint32 NumberOfBytesInPlaintext;
	Packet.SerializeIntPacked(NumberOfBytesInPlaintext);

	if (NumberOfBytesInPlaintext == 0 || NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
	{
		// Remove header
		FBitReader Copy(Packet.GetData() + (Packet.GetNumBytes() - Packet.GetBytesLeft()), Packet.GetBitsLeft());
		Packet = Copy;

		if (NumberOfBytesInPlaintext > RemotePublicKeyMaxPlaintextLength)
		{
			UE_LOG(PacketHandlerLog, Warning, TEXT("RSA Decryption skipped as cipher text size is too large for this key size. Increase key size or send smaller packets."));
		}
		return;
	}

	TArray<byte> PlainText;
	TArray<byte> CipherText;

	// Copy byte stream to PlainText from Packet
	// not including the NumOfBytesPT field
	for (int32 i = Packet.GetPosBits() / 8; i < Packet.GetNumBytes(); ++i)
	{
		CipherText.Add(Packet.GetData()[i]);
	}

	DecryptWithPrivate(CipherText, PlainText);

	FBitReader Copy(PlainText.GetData(), NumberOfBytesInPlaintext * 8);
	Packet = Copy;

	UE_LOG(PacketHandlerLog, Log, TEXT("RSA Decrypted"));
}

void RSAEncryptionHandlerComponent::SavePublicKeyModulus(TArray<byte>& OutModulus)
{
	uint32 ModulusSize = static_cast<uint32>(PublicKey.GetModulus().ByteCount());

	CryptoPP::Integer Modulus = PublicKey.GetModulus();

	for (uint32 i = 0; i < ModulusSize; ++i)
	{
		OutModulus.Add(Modulus.GetByte(i));
	}
}

void RSAEncryptionHandlerComponent::SavePublicKeyExponent(TArray<byte>& OutExponent)
{
	uint32 ExponentSize = static_cast<uint32>(PublicKey.GetPublicExponent().ByteCount());

	CryptoPP::Integer Exponent = PublicKey.GetPublicExponent();

	for (uint32 i = 0; i < ExponentSize; ++i)
	{
		OutExponent.Add(Exponent.GetByte(i));
	}
}

void RSAEncryptionHandlerComponent::EncryptWithRemotePublic(const TArray<byte>& InPlainText, TArray<byte>& OutCipherText)
{
	OutCipherText.Reset(); // Reset array
	OutCipherText.Reserve(RemotePublicKeyFixedCiphertextLength); // Reserve memory

	byte* CipherTextBuffer = new byte[RemotePublicKeyFixedCiphertextLength];

	RemotePublicEncryptor.Encrypt(Rng, InPlainText.GetData(), InPlainText.Num(), CipherTextBuffer);

	for (uint32 i = 0; i < RemotePublicKeyFixedCiphertextLength; ++i)
	{
		OutCipherText.Add(CipherTextBuffer[i]);
	}

	delete[] CipherTextBuffer;
}

void RSAEncryptionHandlerComponent::DecryptWithPrivate(const TArray<byte>& InCipherText, TArray<byte>& OutPlainText)
{
	byte* PlainTextTextBuffer = new byte[PrivateKeyMaxPlaintextLength];

	PrivateDecryptor.Decrypt(Rng, InCipherText.GetData(), InCipherText.Num(), PlainTextTextBuffer);

	for (uint32 i = 0; i < PrivateKeyMaxPlaintextLength; ++i)
	{
		OutPlainText.Add(PlainTextTextBuffer[i]);
	}

	delete[] PlainTextTextBuffer;
}

// MODULE INTERFACE
TSharedPtr<HandlerComponent> FRSAEncryptorHandlerComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new RSAEncryptionHandlerComponent);
}