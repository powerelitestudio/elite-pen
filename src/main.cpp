#include "application.hpp"

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX common_controls{sizeof(INITCOMMONCONTROLSEX),
                                         ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES};
    InitCommonControlsEx(&common_controls);

    elite_pen::win::Application application;
    const int result = application.run();

    if (SUCCEEDED(com_result)) CoUninitialize();
    return result;
}
