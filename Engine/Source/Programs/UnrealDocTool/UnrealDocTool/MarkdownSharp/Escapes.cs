// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;

namespace MarkdownSharp
{
	internal static class Escapes
	{
		private const string _escapeCharacters = @"\`*_{}[]()>#+-.!";
		private static readonly KeyValuePair<char, string>[] _escapeTable;
		private static readonly Regex _hashFinder;

		static Escapes()
		{
			_escapeTable = new KeyValuePair<char, string>[_escapeCharacters.Length];
			string pattern = "";
			for (int i = 0; i < _escapeCharacters.Length; ++i)
			{
				char c = _escapeCharacters[i];
				string hash = c.ToString().GetHashCode().ToString();
				_escapeTable[i] = new KeyValuePair<char,string>(c, hash);

				if (pattern != "") pattern += "|(" + hash + ")";
				else pattern += "(" + hash + ")";
			}
			_hashFinder = new Regex(pattern, RegexOptions.Compiled | RegexOptions.ExplicitCapture);
		}

		/// <summary>
		/// Gets the escape code for a single character
		/// </summary>
		public static string get(char c)
		{
			foreach(var pair in _escapeTable)
				if (pair.Key == c)
					return pair.Value;
			throw new IndexOutOfRangeException("The requested character can not be escaped");
		}

		/// <summary>
		/// Gets the character that a hash refers to
		/// </summary>
		private static char getInverse(string s)
		{
			foreach (var pair in _escapeTable)
				if (pair.Value == s)
					return pair.Key;
			throw new IndexOutOfRangeException("The requested hash can not be found");
		}

		/// <summary>
		/// Encodes any escaped characters such as \`, \*, \[ etc
		/// </summary>
		public static string BackslashEscapes(string text)
		{
			int len = text.Length, first = 0, i = 0;
			var sb = new StringBuilder(len);
			while (i < len)
			{
				if (text[i] == '\\' && i + 1 < len && Contains(_escapeCharacters, text[i + 1]))
				{
					sb.Append(text, first, i - first);
					sb.Append(get(text[++i]));
					first = ++i;
				}
				else ++i;
			}
			if (first == 0) return text;
			sb.Append(text, first, i - first);
			return sb.ToString();
		}

		/// <summary>
		/// Encodes Bold [ * ] and Italic [ _ ] characters
		/// </summary>
		public static string BoldItalic(string text)
		{
			int len = text.Length, first = 0, i = 0;
			var sb = new StringBuilder(len);
			while (i < len)
			{
				if ('*' == text[i])
				{
					sb.Append(text, first, i - first);
					sb.Append(get('*'));
					first = ++i;
				}
				else if ('_' == text[i])
				{
					sb.Append(text, first, i - first);
					sb.Append(get('_'));
					first = ++i;
				}
				else ++i;
			}
			if (first == 0) return text;
			sb.Append(text, first, i - first);
			return sb.ToString();
		}

		/// <summary>
		/// Encodes all chars of the second parameter.
		/// </summary>
		public static string Escape(string text, string escapes)
		{
			int len = text.Length, first = 0, i = 0;
			var sb = new StringBuilder(len);
			while (i < len)
			{
				if (Contains(escapes, text[i]))
				{
					sb.Append(text, first, i - first);
					sb.Append(get(text[i]));
					first = ++i;
				}
				else ++i;
			}
			if (first == 0) return text;
			sb.Append(text, first, i - first);
			return sb.ToString();
		}

		/// <summary>
		/// encodes problem characters in URLs, such as 
		/// * _  and optionally ' () []  :
		/// this is to avoid problems with markup later
		/// </summary>
		public static string ProblemUrlChars(string url)
		{
			url = url.Replace("*", "%2A");
			url = url.Replace("_", "%5F");
			url = url.Replace("'", "%27");
			url = url.Replace("(", "%28");
			url = url.Replace(")", "%29");
			url = url.Replace("[", "%5B");
			url = url.Replace("]", "%5D");
			if (url.Length > 7 && Contains(url.Substring(7), ':'))
			{
				// replace any colons in the body of the URL that are NOT followed by 2 or more numbers
				url = url.Substring(0, 7) + Regex.Replace(url.Substring(7), @":(?!\d{2,})", "%3A");
			}

			return url;
		}

		private static bool Contains(string s, char c)
		{
			int len = s.Length;
			for (int i = 0; i < len; ++i)
				if (s[i] == c)
					return true;
			return false;
		}

		/// <summary>
		/// swap back in all the special characters we've hidden
		/// </summary>
		public static string Unescape(string text)
		{
			return _hashFinder.Replace(text, match => getInverse(match.Value).ToString());
		}
	}
}