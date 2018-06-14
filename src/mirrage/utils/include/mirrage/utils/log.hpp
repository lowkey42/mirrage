/** logging helper ***********************************************************
 *                                                                           *
 * Copyright (c) 2014 Florian Oetke                                          *
 *  This file is distributed under the MIT License                           *
 *  See LICENSE file for details.                                            *
\*****************************************************************************/

#pragma once

#include <plog/Log.h>

namespace mirrage::util {
	extern std::string print_stacktrace();
}

#ifndef _MSC_VER
#define MIRRAGE_LIKELY(c) __builtin_expect((c), 1)
#define MIRRAGE_UNLIKELY(c) __builtin_expect((c), 0)
#else
#define MIRRAGE_LIKELY(c) c
#define MIRRAGE_UNLIKELY(c) c
#endif

/// logs the given error message and ends the program with a stacktrace without unwinding the stack.
#define MIRRAGE_FAIL(M)                                                                                       \
	do {                                                                                                      \
		IF_LOG_(PLOG_DEFAULT_INSTANCE, plog::fatal)                                                           \
		(*plog::get<PLOG_DEFAULT_INSTANCE>()) +=                                                              \
		        (plog::Record(plog::fatal, PLOG_GET_FUNC(), __LINE__, PLOG_GET_FILE(), PLOG_GET_THIS()) << M) \
		        << "\n"                                                                                       \
		        << mirrage::util::print_stacktrace();                                                         \
		std::abort();                                                                                         \
	} while(false)

#define MIRRAGE_INVARIANT(C, M)    \
	do {                           \
		if(MIRRAGE_UNLIKELY(!(C))) \
			MIRRAGE_FAIL(M);       \
	} while(false)
