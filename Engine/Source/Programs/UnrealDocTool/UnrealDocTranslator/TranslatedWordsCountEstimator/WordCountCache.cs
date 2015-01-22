// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using Perforce.P4;
using System.Text.RegularExpressions;

namespace TranslatedWordsCountEstimator
{
    public class WordCountCache
    {
        public int Get(FileMetaData file)
        {
            int output;

            if (!wordCountCache.TryGetValue(file, out output))
            {
                output = UDNFilesHelper.GetWordCount(UDNFilesHelper.Get(file));
                wordCountCache.Add(file, output);
            }

            return output;
        }

        private readonly Dictionary<FileMetaData, int> wordCountCache = new Dictionary<FileMetaData, int>(); 
    }
}
