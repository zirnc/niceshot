#pragma once

#include <ports/screenshot_repository.h>
#include <sqlite3.h>

#include <filesystem>

namespace niceshot::sqlite {

class SqliteScreenshotRepository final : public ScreenshotRepository {
 public:
  explicit SqliteScreenshotRepository(std::filesystem::path db_path);
  ~SqliteScreenshotRepository() override;

  void upsert(const Screenshot& screenshot) override;
  std::vector<Screenshot> recent(int limit) override;

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace niceshot::sqlite