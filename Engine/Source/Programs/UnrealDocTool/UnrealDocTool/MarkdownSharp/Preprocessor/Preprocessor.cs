// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.Preprocessor
{
    using System.Globalization;
    using System.IO;
    using System.Text.RegularExpressions;

    using DotLiquid;

    using MarkdownSharp.EpicMarkdown;
    using MarkdownSharp.EpicMarkdown.Parsers;

    public class Preprocessor
    {
        public Preprocessor(Markdown markdown)
        {
            filter = new Filter();
            filter.Add("PUBLISH", publish =>
                {
                    var publishTypes = publish.Split(',').Select(e => e.Trim()).ToArray();

                    foreach (var publishType in publishTypes)
                    {
                        if (!markdown.AllSupportedAvailability.Contains(
                                publishType, StringComparer.OrdinalIgnoreCase))
                        {
                            throw new FilterPredicateException(
                                Language.Message("PublishTypeNotRecognized", publishType));
                        }
                    }

                    return
                        markdown.PublishFlags.Any(
                            allowedPublishType =>
                            publishTypes.Any(pt => allowedPublishType.Equals(pt, StringComparison.OrdinalIgnoreCase)));
                });
        }

        public static void AddChangesToBounds(
            PreprocessedTextLocationMap map,
            List<PreprocessingTextChange> changes,
            PreprocessedData data,
            PreprocessedTextType type)
        {
            if (data == null)
            {
                return;
            }

            foreach (var change in changes)
            {
                if (change.RemoveCharsCount == 0)
                {
                    continue;
                }

                var start = change.Index;
                var end = change.Index + change.RemoveCharsCount - 1;

                if(map != null)
                {
                    start = map.GetOriginalPosition(start, PositionRounding.Up);
                    end = map.GetOriginalPosition(end, PositionRounding.Down);
                }

                data.PreprocessedTextBounds.Add(
                    new PreprocessedTextBound(
                        type, start, end));
            }
        }

        public string Preprocess(string text, TransformationData transData, PreprocessedData data, string relativePathToLinkedFile = null, bool full = true)
        {
            var relPath = relativePathToLinkedFile ?? transData.CurrentFolderDetails.CurrentFolderFromMarkdownAsTopLeaf;

            text = Normalizer.Normalize(text, data.TextMap);

            text = OffensiveWordFilterHelper.CheckAndGenerateInfos(text, data.TextMap);

            text = EscapeChars(text, data.TextMap);

            text = DoCodeSpans(text, data.TextMap);

            text = data.Metadata.ParseMetadata(text, transData, data, full);

            text = filter.FilterOut(text, transData, data);

            text = data.Variables.ParseVariablesDefinition(text, relPath, transData, data.TextMap);

            data.ExcerptTextMap = new PreprocessedTextLocationMap(data.TextMap);
            text = data.Excerpts.ParseExcerpts(text, transData, data.TextMap);

            text = data.ReferenceLinks.Parse(text, data);

            text = CutoutComments(text, data);

            return text;
        }

        public static string Replace(Regex pattern, string text, MatchEvaluator evaluator, PreprocessedTextLocationMap locationMap, List<PreprocessingTextChange> changesDone = null)
        {
            if (locationMap == null && changesDone == null)
            {
                return pattern.Replace(text, evaluator);
            }

            var changes = changesDone ?? new List<PreprocessingTextChange>();
            
            var output = pattern.Replace(text, m => PreprocessingTextChange.MatchEvaluatorWithTextChangeEmitWrapper(m, evaluator, changes));

            if (locationMap != null)
            {
                locationMap.ApplyChanges(changes);
            }

            return output;
        }

        public static string Replace(Regex pattern, string text, string replacement, PreprocessedTextLocationMap locationMap)
        {
            return Replace(pattern, text, m => replacement, locationMap);
        }

        public static readonly Regex _codeSpan = new Regex(@"
                    (?<!\\)   # Character before opening ` can't be a backslash
                    (`+)      # $1 = Opening run of `
                    (.+?)     # $2 = The code block
                    (?<!`)
                    \1
                    (?!`)", RegexOptions.IgnorePatternWhitespace | RegexOptions.Compiled);

        /// <summary>
        /// Turn Markdown `code spans` into HTML code tags
        /// </summary>
        private static string DoCodeSpans(string text, PreprocessedTextLocationMap locationMap)
        {
            //    * You can use multiple backticks as the delimiters if you want to
            //        include literal backticks in the code span. So, this input:
            //
            //        Just type ``foo `bar` baz`` at the prompt.
            //
            //        Will translate to:
            //
            //          <p>Just type <code>foo `bar` baz</code> at the prompt.</p>
            //
            //        There's no arbitrary limit to the number of backticks you
            //        can use as delimters. If you need three consecutive backticks
            //        in your code, use four for delimiters, etc.
            //
            //    * You can use spaces to get literal backticks at the edges:
            //
            //          ... type `` `bar` `` ...
            //
            //        Turns to:
            //
            //          ... type <code>`bar`</code> ...         
            //

            return Replace(_codeSpan, text, CodeSpanEvaluator, locationMap);
        }

        private static string CodeSpanEvaluator(Match match)
        {
            string span = match.Groups[2].Value;
            span = Regex.Replace(span, @"^[ ]*", ""); // leading whitespace
            span = Regex.Replace(span, @"[ ]*$", ""); // trailing whitespace
            span = Markdown.EncodeCode(span);

            return Templates.Code.Render(Hash.FromAnonymousObject(new { value = span }));
        }

        private static readonly Regex CommentsPattern = new Regex(@"(?<start><!--)|(?<end>-->)", RegexOptions.Compiled);

        public static string CutoutComments(string text, PreprocessedData data)
        {
            var level = 0;
            var toRemove = new List<Interval>();
            var start = 0;

            foreach (Match match in CommentsPattern.Matches(text))
            {
                if (match.Groups["start"].Success)
                {
                    if (level == 0)
                    {
                        start = match.Index;
                    }

                    ++level;
                }
                else
                {
                    --level;

                    if (level == 0)
                    {
                        Interval i;

                        i.Start = start;
                        i.End = match.Index + match.Length;

                        toRemove.Add(i);
                    }
                }
            }

            return Remove(text, toRemove, data);
        }

        private static string Remove(string text, List<Interval> toRemove, PreprocessedData data)
        {
            var map = data != null ? data.TextMap : null;

            if (toRemove.Count == 0)
            {
                return text;
            }

            List<PreprocessingTextChange> changes = null;

            changes = new List<PreprocessingTextChange>();

            var output = new StringBuilder();

            var lastEnd = 0;

            foreach(var r in toRemove)
            {
                changes.Add(new PreprocessingTextChange(r.Start, r.End - r.Start, 0));

                if (lastEnd != r.Start)
                {
                    output.Append(text.Substring(lastEnd, r.Start - lastEnd));
                }

                lastEnd = r.End;
            }

            output.Append(text.Substring(lastEnd, text.Length - lastEnd));

            Preprocessor.AddChangesToBounds(map, changes, data, PreprocessedTextType.Comment);

            if (map != null)
            {
                map.ApplyChanges(changes);
            }

            return output.ToString();
        }

        private readonly Filter filter;

        private static readonly char[] BackslashEscapedChars = { '%', '[', ']', '_', '*' };

        public static string EscapeChars(string text, PreprocessedTextLocationMap map)
        {
            foreach (var c in BackslashEscapedChars)
            {
                text = Replace(new Regex(Regex.Escape(@"\" + c)), text, string.Format("\x19{0}\x19", c.GetHashCode()), map);
            }

            return text;
        }

        public static string UnescapeChars(string text, bool leaveBackslash = false, PreprocessedTextLocationMap map = null)
        {
            foreach (var c in BackslashEscapedChars)
            {
                text = Replace(
                    new Regex(Regex.Escape(string.Format("\x19{0}\x19", c.GetHashCode()))),
                    text,
                    (leaveBackslash ? "\\" : "") + c.ToString(CultureInfo.InvariantCulture),
                    map);
            }

            return text;
        }

        public static string Trim(string text, char c, PreprocessedTextLocationMap textMap)
        {
            text = TrimLeft(text, c, textMap);
            return TrimRight(text, c, textMap);
        }

        private static string TrimLeft(string text, char c, PreprocessedTextLocationMap textMap)
        {
            return Replace(new Regex(@"\A" + Regex.Escape(c.ToString()) + @"+"), text, "", textMap);
        }

        private static string TrimRight(string text, char c, PreprocessedTextLocationMap textMap)
        {
            return Replace(new Regex(Regex.Escape(c.ToString()) + @"+\z"), text, "", textMap);
        }
    }
}
