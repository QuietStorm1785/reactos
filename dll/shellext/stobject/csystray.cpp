/*
* PROJECT:     ReactOS system libraries
* LICENSE:     GPL - See COPYING in the top level directory
* FILE:        dll\win32\stobject\csystray.cpp
* PURPOSE:     Systray shell service object implementation
* PROGRAMMERS: David Quintana <gigaherz@gmail.com>
*/

#include "precomp.h"

WINE_DEFAULT_DEBUG_CHANNEL(stobject);

SysTrayIconHandlers_t g_IconHandlers [] = {
        { Volume_Init, Volume_Shutdown, Volume_Update, Volume_Message }
};
const int             g_NumIcons = _countof(g_IconHandlers);

const GUID CLSID_SysTray = { 0x35CEC8A3, 0x2BE6, 0x11D2, { 0x87, 0x73, 0x92, 0xE2, 0x20, 0x52, 0x41, 0x53 } };

CSysTray::CSysTray() {}
CSysTray::~CSysTray() {}

HRESULT CSysTray::InitIcons()
{
    for (int i = 0; i < g_NumIcons; i++)
    {
        HRESULT hr = g_IconHandlers[i].pfnInit(this);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT CSysTray::ShutdownIcons()
{
    for (int i = 0; i < g_NumIcons; i++)
    {
        HRESULT hr = g_IconHandlers[i].pfnShutdown(this);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT CSysTray::UpdateIcons()
{
    for (int i = 0; i < g_NumIcons; i++)
    {
        HRESULT hr = g_IconHandlers[i].pfnUpdate(this);
        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

HRESULT CSysTray::ProcessIconMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    for (int i = 0; i < g_NumIcons; i++)
    {
        HRESULT hr = g_IconHandlers[i].pfnMessage(this, uMsg, wParam, lParam);
        if (FAILED(hr))
            return hr;

        if (hr != S_FALSE)
            return hr;
    }

    return S_OK;
}

HRESULT CSysTray::NotifyIcon(INT code, UINT uId, HICON hIcon, LPCWSTR szTip)
{
    NOTIFYICONDATA nim;
    nim.cbSize = sizeof(NOTIFYICONDATA);
    nim.uFlags = NIF_ICON | NIF_STATE | NIF_TIP;
    nim.hIcon = hIcon;
    nim.uID = uId;
    nim.uCallbackMessage = uId;
    nim.dwState = 0;
    nim.dwStateMask = 0;
    nim.hWnd = m_hWnd;
    nim.uVersion = NOTIFYICON_VERSION;
    if (szTip)
        StringCchCopy(nim.szTip, _countof(nim.szTip), szTip);
    else
        nim.szTip[0] = 0;
    BOOL ret = Shell_NotifyIcon(code, &nim);
    return ret ? S_OK : E_FAIL;
}

DWORD WINAPI CSysTray::s_SysTrayThreadProc(PVOID param)
{
    CSysTray * st = (CSysTray*) param;
    return st->SysTrayThreadProc();
}

HRESULT CSysTray::SysTrayMessageLoop()
{
    BOOL ret;
    MSG msg;

    while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (ret < 0)
            break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return S_OK;
}

HRESULT CSysTray::SysTrayThreadProc()
{
    WCHAR strFileName[MAX_PATH];
    GetModuleFileNameW(g_hInstance, strFileName, MAX_PATH);
    HMODULE hLib = LoadLibraryW(strFileName);

    CoInitializeEx(NULL, COINIT_DISABLE_OLE1DDE | COINIT_APARTMENTTHREADED);

    Create(NULL);

    HRESULT ret = SysTrayMessageLoop();

    CoUninitialize();

    FreeLibraryAndExitThread(hLib, ret);
}

HRESULT CSysTray::CreateSysTrayThread()
{
    DbgPrint("CSysTray Init TODO: Initialize tray icon handlers.\n");

    HANDLE hThread = CreateThread(NULL, 0, s_SysTrayThreadProc, this, 0, NULL);

    CloseHandle(hThread);

    return S_OK;
}

HRESULT CSysTray::DestroySysTrayWindow()
{
    DestroyWindow();
    hwndSysTray = NULL;
    return S_OK;
}

// *** IOleCommandTarget methods ***
HRESULT STDMETHODCALLTYPE CSysTray::QueryStatus(const GUID *pguidCmdGroup, ULONG cCmds, OLECMD prgCmds [], OLECMDTEXT *pCmdText)
{
    UNIMPLEMENTED;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE CSysTray::Exec(const GUID *pguidCmdGroup, DWORD nCmdID, DWORD nCmdexecopt, VARIANT *pvaIn, VARIANT *pvaOut)
{
    if (!IsEqualGUID(*pguidCmdGroup, CGID_ShellServiceObject))
        return E_FAIL;

    switch (nCmdID)
    {
    case OLECMDID_NEW: // init
        return CreateSysTrayThread();
    case OLECMDID_SAVE: // shutdown
        return DestroySysTrayWindow();
    }
    return S_OK;
}

BOOL CSysTray::ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT &lResult, DWORD dwMsgMapID)
{
    HRESULT hr;

    if (hWnd != m_hWnd)
        return FALSE;

    switch (uMsg)
    {
    case WM_CREATE:
        InitIcons();
        SetTimer(1, 2000, NULL);
        return TRUE;
    case WM_TIMER:
        UpdateIcons();
        return TRUE;
    case WM_DESTROY:
        ShutdownIcons();
        return TRUE;
    }

    DbgPrint("SysTray message received %u (%08p %08p)\n", uMsg, wParam, lParam);

    hr = ProcessIconMessage(uMsg, wParam, lParam);
    if (FAILED(hr))
        return FALSE;

    if (hr == S_FALSE)
        return FALSE;

    return TRUE;
}
