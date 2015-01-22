// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Configuration;
using System.Text;
using System.Text.RegularExpressions;

namespace TwikiToMarkdownFile
{
    /// <summary>
    /// TwikiToMarkdown conversion tool for Epic's Twiki documents to Markdown.
    /// </summary>
    public class TwikiToMarkdownFile
    {
        #region Constructors and Options

        /// <summary>
        /// TwikiToMarkdown Constructor
        /// </summary>
        public TwikiToMarkdownFile()
        {
        }

        private static readonly Dictionary<string, string> _codeBlocks = new Dictionary<string, string>();

        #endregion

        private static int _tabWidth = 4;

        private Dictionary<string, string> twikiMarkdownLookup;

        private int CountOfContinueTableExcludes = 0;

        /// <summary>
        /// maximum nested depth of [] and () supported by the transform; implementation detail
        /// </summary>
        private const int _nestDepth = 6;

        /// <summary>
        /// Allow calling process to set the dictionary lookup values for twiki to markdown file locations
        /// </summary>
        public Dictionary<string,string> TwikiMarkdownLookup
        {
            set { twikiMarkdownLookup = value; }
            get { return twikiMarkdownLookup; }
        }


        // Match lines with TopicInfo meta tag
        private static Regex TopicInfoMetaMatch = new Regex(@"\%"+"META:TOPICINFO.+author=\"([^\"]+)\" date=\"([^\"]+)\" format=\"([^\"]+)\" version=\"([^\"]+)\".+"+@"\%\n", RegexOptions.Multiline | RegexOptions.Compiled);
        
        // Match lines with TopicParent meta tag
        private static Regex TopicParentMetaMatch = new Regex(@"\%" + "META:TOPICPARENT.+name=\"([^\"]+)\".+" + @"\%$\n", RegexOptions.Multiline | RegexOptions.Compiled);

        // Match lines with Field meta tag
        private static Regex FieldMetaMatch = new Regex(@"\%" + "META:FIELD.+name=\"[^\"]+\" attributes=\"[^\"]*\" title=\"([^\"]+)\" value=\"([^\"]*)\".+" + @"\%$\n", RegexOptions.Multiline | RegexOptions.Compiled);

        // Match lines with meta tags to be deleted
        private static Regex RemoveMetaMatch = new Regex(@"\%" + "META:[FILEATTACHMENT|TOPICMOVED|FORM].+$\n", RegexOptions.Multiline | RegexOptions.Compiled);
 
        private string MetaFieldInfo(Match match, ref String MetaData)
        {
            //Only keep Document availability
            if (match.Groups[1].Value.ToLower() == "document availability")
            {
                MetaData += match.Groups[1].Value + ":" + match.Groups[2].Value + "\n";
            }
            return "";
        }

        private string DoMetaData(string text)
        {

            String MetaData = "";
            //TOPICINFO metadata
            text = TopicInfoMetaMatch.Replace(text, "");
            //TOPICPARENT metadata
            text = TopicParentMetaMatch.Replace(text, "");
            //FIELD metadata
            text = FieldMetaMatch.Replace(text, EveryMatch => MetaFieldInfo(EveryMatch, ref MetaData));
            // Meta data to be removed
            text = RemoveMetaMatch.Replace(text, "");

            //Delete any other meta.

            return MetaData+text;
        }

        // Match lines with br's to be replaced where they are the last item on the line
        private static Regex ReplaceNewLinesTwiki = new Regex("<br/*>[ ]*\n|%BR%[ ]*\n", RegexOptions.Multiline | RegexOptions.Compiled);

        // Match any remaining br's
        private static Regex ReplaceOtherNewLinesTwiki = new Regex("%BR%[ ]*", RegexOptions.Multiline | RegexOptions.Compiled);


        /// <summary>
        /// Twiki formating br replaced
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceNewLines(string text)
        {
            //return two spaces twiki syntax for br.
            text = ReplaceNewLinesTwiki.Replace(text, "  \n");
            return ReplaceOtherNewLinesTwiki.Replace(text, "<br />");
        }


        // Match lines with meta tags to be deleted
        private static Regex RemoveUnusableTwiki = new Regex("<!--(" + @"\S|\s" + ")*?-->|<.*noautolink>", RegexOptions.Multiline | RegexOptions.Compiled);
        // removed |<div style=\"padding(.+\n)+</div>\n from above.

        private string RemoveUnused(Match match)
        {

            if (match.Groups[0].Value.ToUpper().Contains("STOPPUBLISH") || match.Groups[0].Value.ToUpper().Contains("STARTPUBLISH"))
            {
                // these must not be removed
                return match.Groups[0].Value;
            }
            else
            {
                //return a space to cover cases where someone has placed <br/> in place of a space in the document. 
                return (" ");
            }
        }

        /// <summary>
        /// Twiki formating unusable removed here
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoCleanUnused(string text)
        {
            return RemoveUnusableTwiki.Replace(text, RemoveUnused);
        }


        // Match header text combine with removing horizontal rules around h1 and h2
        private static Regex MatchTwikiHeaders = new Regex(@"[\n]*(<hr[^>]*?>\n)?^---[-]*(?<HeaderDepth>\++)(?<HeaderContent>.*)(\n<hr[^>]*?>)?", RegexOptions.Multiline | RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);
 

