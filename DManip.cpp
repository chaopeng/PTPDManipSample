#include "stdafx.h"

#include "DManip.h"

using namespace std;

PTPDManip* gpDManip = new PTPDManip();
IDManipInterop* gpInterop = gpDManip;

HRESULT PTPDManip::Initialize(_In_ HWND hWnd) {
  HRESULT hr = S_OK;

  // The IDirectManipulationManager is the main entrypoint of the DManip API
  // surface. We use it to get other objects, create "viewports", and activate
  // DManip processing.
  IFC(CoCreateInstance(CLSID_DirectManipulationManager, nullptr,
                       CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_manager)));

  // The IDirectManipulationUpdateManager is how we inform DManip that we are
  // "rendering" a frame - when we do so, we'll get callbacks for any active
  // manipulations on our viewports.
  IFC(_manager->GetUpdateManager(IID_PPV_ARGS(&_updateManager)));

  // Create our fake viewport, to which we'll direct all incoming PTP gesturing
  // contacts.
  IFC(_manager->CreateViewport(nullptr, hWnd, IID_PPV_ARGS(&_viewport)));

  // We want our viewport to be pannable/zoomable, with inertia and pan railing
  // (axis-locking).
  DIRECTMANIPULATION_CONFIGURATION targetConfiguration =
      DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_X |
      DIRECTMANIPULATION_CONFIGURATION_RAILS_Y |
      DIRECTMANIPULATION_CONFIGURATION_SCALING |
      DIRECTMANIPULATION_CONFIGURATION_SCALING_INERTIA;
  IFC(_viewport->ActivateConfiguration(targetConfiguration));

  // By specifying manual update, we take on the responsibility of calling
  // Update() on each "render tick", rather than just during inertia. It also
  // means that during a running manipulation, DManip will wait until we call
  // Update() to tell us of any generated motion, rather than queuing a
  // notification from the input event itself.
  IFC(_viewport->SetViewportOptions(
      DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE));

  // The CViewportEventHandler is where we'll actually get the callbacks from
  // DManip that tell us the viewport's state as well as how the content has
  // been manipulated.
  IFC_OOM(_handler = Make<CViewportEventHandler>());
  IFC(_viewport->AddEventHandler(hWnd, _handler.Get(),
                                 &_viewportHandlerCookie));

  // Set the default viewport rect. Whenever a manipulation completes, we'll
  // reset the content back here. In normal scenarios, we'd take more care to
  // set up the viewport rect, since it controls how input chains between
  // viewports and how to translate from client coordinates to viewport
  // coordinates. However, since we don't have any visual content, or other
  // viewports, we can skip that.
  IFC(_viewport->SetViewportRect(&c_defaultRect));

  // Activate DirectManipulation on our target window. This must be done on the
  // toplevel window, so that when the input stack performs its window hit-test,
  // it realizes the target window is DManip-enabled, so the input flows to
  // DManip rather than the normal input queue for the window. When PTP input
  // flows to the normal input queue, it is implicitly converted to mousewheel -
  // flowing to DManip bypasses that.
  IFC(_manager->Activate(hWnd));

  // Enable the viewport so that DManip will allow it to be manipulated.
  IFC(_viewport->Enable());

  // Subclass the hWnd so we can intercept some messages.
  _hookedWndProc =
      (WNDPROC)SetWindowLong(hWnd, GWL_WNDPROC, (LONG)WndProcStatic);

  // Start a timer for our "render loop"
  _renderTimer = 1;
  SetTimer(hWnd, _renderTimer, USER_TIMER_MINIMUM, nullptr);
  ;

Cleanup:
  return hr;
}

HRESULT PTPDManip::ResetViewport() {
  HRESULT hr = S_OK;

  // By zooming the primary content to a rect that matches the viewport rect,
  // we reset the content's transform to identity.
  RECT rcViewport;
  IFC(_viewport->GetViewportRect(&rcViewport));
  IFC(_viewport->ZoomToRect(static_cast<float>(rcViewport.left),
                            static_cast<float>(rcViewport.top),
                            static_cast<float>(rcViewport.right),
                            static_cast<float>(rcViewport.bottom), FALSE));

Cleanup:
  return hr;
}

