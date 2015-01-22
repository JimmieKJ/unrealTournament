// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidString.cpp: Android implementations of string functions
=============================================================================*/

#include "CorePrivatePCH.h"

int vswprintf( TCHAR *buf, int max, const TCHAR *fmt, va_list args )
{
	if (fmt == NULL)
	{
		if ((max > 0) && (buf != NULL))
			*buf = 0;
		return(0);
	}

	int fmtlen = FAndroidPlatformString::Strlen(fmt);
	TCHAR *src = (TCHAR *)FMemory_Alloca((fmtlen + 1) * sizeof (TCHAR));
	FAndroidPlatformString::Strcpy(src, fmtlen + 1, fmt);

	TCHAR *dst = buf;
	TCHAR *enddst = dst + (max - 1);

	//printf("printf by-hand formatting "); unicode_str_to_stdout(src);

	while ((*src) && (dst < enddst))
	{
		if (*src != '%')
		{
			*dst = *src;
			dst++;
			src++;
			continue;
		}

		TCHAR *percent_ptr = src;
		int fieldlen = 0;
		int precisionlen = -1;

		src++; // skip the '%' char...

		while (*src == ' ')
		{
			*dst = ' ';
			dst++;
			src++;
		}

		// check for field width requests...
		if ((*src == '-') || ((*src >= '0') && (*src <= '9')))
		{
			TCHAR *ptr = src + 1;
			while ((*ptr >= '0') && (*ptr <= '9'))
				ptr++;

			TCHAR ch = *ptr;
			*ptr = '\0';
			fieldlen = atoi(TCHAR_TO_ANSI(src));
			*ptr = ch;

			src = ptr;
		}

		if (*src == '.')
		{
			TCHAR *ptr = src + 1;
			while ((*ptr >= '0') && (*ptr <= '9'))
				ptr++;

			TCHAR ch = *ptr;
			*ptr = '\0';
			precisionlen = atoi(TCHAR_TO_ANSI(src + 1));
			*ptr = ch;
			src = ptr;
		}

		// Check for 'ls' field, change to 's'
		if ((src[0] == 'l' && src[1] == 's'))
		{
			src++;
		}

		switch (*src)
		{
		case '%':
			{
				src++;
				*dst = '%';
				dst++;
			}
			break;

		case 'c':
			{
				TCHAR val = (TCHAR) va_arg(args, int);
				src++;
				*dst = val;
				dst++;
			}
			break;

		case 'd':
		case 'i':
		case 'X':
		case 'x':
		case 'u':
		case 'p':
			{
				src++;
				int val = va_arg(args, int);
				ANSICHAR ansinum[30];
				ANSICHAR fmtbuf[30];

				// Yes, this is lame.
				int cpyidx = 0;
				while (percent_ptr < src)
				{
					fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
					percent_ptr++;
					cpyidx++;
				}
				fmtbuf[cpyidx] = 0;

				int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
				if ((dst + rc) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				for (int i = 0; i < rc; i++)
				{
					*dst = (TCHAR) ansinum[i];
					dst++;
				}
			}
			break;

		case 'l':
		case 'I':
			{
				if ((src[0] == 'l' && src[1] != 'l') ||
					(src[0] == 'I' && (src[1] != '6' || src[2] != '4')))
				{
					printf("Unknown percent [%lc%lc%lc] in Android:vswprintf() [%s]\n.", src[0], src[1], src[2], TCHAR_TO_ANSI(fmt));
					src++;  // skip it, I guess.
					break;
				}

				// Yes, this is lame.
				int cpyidx = 0;
				unsigned long long val = va_arg(args, unsigned long long);
				ANSICHAR ansinum[60];
				ANSICHAR fmtbuf[30];
				if (src[0] == 'l')
				{
					src += 3;
				}
				else
				{
					src += 4;
					strcpy(fmtbuf, "%L");
					percent_ptr += 4;
					cpyidx = 2;
				}

				while (percent_ptr < src)
				{
					fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
					percent_ptr++;
					cpyidx++;
				}
				fmtbuf[cpyidx] = 0;

				int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
				if ((dst + rc) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				for (int i = 0; i < rc; i++)
				{
					*dst = (TCHAR) ansinum[i];
					dst++;
				}
			}
			break;

		case 'f':
		case 'e':
		case 'g':
			{
				src++;
				double val = va_arg(args, double);
				ANSICHAR ansinum[30];
				ANSICHAR fmtbuf[30];

				// Yes, this is lame.
				int cpyidx = 0;
				while (percent_ptr < src)
				{
					fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
					percent_ptr++;
					cpyidx++;
				}
				fmtbuf[cpyidx] = 0;

				int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
				if ((dst + rc) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				for (int i = 0; i < rc; i++)
				{
					*dst = (TCHAR) ansinum[i];
					dst++;
				}
			}
			break;

		case 's':
			{
				src++;
				static const TCHAR* Null = TEXT("(null)");
				const TCHAR *val = va_arg(args, TCHAR *);
				if (val == NULL)
					val = Null;

				int rc = FAndroidPlatformString::Strlen(val);
				int spaces = FPlatformMath::Max(FPlatformMath::Abs(fieldlen) - rc, 0);
				if ((dst + rc + spaces) > enddst)
					return -1;	// Fail - the app needs to create a larger buffer and try again
				if (spaces > 0 && fieldlen > 0)
				{
					for (int i = 0; i < spaces; i++)
					{
						*dst = TEXT(' ');
						dst++;
					}
				}
				for (int i = 0; i < rc; i++)
				{
					*dst = *val;
					dst++;
					val++;
				}
				if (spaces > 0 && fieldlen < 0)
				{
					for (int i = 0; i < spaces; i++)
					{
						*dst = TEXT(' ');
						dst++;
					}
				}
			}
			break;

		default:
			printf("Unknown percent [%%%c] in Android:vswprintf().\n", *src);
			src++;  // skip char, I guess.
			break;
		}
	}

	// Check if we were able to finish the entire format string
	// If not, the app needs to create a larger buffer and try again
	if(*src)
		return -1;

	*dst = 0;  // null terminate the new string.
	return(dst - buf);
}
