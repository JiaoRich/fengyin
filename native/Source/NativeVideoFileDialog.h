#pragma once
#include <juce_core/juce_core.h>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <commctrl.h>
 #pragma comment(lib, "comctl32.lib")
 #include <shobjidl.h>
 #include <shlobj.h>
 #include <wrl/client.h>
 #include <atomic>
 #include <string>

namespace fengyin
{
// Only the short-lived selector process calls Shell/COM. In particular it has
// no window ownership or input-queue attachment to the audio/UI process.
namespace videoDialog
{
inline bool writePacket(const juce::File& file, const juce::var& packet)
{
    const auto temporary = file.getSiblingFile(file.getFileName() + ".writing");
    return temporary.replaceWithText(juce::JSON::toString(packet))
        && MoveFileExW(temporary.getFullPathName().toWideCharPointer(),
                       file.getFullPathName().toWideCharPointer(), MOVEFILE_REPLACE_EXISTING) != FALSE;
}
inline juce::var packet(const char* status, const juce::String& value = {})
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("status", status);
    object->setProperty("value", value);
    return juce::var(object.release());
}

inline UINT foregroundRequestMessage()
{
    static const UINT message = RegisterWindowMessageW(L"FengYin.VideoPicker.RequestForeground");
    return message;
}

inline DWORD foregroundProcess()
{
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process;
}

inline LRESULT CALLBACK dialogSubclass(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
                                        UINT_PTR subclassId, DWORD_PTR hostPid)
{
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(window, dialogSubclass, subclassId);
    else if (message == foregroundRequestMessage())
    {
        // This runs on the selector's STA. Recheck the foreground after the
        // asynchronous handoff so a user switching apps keeps their focus.
        if (foregroundProcess() == hostPid && IsWindowVisible(window))
        {
            if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
            SetForegroundWindow(window);
        }
        return 0;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

class Events final : public IFileDialogEvents
{
public:
    Events(juce::File directory, RECT bounds, DWORD host) : folder(std::move(directory)), ownerBounds(bounds), hostPid(host) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** result) override
    {
        if (result == nullptr) return E_POINTER;
        *result = nullptr;
        if (id != IID_IUnknown && id != IID_IFileDialogEvents) return E_NOINTERFACE;
        *result = static_cast<IFileDialogEvents*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { const auto count = --references; if (!count) delete this; return count; }
    HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog* dialog) override
    {
        Microsoft::WRL::ComPtr<IShellItem> item;
        PWSTR text = nullptr;
        if (FAILED(dialog->GetResult(&item)) || FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &text))) return S_FALSE;
        const juce::File selected {juce::String(text)};
        CoTaskMemFree(text);
        if (!selected.existsAsFile()) return S_FALSE;
        // GetResult is explicitly supported here by Microsoft. Publish before
        // Show/EndDialog reactivates other windows: that closing path stalled
        // in the Honor dump and must never gate the main process's callback.
        accepted = writePacket(folder.getChildFile("result.json"), packet("selected", selected.getFullPathName()));
        if (!accepted) MessageBoxW(nullptr, L"无法返回所选文件，请重试或取消。", L"选择伴奏视频", MB_OK | MB_ICONWARNING);
        return accepted ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog* dialog) override { return publishWindow(dialog); }
    HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog*, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog* dialog) override { return publishWindow(dialog); }
    HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog* dialog) override { return publishWindow(dialog); }
    HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE* response) override
    { *response = FDESVR_DEFAULT; return S_OK; }
    HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE* response) override
    { *response = FDEOR_DEFAULT; return S_OK; }
    bool accepted = false;
private:
    std::atomic<ULONG> references {1};
    juce::File folder;
    RECT ownerBounds {};
    DWORD hostPid = 0;
    bool published = false;
    HRESULT publishWindow(IFileDialog* dialog)
    {
        if (published) return S_OK;
        Microsoft::WRL::ComPtr<IOleWindow> window;
        HWND handle = nullptr;
        if (FAILED(dialog->QueryInterface(IID_PPV_ARGS(&window))) || FAILED(window->GetWindow(&handle)) || !handle) return S_OK;
        MONITORINFO monitor {sizeof(monitor)};
        RECT bounds {};
        if (GetMonitorInfoW(MonitorFromRect(&ownerBounds, MONITOR_DEFAULTTONEAREST), &monitor) && GetWindowRect(handle, &bounds))
        {
            const auto& area = monitor.rcWork;
            const auto width = juce::jmin(bounds.right - bounds.left, area.right - area.left);
            const auto height = juce::jmin(bounds.bottom - bounds.top, area.bottom - area.top);
            const auto x = juce::jlimit(area.left, area.right - width, (ownerBounds.left + ownerBounds.right - width) / 2);
            const auto y = juce::jlimit(area.top, area.bottom - height, (ownerBounds.top + ownerBounds.bottom - height) / 2);
            SetWindowPos(handle, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        const bool canActivate = foregroundRequestMessage() != 0
            && SetWindowSubclass(handle, dialogSubclass, 1, hostPid) != FALSE;
        auto ready = packet("ready");
        ready.getDynamicObject()->setProperty("canActivate", canActivate);
        ready.getDynamicObject()->setProperty("window", static_cast<juce::int64>(reinterpret_cast<INT_PTR>(handle)));
        published = writePacket(folder.getChildFile("ready.json"), ready);
        return S_OK;
    }
};
} // namespace videoDialog

