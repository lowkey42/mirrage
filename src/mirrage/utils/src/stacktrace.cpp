/**
  Based on https://gist.github.com/jvranish/4441299
 */

#include <mirrage/utils/stacktrace.hpp>

#include <mirrage/utils/log.hpp>

#include <iostream>

#ifdef STACKTRACE

#include <cstdio>
#include <sstream>

#ifdef WIN
#include <imagehlp.h>
#include <windows.h>
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace mirrage::util {

	namespace {
		void set_signal_handler();

		std::string EXE_NAME;

		std::string demangleStacktraceEntry(void* t) {
			FILE* fp;
			char  var[40];

			std::stringstream buffer;
#ifdef __APPLE__
			buffer << "atos -o " << t << " \"" << EXE_NAME << "\"";
#else
			buffer << "addr2line -C -s -f -p " << t << " -e \"" << EXE_NAME << "\"";
#endif

			std::string command = buffer.str();

			fp = popen(command.c_str(), "r");

			buffer.str(std::string());

			while(fgets(var, sizeof(var), fp) != NULL)
				buffer << var;

			pclose(fp);

			return buffer.str();
		}
	} // namespace

	bool is_stacktrace_available() { return !EXE_NAME.empty(); }


#ifdef _WIN32

	namespace {
		typedef void (*RtlCaptureContextFunc)(CONTEXT* ContextRecord);
		RtlCaptureContextFunc rtlCaptureContext;
	} // namespace

	void init_stacktrace(std::string exeName) {
		EXE_NAME = exeName;

		HINSTANCE kernel32 = LoadLibrary("Kernel32.dll");

		rtlCaptureContext = (RtlCaptureContextFunc) GetProcAddress(kernel32, "RtlCaptureContext");

		set_signal_handler();
		MIRRAGE_INFO("Startet from " << exeName);
	}

	namespace {
		std::string windows_print_stacktrace(CONTEXT* context) {
			std::stringstream ret;
			SymInitialize(GetCurrentProcess(), 0, TRUE);

			DWORD MachineType;
			bool  first = true;

/* setup initial stack frame */
#ifdef _M_IX86
			MachineType = IMAGE_FILE_MACHINE_I386;
			STACKFRAME frame;

			frame.AddrPC.Offset    = context->Eip;
			frame.AddrPC.Mode      = AddrModeFlat;
			frame.AddrFrame.Offset = context->Ebp;
			frame.AddrFrame.Mode   = AddrModeFlat;
			frame.AddrStack.Offset = context->Esp;
			frame.AddrStack.Mode   = AddrModeFlat;
#elif _M_X64
			MachineType = IMAGE_FILE_MACHINE_AMD64;
			STACKFRAME64 frame;

			frame.AddrPC.Offset    = context->Rip;
			frame.AddrPC.Mode      = AddrModeFlat;
			frame.AddrFrame.Offset = context->Rbp;
			frame.AddrFrame.Mode   = AddrModeFlat;
			frame.AddrStack.Offset = context->Rsp;
			frame.AddrStack.Mode   = AddrModeFlat;
#elif _M_IA64
			MachineType = IMAGE_FILE_MACHINE_IA64;
			STACKFRAME64 frame;

			frame.AddrPC.Offset     = context->StIIP;
			frame.AddrPC.Mode       = AddrModeFlat;
			frame.AddrFrame.Offset  = context->IntSp;
			frame.AddrFrame.Mode    = AddrModeFlat;
			frame.AddrBStore.Offset = context->RsBSP;
			frame.AddrBStore.Mode   = AddrModeFlat;
			frame.AddrStack.Offset  = context->IntSp;
			frame.AddrStack.Mode    = AddrModeFlat;
#else
#error "Unsupported platform"
#endif

			memset(&frame, 0, sizeof(frame));


#ifdef _M_IX86
			while(StackWalk(MachineType,
			                GetCurrentProcess(),
			                GetCurrentThread(),
			                &frame,
			                context,
			                0,
			                SymFunctionTableAccess,
			                SymGetModuleBase,
			                0)) {
				if(first)
					first = false;
				else
					ret << "Called From ";
				ret << demangleStacktraceEntry((void*) frame.AddrPC.Offset) << std::endl;
			}

#else
			while(StackWalk64(MachineType,
			                  GetCurrentProcess(),
			                  GetCurrentThread(),
			                  &frame,
			                  context,
			                  0,
			                  SymFunctionTableAccess64,
			                  SymGetModuleBase64,
			                  0)) {
				if(first)
					first = false;
				else
					ret << "Called From ";
				ret << demangleStacktraceEntry((void*) frame.AddrPC.Offset) << std::endl;
			}
#endif

			SymCleanup(GetCurrentProcess());

			return ret.str();
		}

		LONG WINAPI windows_exception_handler(EXCEPTION_POINTERS* ExceptionInfo) {
			switch(ExceptionInfo->ExceptionRecord->ExceptionCode) {
				case EXCEPTION_ACCESS_VIOLATION: std::cerr << "Error: EXCEPTION_ACCESS_VIOLATION"; break;
				case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
					std::cerr << "Error: EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
					break;
				case EXCEPTION_BREAKPOINT: std::cerr << "Error: EXCEPTION_BREAKPOINT"; break;
				case EXCEPTION_DATATYPE_MISALIGNMENT:
					std::cerr << "Error: EXCEPTION_DATATYPE_MISALIGNMENT";
					break;
				case EXCEPTION_FLT_DENORMAL_OPERAND:
					std::cerr << "Error: EXCEPTION_FLT_DENORMAL_OPERAND";
					break;
				case EXCEPTION_FLT_DIVIDE_BY_ZERO: std::cerr << "Error: EXCEPTION_FLT_DIVIDE_BY_ZERO"; break;
				case EXCEPTION_FLT_INEXACT_RESULT: std::cerr << "Error: EXCEPTION_FLT_INEXACT_RESULT"; break;
				case EXCEPTION_FLT_INVALID_OPERATION:
					std::cerr << "Error: EXCEPTION_FLT_INVALID_OPERATION";
					break;
				case EXCEPTION_FLT_OVERFLOW: std::cerr << "Error: EXCEPTION_FLT_OVERFLOW"; break;
				case EXCEPTION_FLT_STACK_CHECK: std::cerr << "Error: EXCEPTION_FLT_STACK_CHECK"; break;
				case EXCEPTION_FLT_UNDERFLOW: std::cerr << "Error: EXCEPTION_FLT_UNDERFLOW"; break;
				case EXCEPTION_ILLEGAL_INSTRUCTION:
					std::cerr << "Error: EXCEPTION_ILLEGAL_INSTRUCTION";
					break;
				case EXCEPTION_IN_PAGE_ERROR: std::cerr << "Error: EXCEPTION_IN_PAGE_ERROR"; break;
				case EXCEPTION_INT_DIVIDE_BY_ZERO: std::cerr << "Error: EXCEPTION_INT_DIVIDE_BY_ZERO"; break;
				case EXCEPTION_INT_OVERFLOW: std::cerr << "Error: EXCEPTION_INT_OVERFLOW"; break;
				case EXCEPTION_INVALID_DISPOSITION:
					std::cerr << "Error: EXCEPTION_INVALID_DISPOSITION";
					break;
				case EXCEPTION_NONCONTINUABLE_EXCEPTION:
					std::cerr << "Error: EXCEPTION_NONCONTINUABLE_EXCEPTION";
					break;
				case EXCEPTION_PRIV_INSTRUCTION: std::cerr << "Error: EXCEPTION_PRIV_INSTRUCTION"; break;
				case EXCEPTION_SINGLE_STEP: std::cerr << "Error: EXCEPTION_SINGLE_STEP"; break;
				case EXCEPTION_STACK_OVERFLOW: std::cerr << "Error: EXCEPTION_STACK_OVERFLOW"; break;
				default: std::cerr << "Error: Unrecognized Exception"; break;
			}
			std::cerr << " at ";

			/* If this is a stack overflow then we can't walk the stack, so just show
				  where the error happened */
			if(EXCEPTION_STACK_OVERFLOW != ExceptionInfo->ExceptionRecord->ExceptionCode)
				std::cerr << windows_print_stacktrace(ExceptionInfo->ContextRecord) << std::endl;

			else
#ifdef _M_IX86
				std::cerr << demangleStacktraceEntry((void*) ExceptionInfo->ContextRecord->Eip)
				          << "; Sorry, no stacktrace for windows here :-(\n Please upgrade to Linux"
				          << std::endl;
#else
				std::cerr << demangleStacktraceEntry((void*) ExceptionInfo->ContextRecord->Rip)
				          << "; Sorry, no stacktrace for windows here :-(\n Please upgrade to Linux"
				          << std::endl;
#endif

			return EXCEPTION_EXECUTE_HANDLER;
		}

		void set_signal_handler() { SetUnhandledExceptionFilter(windows_exception_handler); }
	} // namespace

	std::string gen_stacktrace(int framesToSkip) {
		if(!is_stacktrace_available())
			return {};

		std::stringstream ret;

		CONTEXT context;
		memset(&context, 0, sizeof(context));
		context.ContextFlags = CONTEXT_CONTROL;

		// Capture the thread context
		rtlCaptureContext(&context);

		ret << windows_print_stacktrace(&context) << std::endl;

		return ret.str();
	}


#else
	void init_stacktrace(std::string exeName) {
		EXE_NAME = exeName;
		set_signal_handler();
	}

	namespace {
		void printStackTrace(std::string error) { MIRRAGE_CRASH_REPORT("\n" << error); }

		void posix_signal_handler(int sig, siginfo_t* siginfo, void*) {
			switch(sig) {
				case SIGSEGV:
					fputs("SEGFAULT\n", stderr);
					printStackTrace("Caught SIGSEGV: Segmentation Fault");
					break;
				case SIGINT:
					printStackTrace("Caught SIGINT: Interactive attention signal, (usually ctrl+c)");
					break;
				case SIGFPE:
					switch(siginfo->si_code) {
						case FPE_INTDIV: printStackTrace("Caught SIGFPE: (integer divide by zero)"); break;
						case FPE_INTOVF: printStackTrace("Caught SIGFPE: (integer overflow)"); break;
						case FPE_FLTDIV:
							printStackTrace("Caught SIGFPE: (floating-point divide by zero)");
							break;
						case FPE_FLTOVF: printStackTrace("Caught SIGFPE: (floating-point overflow)"); break;
						case FPE_FLTUND: printStackTrace("Caught SIGFPE: (floating-point underflow)"); break;
						case FPE_FLTRES:
							printStackTrace("Caught SIGFPE: (floating-point inexact result)");
							break;
						case FPE_FLTINV:
							printStackTrace("Caught SIGFPE: (floating-point invalid operation)");
							break;
						case FPE_FLTSUB: printStackTrace("Caught SIGFPE: (subscript out of range)"); break;
						default: printStackTrace("Caught SIGFPE: Arithmetic Exception"); break;
					}
				case SIGILL:
					switch(siginfo->si_code) {
						case ILL_ILLOPC: printStackTrace("Caught SIGILL: (illegal opcode)"); break;
						case ILL_ILLOPN: printStackTrace("Caught SIGILL: (illegal operand)"); break;
						case ILL_ILLADR: printStackTrace("Caught SIGILL: (illegal addressing mode)"); break;
						case ILL_ILLTRP: printStackTrace("Caught SIGILL: (illegal trap)"); break;
						case ILL_PRVOPC: printStackTrace("Caught SIGILL: (privileged opcode)"); break;
						case ILL_PRVREG: printStackTrace("Caught SIGILL: (privileged register)"); break;
						case ILL_COPROC: printStackTrace("Caught SIGILL: (coprocessor error)"); break;
						case ILL_BADSTK: printStackTrace("Caught SIGILL: (internal stack error)"); break;
						default: printStackTrace("Caught SIGILL: Illegal Instruction"); break;
					}
					break;
				case SIGTERM:
					printStackTrace("Caught SIGTERM: a termination request was sent to the program");
					break;
				case SIGABRT:
					printStackTrace("Caught SIGABRT: usually caused by an abort() or assert()");
					break;
				default: break;
			}

			// generate core dump
			//signal(SIGSEGV, SIG_DFL);
			//kill(getpid(), SIGSEGV);
			_Exit(1);
		}

		char alternate_stack[SIGSTKSZ * 2];

		void set_signal_handler() {
			/* setup alternate stack */
			{
				stack_t ss;
				/* malloc is usually used here, I'm not 100% sure my static allocation
				   is valid but it seems to work just fine. */
				ss.ss_sp = (void*) alternate_stack;
				ss.ss_size = sizeof(alternate_stack);
				ss.ss_flags = 0;

				if(sigaltstack(&ss, NULL) != 0)
					std::cerr << "failed to create alternate stack for handlers!" << std::endl;
			}

			/* register our signal handlers */
			{
				struct sigaction sig_action;
				sig_action.sa_sigaction = posix_signal_handler;
				sigemptyset(&sig_action.sa_mask);

#ifdef __APPLE__
				/* for some reason we backtrace() doesn't work on osx
					   when we use an alternate stack */
				sig_action.sa_flags = SA_SIGINFO;
#else
				sig_action.sa_flags = SA_SIGINFO | SA_ONSTACK;
#endif

				if(sigaction(SIGSEGV, &sig_action, NULL) != 0)
					MIRRAGE_ERROR("failed to register handler for SIGSEGV!");
				if(sigaction(SIGFPE, &sig_action, NULL) != 0)
					MIRRAGE_ERROR("failed to register handler for SIGFPE!");
				if(sigaction(SIGINT, &sig_action, NULL) != 0)
					MIRRAGE_ERROR("failed to register handler for SIGINT!");
				if(sigaction(SIGILL, &sig_action, NULL) != 0)
					MIRRAGE_ERROR("failed to register handler for SIGILL!");
				if(sigaction(SIGTERM, &sig_action, NULL) != 0)
					MIRRAGE_ERROR("failed to register handler for SIGTERM!");
				if(sigaction(SIGABRT, &sig_action, NULL) != 0)
					MIRRAGE_ERROR("failed to register handler for SIGABRT!");
			}
		}
	} // namespace

	std::string gen_stacktrace(int framesToSkip) {
		if(!is_stacktrace_available())
			return {};

		std::stringstream ret;
		void* trace[32];

		auto trace_size = backtrace(trace, 32);
		char** messages = backtrace_symbols(trace, trace_size);

		/* skip first stack frame (points here) */

		bool first = true;

		for(auto i = 1 + framesToSkip; i < trace_size; ++i) {
			if(first)
				first = false;
			else
				ret << "Called From ";
			ret << messages[i] << std::endl << "    " << demangleStacktraceEntry(trace[i]) << std::endl;
		}

		return ret.str();
	}
#endif
} // namespace mirrage::util

#else
namespace mirrage::util {
	void init_stacktrace(std::string exeName) {}

	std::string gen_stacktrace(int stacksToSkip) { return ""; }

	bool is_stacktrace_available() { return false; }
} // namespace mirrage::util
#endif
