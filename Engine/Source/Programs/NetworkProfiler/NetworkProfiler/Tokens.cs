// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace NetworkProfiler
{
	/** Enum values need to be in sync with UE3 */
	public enum ETokenTypes
	{
		FrameMarker			= 0,	// Frame marker, signaling beginning of frame.	
		SocketSendTo,				// FSocket::SendTo
		SendBunch,					// UChannel::SendBunch
		SendRPC,					// Sending RPC
		ReplicateActor,				// Replicated object	
		ReplicateProperty,			// Property being replicated.
		EndOfStreamMarker,			// End of stream marker		
		Event,						// Event
		RawSocketData,				// Raw socket data being sent
		SendAck,					// Ack being sent
		WritePropertyHeader,		// Property header being written
		ExportBunch,				// Exported GUIDs
		MustBeMappedGuids,			// Must be mapped GUIDs
		BeginContentBlock,			// Content block headers
		EndContentBlock,			// Content block footers
		WritePropertyHandle,		// Property handles
		MaxAndInvalid,				// Invalid token, also used as the max token index
	}

	/** Enum values need to be in sync with UE3 */
	public enum EChannelTypes
	{
		Invalid				= 0,	// Invalid type.
		Control,					// Connection control.
		Actor,						// Actor-update channel.
		File,						// Binary file transfer.
		Voice,						// Voice channel
		Max,
	}

	/**
	 * Base class of network token/ events
	 */
	class TokenBase
	{
		/** Type of token. */
		public ETokenTypes TokenType = ETokenTypes.MaxAndInvalid;

		/** Network stream this token belongs to. */
		protected NetworkStream NetworkStream = null;

		/** Stats about token types being serialized. */
		public static int[] TokenTypeStats = Enumerable.Repeat(0, (int)ETokenTypes.MaxAndInvalid).ToArray();

		/**
		 * Reads the next token from the stream and returns it.
		 * 
		 * @param	BinaryStream	Stream used to serialize from
		 * @param	InNetworkStream	Network stream this token belongs to
		 * @return	Token serialized
		 */
		public static TokenBase ReadNextToken(BinaryReader BinaryStream, NetworkStream InNetworkStream)
		{
			TokenBase SerializedToken = null;

			ETokenTypes TokenType = (ETokenTypes) BinaryStream.ReadByte();
			// Handle token specific serialization.
			switch( TokenType )
			{
				case ETokenTypes.FrameMarker:
					SerializedToken = new TokenFrameMarker( BinaryStream );
					break;
				case ETokenTypes.SocketSendTo:
					SerializedToken = new TokenSocketSendTo( BinaryStream );
					break;
				case ETokenTypes.SendBunch:
					SerializedToken = new TokenSendBunch( BinaryStream );
					break;
				case ETokenTypes.SendRPC:
					SerializedToken = new TokenSendRPC( BinaryStream );
					break;
				case ETokenTypes.ReplicateActor:
					SerializedToken = new TokenReplicateActor( BinaryStream );
					break;
				case ETokenTypes.ReplicateProperty:
					SerializedToken = new TokenReplicateProperty( BinaryStream );
					break;
				case ETokenTypes.EndOfStreamMarker:
					SerializedToken = new TokenEndOfStreamMarker();
					break;
				case ETokenTypes.Event:
					SerializedToken = new TokenEvent( BinaryStream );
					break;
				case ETokenTypes.RawSocketData:
					SerializedToken = new TokenRawSocketData( BinaryStream );
					break;
				case ETokenTypes.SendAck:
					SerializedToken = new TokenSendAck( BinaryStream );
					break;
				case ETokenTypes.WritePropertyHeader:
					SerializedToken = new TokenWritePropertyHeader( BinaryStream );
					break;
				case ETokenTypes.ExportBunch:
					SerializedToken = new TokenExportBunch( BinaryStream );
					break;
				case ETokenTypes.MustBeMappedGuids:
					SerializedToken = new TokenMustBeMappedGuids( BinaryStream );
					break;
				case ETokenTypes.BeginContentBlock:
					SerializedToken = new TokenBeginContentBlock( BinaryStream );
					break;
				case ETokenTypes.EndContentBlock:
					SerializedToken = new TokenEndContentBlock( BinaryStream );
					break;
				case ETokenTypes.WritePropertyHandle:
					SerializedToken = new TokenWritePropertyHandle( BinaryStream );
					break;
				default:
					throw new InvalidDataException();
			}

			TokenTypeStats[(int)TokenType]++;
			SerializedToken.NetworkStream = InNetworkStream;
			SerializedToken.TokenType = TokenType;
			return SerializedToken;
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public virtual List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return new List<string>();
		}

		/**
		 * Returns whether the token matches/ passes based on the passed in filters.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 */
		public virtual bool MatchesFilters( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return true;
		}
	}

	/**
	 * End of stream token.
	 */
	class TokenEndOfStreamMarker : TokenBase
	{
	}

	/**
	 * Frame marker token.
	 */ 
	class TokenFrameMarker : TokenBase
	{
		/** Relative time of frame since start of engine. */
		public float RelativeTime;

		/** Constructor, serializing members from passed in stream. */
		public TokenFrameMarker(BinaryReader BinaryStream)
		{
			RelativeTime = BinaryStream.ReadSingle();
		}

		/**
		 * Converts the token into a multi-string description.
		 * 
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "FRAME MARKER" );
			ReturnList.Add( "   Absolute time : " + RelativeTime );
			return ReturnList;
		}
	}

	/**
	 * FSocket::SendTo token. A special address of 0.0.0.0 is used for ::Send
	 */ 
	class TokenSocketSendTo : TokenBase
	{
		/** Thread ID of thread this token was emitted from. */
		public UInt32 ThreadId;
		/** Socket debug description name index. "Unreal" is special name for game traffic. */
		public int SocketNameIndex;
		/** Bytes actually sent by low level code. */
		public UInt16 BytesSent;
		/** Number of bits representing the packet id */
		public UInt16 NumPacketIdBits;
		/** Number of bits representing bunches */
		public UInt16 NumBunchBits;
		/** Number of bits representing acks */
		public UInt16 NumAckBits;
		/** Number of bits used for padding */
		public UInt16 NumPaddingBits;
		/** IP in network byte order. */
		public UInt32 NetworkByteOrderIP;

		/** Constructor, serializing members from passed in stream. */
		public TokenSocketSendTo(BinaryReader BinaryStream)
		{
			ThreadId = BinaryStream.ReadUInt32();
			SocketNameIndex = BinaryStream.ReadInt32();
			BytesSent = BinaryStream.ReadUInt16();
			NumPacketIdBits = BinaryStream.ReadUInt16();
			NumBunchBits = BinaryStream.ReadUInt16();
			NumAckBits = BinaryStream.ReadUInt16();
			NumPaddingBits = BinaryStream.ReadUInt16();
			NetworkByteOrderIP = BinaryStream.ReadUInt32();
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "SOCKET SEND TO" );
			ReturnList.Add( "   ThreadId      : " + ThreadId );
			ReturnList.Add( "   SocketName    : " + NetworkStream.GetName(SocketNameIndex) );
			ReturnList.Add( "   DesiredBytesSent : " + (NumPacketIdBits + NumBunchBits + NumAckBits + NumPaddingBits) / 8.0f );
			ReturnList.Add( "      NumPacketIdBits  : " + NumPacketIdBits );
			ReturnList.Add( "      NumBunchBits     : " + NumBunchBits );
			ReturnList.Add( "      NumAckBits       : " + NumAckBits );
			ReturnList.Add( "      NumPaddingBits   : " + NumPaddingBits );
			ReturnList.Add( "   BytesSent     : " + BytesSent );
			ReturnList.Add( "   Destination   : " + NetworkByteOrderIP );
			return ReturnList;
		}
	}

	/**
	 * UChannel::SendBunch token, NOTE that this is NOT SendRawBunch	
	 */
	class TokenSendBunch : TokenBase
	{
		/** Channel index. */
		public UInt16 ChannelIndex;
		/** Channel type. */
		public byte ChannelType;
		/** Number of header bits serialized/sent. */
		public UInt16 NumHeaderBits;
		/** Number of non-header bits serialized/sent. */
		public UInt16 NumPayloadBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenSendBunch(BinaryReader BinaryStream)
		{
			ChannelIndex = BinaryStream.ReadUInt16();
			ChannelType = BinaryStream.ReadByte();
			NumHeaderBits = BinaryStream.ReadUInt16();
			NumPayloadBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Gets the total number of bits serialized for the bunch.
		 */
		public int GetNumTotalBits()
		{
			return NumHeaderBits + NumPayloadBits;
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "SEND BUNCH" );
			ReturnList.Add( "   Channel Index : " + ChannelIndex );
			ReturnList.Add( "   Channel Type  : " + ChannelType );
			ReturnList.Add( "   NumTotalBits  : " + GetNumTotalBits() );
			ReturnList.Add( "      NumHeaderBits : " + NumHeaderBits );
			ReturnList.Add( "      NumPayloadBits: " + NumPayloadBits );
			ReturnList.Add( "   NumTotalBytes : " + GetNumTotalBits() / 8.0f );
			return ReturnList;
		}
	}

	/**
	 * Token for RPC replication
	 */
	class TokenSendRPC : TokenBase
	{
		/** Name table index of actor name. */
		public int ActorNameIndex;
		/** Name table index of function name. */
		public int FunctionNameIndex;
		/** Number of bits serialized/sent for the header. */
		public UInt16 NumHeaderBits;
		/** Number of bits serialized/sent for the parameters. */
		public UInt16 NumParameterBits;
		/** Number of bits serialized/sent for the footer. */
		public UInt16 NumFooterBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenSendRPC( BinaryReader BinaryStream )
		{
			ActorNameIndex = BinaryStream.ReadInt32();
			FunctionNameIndex = BinaryStream.ReadInt32();
			NumHeaderBits = BinaryStream.ReadUInt16(); 					
			NumParameterBits = BinaryStream.ReadUInt16();
			NumFooterBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Gets the total number of bits serialized for the RPC.
		 */
		public int GetNumTotalBits()
		{
			return NumHeaderBits + NumParameterBits + NumFooterBits;
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "SEND RPC" );
			ReturnList.Add( "   Actor         : " + NetworkStream.GetName(ActorNameIndex) );
			ReturnList.Add( "   Function      : " + NetworkStream.GetName(FunctionNameIndex) );
			ReturnList.Add( "   NumTotalBits  : " + GetNumTotalBits() );
			ReturnList.Add( "      NumHeaderBits    : " + NumHeaderBits );
			ReturnList.Add( "      NumParameterBits : " + NumParameterBits );
			ReturnList.Add( "      NumFooterBits    : " + NumFooterBits );
			ReturnList.Add( "   NumTotalBytes : " + GetNumTotalBits() / 8.0f );
			return ReturnList;
		}

		/**
		 * Returns whether the token matches/ passes based on the passed in filters.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 * 
		 * @return true if it matches, false otherwise
		 */
		public override bool MatchesFilters( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return	( ActorFilter.Length == 0 || NetworkStream.GetName(ActorNameIndex).ToUpperInvariant().Contains( ActorFilter.ToUpperInvariant() ) )
			&&		( RPCFilter.Length == 0 || NetworkStream.GetName(FunctionNameIndex).ToUpperInvariant().Contains( RPCFilter.ToUpperInvariant() ));
		}
	}

	/**
	 * Actor replication token. Like the frame marker, this doesn't actually correlate
	 * with any data transfered but is status information for parsing. Properties are 
	 * removed from stream after parsing and moved into actors.
	 */ 
	class TokenReplicateActor : TokenBase
	{
		/** Whether bNetDirty was set on Actor. */
		public bool bNetDirty;
		/** Whether bNetInitial was set on Actor. */
		public bool bNetInitial;
		/** Whether bNetOwner was set on Actor. */
		public bool bNetOwner;
		/** Name table index of actor name */
		public int ActorNameIndex;
		/** Time in ms to replicate this actor */
		public float TimeInMS;

        /** List of property tokens that were serialized for this actor. */
		public List<TokenReplicateProperty> Properties;

		/** List of property header tokens that were serialized for this actor. */
		public List<TokenWritePropertyHeader> PropertyHeaders;

		/** Constructor, serializing members from passed in stream. */
		public TokenReplicateActor(BinaryReader BinaryStream)
		{
			byte NetFlags = BinaryStream.ReadByte();
			bNetDirty = (NetFlags & 1) == 1;
			bNetInitial = (NetFlags & 2) == 2;
			bNetOwner = (NetFlags & 4) == 4;
			ActorNameIndex = BinaryStream.ReadInt32();
            TimeInMS = BinaryStream.ReadSingle();
			Properties = new List<TokenReplicateProperty>();
			PropertyHeaders = new List<TokenWritePropertyHeader>();
		}

		/**
		 * Returns the number of bits for this replicated actor while taking filters into account.
		 * 
		 * @param	ActorFilter		Filter for actor name
		 * @param	PropertyFilter	Filter for property name
		 * @param	RPCFilter		Unused
		 */
		public int GetNumReplicatedBits( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			int NumReplicatedBits = 0;
			foreach( var Property in Properties )
			{
				if( Property.MatchesFilters( ActorFilter, PropertyFilter, RPCFilter ) )
				{
					NumReplicatedBits += Property.NumBits;
				}
			}

			foreach( var PropertyHeader in PropertyHeaders )
			{
				if( PropertyHeader.MatchesFilters( ActorFilter, PropertyFilter, RPCFilter ) )
				{
					NumReplicatedBits += PropertyHeader.NumBits;
				}
			}

			return NumReplicatedBits;
		}
	 
		/**
		 * Converts the token into a multi-string description.
		 * 
		 * @param	ActorFilter		Filter for actor name
		 * @param	PropertyFilter	Filter for property name
		 * @param	RPCFilter		Unused
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			int NumReplicatedBits = GetNumReplicatedBits( ActorFilter, PropertyFilter, RPCFilter );

			var ReturnList = new List<string>();
			ReturnList.Add( "REPLICATE ACTOR" );
			ReturnList.Add( "   Actor         : " + NetworkStream.GetName(ActorNameIndex) );
			ReturnList.Add( "   Flags         : " + (bNetDirty ? "bNetDirty " : "") + (bNetInitial ? "bNetInitial" : "") + (bNetOwner ? "bNetOwner" : "") );
			ReturnList.Add( "   NumBits       : " + NumReplicatedBits );
			ReturnList.Add( "   NumBytes      : " + NumReplicatedBits / 8.0f );
			
			if( Properties.Count > 0 )
			{
				ReturnList.Add( "   PROPERTIES" );
				foreach( var Property in  Properties )
				{
					if( Property.MatchesFilters( ActorFilter, PropertyFilter, RPCFilter ) )
					{
						ReturnList.Add( "      Property      : " + NetworkStream.GetName(Property.PropertyNameIndex) );
						ReturnList.Add( "      NumBits       : " + Property.NumBits );
						ReturnList.Add( "      NumBytes      : " + Property.NumBits / 8.0f);
					}
				}
			}

			if( PropertyHeaders.Count > 0 )
			{
				ReturnList.Add( "   PROPERTY HEADERS" );
				foreach( var PropertyHeader in  PropertyHeaders )
				{
					if( PropertyHeader.MatchesFilters( ActorFilter, PropertyFilter, RPCFilter ) )
					{
						ReturnList.Add( "      Property      : " + NetworkStream.GetName(PropertyHeader.PropertyNameIndex) );
						ReturnList.Add( "      NumBits       : " + PropertyHeader.NumBits );
						ReturnList.Add( "      NumBytes      : " + PropertyHeader.NumBits / 8.0f);
					}
				}
			}
			return ReturnList;
		}
	
		/**
		 * Returns whether the token matches/ passes based on the passed in filters.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 * 
		 * @return true if it matches, false otherwise
		 */
		public override bool MatchesFilters( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			bool ContainsMatchingProperty = false || Properties.Count == 0;		// NOTE - Show actors with 0 properties
			foreach( var Property in Properties )
			{
				if( Property.MatchesFilters( ActorFilter, PropertyFilter, RPCFilter ) )
				{
					ContainsMatchingProperty = true;
					break;
				}
			}
			return (ActorFilter.Length == 0 || NetworkStream.GetName(ActorNameIndex).ToUpperInvariant().Contains( ActorFilter.ToUpperInvariant() )) && ContainsMatchingProperty;
		}
	}

	/**
	 * Token for property replication. Context determines which actor this belongs to.
	 */
	class TokenReplicateProperty : TokenBase
	{
		/** Name table index of property name. */
		public int		PropertyNameIndex;
		/** Number of bits serialized/ sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenReplicateProperty(BinaryReader BinaryStream)
		{
            PropertyNameIndex = BinaryStream.ReadInt32();
			NumBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Returns whether the token matches/ passes based on the passed in filters.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 * 
		 * @return true if it matches, false otherwise
		 */
		public override bool MatchesFilters( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return PropertyFilter.Length == 0 || NetworkStream.GetName(PropertyNameIndex).ToUpperInvariant().Contains( PropertyFilter.ToUpperInvariant() );
		}
	}			

	/**
	 * Token for property header replication. Context determines which actor this belongs to.
	 */
	class TokenWritePropertyHeader : TokenBase
	{
		/** Name table index of property name. */
		public int		PropertyNameIndex;
		/** Number of bits serialized/ sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenWritePropertyHeader(BinaryReader BinaryStream)
		{
            PropertyNameIndex = BinaryStream.ReadInt32();
			NumBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Returns whether the token matches/ passes based on the passed in filters.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 * 
		 * @return true if it matches, false otherwise
		 */
		public override bool MatchesFilters( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return PropertyFilter.Length == 0 || NetworkStream.GetName(PropertyNameIndex).ToUpperInvariant().Contains( PropertyFilter.ToUpperInvariant() );
		}
	}

	/**
	 * Token for exported GUID bunches.
	 */
	class TokenExportBunch : TokenBase
	{
		/** Number of bits serialized/ sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenExportBunch(BinaryReader BinaryStream)
		{
			NumBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "EXPORTED GUIDS" );
			ReturnList.Add( "   NumBits          : " + NumBits );
			ReturnList.Add( "   NumBytes         : " + NumBits / 8.0f);
			return ReturnList;
		}
	}

	/**
	 * Token for must be mapped GUIDs.
	 */
	class TokenMustBeMappedGuids : TokenBase
	{
		/** Number of GUIDs serialized/sent. */
		public UInt16	NumGuids;

		/** Number of bits serialized/sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenMustBeMappedGuids(BinaryReader BinaryStream)
		{
			NumGuids = BinaryStream.ReadUInt16();
			NumBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "MUST BE MAPPED GUIDS" );
			ReturnList.Add( "   NumGuids         : " + NumGuids );
			ReturnList.Add( "   NumBits          : " + NumBits );
			ReturnList.Add( "   NumBytes         : " + NumBits / 8.0f);
			return ReturnList;
		}
	}

	/**
	 * Token for content block headers.
	 */
	class TokenBeginContentBlock : TokenBase
	{
		/** Name table index of property name. */
		public int		ObjectNameIndex;
		/** Number of bits serialized/ sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenBeginContentBlock(BinaryReader BinaryStream)
		{
            ObjectNameIndex = BinaryStream.ReadInt32();
			NumBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Returns whether the token matches/ passes based on the passed in filters.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 * 
		 * @return true if it matches, false otherwise
		 */
		public override bool MatchesFilters( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return ActorFilter.Length == 0 || NetworkStream.GetName(ObjectNameIndex).ToUpperInvariant().Contains( ActorFilter.ToUpperInvariant() );
		}
	}

	/**
	 * Token for property header replication. Context determines which actor this belongs to.
	 */
	class TokenEndContentBlock : TokenBase
	{
		/** Name table index of property name. */
		public int		ObjectNameIndex;
		/** Number of bits serialized/ sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenEndContentBlock(BinaryReader BinaryStream)
		{
            ObjectNameIndex = BinaryStream.ReadInt32();
			NumBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Returns whether the token matches/ passes based on the passed in filters.
		 * 
		 * @param	ActorFilter		Actor filter to match against
		 * @param	PropertyFilter	Property filter to match against
		 * @param	RPCFilter		RPC filter to match against
		 * 
		 * @return true if it matches, false otherwise
		 */
		public override bool MatchesFilters( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			return ActorFilter.Length == 0 || NetworkStream.GetName(ObjectNameIndex).ToUpperInvariant().Contains( ActorFilter.ToUpperInvariant() );
		}
	}

	/**
	 * Token for property hgandle replication. Context determines which actor this belongs to.
	 */
	class TokenWritePropertyHandle : TokenBase
	{
		/** Number of bits serialized/ sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenWritePropertyHandle(BinaryReader BinaryStream)
		{
			NumBits = BinaryStream.ReadUInt16();
		}
	}

	/**
	 * Token for events.
	 */
	class TokenEvent : TokenBase
	{
		/** Name table index of event name. */
		public int EventNameNameIndex;
		/** Name table index of event description. */
		public int EventDescriptionNameIndex;

		/** Constructor, serializing members from passedin stream. */
		public TokenEvent(BinaryReader BinaryStream)
		{
			EventNameNameIndex = BinaryStream.ReadInt32();
			EventDescriptionNameIndex = BinaryStream.ReadInt32();
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "EVENT" );
			ReturnList.Add( "   Type          : " + NetworkStream.GetName(EventNameNameIndex) );
			ReturnList.Add( "   Description   : " + NetworkStream.GetName(EventDescriptionNameIndex) );
			return ReturnList;
		}
	}

	/**
	 * Token for raw socket data. Not captured by default in UE3.
	 */
	class TokenRawSocketData : TokenBase
	{
		/** Raw data. */
		public byte[] RawData;

		/** Constructor, serializing members from passed in stream. */
		public TokenRawSocketData(BinaryReader BinaryStream)
		{
			int Size = BinaryStream.ReadUInt16();
			RawData = BinaryStream.ReadBytes( Size );
		}
	}

	/**
	 * Token for sent acks.
	 */
	class TokenSendAck : TokenBase
	{
		/** Number of bits serialized/sent. */
		public UInt16	NumBits;

		/** Constructor, serializing members from passed in stream. */
		public TokenSendAck(BinaryReader BinaryStream)
		{
			NumBits = BinaryStream.ReadUInt16();
		}

		/**
		 * Converts the token into a multi-string description.
		 */
		public override List<string> ToDetailedStringList( string ActorFilter, string PropertyFilter, string RPCFilter )
		{
			var ReturnList = new List<string>();
			ReturnList.Add( "SEND ACK" );
			ReturnList.Add( "   NumBits  : " + NumBits );
			ReturnList.Add( "   NumBytes : " + NumBits / 8.0f );
			return ReturnList;
		}
	}
}