// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace MarkdownSharp.EpicMarkdown.Parsers.SpanParser
{
    using System.Globalization;
    using System.Text.RegularExpressions;

    public class EMDecorationTag : TagMatch
    {
        public EMDecorationType Type { get; private set; }

        protected EMDecorationTag(int index, string text, EMDecorationType type)
            : base(index, text)
        {
            Type = type;
        }

        private static readonly Regex DecorationPattern = new Regex(@"_+|\*+", RegexOptions.Compiled);

        public static void Append(ICollection<ITagMatch> output, string text)
        {
            AppendFromFragment(output, text);
        }

        private static void AppendFromFragment(ICollection<ITagMatch> output, string text)
        {
            var matches = DecorationPattern.Matches(text).Cast<Match>().ToList();
            matches = FilterOutMidwordTags(text, matches);

            // underscore italics i.e. _text_
            AppendDecorationTags(output, matches, EMDecorationType.Italic, '_');

            // underscore bold i.e. __text__
            AppendDecorationTags(output, matches, EMDecorationType.Bold, '_');

            // asterisk italics i.e. *text*
            AppendDecorationTags(output, matches, EMDecorationType.Italic, '*');

            // asterisk bold i.e. **text**
            AppendDecorationTags(output,matches, EMDecorationType.Bold, '*');
        }

        private  static readonly Regex WordCharacter = new Regex(@"\w", RegexOptions.Compiled);

        private static List<Match> FilterOutMidwordTags(string text, IEnumerable<Match> matches)
        {
            var output = new List<Match>();

            foreach (var match in matches)
            {
                if (match.Index > 0 && match.Index + match.Length < text.Length
                    && WordCharacter.IsMatch(text[match.Index - 1].ToString(CultureInfo.InvariantCulture))
                    && WordCharacter.IsMatch(text[match.Index + match.Length].ToString(CultureInfo.InvariantCulture)))
                {
                    continue;
                }

                output.Add(match);
            }

            return output;
        }

        private static void AppendDecorationTags(ICollection<ITagMatch> output, IEnumerable<Match> decorationMatches, EMDecorationType type, char decorationSymbol)
        {
            var matchesArray = FilterDecorationMatches(decorationMatches, type, decorationSymbol).ToArray();

            var pairs = matchesArray.Length;

            for (var i = 0; i < pairs; ++i)
            {
                var match = matchesArray[i];
                output.Add(new EMDecorationTag(match.Index, match.Value, type));
            }
        }

        private static IEnumerable<Match> FilterDecorationMatches(IEnumerable<Match> matches, EMDecorationType type, char decorationSymbol)
        {
            return
                matches.Where(
                    m => (type == EMDecorationType.Bold) == m.Length > 1 && m.Value[0] == decorationSymbol);
        }
    }
}
