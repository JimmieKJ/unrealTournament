// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Text;
using System.Collections.Generic;
using System.Linq;
using Microsoft.VisualStudio.Language.Spellchecker;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace SpellChecker.Test
{
    [TestClass]
    public class WordLogicTests
    {
        void CheckWords(string sentence, List<string> expected)
        {
            List<string> words = SpellingTagger.GetWordsInText(sentence).Select(s => sentence.Substring(s.Start, s.Length)).ToList();

            string errorMessage = string.Format("Got list: [{0}].  Expected: [{1}]", string.Join(", ", words), string.Join(", ", expected));

            CollectionAssert.AreEqual(expected, words, errorMessage);
        }

        [TestMethod]
        public void GetWordsTest()
        {
            CheckWords("This!  Is an amazing1 sentence, I think; but_some of ThEse words Should be ignore.d I think.",
             new List<string>() { "This", "Is", "an", "amazing1", "sentence", "I", "think", "but_some", "of", "ThEse", "words", "Should", "be", "ignore.d", "I", "think." });
        }

        [TestMethod]
        public void SkipWordsWithNumbers()
        {
            foreach (string word in Words("1skip", "sk1p", "skip1"))
                NotAWord(word);
        }

        [TestMethod]
        public void SkipWordsWithUnderscores()
        {
            foreach (string word in Words("_skip", "sk_p", "skip_"))
                NotAWord(word);
        }

        [TestMethod]
        public void SkipCamelCaseWords()
        {
            foreach (string word in Words("IWord", "someWord", "WriteLine", "skipthiS"))
                NotAWord(word);

            foreach (string word in Words("This", "I"))
                AWord(word);
        }

        [TestMethod]
        public void BoundaryCases()
        {
            foreach (string word in Words("!@#$%S", "a", "A"))
                AWord(word);

            foreach (string word in Words("", "0", "IN", "_", "iN"))
                NotAWord(word);
        }

        [TestMethod]
        public void SkipAllUppercaseWords()
        {
            foreach (string word in Words("ALLCAPS", "CAPS"))
                NotAWord(word);

            foreach (string word in Words("   A", "I", "Oh"))
                AWord(word);
        }

        [TestMethod]
        public void SkipWordsWithDotInTheMiddle()
        {
            foreach (string word in Words("foo.exe", "wordlogictests.cs", "include.h"))
                NotAWord(word);

            foreach (string word in Words("this."))
                AWord(word);
        }

        #region Helpers
        void NotAWord(string word)
        {
            Assert.IsFalse(SpellingTagger.ProbablyARealWord(word), word + " should fail the real word check");
        }

        void AWord(string word)
        {
            Assert.IsTrue(SpellingTagger.ProbablyARealWord(word), word + " should pass the real word check");
        }

        IEnumerable<string> Words(params string[] words)
        {
            return words;
        }
        #endregion
    }
}
