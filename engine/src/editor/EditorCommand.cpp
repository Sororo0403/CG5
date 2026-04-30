#include "EditorCommand.h"
#include "IEditableScene.h"
#include <utility>

void EditorCommandManager::Execute(std::unique_ptr<IEditorCommand> command) {
    if (!command) {
        return;
    }
    command->Execute();
    undoStack_.push_back(std::move(command));
    redoStack_.clear();
}

void EditorCommandManager::Undo() {
    if (undoStack_.empty()) {
        return;
    }
    std::unique_ptr<IEditorCommand> command = std::move(undoStack_.back());
    undoStack_.pop_back();
    command->Undo();
    redoStack_.push_back(std::move(command));
}

void EditorCommandManager::Redo() {
    if (redoStack_.empty()) {
        return;
    }
    std::unique_ptr<IEditorCommand> command = std::move(redoStack_.back());
    redoStack_.pop_back();
    command->Execute();
    undoStack_.push_back(std::move(command));
}

void EditorCommandManager::Clear() {
    undoStack_.clear();
    redoStack_.clear();
}

bool EditorCommandManager::CanUndo() const { return !undoStack_.empty(); }

bool EditorCommandManager::CanRedo() const { return !redoStack_.empty(); }

SceneStateCommand::SceneStateCommand(IEditableScene &scene, std::string name,
                                     std::string beforeState,
                                     std::string afterState)
    : scene_(&scene), name_(std::move(name)),
      beforeState_(std::move(beforeState)), afterState_(std::move(afterState)) {
}

void SceneStateCommand::Execute() {
    if (scene_) {
        scene_->RestoreSceneState(afterState_, nullptr);
    }
}

void SceneStateCommand::Undo() {
    if (scene_) {
        scene_->RestoreSceneState(beforeState_, nullptr);
    }
}

const char *SceneStateCommand::GetName() const { return name_.c_str(); }
