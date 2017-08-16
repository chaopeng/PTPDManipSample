#include "stdafx.h"

LRESULT CALLBACK WndProc(
    _In_ HWND hWnd,
    _In_ UINT message,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

HWND MakeWindow()
{
    WNDCLASSEX wcex = { 0 };
    HWND hWnd;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpfnWndProc = WndProc;
    wcex.lpszClassName = L"PTPDManipClass";
    hWnd = CreateWindowEx(0, MAKEINTATOM(RegisterClassEx(&wcex)), L"PTPDManipwindow", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 300, nullptr, nullptr, nullptr, nullptr);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    return hWnd;
}

int main()
{
    HWND hWnd = MakeWindow();
    if (hWnd == nullptr)
    {
        return -1;
    }

    if (FAILED(gpInterop->Initialize(hWnd)))
    {
        return -2;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

