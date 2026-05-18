//
// Created by Tsung Xu on 2026/4/2.
// Refactored on 2026/4/24 — main.cpp 瘦身为 ~50 行入口。
// Copyright (c) 2026 OrderSeek AI（让机器理解世界的秩序）. All rights reserved.
//

#include "application_runner.h"
#include "data_collection/common/runtime_config.h"

#include <csignal>
#include <cstring>
#include <execinfo.h>

using namespace aurora;

// ===== Signal handling =====
static ApplicationRunner* g_runner = nullptr;

static void signalHandler(int signum) {
    if (signum == SIGSEGV || signum == SIGABRT) {
        void* buffer[32];
        int nframes = backtrace(buffer, 32);
        std::cerr << "\n[FATAL] Signal " << signum << " (" << strsignal(signum) << ")" << std::endl;
        backtrace_symbols_fd(buffer, nframes, STDERR_FILENO);
        _exit(signum);
    }
    // SIGTERM/SIGINT: graceful shutdown
    if (g_runner) {
        g_runner->requestShutdown();
    }
}

static void installSignalHandlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
}

int main(int argc, char** argv) {
    // 解析命令行配置
    auto config = config::RuntimeConfig::fromArgs(argc, argv);
    if (config.model_path.empty() && argc > 1) {
        // --help was printed by fromArgs
        auto help_it = std::find(argv, argv + argc, std::string("--help"));
        if (help_it != argv + argc) return 0;
    }

    installSignalHandlers();

    ApplicationRunner runner(config);
    g_runner = &runner;

    if (!runner.initialize()) {
        return -1;
    }

    int result = runner.run();
    runner.shutdown();
    g_runner = nullptr;

    return result;
}
