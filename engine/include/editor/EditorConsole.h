#pragma once
#include <string>
#include <vector>

class EditorConsole {
  public:
    void AddLog(const std::string &message);
    void Clear();
    const std::vector<std::string> &GetLogs() const { return logs_; }

  private:
    std::vector<std::string> logs_;
};
