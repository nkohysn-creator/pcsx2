// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Pcsx2Defs.h"
#include "CrashHandler.h"
#include "DynamicLibrary.h"
#include "FileSystem.h"
#include "StringUtil.h"
#include <cinttypes>
#include <cstdio>
#include <ctime>

#if defined(_WIN32)
#include "RedtapeWindows.h"

#include "StackWalker.h"
#include <DbgHelp.h>

class CrashHandlerStackWalker : public StackWalker
{
public:
	explicit CrashHandlerStackWalker(HANDLE out_file);
	~CrashHandlerStackWalker();

protected:
	void OnOutput(LPCSTR szText) override;

private:
	HANDLE m_out_file;
};

CrashHandlerStackWalker::CrashHandlerStackWalker(HANDLE out_file)
	: StackWalker(RetrieveVerbose, nullptr, GetCurrentProcessId(), GetCurrentProcess())
	, m_out_file(out_file)
{
}

CrashHandlerStackWalker::~CrashHandlerStackWalker() = default;

void CrashHandlerStackWalker::OnOutput(LPCSTR szText)
{
	if (m_out_file)
	{
		DWORD written;
		WriteFile(m_out_file, szText, static_cast<DWORD>(std::strlen(szText)), &written, nullptr);
	}

	OutputDebugStringA(szText);
}

static bool WriteMinidump(HMODULE hDbgHelp, HANDLE hFile, HANDLE hProcess, DWORD process_id, DWORD thread_id,
	PEXCEPTION_POINTERS exception, MINIDUMP_TYPE type)
{
	using PFNMINIDUMPWRITEDUMP =
		BOOL(WINAPI*)(HANDLE hProcess, DWORD ProcessId, HANDLE hFile, MINIDUMP_TYPE DumpType,
			PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
			PMINIDUMP_CALLBACK_INFORMATION CallbackParam);

	PFNMINIDUMPWRITEDUMP minidump_write_dump =
		hDbgHelp ? reinterpret_cast<PFNMINIDUMPWRITEDUMP>(GetProcAddress(hDbgHelp, "MiniDumpWriteDump")) : nullptr;
	if (!minidump_write_dump)
		return false;

	MINIDUMP_EXCEPTION_INFORMATION mei = {};
	if (exception)
	{
		mei.ThreadId = thread_id;
		mei.ExceptionPointers = exception;
		mei.ClientPointers = FALSE;
		return minidump_write_dump(hProcess, process_id, hFile, type, &mei, nullptr, nullptr);
	}

	__try
	{
		RaiseException(EXCEPTION_INVALID_HANDLE, 0, 0, nullptr);
	}
	__except (WriteMinidump(hDbgHelp, hFile, GetCurrentProcess(), GetCurrentProcessId(), GetCurrentThreadId(),
				  GetExceptionInformation(), type),
		EXCEPTION_EXECUTE_HANDLER)
	{
	}

	return true;
}

static std::wstring s_write_directory;
static DynamicLibrary s_dbghelp_module;
static bool s_in_crash_handler = false;

static void GenerateCrashFilename(wchar_t* buf, size_t len, const wchar_t* prefix, const wchar_t* extension)
{
	SYSTEMTIME st = {};
	GetLocalTime(&st);

	_snwprintf_s(buf, len, _TRUNCATE, L"%s%scrash-%04u-%02u-%02u-%02u-%02u-%02u-%03u.%s",
		prefix ? prefix : L"", prefix ? L"\\" : L"",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, extension);
}

