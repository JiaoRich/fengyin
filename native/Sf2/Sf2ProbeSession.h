#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#else
 #include <dlfcn.h>
#endif

namespace fengyin::sf2
{
inline juce::File moduleDirectory()
{
   #if JUCE_WINDOWS
    HMODULE module = nullptr; wchar_t path[32768] {};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory), &module) && GetModuleFileNameW(module, path, 32768))
        return juce::File(juce::String(path)).getParentDirectory();
   #else
    Dl_info info {};
    if (dladdr(reinterpret_cast<void*>(&moduleDirectory), &info) && info.dli_fname)
        return juce::File(juce::String::fromUTF8(info.dli_fname)).getParentDirectory();
   #endif
    return {};
}

inline juce::var inspectInChild(const juce::File& source, const std::function<bool()>& cancelled,
                               juce::File helper = {})
{
    if (helper == juce::File()) helper = moduleDirectory().getChildFile(
       #if JUCE_WINDOWS
        "FengYinSF2Probe.exe"
       #else
        "FengYinSF2Probe"
       #endif
    );
    juce::TemporaryFile output(".json");
    if (!helper.existsAsFile()) return {};
   #if JUCE_WINDOWS
    struct Process {
        HANDLE job = nullptr, handle = nullptr;
        ~Process() { if (job) CloseHandle(job); if (handle) CloseHandle(handle); }
    } process;
    process.job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = 768ull * 1024 * 1024;
    if (!process.job || !SetInformationJobObject(process.job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) return {};
    const auto command = helper.getFullPathName().quoted() + " --inspect " + source.getFullPathName().quoted() + " " + output.getFile().getFullPathName().quoted();
    std::wstring mutableCommand(command.toWideCharPointer());
    STARTUPINFOW startup {sizeof(startup)}; PROCESS_INFORMATION child {};
    if (!CreateProcessW(helper.getFullPathName().toWideCharPointer(), mutableCommand.data(), nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, helper.getParentDirectory().getFullPathName().toWideCharPointer(), &startup, &child)) return {};
    process.handle = child.hProcess;
    if (!AssignProcessToJobObject(process.job, child.hProcess)) {
        TerminateProcess(child.hProcess, 1); CloseHandle(child.hThread); return {};
    }
    ResumeThread(child.hThread); CloseHandle(child.hThread);
    const auto running = [&] { return WaitForSingleObject(process.handle, 0) == WAIT_TIMEOUT; };
   #else
    juce::ChildProcess process;
    if (!process.start(juce::StringArray {helper.getFullPathName(), "--inspect", source.getFullPathName(), output.getFile().getFullPathName()}, 0)) return {};
    const auto running = [&] { return process.isRunning(); };
   #endif
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 30000.;
    while (running()) {
        if (cancelled() || juce::Time::getMillisecondCounterHiRes() > deadline) {
           #if !JUCE_WINDOWS
            process.kill();
           #endif
            return {};
        }
        juce::Thread::sleep(20);
    }
    if (cancelled() || output.getFile().getSize() > 1024 * 1024) return {};
    return juce::JSON::parse(output.getFile());
}
}
