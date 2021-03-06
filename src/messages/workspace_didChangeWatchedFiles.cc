#include "clang_complete.h"
#include "message_handler.h"
#include "project.h"
#include "pipeline.hh"
#include "working_files.h"
using namespace ccls;

namespace {
MethodType kMethodType = "workspace/didChangeWatchedFiles";

enum class lsFileChangeType {
  Created = 1,
  Changed = 2,
  Deleted = 3,
};
MAKE_REFLECT_TYPE_PROXY(lsFileChangeType);

struct lsFileEvent {
  lsDocumentUri uri;
  lsFileChangeType type;
};
MAKE_REFLECT_STRUCT(lsFileEvent, uri, type);

struct lsDidChangeWatchedFilesParams {
  std::vector<lsFileEvent> changes;
};
MAKE_REFLECT_STRUCT(lsDidChangeWatchedFilesParams, changes);

struct In_WorkspaceDidChangeWatchedFiles : public NotificationInMessage {
  MethodType GetMethodType() const override { return kMethodType; }
  lsDidChangeWatchedFilesParams params;
};
MAKE_REFLECT_STRUCT(In_WorkspaceDidChangeWatchedFiles, params);
REGISTER_IN_MESSAGE(In_WorkspaceDidChangeWatchedFiles);

struct Handler_WorkspaceDidChangeWatchedFiles
    : BaseMessageHandler<In_WorkspaceDidChangeWatchedFiles> {
  MethodType GetMethodType() const override { return kMethodType; }
  void Run(In_WorkspaceDidChangeWatchedFiles* request) override {
    for (lsFileEvent& event : request->params.changes) {
      std::string path = event.uri.GetPath();
      Project::Entry entry;
      {
        std::lock_guard<std::mutex> lock(project->mutex_);
        auto it = project->absolute_path_to_entry_index_.find(path);
        if (it == project->absolute_path_to_entry_index_.end())
          continue;
        entry = project->entries[it->second];
      }
      bool is_interactive =
          working_files->GetFileByFilename(entry.filename) != nullptr;
      switch (event.type) {
        case lsFileChangeType::Created:
        case lsFileChangeType::Changed: {
          pipeline::Index(path, entry.args, is_interactive);
          if (is_interactive)
            clang_complete->NotifySave(path);
          break;
        }
        case lsFileChangeType::Deleted:
          pipeline::Index(path, entry.args, is_interactive);
          break;
      }
    }
  }
};
REGISTER_MESSAGE_HANDLER(Handler_WorkspaceDidChangeWatchedFiles);
}  // namespace
