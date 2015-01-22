// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace NetworkProfiler
{
	/**
	 * Encapsulates entire network stream, split into frames. Also contains name table
	 * used to convert indices back into strings.
	 */
	class NetworkStream
	{
		/** Per packet overhead to take into account for total outgoing bandwidth. */
		//public static int PacketOverhead = 48;
		public static int PacketOverhead = 28;

		/** Array of unique names. Code has fixed indexes into it.					*/
		public List<string> NameArray = new List<string>();
		
		/** Internal dictionary from class name to index in name array, used by GetClassNameIndex. */
		private Dictionary<string,int> ClassNameToNameIndex = new Dictionary<string,int>();
		
		/** Index of "Unreal" name in name array.									*/
		public int NameIndexUnreal = -1;

		/** At the highest level, the entire stream is a series of frames.			*/
		public List<PartialNetworkStream> Frames = new List<PartialNetworkStream>();

		/** Mapping from property name to summary */
		public Dictionary<int,TypeSummary> PropertyNameToSummary = new Dictionary<int,TypeSummary>();
		/** Mapping from actor name to summary */
		public Dictionary<int,TypeSummary> ActorNameToSummary = new Dictionary<int,TypeSummary>();
		/** Mapping from RPC name to summary */
		public Dictionary<int,TypeSummary> RPCNameToSummary = new Dictionary<int,TypeSummary>();

		/**
		 * Returns the name associated with the passed in index.
		 * 
		 * @param	Index	Index in name table
		 * @return	Name associated with name table index
		 */
		public string GetName(int Index)
		{
			return NameArray[Index];
		}

		/**
		 * Returns the class name index for the passed in actor name index
		 * 
		 * @param	ActorNameIndex	Name table entry of actor
		 * @return	Class name table index of actor's class
		 */
		public int GetClassNameIndex(int ActorNameIndex)
		{
			
            int ClassNameIndex = 0;
            try
            {
                // Class name is actor name with the trailing _XXX cut off.
                string ActorName = GetName(ActorNameIndex);
                string ClassName = ActorName;
                

                int CharIndex = ActorName.LastIndexOf('_');
                if (CharIndex >= 0)
                {
                    ClassName = ActorName.Substring(0, CharIndex);
                }

                

                // Find class name index in name array.
                
                if (ClassNameToNameIndex.ContainsKey(ClassName))
                {
                    // Found.
                    ClassNameIndex = ClassNameToNameIndex[ClassName];
                }
                // Not found, add to name array and then also dictionary.
                else
                {
                    ClassNameIndex = NameArray.Count;
                    NameArray.Add(ClassName);
                    ClassNameToNameIndex.Add(ClassName, ClassNameIndex);
                }
            }
            catch (System.Exception e)
            {
                System.Console.WriteLine("Error Parsing ClassName for Actor: " + ActorNameIndex + e.ToString());
            }

			return ClassNameIndex;
		}

		/**
		 * Updates the passed in summary dictionary with information of new event.
		 * 
		 * @param	Summaries	Summaries dictionary to update (usually ref to ones contained in this class)
		 * @param	NameIndex	Index of object in name table (e.g. property, actor, RPC)
		 * @param	SizeBits	Size in bits associated with object occurence
		 */
		public void UpdateSummary( ref Dictionary<int,TypeSummary> Summaries, int NameIndex, int SizeBits, float TimeInMS )
		{
			if( Summaries.ContainsKey( NameIndex ) )
			{
				var Summary = Summaries[NameIndex];
				Summary.Count++;
				Summary.SizeBits += SizeBits;
                Summary.TimeInMS += TimeInMS;
			}
			else
			{
				Summaries.Add( NameIndex, new TypeSummary( SizeBits, TimeInMS ) );
			}
		}
	}

	/** Type agnostic summary for property & actor replication and RPCs. */
	class TypeSummary
	{
		/** Number of times property was replicated or RPC was called, ... */ 
		public long Count = 1;
		/** Total size in bits. */
		public long SizeBits;
		/** Total ms */
		public float TimeInMS;

		/** Constructor */
		public TypeSummary( long InSizeBits, float InTimeInMS )
		{
			SizeBits = InSizeBits;
            TimeInMS = InTimeInMS;
		}
	}
}