        /// <summary>
        /// For each + matched with MatchTwikiHeaders replace with # and remove the --- formatting
        /// </summary>
        /// <param name="match">regex match for headers</param>
        /// <param name="Title">Title metadata parameter</param>
        /// <returns></returns>
        private string ReplaceHeaders(Match match, StringBuilder Title)
        {
            StringBuilder Text = new StringBuilder();
            // Header 1 should be set to title, only take the first header 1 for this.
            if (Title.Length == 0 && match.Groups["HeaderDepth"].Value.Length == 1)
            {
                Title.Append("Title:");
                Title.Append(match.Groups["HeaderContent"].Value);
                Title.Append("\n");
            }
            else
            {
                Text.Append("\n\n\n");// ensure new line before

                for (int i = 0; i < match.Groups["HeaderDepth"].Value.Length; ++i)
                {
                    Text.Append("#");
                }

                Text.Append(match.Groups["HeaderContent"].Value);
            }
            return Text.ToString();
        }
        
        /// <summary>
        /// Replace twiki formatted headers with markdown
        /// </summary>
        /// <param name="text">text to check for headers</param>
        /// <returns></returns>
        private string DoReplaceHeaders(string text)
        {
            StringBuilder Title = new StringBuilder();
            text = MatchTwikiHeaders.Replace(text, EveryMatch => ReplaceHeaders(EveryMatch, Title));
            return Title.Append(text).ToString();
        }

        // Match bookmark text
        private static Regex MatchTwikiBookmarks = new Regex(@"^(\#.+)", RegexOptions.Multiline | RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);

        /// <summary>
        /// Replace twiki #Bookmark with markdown (#Bookmark)
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceBookmarks(Match match)
        {
            return "(" + match.Groups[1].Value.Replace(" ","") + ")";
        }

        /// <summary>
        /// Replace twiki fomatted bookmarks with markdown
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceBookmarks(string text)
        {
            return MatchTwikiBookmarks.Replace(text, ReplaceBookmarks);
        }

        // Match table followed by anything not \n
        private static Regex MatchTableNoNextReturn = new Regex(@"(\|\n)([^\n\| ]+)", RegexOptions.Multiline | RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);

        private static Regex MatchStyleLines = new Regex("(<style type=\"text/css\" media=\"all\">@import[ ]*\"|<link rel=\"stylesheet\" href=\")(/pub/|%ATTACHURL%/)([^\"]*?\")(;</style>| type=\"text/css\" media=\"screen\" />)", RegexOptions.Compiled);

        /// <summary>
        /// Some ad hoc cleaning required for specific files
        /// </summary>
        /// <param name="Text"></param>
        /// <returns></returns>
        private string DoCleanAdHoc(string Text)
        {
            //Ad-hoc where a table is immediately followed by a non-empty line then insert a new line between them so that later in our script run we can detect the table.
            Text = MatchTableNoNextReturn.Replace(Text, "$1\n$2");

            //Ad-hoc make sure the page looks to the css folder for style sheets
            Text = MatchStyleLines.Replace(Text, "<style type=\"text/css\" media=\"all\">@import \"[RELATIVEHTMLPATH]/Include/css/$3;</style>");

            return Text;
        }

        private static Regex MatchBreadcrumbDiv = new Regex("<div style=\"padding:0px 0px 0px 4px;font:10px Verdana;\">\n((.*\n)*?)(?=</div>)</div>", RegexOptions.Multiline | RegexOptions.Compiled);
        /// <summary>
        /// Convert old breadcrumb div
        /// </summary>
        /// <param name="Text"></param>
        /// <returns></returns>
        private string DoBreadcrumbConversion(string Text)
        {
            StringBuilder Crumbs = new StringBuilder();

            Text = MatchBreadcrumbDiv.Replace(Text, EveryMatch => CrumbProcessor(EveryMatch, Crumbs));
            return Crumbs.Append(Text).ToString();
        }

        private static Regex MatchBreadcrumbLinkData = new Regex(@"[^\(]*?\(([^\)]*?)\)", RegexOptions.Compiled);

        /// <summary>
        /// Crumb processor
        /// </summary>
        /// <param name="match">Crumbs information was found in the document</param>
        /// <param name="Crumbs">Store for the crumbs document links</param>
        /// <returns></returns>
        private string CrumbProcessor(Match match, StringBuilder Crumbs)
        {
            //Match 1 is crumbs data with no div surrounding, split by rows
            foreach (string CrumbRow in match.Groups[1].Value.Split('\n'))
            {
                //Get just the link text into a MatchCollection
                MatchCollection CrumbDataFromRow = MatchBreadcrumbLinkData.Matches(CrumbRow);


                for (int i = 0; i<CrumbDataFromRow.Count; ++i)
                {
                    //When matches were found add crumbs data
                    if (i == 0)
                    {
                        Crumbs.Append("Crumbs:");
                    }
                    Crumbs.Append(CrumbDataFromRow[i].Groups[1].Value.Trim());

                    //comma separate values, exclude last value
                    if (i == CrumbDataFromRow.Count - 1)
                    {
                        Crumbs.Append("\n");
                    }
                    else
                    {
                        Crumbs.Append(", ");
                    }
                }
            }
            //Replace entire existing crumbs with nothing
            return "";
        }

        private static Regex MatchHeadersWithHRs = new Regex(@"(\*{3}(" + "\n)+)?(#{1,2}.*\n+)" + @"\*{3}\n", RegexOptions.Multiline | RegexOptions.Compiled);
        /// <summary>
        /// remove the horizontal rules any places where put under h2 headings or surrounding the h1 heading
        /// </summary>
        /// <param name="Text"></param>
        /// <returns></returns>
        private string DoRemoveHRsFromHeaders(string Text)
        {
            Text = MatchHeadersWithHRs.Replace(Text, "\n$3");

            return Text;
        }

        // Match bold highlighted text where * is not followed by =, *= common programing language symbol
        private static Regex MatchTwikiBold = new Regex(@"(^|\W)(\*)([^ ].*?[^ ])\2(?=\W|$)", RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline | RegexOptions.Compiled);