inline int runNativeVideoFileDialog(const juce::File& folder)
{
    const auto request = juce::JSON::parse(folder.getChildFile("request.json"));
    if (!request.isObject()) return 2;
    const auto init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(init)) return 3;
    const auto result = [&]() -> HRESULT
    {
        Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
        auto hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
        if (FAILED(hr)) return hr;
        FILEOPENDIALOGOPTIONS options {};
        if (FAILED(hr = dialog->GetOptions(&options))) return hr;
        if (FAILED(hr = dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR))) return hr;
        const COMDLG_FILTERSPEC filters[] {{L"视频文件", L"*.mp4;*.mov;*.m4v;*.avi;*.mkv;*.webm"}, {L"所有文件", L"*.*"}};
        if (FAILED(hr = dialog->SetFileTypes(2, filters))) return hr;
        dialog->SetTitle(L"选择伴奏视频");
        // A stable client GUID lets Windows remember the actual last visited
        // place across releases, including its localized Known Folder names.
        const GUID client {0x24dc16ba, 0xa5b8, 0x4c8b, {0xba,0xac,0x01,0x85,0x60,0x67,0x24,0x9f}};
        dialog->SetClientGuid(client);
        Microsoft::WRL::ComPtr<IShellItem> movies;
        if (SUCCEEDED(SHCreateItemInKnownFolder(FOLDERID_Videos, 0, nullptr, IID_PPV_ARGS(&movies)))) dialog->SetDefaultFolder(movies.Get());
        RECT bounds {int(request["left"]), int(request["top"]), int(request["right"]), int(request["bottom"])};
        Microsoft::WRL::ComPtr<videoDialog::Events> events;
        events.Attach(new videoDialog::Events(folder, bounds, static_cast<DWORD>(static_cast<juce::int64>(request["hostPid"]))));
        DWORD cookie = 0;
        if (FAILED(hr = dialog->Advise(events.Get(), &cookie))) return hr;
        // NULL is intentional, not GetActiveWindow or the host HWND. There is
        // no cross-process owner/activation dependency or main UI modal loop.
        hr = dialog->Show(nullptr);
        dialog->Unadvise(cookie);
        return hr;
    }();
    if (!folder.getChildFile("result.json").existsAsFile())
        videoDialog::writePacket(folder.getChildFile("result.json"), videoDialog::packet(
            result == HRESULT_FROM_WIN32(ERROR_CANCELLED) ? "cancelled" : "error", juce::String::toHexString(static_cast<int>(result))));
    CoUninitialize();
    return SUCCEEDED(result) || result == HRESULT_FROM_WIN32(ERROR_CANCELLED) ? 0 : 4;
}

