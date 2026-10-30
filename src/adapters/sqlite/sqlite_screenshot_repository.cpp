#include "sqlite_screenshot_repository.h"

namespace niceshot::sqlite {

SqliteScreenshotRepository::SqliteScreenshotRepository(
    std::filesystem::path db_path) {
  std::filesystem::create_directories(db_path.parent_path());

  if (sqlite3_open_v2(db_path.c_str(), &db_,
                      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                      nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }

  sqlite3_busy_timeout(db_, 5000);
  // WAL 让 watch 写入时，list 仍然可以读。
  sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

  const char* schema = R"sql(
    CREATE TABLE IF NOT EXISTS screenshots (
      path        TEXT PRIMARY KEY,
      captured_at INTEGER NOT NULL,
      first_seen  INTEGER NOT NULL,
      last_seen   INTEGER NOT NULL
    );
  )sql";

  char* error = nullptr;
  if (sqlite3_exec(db_, schema, nullptr, nullptr, &error) != SQLITE_OK) {
    std::string message = error;
    sqlite3_free(error);
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error(message);
  }
}

SqliteScreenshotRepository::~SqliteScreenshotRepository() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
  }
}

void SqliteScreenshotRepository::upsert(const Screenshot& screenshot) {
  const auto captured_at =
      std::chrono::system_clock::to_time_t(screenshot.captured_at);
  const char* sql =
      "INSERT INTO screenshots(path, captured_at, first_seen, last_seen) "
      "VALUES(?, ?, ?, ?)"
      "ON CONFLICT(path) DO UPDATE SET "
      "captured_at = excluded.captured_at,"
      "last_seen = excluded.last_seen;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }

  sqlite3_bind_text(stmt, 1, screenshot.path.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, captured_at);
  sqlite3_bind_int64(stmt, 3, captured_at);
  sqlite3_bind_int64(stmt, 4, captured_at);

  if (sqlite3_step(stmt) != SQLITE_DONE) {
    const std::string message = sqlite3_errmsg(db_);
    sqlite3_finalize(stmt);
    throw std::runtime_error(message);
  }
  sqlite3_finalize(stmt);
}

std::vector<Screenshot> SqliteScreenshotRepository::recent(int limit) {
  std::vector<Screenshot> rec;
  const char* sql =
      "SELECT path, captured_at FROM screenshots "
      "ORDER BY captured_at DESC LIMIT ?";

  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
  sqlite3_bind_int(stmt, 1, limit);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const auto* text = sqlite3_column_text(stmt, 0);
    const auto captured_at = sqlite3_column_int64(stmt, 1);

    std::string path(reinterpret_cast<const char*>(text));
    rec.push_back(
        {path, std::chrono::sys_seconds{std::chrono::seconds{captured_at}}});
  }

  sqlite3_finalize(stmt);
  return rec;
}
}  // namespace niceshot::sqlite