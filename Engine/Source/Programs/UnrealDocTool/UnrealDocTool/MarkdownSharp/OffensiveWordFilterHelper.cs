// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using DotLiquid;
using Microsoft.VisualBasic.FileIO;

namespace MarkdownSharp
{
    using MarkdownSharp.Preprocessor;

    public static class OffensiveWordFilterHelper
    {
        static readonly HashSet<string> OffensiveWordList = new HashSet<string>();
        static readonly Regex WordSplitPattern = new Regex(@"\w+", RegexOptions.Compiled);

        private static TransformationData _transformationData;
        private static bool _thisIsPreview;

        public static void Init(TransformationData transformationData, bool thisIsPreview)
        {
            var offensiveWordList = new FileInfo(Path.Combine(transformationData.CurrentFolderDetails.AbsoluteMarkdownPath, "Include", "NoRedist","OffensiveWordList", "wordlist.csv"));

            if (!offensiveWordList.Exists)
            {
                return;
            }

            var parser = new TextFieldParser(offensiveWordList.FullName);
            parser.HasFieldsEnclosedInQuotes = true;
            parser.Delimiters = new string[] {","};

            // for every line in file
            while (!parser.EndOfData)
            {
                var cells = parser.ReadFields();

                OffensiveWordList.Add(cells[0].ToLower());
            }

            _transformationData = transformationData;
            _thisIsPreview = thisIsPreview;
        }

        public static string CheckAndGenerateInfos(string text, PreprocessedTextLocationMap map)
        {
            if (OffensiveWordList.Count == 0)
            {
                return text;
            }

            var changes = new List<PreprocessingTextChange>();

            var output = WordSplitPattern.Replace(text, match => Evaluator(match, changes));

            map.ApplyChanges(changes);

            return output;
        }

        private static string Evaluator(Match match, List<PreprocessingTextChange> changes)
        {
            var word = match.Value;
            
            if (OffensiveWordList.Contains(word.ToLower()))
            {
                var errorId = _transformationData.ErrorList.Count;
                _transformationData.ErrorList.Add(Markdown.GenerateError(Language.Message("FoundOffensiveWord", word),
                                                                       MessageClass.Info, word,
                                                                       errorId,
                                                                       _transformationData));

                if (_thisIsPreview)
                {
                    var output = Templates.ErrorHighlight.Render(Hash.FromAnonymousObject(
                        new
                            {
                                errorId = errorId,
                                errorText = word
                            }));

                    changes.Add(new PreprocessingTextChange(match.Index, match.Length, output.Length));

                    return output;
                }
            }

            return word;
        }
    }
}