#include "App.h"

#include <Windows.h>
#include <exception>

namespace {

void ShowErrorMessage(const char *message) {
    MessageBoxA(nullptr, message, "App Error", MB_OK | MB_ICONERROR);
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    try {
        App app;
        return app.Run(instance, showCommand);
    } catch (const std::exception &e) {
        ShowErrorMessage(e.what());
    } catch (...) {
        ShowErrorMessage("Unknown error");
    }

    return 1;
}
