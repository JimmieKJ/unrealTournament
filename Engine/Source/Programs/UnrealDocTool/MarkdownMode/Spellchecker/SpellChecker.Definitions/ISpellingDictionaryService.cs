// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using Microsoft.VisualStudio.Text;

namespace SpellChecker.Definitions
{
    /// <summary>
    /// Arguments for when a spelling dictionary has changed.
    /// </summary>
    public class SpellingEventArgs : EventArgs
    {
        public SpellingEventArgs(string word)
        {
            this.Word = word;
        }

        /// <summary>
        /// Word placed in the dictionary.  If <c>null</c>, it means
        /// the entire dictionary has changed, and words that may have
        /// been ignored before may now no longer be in the dictionary.
        /// </summary>
        public string Word { get; private set; }
    }

    /// <summary>
    /// Used to retrieve the spelling dictionary service for a given buffer.  The spelling dictionary service
    /// also aggregates <see cref="IBufferSpecificDictionary"/> objects.
    /// </summary>
    /// <remarks>
    /// This is a MEF component, and should be imported with:
    /// [Import] ISpellingDictionaryServiceFactory;
    /// </remarks>
    public interface ISpellingDictionaryService
    {
        ISpellingDictionary GetDictionary(ITextBuffer buffer);
    }

    /// <summary>
    /// Provides dictionary services for ignoring words, adding them to a dictionary, seeing if a word should be ignored,
    /// and an event for when words are ignored or added to the dictionary.
    /// </summary>
    public interface ISpellingDictionary
    {
        /// <summary>
        /// Add the given word to the dictionary, so that it will no longer show up as an
        /// incorrect spelling.
        /// </summary>
        /// <param name="word">The word to add to the dictionary.</param>
        /// <returns><c>true</c> if the word was successfully added to the dictionary, even if it was already in the dictionary.</returns>
        bool AddWordToDictionary(string word);

        /// <summary>
        /// Ignore the given word, but don't add it to the dictionary.
        /// </summary>
        /// <param name="word">The word to be ignored.</param>
        /// <returns><c>true</c> if the word was successfully marked as ignored.</returns>
        bool IgnoreWord(string word);

        /// <summary>
        /// Check the ignore dictionary for the given word.
        /// </summary>
        /// <param name="word"></param>
        /// <returns></returns>
        bool ShouldIgnoreWord(string word);

        /// <summary>
        /// Raised when the dictionary has been changed.
    /// When a new word is added to the dictionary, the event arguments
    /// contains the word that was added.
        /// </summary>
        event EventHandler<SpellingEventArgs> DictionaryUpdated;
    }
}