static void WriteMinidumpAndCallstack(PEXCEPTION_POINTERS exi)
{
	s_in_crash_handler = true;

	wchar_t filename[1024] = {};
	GenerateCrashFilename(filename, std::size(filename), s_write_directory.empty() ? nullptr : s_write_directory.c_str(), L"txt");

	// might fail
	HANDLE hFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (exi && hFile != INVALID_HANDLE_VALUE)
	{
		char line[1024];
		DWORD written;
		std::snprintf(line, std::size(line), "Exception 0x%08X at 0x%p\n", static_cast<unsigned>(exi->ExceptionRecord->ExceptionCode),
			exi->ExceptionRecord->ExceptionAddress);
		WriteFile(hFile, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
	}

	GenerateCrashFilename(filename, std::size(filename), s_write_directory.empty() ? nullptr : s_write_directory.c_str(), L"dmp");

	const MINIDUMP_TYPE minidump_type =
		static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithHandleData | MiniDumpWithProcessThreadData |
								   MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory);
	const HANDLE hMinidumpFile = CreateFileW(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
	if (hMinidumpFile == INVALID_HANDLE_VALUE ||
		!WriteMinidump(static_cast<HMODULE>(s_dbghelp_module.GetHandle()), hMinidumpFile, GetCurrentProcess(), GetCurrentProcessId(),
			GetCurrentThreadId(), exi, minidump_type))
	{
		static const char error_message[] = "Failed to write minidump file.\n";
		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD written;
			WriteFile(hFile, error_message, sizeof(error_message) - 1, &written, nullptr);
		}
	}
	if (hMinidumpFile != INVALID_HANDLE_VALUE)
		CloseHandle(hMinidumpFile);

	CrashHandlerStackWalker sw(hFile);
	sw.ShowCallstack(GetCurrentThread(), exi ? exi->ContextRecord : nullptr);

	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
}

