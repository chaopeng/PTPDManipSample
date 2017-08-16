#pragma once

#include <iostream>

#include <Windows.h>
#include <wrl.h>
#include <directmanipulation.h>
#include <strsafe.h>

#define IFC(expr) { hr = (expr); if (FAILED(hr)) goto Cleanup; }
#define IFC_OOM(expr) { if ((expr) == nullptr) { hr = E_OUTOFMEMORY; goto Cleanup; } }

class IDManipInterop
{
public:
    virtual HRESULT Initialize(
        _In_ HWND hWnd) = 0;
};

extern IDManipInterop* gpInterop;
