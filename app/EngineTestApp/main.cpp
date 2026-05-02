#include "DirectXCommon.h"
#include "Input.h"
#include "SrvManager.h"
#include "WinApp.h"

#include <Windows.h>
#include <chrono>
#include <exception>
#include <string>

namespace {

constexpr int kClientWidth = 1280;
constexpr int kClientHeight = 720;

float CalculateDeltaTime(std::chrono::steady_clock::time_point &previous) {
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<float> delta = now - previous;
    previous = now;
    return delta.count();
}

void ShowErrorMessage(const char *message) {
    MessageBoxA(nullptr, message, "EngineTestApp Error", MB_OK | MB_ICONERROR);
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    try {
        WinApp winApp;
        winApp.Initialize(hInstance, nCmdShow, kClientWidth, kClientHeight,
                          L"Engine Test App");

        DirectXCommon dxCommon;
        dxCommon.Initialize(winApp.GetHwnd(), winApp.GetWidth(),
                            winApp.GetHeight());

        SrvManager srvManager;
        srvManager.Initialize(&dxCommon);

        Input input;
        input.Initialize(hInstance, winApp.GetHwnd());

        int currentWidth = winApp.GetWidth();
        int currentHeight = winApp.GetHeight();
        auto previousTime = std::chrono::steady_clock::now();

        while (winApp.ProcessMessage()) {
            const float deltaTime = CalculateDeltaTime(previousTime);
            input.Update(deltaTime);

            if (input.IsKeyTrigger(DIK_ESCAPE)) {
                PostQuitMessage(0);
                continue;
            }

            const int width = winApp.GetWidth();
            const int height = winApp.GetHeight();
            if (width != currentWidth || height != currentHeight) {
                currentWidth = width;
                currentHeight = height;
                dxCommon.Resize(width, height);
            }

            dxCommon.BeginFrame();
            dxCommon.EndFrame();
        }

        return 0;
    } catch (const std::exception &e) {
        ShowErrorMessage(e.what());
    } catch (...) {
        ShowErrorMessage("Unknown error");
    }

    return 1;
}