#if defined(PCSX2_EXPERIMENTAL_SSE2)
// Diagnostic only: log the first unsupported opcode and continue normal crash handling.
static LONG CALLBACK ExperimentalSSE2IllegalInstructionTrace(PEXCEPTION_POINTERS exi)
{
	if (!exi || !exi->ExceptionRecord || exi->ExceptionRecord->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
		return EXCEPTION_CONTINUE_SEARCH;

	static volatile LONG s_written = 0;
	if (InterlockedCompareExchange(&s_written, 1, 0) != 0)
		return EXCEPTION_CONTINUE_SEARCH;

	wchar_t filename[1024] = {};
	_snwprintf_s(filename, std::size(filename), _TRUNCATE, L"%ls%lsSSE2_ILLEGAL_INSTRUCTION.txt",
		s_write_directory.c_str(), s_write_directory.empty() ? L"" : L"\\");
	HANDLE file = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return EXCEPTION_CONTINUE_SEARCH;

	const void* pc = exi->ExceptionRecord->ExceptionAddress;
	unsigned char code[16] = {};
	SIZE_T bytes_read = 0;
	const BOOL read_ok = ReadProcessMemory(GetCurrentProcess(), pc, code, sizeof(code), &bytes_read);

	MEMORY_BASIC_INFORMATION mbi = {};
	VirtualQuery(pc, &mbi, sizeof(mbi));
	const char* region = (mbi.Type == MEM_PRIVATE) ? "MEM_PRIVATE (possibly JIT)" :
		(mbi.Type == MEM_IMAGE) ? "MEM_IMAGE (module)" : "other";

	char output[1024] = {};
	int length = std::snprintf(output, sizeof(output),
		"PCSX2 experimental SSE2 - illegal instruction\r\n"
		"Exception: 0x%08X\r\n"
		"Address: %p\r\n"
		"Thread ID: %lu\r\n"
		"Memory type: %s\r\n"
		"Bytes (%zu): ",
		static_cast<unsigned>(exi->ExceptionRecord->ExceptionCode), pc,
		static_cast<unsigned long>(GetCurrentThreadId()), region,
		read_ok ? static_cast<size_t>(bytes_read) : static_cast<size_t>(0));

	if (length > 0 && static_cast<size_t>(length) < sizeof(output))
	{
		size_t used = static_cast<size_t>(length);
		for (SIZE_T i = 0; read_ok && i < bytes_read && used + 4 < sizeof(output); i++)
		{
			const int added = std::snprintf(output + used, sizeof(output) - used, "%02X ", code[i]);
			if (added < 0) break;
			used += static_cast<size_t>(added);
		}
		if (used + 3 < sizeof(output))
		{
			output[used++] = '\r';
			output[used++] = '\n';
		}
		DWORD written = 0;
		WriteFile(file, output, static_cast<DWORD>(used), &written, nullptr);
	}
	CloseHandle(file);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

static LONG NTAPI ExceptionHandler(PEXCEPTION_POINTERS exi)
{
	// if the debugger is attached, or we're recursively crashing, let it take care of it.
	if (!s_in_crash_handler)
		WriteMinidumpAndCallstack(exi);

	// returning EXCEPTION_CONTINUE_SEARCH makes sense, except for the fact that it seems to leave zombie processes
	// around. instead, force ourselves to terminate.
	TerminateProcess(GetCurrentProcess(), 0xFEFEFEFEu);
	return EXCEPTION_CONTINUE_SEARCH;
}

bool CrashHandler::Install()
{
	// load dbghelp at install/startup, that way we're not LoadLibrary()'ing after a crash
	// .. because that probably wouldn't go down well.
	HMODULE mod = StackWalker::LoadDbgHelpLibrary();
	if (mod)
		s_dbghelp_module.Adopt(mod);

#if defined(PCSX2_EXPERIMENTAL_SSE2)
	AddVectoredExceptionHandler(1, ExperimentalSSE2IllegalInstructionTrace);
#endif
	SetUnhandledExceptionFilter(ExceptionHandler);
	return true;
}

void CrashHandler::SetWriteDirectory(std::string_view dump_directory)
{
	s_write_directory = FileSystem::GetWin32Path(dump_directory);
}

void CrashHandler::WriteDumpForCaller()
{
	WriteMinidumpAndCallstack(nullptr);
}

#elif !defined(__APPLE__) && defined(HAS_LIBBACKTRACE)

#include "FileSystem.h"

#include <backtrace.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace CrashHandler
{
	struct BacktraceBuffer
	{
		char* buffer;
		size_t used;
		size_t size;
	};

	static const char* GetSignalName(int signal_no);
	static void AllocateBuffer(BacktraceBuffer* buf);
	static void FreeBuffer(BacktraceBuffer* buf);
	static void AppendToBuffer(BacktraceBuffer* buf, const char* format, ...);
	static int BacktraceFullCallback(void* data, uintptr_t pc, const char* filename, int lineno, const char* function);
	static void LogCallstack(int signal, const void* exception_pc);

	static std::recursive_mutex s_crash_mutex;
	static bool s_in_signal_handler = false;

	static backtrace_state* s_backtrace_state = nullptr;
} // namespace CrashHandler

const char* CrashHandler::GetSignalName(int signal_no)
{
	switch (signal_no)
	{
		// Don't need to list all of them, there's only a couple we register.
		// clang-format off
		case SIGSEGV: return "SIGSEGV";
		case SIGBUS: return "SIGBUS";
		default: return "UNKNOWN";
			// clang-format on
	}
}

void CrashHandler::AllocateBuffer(BacktraceBuffer* buf)
{
	buf->used = 0;
	buf->size = __pagesize;
	buf->buffer = static_cast<char*>(mmap(nullptr, buf->size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
	if (buf->buffer == static_cast<char*>(MAP_FAILED))
	{
		buf->buffer = nullptr;
		buf->size = 0;
	}
}

void CrashHandler::FreeBuffer(BacktraceBuffer* buf)
{
	if (buf->buffer)
		munmap(buf->buffer, buf->size);
}

void CrashHandler::AppendToBuffer(BacktraceBuffer* buf, const char* format, ...)
{
	std::va_list ap;
	va_start(ap, format);

	// Hope this doesn't allocate memory... it *can*, but hopefully unlikely since
	// it won't be the first call, and we're providing the buffer.
	if (buf->size > 0 && buf->used < (buf->size - 1))
	{
		const int written = std::vsnprintf(buf->buffer + buf->used, buf->size - buf->used, format, ap);
		if (written > 0)
			buf->used += static_cast<size_t>(written);
	}

	va_end(ap);
}

int CrashHandler::BacktraceFullCallback(void* data, uintptr_t pc, const char* filename, int lineno,
	const char* function)
{
	BacktraceBuffer* buf = static_cast<BacktraceBuffer*>(data);
	AppendToBuffer(buf, "  %016p", pc);
	if (function)
		AppendToBuffer(buf, " %s", function);
	if (filename)
		AppendToBuffer(buf, " [%s:%d]", filename, lineno);

	AppendToBuffer(buf, "\n");
	return 0;
}

void CrashHandler::LogCallstack(int signal, const void* exception_pc)
{
	BacktraceBuffer buf;
	AllocateBuffer(&buf);
	if (signal != 0 || exception_pc)
		AppendToBuffer(&buf, "*************** Unhandled %s at %p ***************\n", GetSignalName(signal), exception_pc);
	else
		AppendToBuffer(&buf, "*******************************************************************\n");

	const int rc = backtrace_full(s_backtrace_state, 0, BacktraceFullCallback, nullptr, &buf);
	if (rc != 0)
		AppendToBuffer(&buf, "  backtrace_full() failed: %d\n");

	AppendToBuffer(&buf, "*******************************************************************\n");

	if (buf.used > 0)
		write(STDERR_FILENO, buf.buffer, buf.used);

	FreeBuffer(&buf);
}

void CrashHandler::CrashSignalHandler(int signal, siginfo_t* siginfo, void* ctx)
{
	std::unique_lock lock(s_crash_mutex);

	// If we crash somewhere in libbacktrace, don't bother trying again.
	if (!s_in_signal_handler)
	{
		s_in_signal_handler = true;

#if defined(__APPLE__) && defined(__x86_64__)
		void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext->__ss.__rip);
#elif defined(__FreeBSD__) && defined(__x86_64__)
		void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.mc_rip);
#elif defined(__x86_64__)
		void* const exception_pc = reinterpret_cast<void*>(static_cast<ucontext_t*>(ctx)->uc_mcontext.gregs[REG_RIP]);
#else
		void* const exception_pc = nullptr;
#endif

		LogCallstack(signal, exception_pc);

		s_in_signal_handler = false;
	}

	lock.unlock();

	// We can't continue from here. Just bail out and dump core.
	std::fputs("Aborting application.\n", stderr);
	std::fflush(stderr);
	std::abort();
}

bool CrashHandler::Install()
{
	const std::string progpath = FileSystem::GetProgramPath();
	s_backtrace_state = backtrace_create_state(progpath.empty() ? nullptr : progpath.c_str(), 0, nullptr, nullptr);
	if (!s_backtrace_state)
		return false;

	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO | SA_NODEFER;
	sa.sa_sigaction = CrashSignalHandler;
	if (sigaction(SIGBUS, &sa, nullptr) != 0)
		return false;
	if (sigaction(SIGSEGV, &sa, nullptr) != 0)
		return false;

	return true;
}

void CrashHandler::SetWriteDirectory(std::string_view dump_directory)
{
}

void CrashHandler::WriteDumpForCaller()
{
	LogCallstack(0, nullptr);
}

#else

bool CrashHandler::Install()
{
	return false;
}

void CrashHandler::SetWriteDirectory(std::string_view dump_directory)
{
}

void CrashHandler::WriteDumpForCaller()
{
}

void CrashHandler::CrashSignalHandler(int signal, siginfo_t* siginfo, void* ctx)
{
	// We can't continue from here. Just bail out and dump core.
	std::fputs("Aborting application.\n", stderr);
	std::fflush(stderr);
	std::abort();
}

#endif