        /// <summary>
        /// Replace twiki *Bold* with markdown **Bold**
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceBold(Match match)
        {
            return match.Groups[1].Value + "**" + match.Groups[3].Value + "**" + match.Groups[4].Value;
        }
        
        /// <summary>
        /// Replace twiki formatted bold highlighted text with markdown syntax
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceBold(string text)
        {
            return MatchTwikiBold.Replace(text, ReplaceBold);
        }

        // Match bold fixed highlighted text
        //private static Regex MatchStrictTwikiBoldFixed = new Regex(@"(^|\W)(==)([^ ].*?[^ ])\2(?=\W|$)",
        private static Regex MatchStrictTwikiBoldFixed = new Regex(@"(^|[\W])(==) (?=\S) ([^\r]*?\S[=]*) \2 (?=[\W]|$)",
            RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline | RegexOptions.Compiled);

        // Match fixed highlighted text
        private static Regex MatchStrictTwikiFixed = new Regex(@"(^|\W)(=)([^ ].*?[^ ])\2(?=\W|$)",
            RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline | RegexOptions.Compiled);

        // Match fixed highlighted text with = at end
        private static Regex MatchStrictTwikiFixedWithEquality = new Regex(@"(^|\W)(=)(\S*?\2)\2(?=\W|$)",
            RegexOptions.IgnorePatternWhitespace | RegexOptions.Multiline | RegexOptions.Compiled);
        
        /// <summary>
        /// Replace twiki ==Bold Fixed== with markdown **`Bold Code span`**
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceBoldFixed(Match match)
        {
            return match.Groups[1].Value + "**`" + match.Groups[3].Value + "`**" + match.Groups[4].Value;
        }

        /// <summary>
        /// Replace twiki =Fixed= with markdown `Code span`
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceFixed(Match match)
        {
            return match.Groups[1].Value + "`" + match.Groups[3].Value + "`" + match.Groups[4].Value;
        }

        /// <summary>
        /// Replace twiki =Fixed== with markdown `Code span=` equality is an edge case, the symbol = is used for a lot in twiki
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceFixedEquality(Match match)
        {
            return match.Groups[1].Value + "`" + GetHashValueAndStore(match.Groups[3].Value) + "`" + match.Groups[4].Value;
        }

        /// <summary>
        /// Replace twiki formatted fixed formatting styles with markdown syntax
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceFixed(string text)
        {
            
            //Ordering of these three is important
            text = MatchStrictTwikiBoldFixed.Replace(text, ReplaceBoldFixed);
            //Edge case where we have xyz= within =xyz== convert to `xyz=`
            text = MatchStrictTwikiFixedWithEquality.Replace(text, ReplaceFixedEquality);
            //Encode any == left so that are not considered part of the fixed
            text = text.Replace("==", GetHashValueAndStore("=="));
            return MatchStrictTwikiFixed.Replace(text, ReplaceFixed);
        }

        // Find tables with \ to indicate new line
        private static Regex MatchTableCellWithContinuation = new Regex(@"(?<=\|\\?\n?)([^\\|\|]*\\\n[^|]*)(?=\n?\|)", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);
        
       // Match tables
        private static string MatchTableRow = @"[ ]*?\|[^\n]*?\n";

        private static Regex BreakLine = new Regex(@"((\|)[^\|]+?)+", RegexOptions.Singleline | RegexOptions.Compiled);

        private static Regex MatchTwikiTextTable = new Regex(@"^\%TABLE([^\%]+\%)*?\n", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);
        
