// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;

namespace Tools.Distill
{
	/// <summary>A helper class to handle SHA256 creation and x509 signature handling</summary>
	public class Crypto
	{
		private static List<string> ValidSerialNumbers = new List<string>()
		{
			// Serial numbers of X509 digital signatures that we allow to be packaged
			"3AF820A6907699580410055228ECADDF",	// Nvidia1
			"534ABED0BE56D9840DD12DDB84F8B031", // Nvidia2
			"610F784D000000000003",				// Microsoft1
			"61062781000000000008", 			// Microsoft2
			"6101CF3E00000000000F", 			// Microsoft3
			"6101B29B000000000015", 			// Microsoft4
			"68697535E381FE1F91C1153528146A27", // Valve
			"7BF6326F70CBEC340BF2D1868FE65B1E", // Valve
			"19599BE196632020F023C96F2F3904D6", // AMD
            "479525AB0000000001C8", 			// Intel
            "64C4C6DAAEE1C43378152EFEC163241C", // Imagination Technologies 
			"00B743783950C32FFA9842A5DC404294", // Perforce
			"1C16CDDAD8961907E5704B82D9A15842", // Epic Games Inc. (old)
			"3E1338505D2C3D34E2DF71AE64C5C24F", // Epic Games Inc. (new)
		};

		/// <summary>Validate the digital signature of the file matches one of the whitelisted serial numbers</summary>
		/// <param name="Info">FileInfo of the file to verify</param>
		/// <returns>True if the digital signature has a whitelisted serial number</returns>
		public static bool ValidateDigitalSignature( FileInfo Info )
		{
			try
			{
				X509Certificate Certificate = X509Certificate.CreateFromSignedFile( Info.FullName );
				if( Certificate != null )
				{
					if( ValidSerialNumbers.Contains( Certificate.GetSerialNumberString() ) )
					{
						return true;
					}
				}
			}
			catch
			{
				// Any exception means that either the file isn't signed or has an invalid certificate
			}

			return false;
		}

		/// <summary>Create a SHA256 checksum of the file contents</summary>
		/// <param name="Info">FileInfo of the file to checksum</param>
		/// <returns>A 32 byte array of the SHA256 checksum</returns>
		/// <remarks>Microsoft no longer recommend using SHA1 as it is insecure</remarks>
		public static byte[] CreateChecksum( FileInfo Info )
		{
			SHA256CryptoServiceProvider Hasher = new SHA256CryptoServiceProvider();
			FileStream Stream = Info.OpenRead();
			byte[] HashData = Hasher.ComputeHash( Stream );
			Stream.Close();

			return HashData;
		}
	}
}
