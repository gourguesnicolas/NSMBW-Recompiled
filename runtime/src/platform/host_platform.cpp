#include "platform/host_platform.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <pwd.h>
#endif

namespace RuntimePlatform {

std::optional<std::filesystem::path> ExecutableDirectory() noexcept {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::nullopt;
        }
        if (length < buffer.size() - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    if (_NSGetExecutablePath(nullptr, &size) != -1 || size == 0) {
        return std::nullopt;
    }
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) != 0) {
        return std::nullopt;
    }
    path.resize(std::char_traits<char>::length(path.c_str()));
    std::error_code ec;
    const auto resolved = std::filesystem::weakly_canonical(path, ec);
    return (ec ? std::filesystem::path(path) : resolved).parent_path();
#elif defined(__linux__)
    std::vector<char> buffer(256);
    for (;;) {
        const auto length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            return std::nullopt;
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            return std::filesystem::path(std::string(buffer.data(), length)).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    return std::nullopt;
#endif
}

std::filesystem::path ApplicationDataDirectory(std::string_view applicationName) {
#if defined(_WIN32)
    PWSTR rawPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &rawPath)) && rawPath) {
        const std::filesystem::path directory = std::filesystem::path(rawPath) / applicationName;
        CoTaskMemFree(rawPath);
        return directory;
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / "Library" / "Application Support" / applicationName;
    }
    if (const passwd* user = getpwuid(getuid()); user && user->pw_dir && *user->pw_dir) {
        return std::filesystem::path(user->pw_dir) / "Library" / "Application Support" / applicationName;
    }
#endif
    return std::filesystem::current_path() / applicationName;
}

std::filesystem::path LogDirectory(std::string_view applicationName) {
    return ApplicationDataDirectory(applicationName) / "Logs";
}

uint64_t CurrentProcessId() noexcept {
#if defined(_WIN32)
    return static_cast<uint64_t>(::GetCurrentProcessId());
#else
    return static_cast<uint64_t>(::getpid());
#endif
}

bool StartNewInstance() noexcept {
#if defined(_WIN32)
    std::wstring executable(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0) {
            return false;
        }
        if (length < executable.size() - 1) {
            executable.resize(length);
            break;
        }
        executable.resize(executable.size() * 2);
    }
    // CreateProcessW may write into the command line buffer, so it gets a copy.
    std::wstring commandLine = GetCommandLineW();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &startup, &process)) {
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    std::string executable;
    std::vector<std::string> arguments;
#if defined(__linux__)
    std::vector<char> link(4096);
    const auto length = ::readlink("/proc/self/exe", link.data(), link.size() - 1);
    if (length > 0) {
        executable.assign(link.data(), static_cast<std::size_t>(length));
    }
    if (FILE* cmdline = std::fopen("/proc/self/cmdline", "rb")) {
        std::string current;
        for (int c = std::fgetc(cmdline); c != EOF; c = std::fgetc(cmdline)) {
            if (c == 0) {
                arguments.push_back(current);
                current.clear();
            } else {
                current.push_back(static_cast<char>(c));
            }
        }
        std::fclose(cmdline);
    }
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size != 0) {
        std::string path(size, '\0');
        if (_NSGetExecutablePath(path.data(), &size) == 0) {
            executable.assign(path.c_str());
        }
    }
#endif
    if (executable.empty()) {
        return false;
    }
    if (arguments.empty()) {
        arguments.push_back(executable);
    }
    const pid_t child = ::fork();
    if (child < 0) {
        return false;
    }
    if (child == 0) {
        std::vector<char*> argv;
        for (auto& argument : arguments) {
            argv.push_back(argument.data());
        }
        argv.push_back(nullptr);
        ::execv(executable.c_str(), argv.data());
        ::_exit(127);
    }
    return true;
#endif
}

} // namespace RuntimePlatform
