// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Linq;

namespace MarkdownSharp.Preprocessor
{
    using System;
    using System.Text.RegularExpressions;

    public static class Normalizer
    {
        private static readonly Regex NewLinePattern = new Regex(@"[\n\r]+([ \t]+[\n\r]+)?", RegexOptions.Compiled);
        private static readonly Regex TabPattern = new Regex(@"\t", RegexOptions.Compiled);

        public static string Normalize(string text, PreprocessedTextLocationMap map)
        {
            text = Preprocessor.Replace(NewLinePattern, text, NewLineEvaluator, map);
            text = Preprocessor.Replace(TabPattern, text, m => TabReplacement(m, text), map);

            return text;
        }

        private static string TabReplacement(Match match, string text)
        {
            var lineStartIndex = text.LastIndexOf('\n', match.Index) + 1;

            var offset = 0;

            for (var i = lineStartIndex; i < match.Index; ++i)
            {
                if (text[i] == '\t')
                {
                    offset += Markdown.TabWidth - offset % Markdown.TabWidth;
                    continue;
                }

                ++offset;
            }

            return new string(' ', Markdown.TabWidth - offset % Markdown.TabWidth);
        }

        private static string NewLineEvaluator(Match match)
        {
            var crCount = match.Value.Count(c => c == '\n');
            var lfCount = match.Value.Count(c => c == '\r');

            if (crCount != 0 && lfCount != 0)
            {
                // Windows
                return lfCount > 1 ? new string('\n', Math.Min(lfCount, 3)) : "\n";
            }

            if (crCount != 0)
            {
                // Mac
                return crCount > 1 ? "\n\n" : "\n";
            }

            // Unix
            return lfCount > 1 ? "\n\n" : "\n";
        }

        private static readonly Regex TrailingWhitespacePattern = new Regex(@"\s*$", RegexOptions.Compiled);

        public static string TrailingWhitespaceRemove(string text)
        {
            return TrailingWhitespacePattern.Replace(text, "");
        }

        private static readonly Regex LeadingWhitespacePattern = new Regex(@"^\s*", RegexOptions.Compiled);

        public static string LeadingWhitespaceRemove(string text, out int leadingCharsCount)
        {
            var leadingCount = 0;
            var output = LeadingWhitespacePattern.Replace(text, m =>
                {
                    leadingCount = m.Length;
                    return "";
                });

            leadingCharsCount = leadingCount;

            return output;
        }

        private static readonly Regex NewlinesLeadingTrailing = new Regex(@"^\n+|\n+\z", RegexOptions.Compiled);

        public static string TrimNewLines(string text)
        {
            return NewlinesLeadingTrailing.Replace(text, "");
        }

        public static string NormalizePath(string path)
        {
            return path.Replace('\\', '/');
        }
    }
}
