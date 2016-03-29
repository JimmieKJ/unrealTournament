// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PacketHandler.h"

#include "CryptoPP/5.6.2/include/rsa.h"
#include "CryptoPP/5.6.2/include/osrng.h"

/* RSA Encryptor Module Interface */
class FRSAEncryptorHandlerComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};


namespace ERSAEncryptionHandler
{
	enum State
	{
		UnInitialized,
		InitializedLocalKeys,
		InitializedLocalKeysSentLocal,
		InitializedLocalRemoteKeys,
		InitializedLocalRemoteKeysSentLocal,
		Initialized
	};
}

/*
* Asymmetric block cipher handler component using RSA as the cipher
*/
class RSAENCRYPTIONHANDLERCOMPONENT_API RSAEncryptionHandlerComponent : public HandlerComponent
{
public:
	/*
	* Initializes default data, can specify key size in bits, increasing the key size
	* will increase the fixed cipher text and max plain text sizes
	*/
	RSAEncryptionHandlerComponent(int32 InKeySizeInBits = 1024);

	/* Initializes the handler component */
	virtual void Initialize() override;

	/* Whether the handler component is valid */
	virtual bool IsValid() const override;

	/* Handles any incoming packets */
	virtual void Incoming(FBitReader& Packet) override;

	/* Handles any outgoing packets */
	virtual void Outgoing(FBitWriter& Packet) override;

protected:
	/* Set the state of the handler */
	void SetState(ERSAEncryptionHandler::State State);

	/* Encrypt outgoing packets */
	virtual void Encrypt(FBitWriter& Packet);

	/* Decrypt incoming packets */
	virtual void Decrypt(FBitReader& Packet);

	/* Pack the local key into a packet */
	virtual void PackLocalKey(FBitWriter& Packet);

	/* Unpack the remote key from a packet */
	virtual void UnPackRemoteKey(FBitReader& Packet);

	/* Encrypt outgoing packet with remote connection's public key */
	virtual void EncryptWithRemotePublic(const TArray<byte>& InPlainText, TArray<byte>& OutCipherText);

	/* Decrypt incoming packet with local private key */
	virtual void DecryptWithPrivate(const TArray<byte>& InCipherText, TArray<byte>& OutPlainText);

	/* Save the public key's modulus in the provided array */
	void SavePublicKeyModulus(TArray<byte>& OutModulus);

	/* Save the public key's exponent in the provided array */
	void SavePublicKeyExponent(TArray<byte>& OutExponent);

	/** Maximum plain text length that can be encrypted with private key */
	uint32 PrivateKeyMaxPlaintextLength;

	/** Fixed cipher text length that will result of private key encryption */
	uint32 PrivateKeyFixedCiphertextLength;

	/** Maximum plain text length that can be encrypted with remote public key */
	uint32 RemotePublicKeyMaxPlaintextLength;

	/** Fixed cipher text length that will be result of RemotePublicKey encryption */
	uint32 RemotePublicKeyFixedCiphertextLength;

	/** Size of Key in Bits */
	uint32 KeySizeInBits;

	/* State of the handler */
	ERSAEncryptionHandler::State State;

	/* Random number generator used to generate key */
	CryptoPP::AutoSeededRandomPool Rng;

	/* RSA parameters for generating key */
	CryptoPP::InvertibleRSAFunction Params;

	/* Encryptor for encrypting using remote's public key */
	CryptoPP::RSAES_OAEP_SHA_Encryptor RemotePublicEncryptor;

	/* Encryptor for encrypting with local private key */
	CryptoPP::RSAES_OAEP_SHA_Encryptor PrivateEncryptor;

	/* Decryptor for decrypting with local private key */
	CryptoPP::RSAES_OAEP_SHA_Decryptor PrivateDecryptor;

	/* Local Public Key */
	CryptoPP::RSA::PublicKey PublicKey;

	/* Local Private Key */
	CryptoPP::RSA::PrivateKey PrivateKey;

	/* Remote Public Key */
	CryptoPP::RSA::PublicKey RemotePublicKey;
};