LRESULT PTPDManip::WndProcStatic(_In_ HWND hWnd,
                                 _In_ UINT uMsg,
                                 _In_ WPARAM wParam,
                                 _In_ LPARAM lParam) {
  return gpDManip->WndProc(hWnd, uMsg, wParam, lParam);
}

LRESULT PTPDManip::WndProc(_In_ HWND hWnd,
                           _In_ UINT uMsg,
                           _In_ WPARAM wParam,
                           _In_ LPARAM lParam) {
  switch (uMsg) {
    // case WM_POINTERDOWN:
    case DM_POINTERHITTEST: {
      // Since WM_POINTER is not exposed for PTP, DManip can't forward over the
      // WM_POINTERDOWN message to allow the application to perform its
      // hit-test. Instead, it sends DM_POINTERHITTEST, with wParam/lParam
      // corresponding to the same info that is in pointer messages. No other
      // pointer type sends this message today, but it's best practice to check
      // that it is indeed a PTP pointer. Since this switch-conditional looks at
      // both messages, you can test with touch by adjusting the pointer type
      // check. We'd also in theory do a hit-test to see which viewport (if any)
      // to call SetContact() on (you can get the point from the pointer info or
      // GET_X_LPARAM/GET_Y_LPARAM), but since we want all PTP input to get sent
      // to DManip, regardless of its location, we will just unconditionally
      // call SetContact() on our viewport.
      UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
      POINTER_INFO info = {};
      if (GetPointerInfo(pointerId, &info) && info.pointerType == PT_TOUCHPAD) {
        _viewport->SetContact(pointerId);
        return 0;
      }
    } break;

    case WM_TIMER: {
      // If it's our render timer, feed it to DManip in case it's received
      // any output that has caused our content to be manipulated.
      if (wParam == _renderTimer) {
        _updateManager->Update(nullptr);
        return 0;
      }
    } break;
  }

  return CallWindowProc(_hookedWndProc, hWnd, uMsg, wParam, lParam);
}

HRESULT CViewportEventHandler::OnViewportStatusChanged(
    _In_ IDirectManipulationViewport* viewport,
    _In_ DIRECTMANIPULATION_STATUS current,
    _In_ DIRECTMANIPULATION_STATUS previous) {
  // The state of our viewport has changed! We'l be in one of three states:
  // * ENABLED: initial state
  // * READY: the previous manipulation has been completed
  // * RUNNING: there are active contacts performing a manipulation
  // * INERTIA: the contacts have lifted, but they have produced residual motion

  PWCHAR names[] = {L"BUILDING", L"ENABLED", L"DISABLED", L"RUNNING",
                    L"INERTIA",  L"READY",   L"SUSPENDED"};
  WCHAR debug[255];
  StringCchPrintf(debug, ARRAYSIZE(debug), L"STATUS: %s\n", names[current]);
  OutputDebugString(debug);
  wcout << debug;

  // Reset the viewport when we're idle, so the content transforms always start
  // at identity.
  if (current == DIRECTMANIPULATION_READY) {
    gpDManip->ResetViewport();
  }
  return S_OK;
}

HRESULT CViewportEventHandler::OnViewportUpdated(
    _In_ IDirectManipulationViewport* viewport) {
  // Fired when all content in a viewport has been updated.
  // Nothing to do here.
  return S_OK;
}

HRESULT CViewportEventHandler::OnContentUpdated(
    _In_ IDirectManipulationViewport* viewport,
    _In_ IDirectManipulationContent* content) {
  // Our content has been updated! Query its new transform.
  // For a pan, we'll get x/y translation (potentially railed).
  // For a zoom, we'll get scale as well as x/y translation, since
  // DManip is trying to keep the center point stable onscreen.
  // It can be ignored, since during a zoom we just care about how
  // the scale itself is changing.
  HRESULT hr = S_OK;

  float xform[6];
  IFC(content->GetContentTransform(xform, ARRAYSIZE(xform)));

  WCHAR debug[255];
  StringCchPrintf(debug, ARRAYSIZE(debug),
                  L"XFORM: scale=%.2fX,%.2fY; xlate=%.2fX,%.2fY\n", xform[0],
                  xform[3], xform[4], xform[5]);
  OutputDebugString(debug);
  wcout << debug;

Cleanup:
  return hr;
}
