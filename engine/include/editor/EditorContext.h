#pragma once

class EditorConsole;
class IEditableScene;

struct EditorContext {
    IEditableScene *scene = nullptr;
    EditorConsole *console = nullptr;
    bool paused = false;
};
