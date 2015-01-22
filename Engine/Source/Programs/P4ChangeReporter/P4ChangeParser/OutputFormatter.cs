/*
 * Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Collections.Generic;
using System.Xml.Serialization;


namespace P4ChangeParser
{
	/// <summary>
	/// Takes the XML data generated from the parser and creates nicely formatted reports
	/// </summary>
	class OutputFormatter
	{

		/// <summary>
		/// Settings for generating a report from a set of changes
		/// </summary>
		public class ReportSettings
		{
			/// Format of this report
			public P4ParsedChangelist.EExportFormats Format;

			/// Include TTP numbers at the end of the one-line summary? (plain text)
			public bool bAppendTTPNumbersToOneLineSummary;

			/// Include P4 changelist numbers at the end of the one-line summary? (plain text)
			public bool bAppendChangelistNumbersToOneLineSummary;

			/// String to use when indenting text
			public string IndentText;

			/// String to use for bullets (or empty for no bullets)
			public string BulletText;

			/// Name of category to automatically create for changes that weren't supplied a category
			/// If this is empty, uncategorized changes will be omitted
			public string UncategorizedCategoryName;

			/// True if one-line summaries of changes should be prefixed with the type of change
			public bool bPrefixChangesWithType;


			/// <summary>
			/// Constructor
			/// </summary>
			public ReportSettings()
			{
				Format = P4ParsedChangelist.EExportFormats.EF_PlainText;
				bAppendTTPNumbersToOneLineSummary = true;
				bAppendChangelistNumbersToOneLineSummary = true;
				IndentText = "   ";
				BulletText = "* ";
				UncategorizedCategoryName = "Uncategorized";
				bPrefixChangesWithType = true;
			}
		}


		/// <summary>
		/// Types of changes
		/// </summary>
		public enum EChangeType
		{
			New,
			Change,
			Fix,
			Unknown,
		}


		/// <summary>
		/// Describes a single change
		/// </summary>
		class ChangeDesc
		{
			/// Type of change
			public EChangeType Type;

			/// One-line description of this change
			public String BriefDescription;

			/// Extras description text
			public List<String> ExtraDescription;

			/// P4 changelist number that this change was submitted with
			public int ChangelistNumber;

			/// Bug TTP numbers
			public List<int> TTPNumbers;


			/// <summary>
			/// Constructor
			/// </summary>
			public ChangeDesc()
			{
				Type = EChangeType.Unknown;
				BriefDescription = String.Empty;
				ExtraDescription = new List<string>();
				ChangelistNumber = 0;
				TTPNumbers = new List<int>();
			}
		}


		/// <summary>
		/// Describes a node in the category tree
		/// </summary>
		class ProjectCategory
		{
			/// Name of this category
			public String Name;

			/// List of changes in this category
			public List<ChangeDesc> Changes;

			/// List of sub-categories
			public List<ProjectCategory> Subcategories;


			/// <summary>
			/// Constructor
			/// </summary>
			public ProjectCategory()
			{
				Name = String.Empty;
				Changes = new List<ChangeDesc>();
				Subcategories = new List<ProjectCategory>();
			}
		}


		/// <summary>
		/// Generates a report from a set of changes using the specified settings
		/// </summary>
		/// <param name="InReportSettings">Report generation settings</param>
		/// <param name="InChanges">Change data</param>
		/// <param name="OutReportString">Output report string</param>
		public static void GenerateReportFromChanges( ReportSettings InReportSettings, List<P4ParsedChangelist> InChanges, out String OutReportString )
		{
			// Sort everything into buckets based on category
			ProjectCategory MainCategory = new ProjectCategory();
			MainCategory.Name = "Main";

			// We'll only create an "uncategorized" category if we need to (later)
			ProjectCategory UncategorizedCategory = null;


			// Process changes
			foreach( var Changelist in InChanges )
			{
				foreach( var Subchange in Changelist.Subchanges )
				{
					ChangeDesc NewChangeDesc = new ChangeDesc();


					// Store changelist number
					NewChangeDesc.ChangelistNumber = Changelist.BaseChangelist.Id;


					// Classify this change
					if( Subchange.StartTag.StartTagName.Equals( "Fix", StringComparison.InvariantCultureIgnoreCase ) )
					{
						NewChangeDesc.Type = EChangeType.Fix;
					}
					else if( Subchange.StartTag.StartTagName.Equals( "Change", StringComparison.InvariantCultureIgnoreCase ) )
					{
						NewChangeDesc.Type = EChangeType.Change;
					}
					else if( Subchange.StartTag.StartTagName.Equals( "New", StringComparison.InvariantCultureIgnoreCase ) )
					{
						NewChangeDesc.Type = EChangeType.New;
					}


					// Store the change description
					NewChangeDesc.BriefDescription = Subchange.StartTag.BriefDescription;

					// Store the extra description
					if( Subchange.AuxiliaryTags.ContainsKey( "extra" ) )
					{
						List< P4AuxiliaryTag > AuxTags = Subchange.AuxiliaryTags[ "extra" ];
						foreach( P4AuxiliaryTag CurAuxTag in AuxTags )
						{
							P4ExtraTag CurExtraTag = CurAuxTag as P4ExtraTag;
							NewChangeDesc.ExtraDescription.Add( CurExtraTag.Description );
						}
					}


					// Iterate over the types of tags attached to this change
					ProjectCategory BestCategory = null;
					foreach( var CurAuxTag in Subchange.AuxiliaryTags.Keys )
					{
						// Now iterate over the ordered list of tags of a single type attached to this change
						List<P4AuxiliaryTag> CurAuxList = Subchange.AuxiliaryTags[ CurAuxTag ];
						foreach( P4AuxiliaryTag CurTag in CurAuxList )
						{
							// TTP number
							if( CurTag is P4TTPTag )
							{
								P4TTPTag TTPTag = (P4TTPTag)CurTag;
								NewChangeDesc.TTPNumbers.Add( TTPTag.TTPNumber );
							}


							// Project (aka. category)
							else if( CurTag is P4ProjTag )
							{
								if( BestCategory == null )
								{
									P4ProjTag ProjTag = (P4ProjTag)CurTag;

									// Parse the string
									String[] CategoryPath = ProjTag.Project.Split( '.' );

									ProjectCategory SearchCategory = MainCategory;
									foreach( var CurCategoryString in CategoryPath )
									{
										// Try to find an existing category at this level in the hierarchy
										bool bFoundExistingCategory = false;
										foreach( var CurCategory in SearchCategory.Subcategories )
										{
											if( CurCategory.Name.Equals( CurCategoryString, StringComparison.InvariantCultureIgnoreCase ) )
											{
												// Found an existing category at this level
												bFoundExistingCategory = true;

												// The subcategory we found now becomes the search path
												SearchCategory = CurCategory;
												break;
											}
										}

										// If we didn't find a category go ahead and create one now
										if( !bFoundExistingCategory )
										{
											var NewSubcategory = new ProjectCategory();
											NewSubcategory.Name = CurCategoryString;
											SearchCategory.Subcategories.Add( NewSubcategory );

											// The new subcategory now becomes the search path
											SearchCategory = NewSubcategory;
										}
									}

									// We now have a subcategory to put this change into
									BestCategory = SearchCategory;
								}
								else
								{
									// We already have a category for this change.  Just ignore this entry.
								}
							}

						}
					}


					// If the user didn't mention a category, then give it a default one
					if( BestCategory == null )
					{
						if( UncategorizedCategory == null && InReportSettings.UncategorizedCategoryName.Length > 0 )
						{
							var NewSubcategory = new ProjectCategory();
							NewSubcategory.Name = InReportSettings.UncategorizedCategoryName;
						}

						// Place the item into the 'uncategorized' category
						BestCategory = UncategorizedCategory;
					}


					if( BestCategory != null )
					{
						// Attach this change to the best category for it
						BestCategory.Changes.Add( NewChangeDesc );
					}
				}
			}



			// Sort the data
			SortCategoriesRecursively( InReportSettings, MainCategory );



			// Write output buffer according to the user's configuration
			using( StringWriter OutputWriter = new StringWriter() )
			{
				// Omit the header name for the main category
				bool bIncludeCategoryHeader = false;
				int IndentLevel = 1;
				WriteCategoryChangesRecursively( OutputWriter, InReportSettings, MainCategory, bIncludeCategoryHeader, IndentLevel );


				// Store the report string
				OutReportString = OutputWriter.ToString();
			}
		}


		/// <summary>
		/// Recursively writes formatted project category to a string buffer
		/// </summary>
		/// <param name="OutputWriter">Output string writer (buffer is appended to)</param>
		/// <param name="InReportSettings">Settings</param>
		/// <param name="InCategory">Category data to write</param>
		/// <param name="bIncludeCategoryHeader">Include the category header></param>
		/// <param name="InIndentLevel">Indent level</param>
		static void WriteCategoryChangesRecursively( StringWriter OutputWriter, ReportSettings InReportSettings, ProjectCategory InCategory, bool bIncludeCategoryHeader, int InIndentLevel )
		{
			// Indent items beneath the header!
			int NextIndentLevel = InIndentLevel;
			if( bIncludeCategoryHeader )
			{
				NextIndentLevel += 1;
			}

			if( InReportSettings.Format == P4ParsedChangelist.EExportFormats.EF_PlainText )
			{
				if( bIncludeCategoryHeader )
				{
					// Category name header
					for( int IndentIndex = 0; IndentIndex < InIndentLevel; ++IndentIndex )
					{
						OutputWriter.Write( InReportSettings.IndentText );
					}
					OutputWriter.Write( InReportSettings.BulletText );
					OutputWriter.WriteLine( InCategory.Name );
				}
			}


			// Recurse!
			foreach( var CurSubcategory in InCategory.Subcategories )
			{
				bool bIncludeSubcategoryHeader = true;
				WriteCategoryChangesRecursively( OutputWriter, InReportSettings, CurSubcategory, bIncludeSubcategoryHeader, NextIndentLevel );
			}



			if( InReportSettings.Format == P4ParsedChangelist.EExportFormats.EF_PlainText )
			{
				// Change description
				foreach( var CurChange in InCategory.Changes )
				{
					String OneLineSummary = CurChange.BriefDescription;


					// Prefix the summary with the "type" of change, if we need to
					if( InReportSettings.bPrefixChangesWithType && CurChange.Type != EChangeType.Unknown )
					{
						String ChangeTypeName = String.Empty;
						switch( CurChange.Type )
						{
							case EChangeType.Fix:
								ChangeTypeName = "Fix";
								break;

							case EChangeType.Change:
								ChangeTypeName = "Change";
								break;

							case EChangeType.New:
								ChangeTypeName = "New";
								break;
						}

						OneLineSummary = ChangeTypeName + ": " + OneLineSummary;
					}


					// Append the TTP numbers too if we need to
					if( InReportSettings.bAppendTTPNumbersToOneLineSummary && CurChange.TTPNumbers.Count > 0 )
					{
						OneLineSummary += " (";
						bool bIsFirstTTPNumber = true;
						foreach( var CurTTPNumber in CurChange.TTPNumbers )
						{
							if( !bIsFirstTTPNumber )
							{
								OneLineSummary += ", ";
							}
							OneLineSummary += "TTP " + CurTTPNumber.ToString();
							bIsFirstTTPNumber = false;
						}
						OneLineSummary += ")";
					}


					// Append the P4 changelist numbers too if we need to
					if( InReportSettings.bAppendChangelistNumbersToOneLineSummary )
					{
						OneLineSummary += " [CL " + CurChange.ChangelistNumber + "]";
					}

		
					// One-line summary
					for( int IndentIndex = 0; IndentIndex < NextIndentLevel; ++IndentIndex )
					{
						OutputWriter.Write( InReportSettings.IndentText );
					}
					OutputWriter.Write( InReportSettings.BulletText );
					OutputWriter.WriteLine( OneLineSummary );


					// Extra description lines
					{
						foreach( string CurLine in CurChange.ExtraDescription )
						{
							// Indent
							int ExtraTextIndentLevel = NextIndentLevel + 1;
							for( int IndentIndex = 0; IndentIndex < ExtraTextIndentLevel; ++IndentIndex )
							{
								OutputWriter.Write( InReportSettings.IndentText );
							}

							OutputWriter.Write( InReportSettings.BulletText );
							OutputWriter.WriteLine( CurLine );
						}
					}
				}
			}
		}



		/// <summary>
		/// Sorts categories alphabetically by name
		/// </summary>
		class CategorySortComparer : IComparer< ProjectCategory >
		{
			// IComparer: Compares two objects and returns a value indicating whether one is less than,
			//     equal to, or greater than the other.
			public int Compare( ProjectCategory x, ProjectCategory y )
			{
				return x.Name.CompareTo( y.Name );
			}
		}


		/// <summary>
		/// Sorts changes based on change type, then alphabetically by summary
		/// </summary>
		class ChangeSortComparer : IComparer<ChangeDesc>
		{
			// IComparer: Compares two objects and returns a value indicating whether one is less than,
			//     equal to, or greater than the other.
			public int Compare( ChangeDesc x, ChangeDesc y )
			{
				// Sort categories such that changes of types earlier in our enum appear first
				if( x.Type < y.Type )
				{
					return -1;
				}
				else if( x.Type > y.Type )
				{
					return 1;
				}

				// Same type, so simply sort alphabetically by description
				return x.BriefDescription.CompareTo( y.BriefDescription );
			}
		}


		/// <summary>
		/// Recursively sorts categories
		/// </summary>
		/// <param name="InReportSettings">Settings</param>
		/// <param name="InCategory">Category to sort (along with all child categories)</param>
		static void SortCategoriesRecursively( ReportSettings InReportSettings, ProjectCategory InCategory )
		{
			// Sort categories by name alphabetically.
			InCategory.Subcategories.Sort( new CategorySortComparer() );

			// Sort changes by their type of change (new -> changed -> fix), then alphabetically
			InCategory.Changes.Sort( new ChangeSortComparer() );

			// Recurse!
			foreach( var CurSubcategory in InCategory.Subcategories )
			{
				SortCategoriesRecursively( InReportSettings, CurSubcategory );
			}
		}
	}
}