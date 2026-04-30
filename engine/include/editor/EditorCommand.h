#pragma once
#include <memory>
#include <string>
#include <vector>

class IEditableScene;

class IEditorCommand {
  public:
    virtual ~IEditorCommand() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual const char *GetName() const = 0;
};

class EditorCommandManager {
  public:
    void Execute(std::unique_ptr<IEditorCommand> command);
    void Undo();
    void Redo();
    void Clear();
    bool CanUndo() const;
    bool CanRedo() const;

  private:
    std::vector<std::unique_ptr<IEditorCommand>> undoStack_;
    std::vector<std::unique_ptr<IEditorCommand>> redoStack_;
};

class SceneStateCommand final : public IEditorCommand {
  public:
    SceneStateCommand(IEditableScene &scene, std::string name,
                      std::string beforeState, std::string afterState);

    void Execute() override;
    void Undo() override;
    const char *GetName() const override;

  private:
    IEditableScene *scene_ = nullptr;
    std::string name_;
    std::string beforeState_;
    std::string afterState_;
};
