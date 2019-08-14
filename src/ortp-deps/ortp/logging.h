/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file logging.h
 * \brief Logging API.
 *
**/

#ifndef ORTP_LOGGING_H
#define ORTP_LOGGING_H

#include <ortp/port.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum {
	ORTP_DEBUG=1,
	ORTP_MESSAGE=1<<1,
	ORTP_WARNING=1<<2,
	ORTP_ERROR=1<<3,
	ORTP_FATAL=1<<4,
	ORTP_TRACE=1<<5,
	ORTP_LOGLEV_END=1<<6
} OrtpLogLevel;


typedef void (*OrtpLogFunc)(OrtpLogLevel lev, const char *fmt, va_list args);

ORTP_PUBLIC void ortp_set_log_file(FILE *file);
ORTP_PUBLIC void ortp_set_log_handler(OrtpLogFunc func);

VAR_DECLSPEC OrtpLogFunc ortp_logv_out;

ORTP_PUBLIC extern unsigned int __ortp_log_mask;

#define ortp_log_level_enabled(level)	(__ortp_log_mask & (level))

#if !defined(_WIN32) && !defined(_WIN32_WCE)
#define ortp_logv(level,fmt,args) \
{\
	if (ortp_logv_out!=NULL && ortp_log_level_enabled(level)) \
		ortp_logv_out(level,fmt,args);\
	if ((level)==ORTP_FATAL) abort();\
}while(0)
#else
ORTP_PUBLIC void ortp_logv(int level, const char *fmt, va_list args);
#endif

ORTP_PUBLIC void ortp_set_log_level_mask(int levelmask);

#ifdef __GNUC__
#define CHECK_FORMAT_ARGS(m,n) __attribute__((format(printf,m,n)))
#else
#define CHECK_FORMAT_ARGS(m,n)
#endif


#ifdef ORTP_DEBUG_MODE
static inline void CHECK_FORMAT_ARGS(1,2) ortp_debug(const char *fmt,...)
{
  va_list args;
  va_start (args, fmt);
  ortp_logv(ORTP_DEBUG, fmt, args);
  va_end (args);
}
#else

#define ortp_debug(...)

#endif

#ifdef ORTP_NOMESSAGE_MODE

#define ortp_log(...)
#define ortp_message(...)
#define ortp_warning(...)

#else

static inline void CHECK_FORMAT_ARGS(2,3) ortp_log(OrtpLogLevel lev, const char *fmt,...) {
	va_list args;
	va_start (args, fmt);
	ortp_logv(lev, fmt, args);
	va_end (args);
}

static inline void CHECK_FORMAT_ARGS(1,2) ortp_message(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	ortp_logv(ORTP_MESSAGE, fmt, args);
	va_end (args);
}

static inline void CHECK_FORMAT_ARGS(1,2) ortp_warning(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	ortp_logv(ORTP_WARNING, fmt, args);
	va_end (args);
}

#endif

static inline void CHECK_FORMAT_ARGS(1,2) ortp_error(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	ortp_logv(ORTP_ERROR, fmt, args);
	va_end (args);
}

static inline void CHECK_FORMAT_ARGS(1,2) ortp_fatal(const char *fmt,...)
{
	va_list args;
	va_start (args, fmt);
	ortp_logv(ORTP_FATAL, fmt, args);
	va_end (args);
}


#ifdef __cplusplus
}
#endif

#endif