        private static Regex MatchTwikiTable = new Regex(@"
                ^							        # Start of a line
                (" + MatchTableRow + @")            # Get Header
                ((" + MatchTableRow + @")*)         # Optional Body Rows for the case that there is only one row
       ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        private static Regex MatchTwikiTableForRowContinuation = new Regex(@"[ ]*?\|([^\|]*?\|)*?\n\n", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        /// <summary>
        /// Convert first row to a markdown header | xyz | xyz | -> | --- | --- |
        /// must keep same indentation
        /// </summary>
        /// <param name="HeaderColumns"></param>
        /// <returns></returns>
        private string InsertHeader(string[] HeaderColumns)
        {
            
            string text = HeaderColumns[0]+"|";

            //Ignore 1st and last |
            for (int i = 2; i < HeaderColumns.Length; ++i)
            {
                text += " --- |";
            }
            return text +"\n";
        }

        /// <summary>
        /// Replace twiki formatted tables with markdown where they do not contain the text TABLE
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceNoTableTable(Match match)
        {
            // table dividers
            string text;
            string[] HeaderColumns = match.Groups[1].Value.Split('|');

            //Is the first row marked as twiki header?
            if (Regex.IsMatch(HeaderColumns[1], @"\*\*"))
            {
                // Table header
                text = match.Groups[1].Value;
                // Divider data
                text += InsertHeader(HeaderColumns);
            }
            else
            {
                // Divider data
                text = InsertHeader(HeaderColumns);
                // First row of table data
                text += match.Groups[1].Value;
            }


            // Table data
            text += match.Groups[2].Value;

            return text;
        }

        private string ReplaceTableWithContinuation(Match match)
        {
            //handle rows with continuation
            List<string> ExcerptSections = new List<string>();

            //Handle cell continues over multiple rows with \
            string text = MatchTableCellWithContinuation.Replace(match.Groups[0].Value, EveryMatch => ProcessCellWithRowContinuation(EveryMatch, ExcerptSections));


            //Do we have excerpt sections to add in, if so insert html comments around
            if (ExcerptSections.Count > 0)
            {
                text += "<!--\n";

                foreach (string ExcerptSection in ExcerptSections)
                {
                    text += ExcerptSection + "\n";
                }
                text += "-->\n";
            }

             return text;
        }

        private string ProcessCellWithRowContinuation(Match match, List<string> ExcerptSections)
        {
            //Replace cell with continuation character with include section instead
            ++CountOfContinueTableExcludes;

            string ExludeIncludeName = "TableInclude" + CountOfContinueTableExcludes;

            StringBuilder Excerpt = new StringBuilder("[EXCERPT:");
            Excerpt.Append(ExludeIncludeName);
            Excerpt.Append("]\n");
            //Added the table cell contents replace \ character from end of lines
            Excerpt.Append(match.Groups[1].Value.Replace("\\\n", "\n"));
            Excerpt.Append("\n[/EXCERPT:");
            Excerpt.Append(ExludeIncludeName);
            Excerpt.Append("]\n");

            ExcerptSections.Add(Excerpt.ToString());

            return "[INCLUDE:#" + ExludeIncludeName + "]";
        }

        /// <summary>
        /// Replace twiki formatted tables with markdown format.
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceTables(string text)
        {
            text = MatchTwikiTableForRowContinuation.Replace(text, ReplaceTableWithContinuation);

            text = MatchTwikiTable.Replace(text, ReplaceNoTableTable);

            //Remove twiki formatting like %TABLE{cellpadding="5"}%
            text = MatchTwikiTextTable.Replace(text, "");

            return text;
        }

        //Find twiki formated internal links
        private static Regex MatchTwikiInternalLinks = new Regex(@"\[\[(\#[^\]]+)\](\[[^\]]+\])\]", RegexOptions.Singleline | RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);

        /// <summary>
        /// Replace twiki formatted internal links with markdown
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceInternalLinks(Match match)
        {
            //remove spaces from bookmarks for markdown compatability, remove underscores
            return match.Groups[2].Value + "(" + match.Groups[1].Value.Replace(" ", "").Replace("_", "") + ")";
        }
        
        /// <summary>
        /// Replace twiki formatted internal links with markdown
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceInternalLinks(string text)
        {
            return MatchTwikiInternalLinks.Replace(text, ReplaceInternalLinks);
        }

        //Find twiki formated internal links [[xyz]] or [[xyz][abc]]
        private static Regex MatchTwikiExternalLinks = new Regex(@"\[\[([^\]]+)\](\[([^\]]+)\])*\]", RegexOptions.Singleline | RegexOptions.Compiled | RegexOptions.IgnorePatternWhitespace);

        //Find ahref style links
        private static Regex MatchAhrefExternalLinks = new Regex("<a href=\"([^\"]+)\">([^<]+)</a>");

        /// <summary>
        /// Replace twiki formatted external links with markdown
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceExternalLinks(Match match)
        {
            if (match.Groups[2].Value.Length > 0)
            {
                if (match.Groups[1].Value.Contains("http"))
                {
                    return match.Groups[2].Value + "(" + match.Groups[1].Value + ")";
                }
                else if (match.Groups[1].Value.Contains("ATTACHURL"))                    
                {
                    //some special case handling for thumbnails, images that link to other file types, and download zip content.
                    if (match.Groups[2].Value.Contains("ATTACHURLPATH"))
                    {
                        //If we are matching on a thumbnail lets get it working here
                        string linkto = match.Groups[1].Value.Replace("%ATTACHURLPATH%/", "");

                        string image = DoReplaceImages(match.Groups[2].Value);

                        return image + "(" + linkto + ")";
                    }
                    else if (match.Groups[2].Value.Contains("src"))
                    {
                        //Thumbnail to other type of file
                        string image = DoReplaceImages(match.Groups[2].Value);
                        return image + "(" +  match.Groups[1].Value.Replace("%ATTACHURL%/", "") + ")";
                    }
                    else
                    {
                        //standard internal link to file
                        return match.Groups[2].Value + "(" + match.Groups[1].Value.Replace("%ATTACHURL%/", "") + ")";
                    }
                }
                else
                {
                    
                    //if can match on value in the TwikiMarkdownLookup dictionary migrate to new location of file
                    if (TwikiMarkdownLookup.ContainsKey(match.Groups[1].Value.Replace(" ", "")))
                    {
                        return match.Groups[2].Value + "(" + TwikiMarkdownLookup[match.Groups[1].Value.Replace(" ", "")] + ")";
                    }
                    //Or might be a # reference into the file
                    else if (TwikiMarkdownLookup.ContainsKey(Regex.Replace(match.Groups[1].Value.Replace(" ", ""), "#.*", "")))
                    {
                        return match.Groups[2].Value + "(" + TwikiMarkdownLookup[Regex.Replace(match.Groups[1].Value.Replace(" ", ""), "#.*", "")] + Regex.Replace(match.Groups[1].Value.Replace(" ", ""), "[^#]*?#", "#") + ")";
                    }
                    else
                    {
                        return match.Groups[2].Value + "(" + match.Groups[1].Value.Replace("Main.", "Main/").Replace("Two.", "Two/").Replace(" ", "") + ")";
                    }
                }
            }
            else
            {
                if (match.Groups[1].Value.Contains("http"))
                {
                    return "[" + match.Groups[1].Value + "](" + match.Groups[1].Value + ")";
                }
                else
                {
                    //if can match on value in the TwikiMarkdownLookup dictionary migrate to new location of file
                    if (TwikiMarkdownLookup.ContainsKey(match.Groups[1].Value.Replace(" ","")))
                    {
                        return "[" + match.Groups[1].Value + "](" + TwikiMarkdownLookup[match.Groups[1].Value.Replace(" ", "")] + ")";
                    }
                    else
                    {
                        return "[" + match.Groups[1].Value + "](" + match.Groups[1].Value.Replace("Main.", "Main/").Replace("Two.", "Two/").Replace(" ", "") + ")";
                    }
                }
            }            
        }

        /// <summary>
        /// Replace ahref formatted external links with markdown
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceAhrefExternalLinks(Match match)
        {
                if (match.Groups[1].Value.Contains("http") || match.Groups[1].Value.Contains("mailto:"))
                {
                    return "[" + match.Groups[2].Value + "](" + match.Groups[1].Value + ")";
                }
                else
                {
                    return "[" + match.Groups[2].Value + "](" + match.Groups[1].Value.Replace(".", "/").Replace(" ", "") + ")";
                }
        }


        /// <summary>
        /// Replace twiki formatted internal links with markdown
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceExternalLinks(string text)
        {
            text = MatchTwikiExternalLinks.Replace(text, ReplaceExternalLinks);
            return MatchAhrefExternalLinks.Replace(text, ReplaceAhrefExternalLinks);
        }

        //Find twiki formated underlines
        private static Regex MatchTwikiUnderscores = new Regex(@"<div[^\>]+><hr[^>]+></div>|<hr[^>]+>|^[ ]{0,3}---", RegexOptions.Multiline | RegexOptions.Compiled);

       
        /// <summary>
        /// Replace div margins and hr lines with markdown empty string
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceUnderscores(string text)
        {
            return MatchTwikiUnderscores.Replace(text, "");
        }

        //Find twiki formated images
        private static Regex MatchTwikiImages = new Regex(@"
                                                            (\[\[([^\]]+)\])?                                                               #Optional Link To = 2
                                                            (<a[ ]href=" + "\""+@"%ATTACH[^/]*?/([^"+"\""+@"]*?)"+"\""+@"[^>]*?>)?             #Optionally images is thumbnail with link 4 = link destination
                                                            ((<img[ ])?(%IMAGE{)?src=" + "\"" + @")?                                                   #Optionally be html img src
                                                            ((?:%ATTACH)|(?:%PUBURL%/%WEB%/))[^/]+/((.*?)(.jpg|.png|.gif|.tga|.bmp|.JPG|.PNG|.GIF|.TGA|.BMP))                                             #Must contain an image filename to include fullstop and 3 or 4 characters extension 9=Filename, 10= Filename without extension, 11=extension
                                                            (" + "\"" + @")?                                                            #Optionally end of image may have a quote mark if this was an img src
                                                            ([ ]*alt=" + "\"" + @"([^" + "\"" + @"]+)" + "\"" + @")?                    #Optionally have alt text. 14 = alt text
                                                            ([ ]* border=[\'|" + "\"" + @"][^\'|^" + "\"" + @"]+[\'|" + "\"" + @"])?   #Optionally have border specified
                                                            ([ ]* width=[\'|" + "\"" + @"]([^\'|^" + "\"" + @"]+)[\'|" + "\"" + @"])?   #Optionally have width specified 16
                                                            ([ ]* height=[\'|" + "\"" + @"]([^\'|^" + "\"" + @"]+)[\'|" + "\"" + @"])?  #Optionally have height specified 18
                                                            (([ ]*/>)?([ ]*</a>)?([ ]*}%\]\])?([ ]*}%)?)                                                          #Optionally have some html formatting depending on the type of image we are matching against
                                                        ", RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        /// <summary>
        /// Replace the many syles of images Epic has used in udn with Markdown.
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceTwikiImages(Match match)
        {
            string text;

            //Do we have alt text?
            if (match.Groups[14].Value.Length>0)
            {
                text="!["+match.Groups[14].Value+"]("+match.Groups[9].Value+")";
            }
            else
            {
                text="!["+match.Groups[9].Value+"]("+match.Groups[9].Value+")";
            }

            //Width and/or Height information?
            if (match.Groups[17].Value.Length >0 || match.Groups[19].Value.Length>0)
            {
                text +="(";
                if (match.Groups[17].Value.Length > 0)
                {
                    text += "w:" + match.Groups[17].Value + " ";
                }
                if (match.Groups[19].Value.Length > 0)
                {
                    text += "h:" + match.Groups[19].Value;
                }
                text +=")";
            }
            //Do we have a thumbnail type image
            if(match.Groups[4].Value.Length>0)
            {
                text = "[" + text + "](./images/" + match.Groups[4].Value + ")";
            }
            //Do we have an image in link?
            else if (match.Groups[2].Value.Length > 0)
            {
                text = "[" + text + "](" + match.Groups[2].Value + ".html)";
            }

            return text;
        }
        
        
        /// <summary>
        /// Replace div margins and hr lines with markdown horizontal rules
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceImages(string text)
        {
            //text = MatchTwikiImages.Replace(text, ReplaceTwikiImages);
            //return MatchTwikiForeignImages.Replace(text, ReplaceTwikiImages);
            return MatchTwikiImages.Replace(text, ReplaceTwikiImages);
        }

        private static Regex MatchTOC = new Regex(@"%TOC[^%]*%", RegexOptions.Multiline | RegexOptions.Compiled);
        private static Regex MatchTOCWithTitles = new Regex("%TOC{title=\"([^\"]+)\"[^%]+%", RegexOptions.Multiline | RegexOptions.Compiled);
 
        /// <summary>
        /// Replace TOC with titles
        /// </summary>
        /// <param name="match"></param>
        /// <returns></returns>
        private string ReplaceTOCWithTitles(Match match)
        {
            return (match.Groups[1].Value + "\n\n"+"[TOC]");
        }


        /// <summary>
        /// Replace TOC rules
        /// </summary>
        /// <param name="text"></param>
        /// <returns></returns>
        private string DoReplaceTOC(string text)
        {
            text = MatchTOCWithTitles.Replace(text, ReplaceTOCWithTitles);
            return MatchTOC.Replace(text, "[TOC]");
        }

        private const string _markerUL = @"[*]";
        private const string _markerOL = @"\d+[.]?";
        private const string _markerDL = @"<b>.*</b>:";
        
        private static string _wholeList = string.Format(@"
            (                               # $1 = whole list
              (                             # $2
                [ ]{{3}}
                ({0})                       # $3 = first list item marker
                [ ]+
              )
              (?s:.+?)
              (                             # $4
                  \z
                |
                  \n{{1,}}
                  (?=\S)
                  (?!                       # Negative lookahead for another list item marker
                    [ ]*
                    {0}[ ]+
                  )
              )
            )", string.Format("(?:{0}|{1}|{2})", _markerUL, _markerOL, _markerDL), 4 - 1);

        private static Regex MatchList = new Regex(@"(?:(?<=\n)|\A\n?)" + _wholeList,
            RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);


        private static Regex MatchStartOfLineSpaces = new Regex(string.Format(@"^([ ]*(?={0}|{1}|{2}))", _markerUL, _markerOL, _markerDL), RegexOptions.Multiline | RegexOptions.Compiled);

        private string DoMatchList(Match match)
        {
            string ListText = match.Groups[1].Value;
            
            //Convert spaces at the start of the lines
            ListText = MatchStartOfLineSpaces.Replace(ListText, EveryMatch => ConvertToMarkdownStartOfLineSpacesForLists(EveryMatch.Groups[1].Value));

            ListText = DoReplaceOrderedLists(ListText);
            ListText = DoReplaceDefinitionLists(ListText);

            //new line at front and back to convert twiki delimiters to markdown
            return "\n" + ListText + "\n";
        }

        private string DoAddNewLineAtStartOfList(string text)
        {
            return MatchList.Replace(text, DoMatchList);
        }

        private static string ConvertToMarkdownStartOfLineSpacesForLists(string text)
        {
            //3,6,9... -> 0,4,8... spaces
            if (text.Length < 4)
            {
                return "";
            }
            else
            {
                return "".PadLeft(((text.Length / 3) - 1) * 4);
            }
        }

        private static Regex MatchOrderedListLine = new Regex("^([ ]*)([0-9]+)[ ]+(-*)[ ]*", RegexOptions.Multiline | RegexOptions.Compiled);

        private string DoMatchOrderedList(Match match)
        {
            return match.Groups[1].Value + match.Groups[2].Value + ". ";
        }
        
        private string DoReplaceOrderedLists(string text)
        {
            return MatchOrderedListLine.Replace(text, DoMatchOrderedList);
        }

        private static Regex MatchDefinitionListLine = new Regex("^([ ]*)<b>(.*)</b>[ ]*:[ ]*", RegexOptions.Multiline | RegexOptions.Compiled);

        private string DoMatchDefinitionList(Match match)
        {
            return match.Groups[1].Value + "$ " + match.Groups[2].Value + " : ";
        }

        private string DoReplaceDefinitionLists(string text)
        {
            return MatchDefinitionListLine.Replace(text, DoMatchDefinitionList);
        }

       
        private static Regex MatchEscapes = new Regex(@"(\\[\`|\*|_|{|}|\[|\]|\(|\)|>|#|\+|-|\.|!])");
        /// <summary>
       /// Markdown allows character sequences to be escaped such as _ if a \ is placed in front of them.
       /// some of the twiki data contains formating characters after a \ which stops the format. The \ needs to be escaped.
       /// </summary>
       /// <param name="text"></param>
       /// <returns></returns>
        private string DoEscapeEscapes(string text)
        {
            return MatchEscapes.Replace(text,"\\$1");
        }

        private static Regex MatchCodeBlocks = new Regex(@"<verbatim>((\s|\S)*?)</verbatim>", RegexOptions.Multiline | RegexOptions.Compiled);

        private static Regex MatchCodeLines = new Regex(@"^", RegexOptions.Multiline | RegexOptions.Compiled);

        private string DoFormatCodeBlocks(Match match)
        {
            //if no return characters this should be matched to code section not block.
            if (!match.Groups[1].Value.Contains("\n"))
            {
                return GetHashValueAndStore("`" + match.Groups[1].Value + "`");
            }
            else
            {
                return GetHashValueAndStore("\n" + MatchCodeLines.Replace(match.Groups[1].Value, "    ") + "\n\n");//Blank line to follow code block
            }
        }

        private static Regex MatchCodeBlockList = new Regex(@"(\*[^\n]*?\n{1,})(<verbatim>(\s|\S)*?</verbatim>)",
            RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        //private static Regex MatchCodeBlockListIndented = new Regex(@"(([ ]*)\*[^\n]*?\n{1,})(\2<verbatim>[ ]*\n((\s|\S)*?)[ ]*</verbatim>[ ]*\n)",
        private static Regex MatchCodeBlockListIndented = new Regex(@"(([ ]*)\*[^\n]*?\n{1,})([ ]*<verbatim>((\s|\S)*?)[ ]*</verbatim>[ ]*\n)",
            RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

/*        private string DoFormatListCodeBlocks(Match match)
        {
            //insert ` ` prior to code to ensure that it is detected in markdown.
            return match.Groups[1].Value + "` `\n" + match.Groups[2].Value + "\n` `\n";
        }*/

        private string DoFormatListCodeBlocksIndented(Match match)
        {
            //insert ` ` prior to code to ensure that it is detected in markdown.
            //return match.Groups[1].Value + "` `\n" + match.Groups[2].Value + "\n` `\n";

            // Split the code into lines so that we can ensure each line is indented by the same as the list that it should be part of
            // and then the ammount required by a code block 4 spaces, plus two for indentation of the asterix and space
            string[] CodeLines = match.Groups[4].Value.Split('\n');

            for (int i = 0; i < CodeLines.Length; i++)
            {
                CodeLines[i] = "      " + match.Groups[2].Value + CodeLines[i];
            }

            return GetHashValueAndStore(match.Groups[1].Value + string.Join("\n", CodeLines) + "\n");
        }

        private static string GetHashValueAndStore(string s)
        {
            string key = "\x1A" + Math.Abs(s.GetHashCode()).ToString() + "\x1A";
            _codeBlocks[key] = s;
            return key;
        }



        private string DoReplaceCodeBlocks(string text)
        {
            //find code blocks indented by the same as the list
            text = MatchCodeBlockListIndented.Replace(text, DoFormatListCodeBlocksIndented);

            //find lists immediately followed by code blocks
            //text = MatchCodeBlockList.Replace(text, DoFormatListCodeBlocks);

            return MatchCodeBlocks.Replace(text, DoFormatCodeBlocks);
        }

        private void Setup()
        {
            CountOfContinueTableExcludes = 0;
            _codeBlocks.Clear();
        }


        private static Regex _unescapes = new Regex("\x1A\\d+\x1A", RegexOptions.Compiled);
        

        private string DoUnhashCodeBlocks(string text)
        {
            return _unescapes.Replace(text, new MatchEvaluator(UnescapeEvaluator));
        }
        private string UnescapeEvaluator(Match match)
        {
            return _codeBlocks[match.Value];
        }

        private static readonly Regex _blocksDiv = new Regex(GetDivBlockPattern(), RegexOptions.Multiline | RegexOptions.IgnorePatternWhitespace);

        /// <summary>
        /// derived pretty much verbatim from PHP Markdown
        /// </summary>
        private static string GetDivBlockPattern()
        {

            // Hashify HTML blocks:
            // We only want to do this for block-level HTML tags, such as headers,
            // lists, and tables. That's because we still want to wrap <p>s around
            // "paragraphs" that are wrapped in non-block-level tags, such as anchors,
            // phrase emphasis, and spans. The list of tags we're looking for is
            // hard-coded:
            //
            // *  List "a" is made of tags which can be both inline or block-level.
            //    These will be treated block-level when the start tag is alone on 
            //    its line, otherwise they're not matched here and will be taken as 
            //    inline later.
            // *  List "b" is made of tags which are always block-level;
            //
            string blockTagsA = "ins|del";

            string blockTagsB = "div";

            // Regular expression for the content of a block tag.
            string attr = @"
            (?>				            # optional tag attributes
              \s			            # starts with whitespace
              (?>
                [^>""/]+	            # text outside quotes
              |
                /+(?!>)		            # slash not followed by >
              |
                ""[^""]*""		        # text inside double quotes (tolerate >)
              |
                '[^']*'	                # text inside single quotes (tolerate >)
              )*
            )?	
            ";

            string content = RepeatString(@"
                (?>
                  [^<]+			        # content without tag
                |
                  <\2			        # nested opening tag
                    " + attr + @"       # attributes
                  (?>
                      />
                  |
                      >", _nestDepth) +   // end of opening tag
                      ".*?" +             // last level nested tag content
            RepeatString(@"
                      </\2\s*>	        # closing nested tag
                  )
                  |				
                  <(?!/\2\s*>           # other tags with a different name
                  )
                )*", _nestDepth);

            string content2 = content.Replace(@"\2", @"\3");

            // First, look for nested blocks, e.g.:
            // 	<div>
            // 		<div>
            // 		tags for inner block must be indented.
            // 		</div>
            // 	</div>
            //
            // The outermost tags must start at the left margin for this to match, and
            // the inner nested divs must be indented.
            // We need to do this before the next, more liberal match, because the next
            // match will start at the first `<div>` and stop at the first `</div>`.
            string pattern = @"
            (?>
                  (?>
                    (?<=\n)     # Starting after a blank line
                    |           # or
                    \A\n?       # the beginning of the doc
                  )
                  (             # save in $1

                    # Match from `\n<tag>` to `</tag>\n`, handling nested tags 
                    # in between.
                      
                        [ ]{0,$less_than_tab}
                        <($block_tags_b_re)   # start tag = $2
                        $attr>                # attributes followed by > and \n
                        $content              # content, support nesting
                        </\2>                 # the matching end tag
                        [ ]*                  # trailing spaces
                        (?=\n+|\Z)            # followed by a newline or end of document

                  | # Special version for tags of group a.

                        [ ]{0,$less_than_tab}
                        <($block_tags_a_re)   # start tag = $3
                        $attr>[ ]*\n          # attributes followed by >
                        $content2             # content, support nesting
                        </\3>                 # the matching end tag
                        [ ]*                  # trailing spaces
                        (?=\n+|\Z)            # followed by a newline or end of document
                      
                  | # Special case just for <hr />. It was easier to make a special 
                    # case than to make the other regex more complicated.
                  
                        [ ]{0,$less_than_tab}
                        <(hr)                 # start tag = $2
                        $attr                 # attributes
                        /?>                   # the matching end tag
                        [ ]*
                        (?=\n{2,}|\Z)         # followed by a blank line or end of document
                  
                  | # Special case for standalone HTML comments:
                  
                      [ ]{0,$less_than_tab}
                      (?s:
                        <!-- .*? -->
                      )
                      [ ]*
                      (?=\n{1,}|\Z)            # followed by a blank line or end of document @UE3 changed from 2 or more newlines to 1 or more, the UDN START and STOP publish comments were not encoding properly
                  
                  | # PHP and ASP-style processor instructions (<? and <%)
                  
                      [ ]{0,$less_than_tab}
                      (?s:
                        <([?%])                # $2
                        .*?
                        \2>
                      )
                      [ ]*
                      (?=\n{2,}|\Z)            # followed by a blank line or end of document
                      
                  )
            )";

            pattern = pattern.Replace("$less_than_tab", (_tabWidth - 1).ToString());
            pattern = pattern.Replace("$block_tags_b_re", blockTagsB);
            pattern = pattern.Replace("$block_tags_a_re", blockTagsA);
            pattern = pattern.Replace("$attr", attr);
            pattern = pattern.Replace("$content2", content2);
            pattern = pattern.Replace("$content", content);

            return pattern;
        }

        /// <summary>
        /// this is to emulate what's evailable in PHP
        /// </summary>
        private static string RepeatString(string text, int count)
        {
            var sb = new StringBuilder(text.Length * count);
            for (int i = 0; i < count; i++)
            {
                sb.Append(text);
            }
            return sb.ToString();
        }

        //static Regex DivClassSplit = new Regex("<[ ]*div[ ]*class=\"([^\"])*\">", RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);
        static Regex EndDiv = new Regex("<[ ]*div[ ]*class=\"([^\"]*)\">([\\S|\\s|\\n]*)</div>", RegexOptions.Compiled | RegexOptions.IgnoreCase | RegexOptions.Multiline);


        private string ReplaceDivs(Match match)
        {
            if (match.Groups[2].Value == "div")
            {
                string Text = match.Groups[0].Value;
                string ClassName = EndDiv.Match(Text).Groups[1].Value;
                if (ClassName.Length > 0)
                {
                    //Replace the div at the start and end
                    Text = EndDiv.Replace(Text, @"[REGION: $1]$2[/REGION: $1]");
                    //Replace </div> at the end
                    //EndDiv.Replace(Text, @"[REGION: \1");
                }
                return (Text);
            }
            else
            {
                return match.Groups[0].Value;
            }
        }


        private string DoDivToRegionConversion(string text)
        {
            return _blocksDiv.Replace(text, ReplaceDivs);
        }

        
        /// <summary>
        /// Transforms the provided Twiki-formatted text to Markdown-formatted
        /// </summary>
        /// <remarks>
        /// </remarks>
        public string Transform(string text)
        {
            if (String.IsNullOrEmpty(text))
            {
                return "";
            }

            Setup();

            text = Normalize(text);

            //Metadata
            text = DoMetaData(text);

            //Use '  ' in place of br's
            text = DoReplaceNewLines(text);

            //Clean Unusable Data
            text = DoCleanUnused(text);

            //Run addhoc cleaning
            text = DoCleanAdHoc(text);

            //Replace verbatim tags
            text = DoReplaceCodeBlocks(text);

            //Replace Twiki Bookmarks with Epic's Markdown format, must be before headers are created due to matching on #
            text = DoReplaceBookmarks(text);

            //Replace ---(+)+ with #'s matching number of +'s
            text = DoReplaceHeaders(text);

            //Replace Twiki bold formatting with Markdown
            text = DoReplaceBold(text);

            text = DoReplaceFixed(text);

            //Replace tables
            text = DoReplaceTables(text);

            //Replace Internal Links
            text = DoReplaceInternalLinks(text);

            //Replace External Links
            text = DoReplaceExternalLinks(text);

            //Ensure new line prior to lists
            text = DoAddNewLineAtStartOfList(text);
            
            //Replace Internal Links
            text = DoReplaceUnderscores(text);

            //Replace Images
            text = DoReplaceImages(text);
            
            //Replace %TOC% with [TOC]
            text = DoReplaceTOC(text);

            text = DoUnhashCodeBlocks(text);
            
            text = DoEscapeEscapes(text);

            text = DoBreadcrumbConversion(text);

            text = DoDivToRegionConversion(text);

            return text;
        }

        /// <summary>
        /// convert all tabs to _tabWidth spaces; 
        /// standardizes line endings from DOS (CR LF) or Mac (CR) to UNIX (LF); 
        /// makes sure text ends with a couple of newlines; 
        /// removes any blank lines (only spaces) in the text
        /// </summary>
        private string Normalize(string text)
        {
            var output = new StringBuilder(text.Length);
            var line = new StringBuilder();
            bool valid = false;

            for (int i = 0; i < text.Length; i++)
            {
                switch (text[i])
                {
                    case '\n':
                        if (valid) output.Append(line);
                        output.Append('\n');
                        line.Length = 0; valid = false;
                        break;
                    case '\r':
                        if ((i < text.Length - 1) && (text[i + 1] != '\n'))
                        {
                            if (valid) output.Append(line);
                            output.Append('\n');
                            line.Length = 0; valid = false;
                        }
                        break;
                    case '\t':
                        int width = (_tabWidth - line.Length % _tabWidth);
                        for (int k = 0; k < width; k++)
                            line.Append(' ');
                        break;
                    case '\x1A':
                        break;
                    default:
                        if (!valid && text[i] != ' ') valid = true;
                        line.Append(text[i]);
                        break;
                }
            }

            if (valid) output.Append(line);
            output.Append('\n');

            // add two newlines to the end before return
            return output.Append("\n\n").ToString();
        }
    }
}