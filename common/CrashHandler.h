// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <string_view>

#ifndef _WIN32
#include <csignal>
#endif

namespace CrashHandler
{
	bool Install();
	void SetWriteDirectory(std::string_view dump_directory);
	void WriteDumpForCaller();

#if defined(_WIN32) && defined(PCSX2_EXPERIMENTAL_SSE2)
	// Persistent trace points for GS software renderer startup and first draw.
	void TraceSSE2Stage(const char* stage);
#endif

#ifndef _WIN32
	// Allow crash handler to be invoked from a signal.
	void CrashSignalHandler(int signal, siginfo_t* siginfo, void* ctx);
#endif
} // namespace CrashHandler
