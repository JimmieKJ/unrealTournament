/*
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Xml.Serialization;
using System.Web.UI;
using Perforce.P4;

namespace P4ChangeParser
{
	/// <summary>
	/// Class representing a Perforce changelist with a description parsed according to the new tagging system
	/// </summary>
	[XmlType("ParsedChangelist")]
	public class P4ParsedChangelist
	{
		/// <summary>
		/// Enumeration of valid export formats for the changelist
		/// </summary>
		public enum EExportFormats
		{
			/// <summary>
			/// Xml export format
			/// </summary>
			EF_Xml,

			/// <summary>
			/// Html export format
			/// </summary>
			EF_Html,

			/// <summary>
			/// Plain text
			/// </summary>	
			EF_PlainText
		};

		#region Member Variables
		/// <summary>
		/// Base changelist information on which the parsed changelist is populated
		/// </summary>
        private SerializableChangelist mBaseChangelist;

		/// <summary>
		/// Subchanges within the changelist, marked by the appearance of start tags in the description
		/// </summary>
		private List<P4ParsedSubchange> mSubchanges = new List<P4ParsedSubchange>();
		#endregion

		#region Static Member Variables
		/// <summary>
		/// Xml Serializer for a list of parsed changelists
		/// </summary>
		private static XmlSerializer mListSerializer = new XmlSerializer(typeof(List<P4ParsedChangelist>));
		#endregion

		#region Properties
		/// <summary>
		/// Base changelist information on which the parsed changelist is populated
		/// </summary>
        public SerializableChangelist BaseChangelist
		{
			get { return mBaseChangelist; }
			set { mBaseChangelist = value; }
		}

		/// <summary>
		/// Subchanges within the changelist, marked by the appearance of start tags in the description
		/// </summary>
		public List<P4ParsedSubchange> Subchanges
		{
		    get { return mSubchanges; }
			set { mSubchanges = value; }
		}
		#endregion

		#region Constructors
		/// <summary>
		/// Construct a P4ParsedChangelist from the provided Changelist
		/// </summary>
		/// <param name="InChangelist">Changelist to use to populate the parsed changelist</param>
		public P4ParsedChangelist(Changelist InChangelist)
		{
			mBaseChangelist = new SerializableChangelist(InChangelist);

			// If the base changelist supports the new tagging system, parse it for tags
			if (ChangelistSupportsTagging(InChangelist))
			{
				ParseSubchanges();
			}
		}

		/// <summary>
		/// Construct a P4ParsedChangelist; Default constructor required for Xml serialization
		/// </summary>
		private P4ParsedChangelist() { }
		#endregion

		#region Public Methods
		/// <summary>
		/// Checks if a provided changelist supports the new tagging system or not
		/// </summary>
		/// <param name="InChangelist">Changelist to check if it supports the new tagging system</param>
		/// <returns>true if the changelist supports the tagging system; false otherwise</returns>
		public static bool ChangelistSupportsTagging(Changelist InChangelist)
		{
			// See if anything in the changelist description matches the start tag regex pattern
            return Regex.IsMatch(InChangelist.Description, P4StartTag.StartTagPattern, RegexOptions.IgnoreCase);
		}

		/// <summary>
		/// Export the provided parsed changelists to the provided file name in the provided format
		/// </summary>
		/// <param name="InExportFormat">Format to export to</param>
		/// <param name="InExportChangelists">Changelists to export</param>
		/// <param name="InExportFileName">Filename to export to</param>
		public static void Export(EExportFormats InExportFormat, List<P4ParsedChangelist> InExportChangelists, String InExportFileName)
		{
			switch( InExportFormat )
			{
				case EExportFormats.EF_Xml:
					ExportXml( InExportChangelists, InExportFileName );
					break;

				case EExportFormats.EF_Html:
					ExportHtml( InExportChangelists, InExportFileName );
					break;

				case EExportFormats.EF_PlainText:
					{
						// Generate plain text output
						var ReportSettings = new OutputFormatter.ReportSettings();
						{
							ReportSettings.Format = InExportFormat;
						}

						string ReportString;
						OutputFormatter.GenerateReportFromChanges( ReportSettings, InExportChangelists, out ReportString );

						// Save the report to disk
						{
							StreamWriter FileStreamWriter = null;
							try
							{
								FileStreamWriter = new StreamWriter( InExportFileName );
								FileStreamWriter.Write( ReportString );
							}
							catch( IOException E )
							{
								Console.WriteLine( "IO error while attempting to export to file {0}", E.Message );
							}
							finally
							{
								FileStreamWriter.Close();
							}
						}

					}
					break;
			}
		}
		#endregion

		#region Helper Methods
		#region Exporting/Importing
		/// <summary>
		/// Export the provided changelists in Xml to the provided file name. 
		/// </summary>
		/// <param name="InExportChangelists">Changelists to export</param>
		/// <param name="InExportFileName">Filename to export to</param>
		private static void ExportXml(List<P4ParsedChangelist> InExportChangelists, String InExportFileName)
		{
			StreamWriter XmlStreamWriter = null;

			// Attempt to serialize the changelists
			try
			{
				XmlStreamWriter = new StreamWriter(InExportFileName);
				mListSerializer.Serialize(XmlStreamWriter, InExportChangelists);
			}
			catch (IOException E)
			{
				Console.WriteLine("IO error while attempting to export to XML! {0}", E.Message);
			}
			catch (Exception E)
			{
				Console.WriteLine("Exception while attempting to export to XML! {0}", E.Message);
			}
			finally
			{
				XmlStreamWriter.Close();
			}
		}

		/// <summary>
		/// Loads changes data from the specified XML file
		/// </summary>
		/// <param name="InFileName">XML file name</param>
		/// <param name="OutChanges">(Out) Changes data</param>
		private static void ImportXml( string InFileName, out List<P4ParsedChangelist> OutChanges )
		{
			OutChanges = null;

			XmlSerializer ListSerializer = new XmlSerializer( typeof( List<P4ParsedChangelist> ) );

			StreamReader XmlStreamReader = null;

			// Attempt to deserialize the change data
			try
			{
				XmlStreamReader = new StreamReader( InFileName );
				OutChanges = (List<P4ParsedChangelist>)ListSerializer.Deserialize( XmlStreamReader );
			}
			catch( IOException E )
			{
				Console.WriteLine( "IO error while attempting to read XML! {0}", E.Message );
			}
			catch( Exception E )
			{
				Console.WriteLine( "Exception while attempting to read XML! {0}", E.Message );
			}
			finally
			{
				XmlStreamReader.Close();
			}
		}


		/// <summary>
		/// Export the provided changelists in HTML to the provided file name.
		/// </summary>
		/// <param name="InExportChangelists">Changelists to export</param>
		/// <param name="InExportFileName">Filename to export to</param>
		private static void ExportHtml(List<P4ParsedChangelist> InExportChangelists, String InExportFileName)
		{
			StreamWriter FileStreamWriter = null;
			HtmlTextWriter HtmlWriter = null;
			
			// Attempt to serialize the changelists
			try
			{
				FileStreamWriter = new StreamWriter(InExportFileName);
				HtmlWriter = new HtmlTextWriter(FileStreamWriter);

				// Write out the opening <HTML> and <BODY> tags
				HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Html);
				HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Body);
				foreach(P4ParsedChangelist ParsedChangelist in InExportChangelists)
				{
					// Write out the current parsed changelist to HTML
					ParsedChangelist.WriteHtml(HtmlWriter);
				}

				// Close the <HTML> and <BODY> tags
				HtmlWriter.RenderEndTag();
				HtmlWriter.RenderEndTag();
			}
			catch (IOException E)
			{
				Console.WriteLine("IO error while attempting to export to HTML {0}", E.Message);
			}
			finally
			{
				HtmlWriter.Close();
				FileStreamWriter.Close();
			}
		}

		/// <summary>
		/// Write out the contents of the parsed changelist to HTML format
		/// </summary>
		/// <param name="HtmlWriter">HTML text writer to use</param>
		private void WriteHtml(HtmlTextWriter HtmlWriter)
		{
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Div);

			// Construct a table to keep base changelist data nicely formatted
			HtmlWriter.AddAttribute(HtmlTextWriterAttribute.Border, "0");
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Table);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Tr);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Td);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.B);
			HtmlWriter.Write("Change:&nbsp;&nbsp;&nbsp;");
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Td);
			HtmlWriter.Write(mBaseChangelist.Id);
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Tr);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Td);
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.B);
			HtmlWriter.Write("Date:   ");
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Td);
            HtmlWriter.Write(mBaseChangelist.ModifiedDate.ToLocalTime().ToShortDateString() + " " + mBaseChangelist.ModifiedDate.ToLocalTime().ToShortTimeString());
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();

			// Output each subchange to HTML as part of an unordered list
			HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Ul);
			foreach (P4ParsedSubchange CurSubchange in Subchanges)
			{
				HtmlWriter.RenderBeginTag(HtmlTextWriterTag.Li);
				CurSubchange.WriteHtml(HtmlWriter);
				HtmlWriter.RenderEndTag();
			}
			HtmlWriter.RenderEndTag();
			HtmlWriter.RenderEndTag();
		}
		#endregion

		#region Parsing
		/// <summary>
		/// Parse out the subchanges from the base changelist
		/// </summary>
		private void ParseSubchanges()
		{
			// Split the base changelist description into pieces according to the start tag regex pattern.
			// This results in an array of the format [potentialStringNotRelevantToTagging][StartTag][SubchangeString][StartTag]
			// [SubchangeString], etc.
			Regex SubchangeSplitter = new Regex(P4StartTag.StartTagPattern, RegexOptions.IgnoreCase);
			String[] Subchanges = SubchangeSplitter.Split(mBaseChangelist.Description);
			
			// Iterate over the split strings
			for (int StringIndex = 0; StringIndex < Subchanges.Length; ++StringIndex)
			{
				// If the current string is a start tag and it has a string in the array after it, parse it as a subchange
				String CurString = Subchanges[StringIndex];
				if (P4StartTag.IsStringAStartTag(CurString) && StringIndex + 1 < Subchanges.Length)
				{
					mSubchanges.Add(new P4ParsedSubchange(CurString, Subchanges[++StringIndex]));
				}
			}
		}
		#endregion
		#endregion
	}
}
