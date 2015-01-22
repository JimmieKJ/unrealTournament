/*
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Serialization;
using System.Web.UI;

namespace P4ChangeParser
{
	/// <summary>
	/// Class representing a subchange declared in a Perforce changelist description
	/// </summary>
	public class P4ParsedSubchange : IXmlSerializable
	{
		#region Static Member Variables
		/// <summary>
		/// Dictionary of types to their appropriate XmlSerializer; Performance optimization
		/// </summary>
		private static Dictionary<Type, XmlSerializer> mXmlSerializers = new Dictionary<Type, XmlSerializer>();
		#endregion

		#region Member Variables
		/// <summary>
		/// Start tag for this subchange
		/// </summary>
		private P4StartTag mStartTag;

		/// <summary>
		/// Auxiliary tags for this subchange; Held in a dictionary of string representing tag type to array of that tag type
		/// </summary>
		private Dictionary<String, List<P4AuxiliaryTag>> mAuxiliaryTags = new Dictionary<String, List<P4AuxiliaryTag>>();
		#endregion

		#region Properties
		/// <summary>
		/// Start tag for this subchange
		/// </summary>
		public P4StartTag StartTag
		{
			get { return mStartTag; }
		}

		/// <summary>
		/// Auxiliary tags for this subchange; Held in a dictionary of string representing tag type to array of that tag type
		/// </summary>
		public Dictionary<String, List<P4AuxiliaryTag>> AuxiliaryTags
		{
			get { return mAuxiliaryTags; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a P4ParsedSubchange with the provided start tag name and subchange string
		/// </summary>
		/// <param name="InStartTagName">Name/type of the start tag</param>
		/// <param name="InSubchangeString">String comprised of the rest of the subchange data</param>
		public P4ParsedSubchange(String InStartTagName, String InSubchangeString)
		{
			// Split the subchange string using the regex pattern for tags. This results in an array of items
			// consisting of [startTagDescription][tag][stringOfInfoForTag][tag][stringOfInfoForTag], etc.
			Regex TagSplitter = new Regex(P4Tag.GenericTagPattern, RegexOptions.IgnoreCase);
            String[] AuxiliaryTags = TagSplitter.Split(InSubchangeString);

            //Remove blank lines
            var NonEmptyAuxiliaryTags = new List<string>();
            foreach (var Tag in AuxiliaryTags)
            {
                if (!string.IsNullOrEmpty(Tag))
                    NonEmptyAuxiliaryTags.Add(Tag);
            }
            AuxiliaryTags = NonEmptyAuxiliaryTags.ToArray();


			// Split the possible description string on newlines to prevent the possibility of non-relevant junk being included as part
			// of the start description.
			String[] NewLineDelimiter = new String[] { "\n" };
			String[] StartDescriptionArgs = AuxiliaryTags[0].Split(NewLineDelimiter, StringSplitOptions.RemoveEmptyEntries);

			// The first item in the split array should be the start tag's description; if it's a tag instead, use the empty string as the description (this case should be
			// validated against during submission)
			String StartDescription = !P4Tag.IsStringInTagFormat(StartDescriptionArgs[0]) ? StartDescriptionArgs[0].Trim() : String.Empty;
			mStartTag = new P4StartTag(InStartTagName, StartDescription);
			
			// Iterate through the array, constructing auxiliary tags as appropriate
			for (int AuxStringIndex = 0; AuxStringIndex < AuxiliaryTags.Length; ++AuxStringIndex)
			{
				String CurString = AuxiliaryTags[AuxStringIndex];

				// Check if the current string is an auxiliary tag, and if so, if there's another non-tag string in the array after it.
				// If there is, it should be the string to use to populate the tag info from.
				if (P4AuxiliaryTag.IsStringAnAuxiliaryTag(CurString) && AuxStringIndex + 1 < AuxiliaryTags.Length && !P4Tag.IsStringInTagFormat(AuxiliaryTags[AuxStringIndex+1]))
				{
					// Using the tag's name, try to find the correct type of auxiliary tag to use
					Type TagType = P4AuxiliaryTag.GetTagTypeFromString(CurString);
					if (TagType != null)
					{
						// Split the population string on new lines. The newly created tag will only utilize the part of the population string
						// that occurs before the first newline, eliminating the possibility of junk in between tags getting incorrectly put into
						// a valid tag.
						String[] TagArgs = AuxiliaryTags[++AuxStringIndex].Split(NewLineDelimiter, StringSplitOptions.RemoveEmptyEntries);

						// If the type was found, create a new instance of it and populate it from the population string
						P4AuxiliaryTag NewTag = (P4AuxiliaryTag)Activator.CreateInstance(TagType);
						if (NewTag != null && NewTag.PopulateFromString(TagArgs[0].Trim()))
						{
							// If one of these tag types already existed, add the new tag to the correct array in the dictionary
							if (mAuxiliaryTags.ContainsKey(CurString))
							{
								mAuxiliaryTags[CurString].Add(NewTag);
							}
							// If this is the first occurrence of this kind of tag type, create a new array for it and put it in
							// the dictionary
							else
							{
								List<P4AuxiliaryTag> AuxTagList = new List<P4AuxiliaryTag>();
								AuxTagList.Add(NewTag);
								mAuxiliaryTags.Add(CurString, AuxTagList);
							}
						}
						// Tell the user of a malformed tag, if the tag couldn't be constructed from the string
						else
						{
							Console.WriteLine("Encountered malformed tag of type {0}; not added to subchange.", CurString);
						}
					}
				}
			}
		}

		/// <summary>
		/// Construct a P4ParsedSubchange; Default constructor necessary for Xml serialization
		/// </summary>
		private P4ParsedSubchange() {}
		#endregion

		#region IXmlSerializable Members

		/// <summary>
		/// Return the Xml schema for this class
		/// </summary>
		/// <returns>Null</returns>
		public System.Xml.Schema.XmlSchema GetSchema()
		{
			return null;
		}

		/// <summary>
		/// Read from an XmlReader to populate object values
		/// </summary>
		/// <param name="Reader">Reader to populate from</param>
		public void ReadXml(XmlReader Reader)
		{
			throw new NotImplementedException();
		}

		/// <summary>
		/// Serialize this class out to Xml via an XmlWriter
		/// </summary>
		/// <param name="Writer">XmlWriter to serialize to</param>
		public void WriteXml(XmlWriter Writer)
		{
			// Write the start tag data out, with type as an attribute of the class tags
			Writer.WriteAttributeString("Type", mStartTag.StartTagName);
			Writer.WriteElementString("BriefDescription", mStartTag.BriefDescription);

			// Serialize out all of the auxiliary keys from the dictionary
			foreach(String CurAuxTag in mAuxiliaryTags.Keys)
			{
				List<P4AuxiliaryTag> CurAuxList = mAuxiliaryTags[CurAuxTag];
				foreach (P4AuxiliaryTag CurTag in CurAuxList)
				{
					if (!mXmlSerializers.ContainsKey(CurTag.GetType()))
					{
						mXmlSerializers.Add(CurTag.GetType(), new XmlSerializer(CurTag.GetType()));
					}
					mXmlSerializers[CurTag.GetType()].Serialize(Writer, CurTag);
				}
			}
		}
		#endregion

		/// <summary>
		/// Write the contents of the parsed subchange to HTML format
		/// </summary>
		/// <param name="HtmlWriter">HTML writer to use</param>
		public void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			// Write out the start tag to HTML
			mStartTag.WriteHtml(HtmlWriter);
			
			// Write each auxiliary tag to HTML in an unordered list
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Ul);
			foreach(String CurAuxTag in mAuxiliaryTags.Keys)
			{
				List<P4AuxiliaryTag> CurAuxList = mAuxiliaryTags[CurAuxTag];
				foreach(P4AuxiliaryTag CurTag in CurAuxList)
				{
					HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Li);
					CurTag.WriteHtml(HtmlWriter);
					HtmlWriter.RenderEndTag();
				}
			}
			HtmlWriter.RenderEndTag();
		}
	}
}
