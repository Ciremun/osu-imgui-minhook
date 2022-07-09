#include <windows.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"

#include "minhook/minhook.h"

typedef BOOL(__stdcall* twglSwapBuffers)(HDC hDc);

HWND g_hwnd = NULL;
HMODULE g_hmodule = NULL;
twglSwapBuffers wglSwapBuffersGateway;

WNDPROC oWndProc;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
        return true;

    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK find_window(HWND hwnd, LPARAM lParam)
{
    DWORD lpdwProcessId;
    GetWindowThreadProcessId(hwnd, &lpdwProcessId);
    if (lpdwProcessId == lParam)
    {
        g_hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

//DWORD __stdcall EjectThread(LPVOID lpParameter) {
//    (void)lpParameter;
//    Sleep(100);
//    FreeLibraryAndExitThread(g_hmodule, 0);
//}

BOOL __stdcall update(HDC hDc)
{
    static bool init = false;
    if (!init)
    {
        EnumWindows(find_window, GetCurrentProcessId());

        // this fixes broken mouse input if injected while minimized
        if (!IsWindowVisible(g_hwnd))
            return wglSwapBuffersGateway(hDc);

        oWndProc = (WNDPROC)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;

        ImGui::StyleColorsDark();
        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplOpenGL3_Init();

        init = true;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Once);
    ImGui::Begin("Hook");
    ImGui::Text("Sample Text");
    ImGui::End();

    io.MouseDrawCursor = io.WantCaptureMouse;

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    return wglSwapBuffersGateway(hDc);
}

DWORD WINAPI hook_init(HMODULE hModule)
{
    g_hmodule = hModule;

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK)
        return 1;

    HMODULE hMod = GetModuleHandleA("opengl32.dll");
    if (hMod == NULL)
        return 1;

    FARPROC swapbuffers_proc = GetProcAddress(hMod, "wglSwapBuffers");
    if (swapbuffers_proc == NULL)
        return 1;

    // Debug

    //AllocConsole();
    //FILE* f;
    //freopen_s(&f, "CONOUT$", "w", stdout);

    if (MH_CreateHook(swapbuffers_proc, &update, (LPVOID *)&wglSwapBuffersGateway) != MH_OK)
        return 1;

    if (MH_EnableHook(swapbuffers_proc) != MH_OK)
        return 1;

    // Cleanup

    //if (MH_DisableHook(MH_ALL_HOOKS) != MH_OK)
    //    return 1;
    // 
    //if (MH_Uninitialize() != MH_OK)
    //    return 1;

    //ImGui_ImplOpenGL3_Shutdown();
    //ImGui_ImplWin32_Shutdown();
    //ImGui::DestroyContext();

    //SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)(oWndProc));

    //CreateThread(0, 0, EjectThread, 0, 0, 0);

    return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)hook_init, hModule, 0, nullptr));
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