class NativeVideoDialogSession final
{
public:
    explicit NativeVideoDialogSession(juce::File helper = {}) : helperExecutable(std::move(helper)) {}
    ~NativeVideoDialogSession() { close(); }
    bool start(RECT bounds)
    {
        close();
        directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("FengYin-video-picker-" + juce::Uuid().toString());
        if (directory.createDirectory().failed()) return false;
        auto request = std::make_unique<juce::DynamicObject>();
        request->setProperty("left", int(bounds.left)); request->setProperty("top", int(bounds.top));
        request->setProperty("right", int(bounds.right)); request->setProperty("bottom", int(bounds.bottom));
        request->setProperty("hostPid", static_cast<juce::int64>(GetCurrentProcessId()));
        if (!videoDialog::writePacket(directory.getChildFile("request.json"), juce::var(request.release()))) { close(); return false; }
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) { close(); return false; }
        const auto executable = (helperExecutable == juce::File()
            ? juce::File::getSpecialLocation(juce::File::currentExecutableFile) : helperExecutable).getFullPathName();
        const auto command = executable.quoted() + " --select-video " + directory.getFullPathName().quoted();
        std::wstring mutableCommand(command.toWideCharPointer());
        STARTUPINFOW startup {sizeof(startup)};
        PROCESS_INFORMATION processInfo {};
        if (!CreateProcessW(executable.toWideCharPointer(), mutableCommand.data(), nullptr, nullptr, FALSE,
                            CREATE_SUSPENDED | CREATE_NO_WINDOW, nullptr, nullptr, &startup, &processInfo)) { close(); return false; }
        process = processInfo.hProcess; pid = processInfo.dwProcessId;
        if (!AssignProcessToJobObject(job, process))
        { TerminateProcess(process, 1); CloseHandle(processInfo.hThread); close(); return false; }
        if (videoDialog::foregroundProcess() == GetCurrentProcessId()) AllowSetForegroundWindow(pid);
        const auto resumed = ResumeThread(processInfo.hThread) != static_cast<DWORD>(-1);
        CloseHandle(processInfo.hThread);
        if (!resumed) { close(); return false; }
        started = juce::Time::getMillisecondCounterHiRes();
        bringToFront();
        return true;
    }
    juce::var result() const { return juce::JSON::parse(directory.getChildFile("result.json")); }
    bool running() const { return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT; }
    HWND window()
    {
        if (!hwnd && running())
        {
            const auto ready = juce::JSON::parse(directory.getChildFile("ready.json"));
            const auto candidate = reinterpret_cast<HWND>(static_cast<INT_PTR>(static_cast<juce::int64>(ready["window"])));
            DWORD candidatePid = 0;
            if (candidate && GetWindowThreadProcessId(candidate, &candidatePid) && candidatePid == pid)
            {
                hwnd = candidate;
                canActivate = static_cast<bool>(ready["canActivate"]);
            }
        }
        return hwnd;
    }
    DWORD processId() const { return pid; }
    juce::var diagnostics()
    {
        auto state = videoDialog::packet(running() ? "running" : "exited");
        auto* object = state.getDynamicObject();
        object->setProperty("pid", static_cast<juce::int64>(pid));
        object->setProperty("window", static_cast<juce::int64>(reinterpret_cast<INT_PTR>(window())));
        object->setProperty("ready", juce::JSON::parse(directory.getChildFile("ready.json")));
        object->setProperty("result", result());
        return state;
    }
    bool startupExpired() const { return juce::Time::getMillisecondCounterHiRes() - started > 60000 && !hwnd; }
    void bringToFront()
    {
        foregroundAttempts = 3;
        nextForegroundAttempt = 0;
    }
    void serviceForegroundRequest()
    {
        const auto handle = window();
        if (foregroundAttempts == 0 || !handle || !running() || !IsWindowVisible(handle)) return;
        const auto foregroundPid = videoDialog::foregroundProcess();
        if (foregroundPid != GetCurrentProcessId())
        {
            // The helper or another application already has focus.
            if (foregroundPid != 0) foregroundAttempts = 0;
            return;
        }
        DWORD handlePid = 0;
        if (!canActivate || !GetWindowThreadProcessId(handle, &handlePid) || handlePid != pid)
        {
            foregroundAttempts = 0;
            juce::Logger::writeToLog("Native video selector foreground handoff unavailable");
            return;
        }
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (now < nextForegroundAttempt) return;
        AllowSetForegroundWindow(pid);
        PostMessageW(handle, videoDialog::foregroundRequestMessage(), 0, 0);
        nextForegroundAttempt = now + 150;
        if (--foregroundAttempts == 0)
            juce::Logger::writeToLog("Native video selector foreground handoff retried");
    }
    void close()
    {
        // Closing the job terminates only this selector. Never wait here: a
        // stuck Shell extension/activation must not block destruction or audio.
        if (job) CloseHandle(job);
        if (process) CloseHandle(process);
        job = nullptr; process = nullptr; hwnd = nullptr; pid = 0;
        canActivate = false;
        foregroundAttempts = 0;
        nextForegroundAttempt = 0;
        if (directory != juce::File()) directory.deleteRecursively();
        directory = {};
    }
private:
    HANDLE job = nullptr, process = nullptr;
    HWND hwnd = nullptr;
    DWORD pid = 0;
    juce::File directory;
    juce::File helperExecutable;
    double started = 0;
    bool canActivate = false;
    int foregroundAttempts = 0;
    double nextForegroundAttempt = 0;
};
} // namespace fengyin
#endif
