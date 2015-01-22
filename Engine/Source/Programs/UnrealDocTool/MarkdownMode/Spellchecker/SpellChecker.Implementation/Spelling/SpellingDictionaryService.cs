//***************************************************************************
//
//    Copyright (c) Microsoft Corporation. All rights reserved.
//    This code is licensed under the Visual Studio SDK license terms.
//    THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
//    ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
//    IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
//    PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//***************************************************************************

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft Corporation.  All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using System;
using System.Collections.Generic;
using System.ComponentModel.Composition;
using System.IO;
using System.Windows.Controls;
using Microsoft.VisualStudio.Text;
using SpellChecker.Definitions;

namespace Microsoft.VisualStudio.Language.Spellchecker
{
    /// <summary>
    /// Spell checking provider based on WPF spell checker
    /// </summary>
    [Export(typeof(ISpellingDictionaryService))]
    internal class SpellingDictionaryServiceFactory : ISpellingDictionaryService
    {
        [ImportMany(typeof(IBufferSpecificDictionaryProvider))]
        IEnumerable<Lazy<IBufferSpecificDictionaryProvider>> BufferSpecificDictionaryProviders = null;

        static GlobalDictionary GlobalDictionary = null;

        public ISpellingDictionary GetDictionary(ITextBuffer buffer)
        {
            ISpellingDictionary service;
            if (buffer.Properties.TryGetProperty(typeof(SpellingDictionaryService), out service))
                return service;

            if (GlobalDictionary == null)
                GlobalDictionary = new GlobalDictionary();

            List<ISpellingDictionary> bufferSpecificDictionaries = new List<ISpellingDictionary>();

            foreach (var provider in BufferSpecificDictionaryProviders)
            {
                var dictionary = provider.Value.GetDictionary(buffer);
                if (dictionary != null)
                    bufferSpecificDictionaries.Add(dictionary);
            }

            service = new SpellingDictionaryService(bufferSpecificDictionaries, GlobalDictionary);
            buffer.Properties[typeof(SpellingDictionaryService)] = service;

            return service;
        }
    }

    internal class SpellingDictionaryService : ISpellingDictionary
    {
        private IList<ISpellingDictionary> _bufferSpecificDictionaries;
        GlobalDictionary _globalDictionary;

        public SpellingDictionaryService(IList<ISpellingDictionary> bufferSpecificDictionaries, GlobalDictionary globalDictionary)
        {
            _bufferSpecificDictionaries = bufferSpecificDictionaries;
            _globalDictionary = globalDictionary;

            foreach (var dictionary in _bufferSpecificDictionaries)
            {
                dictionary.DictionaryUpdated += BufferSpecificDictionaryUpdated;
            }

            // Register to receive events when the global dictionary is updated
            _globalDictionary.RegisterSpellingDictionaryService(this);
        }

        void BufferSpecificDictionaryUpdated(object sender, SpellingEventArgs e)
        {
            RaiseSpellingChangedEvent(e.Word);
        }

        void GlobalDictionaryUpdated(object sender, SpellingEventArgs e)
        {
            RaiseSpellingChangedEvent(e.Word);
        }

        void RaiseSpellingChangedEvent(string word)
        {
            var temp = DictionaryUpdated;
            if (temp != null)
                DictionaryUpdated(this, new SpellingEventArgs(word));
        }

        #region ISpellingDictionary

        public bool AddWordToDictionary(string word)
        {
            if (string.IsNullOrEmpty(word))
                return false;

            foreach (var dictionary in _bufferSpecificDictionaries)
            {
                if (dictionary.AddWordToDictionary(word))
                    return true;
            }

            return _globalDictionary.AddWordToDictionary(word);
        }

        public bool IgnoreWord(string word)
        {
            if (string.IsNullOrEmpty(word) || ShouldIgnoreWord(word))
                return false;

            foreach (var dictionary in _bufferSpecificDictionaries)
            {
                if (dictionary.IgnoreWord(word))
                    return true;
            }

            return _globalDictionary.IgnoreWord(word);
        }

        public bool ShouldIgnoreWord(string word)
        {
            foreach (var dictionary in _bufferSpecificDictionaries)
            {
                if (dictionary.ShouldIgnoreWord(word))
                    return true;
            }

            return _globalDictionary.ShouldIgnoreWord(word);
        }

        public event EventHandler<SpellingEventArgs> DictionaryUpdated;

        #endregion

        internal void GlobalDictionaryUpdated(string word)
        {
            RaiseSpellingChangedEvent(word);
        }
    }

    internal class GlobalDictionary
    {
        private SortedSet<string> _ignoreWords = new SortedSet<string>();
        private string _ignoreWordsFile;

        public GlobalDictionary()
        {
            string localFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"Microsoft\VisualStudio\10.0\SpellChecker");
            if (!Directory.Exists(localFolder))
            {
                Directory.CreateDirectory(localFolder);
            }
            _ignoreWordsFile = Path.Combine(localFolder, "Dictionary.txt");

            LoadIgnoreDictionary();
        }

        #region ISpellingDictionary

        public bool AddWordToDictionary(string word)
        {
            if (string.IsNullOrEmpty(word))
                return false;

            // Add this word to the dictionary file.
            using (StreamWriter writer = new StreamWriter(_ignoreWordsFile, true))
            {
                writer.WriteLine(word);
            }

            IgnoreWord(word);

            return true;
        }

        public bool IgnoreWord(string word)
        {
            if (string.IsNullOrEmpty(word) || ShouldIgnoreWord(word))
                return true;

            lock (_ignoreWords)
                _ignoreWords.Add(word);

            RaiseSpellingChangedEvent(word);

            return true;
        }

        public bool ShouldIgnoreWord(string word)
        {
            lock (_ignoreWords)
                return _ignoreWords.Contains(word);
        }

        #endregion

        #region Helpers

        List<WeakReference> _registeredDictionaries = new List<WeakReference>();

        internal void RegisterSpellingDictionaryService(SpellingDictionaryService dictionary)
        {
            _registeredDictionaries.Add(new WeakReference(dictionary));
        }

        void RaiseSpellingChangedEvent(string word)
        {
            List<WeakReference> referencesToRemove = new List<WeakReference>();
            foreach (var dictionaryRef in _registeredDictionaries)
            {
                var target = dictionaryRef.Target as SpellingDictionaryService;
                if (target != null)
                    target.GlobalDictionaryUpdated(word);
                else
                    referencesToRemove.Add(dictionaryRef);
            }

            foreach (var reference in referencesToRemove)
            {
                _registeredDictionaries.Remove(reference);
            }
        }

        private void LoadIgnoreDictionary()
        {
            if (File.Exists(_ignoreWordsFile))
            {
                _ignoreWords.Clear();
                using (StreamReader reader = new StreamReader(_ignoreWordsFile))
                {
                    string word;
                    while (!string.IsNullOrEmpty((word = reader.ReadLine())))
                    {
                        _ignoreWords.Add(word);
                    }
                }
            }
        }
        #endregion
    }
}
