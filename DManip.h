#pragma once

using namespace Microsoft::WRL;

class CViewportEventHandler : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>,
    Implements<RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::ClassicCom>,
    Microsoft::WRL::FtmBase,
    IDirectManipulationViewportEventHandler
    >>
{
public:
    /// IDirectManipulationViewportEventHandler
    IFACEMETHOD(OnViewportStatusChanged)(
        _In_ IDirectManipulationViewport *viewport,
        _In_ DIRECTMANIPULATION_STATUS current,
        _In_ DIRECTMANIPULATION_STATUS previous);

    IFACEMETHOD(OnViewportUpdated)(
        _In_ IDirectManipulationViewport *viewport);

    IFACEMETHOD(OnContentUpdated)(
        _In_ IDirectManipulationViewport *viewport,
        _In_ IDirectManipulationContent *content);
};

class PTPDManip : public IDManipInterop
{
public:
    // IDManipInterop
    virtual HRESULT Initialize(
        _In_ HWND hWnd) override;

public:
    HRESULT ResetViewport();

private:
    ComPtr<IDirectManipulationManager2> _manager;
    ComPtr<IDirectManipulationUpdateManager> _updateManager;
    ComPtr<IDirectManipulationViewport> _viewport;
    ComPtr<CViewportEventHandler> _handler;
    DWORD _viewportHandlerCookie;
    RECT const c_defaultRect = { 0, 0, 100, 100 };
    UINT_PTR _renderTimer;

private:
    WNDPROC _hookedWndProc;
    static LRESULT APIENTRY WndProcStatic(
        _In_ HWND hWnd,
        _In_ UINT uMsg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam);
    LRESULT APIENTRY WndProc(
        _In_ HWND hWnd,
        _In_ UINT uMsg,
        _In_ WPARAM wParam,
        _In_ LPARAM lParam);
};
