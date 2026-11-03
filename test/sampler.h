// A tiny sampling profiler for test drivers: SIGPROF every 0.5 ms of CPU, the interrupted PC into a buffer, written out at exit
// as hex addresses (resolve with addr2line -f -C -e <exe>). x86-64 and aarch64 Linux.
#pragma once
#include <csignal>
#include <cstdio>
#include <cstdint>
#include <sys/time.h>
#include <ucontext.h>
#include <link.h>
namespace sampler {
inline uintptr_t pcs[1 << 20];
inline volatile int n = 0;
inline void handler(int, siginfo_t*, void* ctx) {
    auto* uc = (ucontext_t*)ctx;
#if defined(__x86_64__)
    uintptr_t pc = uc->uc_mcontext.gregs[REG_RIP];
#else
    uintptr_t pc = uc->uc_mcontext.pc;
#endif
    if (n < (1 << 20)) pcs[n++] = pc;
}
inline void start() {
    struct sigaction sa {}; sa.sa_sigaction = handler; sa.sa_flags = SA_SIGINFO | SA_RESTART; sigaction(SIGPROF, &sa, nullptr);
    itimerval t {}; t.it_interval.tv_usec = 500; t.it_value.tv_usec = 500; setitimer(ITIMER_PROF, &t, nullptr);
}
inline void stop(const char* path) {
    itimerval t {}; setitimer(ITIMER_PROF, &t, nullptr);
    FILE* f = fopen(path, "w");
    dl_iterate_phdr([](dl_phdr_info* info, size_t, void* fp) { fprintf((FILE*)fp, "base %lx\n", (unsigned long)info->dlpi_addr); return 1; }, f);
    for (int i = 0; i < n; i++) fprintf(f, "%lx\n", (unsigned long)pcs[i]); fclose(f);
}
}
