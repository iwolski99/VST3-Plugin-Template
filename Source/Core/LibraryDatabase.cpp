#include "LibraryDatabase.h"

#include <sqlite3.h>

namespace foundry
{

namespace
{
    juce::String sqliteErr (sqlite3* db, const char* prefix)
    {
        juce::String m (prefix);
        if (db != nullptr)
            m += juce::String (": ") + sqlite3_errmsg (db);
        return m;
    }

    juce::int64 columnI64 (sqlite3_stmt* s, int col)
    {
        return (juce::int64) sqlite3_column_int64 (s, col);
    }

    juce::String columnText (sqlite3_stmt* s, int col)
    {
        const auto* t = reinterpret_cast<const char*> (sqlite3_column_text (s, col));
        return t != nullptr ? juce::String::fromUTF8 (t) : juce::String();
    }
}

LibraryDatabase::LibraryDatabase() = default;

LibraryDatabase::~LibraryDatabase()
{
    close();
}

void LibraryDatabase::close()
{
    const juce::ScopedLock sl (lock);

    if (db != nullptr)
    {
        sqlite3_close (db);
        db = nullptr;
    }
}

bool LibraryDatabase::exec (const char* sql, juce::String& error) const
{
    char* err = nullptr;
    const int rc = sqlite3_exec (db, sql, nullptr, nullptr, &err);

    if (rc != SQLITE_OK)
    {
        error = juce::String (err != nullptr ? err : "SQLite error");
        sqlite3_free (err);
        return false;
    }

    return true;
}

bool LibraryDatabase::createSchema (juce::String& error)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS library ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  filename TEXT NOT NULL,"
        "  path TEXT NOT NULL UNIQUE,"
        "  file_type TEXT,"
        "  category TEXT,"
        "  bpm REAL,"
        "  bars INTEGER,"
        "  genre TEXT,"
        "  date_created INTEGER,"
        "  date_indexed INTEGER,"
        "  favorite INTEGER DEFAULT 0,"
        "  generation_type TEXT,"
        "  generation_seed INTEGER,"
        "  parent_source_ids TEXT,"
        "  pattern_id TEXT,"
        "  engine TEXT,"
        "  generation_depth INTEGER DEFAULT 0,"
        "  exists_on_disk INTEGER DEFAULT 1"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_library_filename ON library(filename);"
        "CREATE INDEX IF NOT EXISTS idx_library_category ON library(category);"
        "CREATE INDEX IF NOT EXISTS idx_library_favorite ON library(favorite);"
        "CREATE INDEX IF NOT EXISTS idx_library_created ON library(date_created);"
        "CREATE TABLE IF NOT EXISTS thumbs ("
        "  path TEXT PRIMARY KEY,"
        "  mod_time INTEGER,"
        "  size INTEGER,"
        "  seconds REAL,"
        "  peaks BLOB"
        ");";

    return exec (sql, error);
}

bool LibraryDatabase::open (const juce::File& file, juce::String& error)
{
    close();

    const juce::ScopedLock sl (lock);

    if (! file.getParentDirectory().isDirectory())
        file.getParentDirectory().createDirectory();

    // Keep the string alive: toRawUTF8() points into it, and calling it on the
    // temporary returned by getFullPathName() hands sqlite a dangling pointer.
    const auto path = file.getFullPathName();
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    const int rc = sqlite3_open_v2 (path.toRawUTF8(), &db, flags, nullptr);

    if (rc != SQLITE_OK)
    {
        error = sqliteErr (db, "Could not open library database");
        if (db != nullptr)
        {
            sqlite3_close (db);
            db = nullptr;
        }
        return false;
    }

    dbFile = file;
    sqlite3_busy_timeout (db, 4000);
    exec ("PRAGMA journal_mode=WAL;", error);
    exec ("PRAGMA synchronous=NORMAL;", error);
    exec ("PRAGMA foreign_keys=ON;", error);
    error.clear();

    if (! createSchema (error))
        return false;

    return true;
}

bool LibraryDatabase::rebuild (juce::String& error)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
    {
        error = "Database is not open.";
        return false;
    }

    return exec ("DELETE FROM library; DELETE FROM thumbs;", error);
}

juce::String LibraryDatabase::likePattern (const juce::String& raw)
{
    auto q = raw.trim();
    q = q.replace ("\\", "\\\\").replace ("%", "\\%").replace ("_", "\\_");
    return "%" + q + "%";
}

LibraryItem LibraryDatabase::readRow (sqlite3_stmt* stmt) const
{
    LibraryItem it;
    it.id               = columnI64 (stmt, 0);
    it.filename         = columnText (stmt, 1);
    it.path             = columnText (stmt, 2);
    it.fileType         = columnText (stmt, 3);
    it.category         = categoryFromKey (columnText (stmt, 4));
    it.bpm              = sqlite3_column_double (stmt, 5);
    it.bars             = sqlite3_column_int (stmt, 6);
    it.genre            = columnText (stmt, 7);
    it.dateCreated      = columnI64 (stmt, 8);
    it.dateIndexed      = columnI64 (stmt, 9);
    it.favorite         = sqlite3_column_int (stmt, 10) != 0;
    it.generationType   = columnText (stmt, 11);
    it.generationSeed   = columnI64 (stmt, 12);
    it.parentSourceIds  = columnText (stmt, 13);
    it.patternId        = columnText (stmt, 14);
    it.engine           = columnText (stmt, 15);
    it.generationDepth  = sqlite3_column_int (stmt, 16);
    it.existsOnDisk     = sqlite3_column_int (stmt, 17) != 0;
    return it;
}

bool LibraryDatabase::upsert (const LibraryItem& item, juce::String& error)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
    {
        error = "Database is not open.";
        return false;
    }

    const char* sql =
        "INSERT INTO library (filename, path, file_type, category, bpm, bars, genre,"
        " date_created, date_indexed, favorite, generation_type, generation_seed,"
        " parent_source_ids, pattern_id, engine, generation_depth, exists_on_disk)"
        " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17)"
        " ON CONFLICT(path) DO UPDATE SET"
        " filename=excluded.filename,"
        " file_type=excluded.file_type,"
        " category=excluded.category,"
        " bpm=excluded.bpm,"
        " bars=excluded.bars,"
        " genre=excluded.genre,"
        " date_created=excluded.date_created,"
        " date_indexed=excluded.date_indexed,"
        " generation_type=COALESCE(excluded.generation_type, library.generation_type),"
        " generation_seed=excluded.generation_seed,"
        " parent_source_ids=excluded.parent_source_ids,"
        " pattern_id=excluded.pattern_id,"
        " engine=excluded.engine,"
        " generation_depth=excluded.generation_depth,"
        " exists_on_disk=1,"
        " favorite=library.favorite;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
    {
        error = sqliteErr (db, "Prepare upsert failed");
        return false;
    }

    const auto cat = categoryKey (item.category);
    sqlite3_bind_text  (stmt, 1,  item.filename.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 2,  item.path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 3,  item.fileType.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 4,  cat.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double (stmt, 5, item.bpm);
    sqlite3_bind_int   (stmt, 6,  item.bars);
    sqlite3_bind_text  (stmt, 7,  item.genre.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 8,  (sqlite3_int64) item.dateCreated);
    sqlite3_bind_int64 (stmt, 9,  (sqlite3_int64) item.dateIndexed);
    sqlite3_bind_int   (stmt, 10, item.favorite ? 1 : 0);
    sqlite3_bind_text  (stmt, 11, item.generationType.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 12, (sqlite3_int64) item.generationSeed);
    sqlite3_bind_text  (stmt, 13, item.parentSourceIds.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 14, item.patternId.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text  (stmt, 15, item.engine.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int   (stmt, 16, item.generationDepth);
    sqlite3_bind_int   (stmt, 17, item.existsOnDisk ? 1 : 0);

    const int rc = sqlite3_step (stmt);
    sqlite3_finalize (stmt);

    if (rc != SQLITE_DONE)
    {
        error = sqliteErr (db, "Could not index file");
        return false;
    }

    return true;
}

bool LibraryDatabase::removeByPath (const juce::String& path, juce::String& error)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "DELETE FROM library WHERE path=?1;", -1, &stmt, nullptr) != SQLITE_OK)
    {
        error = sqliteErr (db, "Prepare delete failed");
        return false;
    }

    sqlite3_bind_text (stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step (stmt);
    sqlite3_finalize (stmt);
    return rc == SQLITE_DONE;
}

bool LibraryDatabase::removeById (juce::int64 id, juce::String& error)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "DELETE FROM library WHERE id=?1;", -1, &stmt, nullptr) != SQLITE_OK)
    {
        error = sqliteErr (db, "Prepare delete failed");
        return false;
    }

    sqlite3_bind_int64 (stmt, 1, (sqlite3_int64) id);
    const int rc = sqlite3_step (stmt);
    sqlite3_finalize (stmt);
    return rc == SQLITE_DONE;
}

bool LibraryDatabase::setFavorite (juce::int64 id, bool favorite, juce::String& error)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "UPDATE library SET favorite=?1 WHERE id=?2;", -1, &stmt, nullptr) != SQLITE_OK)
    {
        error = sqliteErr (db, "Prepare favorite failed");
        return false;
    }

    sqlite3_bind_int (stmt, 1, favorite ? 1 : 0);
    sqlite3_bind_int64 (stmt, 2, (sqlite3_int64) id);
    const int rc = sqlite3_step (stmt);
    sqlite3_finalize (stmt);
    return rc == SQLITE_DONE;
}

bool LibraryDatabase::markMissing (const juce::String& path)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "UPDATE library SET exists_on_disk=0 WHERE path=?1;", -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text (stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step (stmt);
    sqlite3_finalize (stmt);
    return rc == SQLITE_DONE;
}

std::vector<LibraryItem> LibraryDatabase::query (Category category,
                                                 const juce::String& search,
                                                 SortMode sort,
                                                 bool favoritesOnly) const
{
    std::vector<LibraryItem> out;
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return out;

    juce::String sql = "SELECT id, filename, path, file_type, category, bpm, bars, genre,"
                       " date_created, date_indexed, favorite, generation_type, generation_seed,"
                       " parent_source_ids, pattern_id, engine, generation_depth, exists_on_disk"
                       " FROM library WHERE exists_on_disk=1";

    const bool searching = search.trim().isNotEmpty();
    const bool filterCat = ! favoritesOnly && category != Category::Unknown;
    int nextParam = 1;
    int catParam = 0, likeParam = 0;

    if (favoritesOnly)
        sql += " AND favorite=1";

    if (filterCat)
    {
        catParam = nextParam++;
        sql += " AND category=?" + juce::String (catParam);
    }

    if (searching)
    {
        likeParam = nextParam++;
        sql += " AND filename LIKE ?" + juce::String (likeParam) + " ESCAPE '\\'";
    }

    switch (sort)
    {
        case SortMode::Newest:    sql += " ORDER BY date_created DESC, filename COLLATE NOCASE"; break;
        case SortMode::Oldest:    sql += " ORDER BY date_created ASC, filename COLLATE NOCASE";  break;
        case SortMode::Bpm:       sql += " ORDER BY bpm ASC, filename COLLATE NOCASE";           break;
        case SortMode::Genre:     sql += " ORDER BY genre COLLATE NOCASE, filename COLLATE NOCASE"; break;
        case SortMode::Favorites: sql += " ORDER BY favorite DESC, filename COLLATE NOCASE";     break;
        case SortMode::Name:
        default:                  sql += " ORDER BY filename COLLATE NOCASE";                    break;
    }

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql.toRawUTF8(), -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    if (catParam > 0)
    {
        const auto cat = categoryKey (category);
        sqlite3_bind_text (stmt, catParam, cat.toRawUTF8(), -1, SQLITE_TRANSIENT);
    }

    if (likeParam > 0)
    {
        const auto like = likePattern (search);
        sqlite3_bind_text (stmt, likeParam, like.toRawUTF8(), -1, SQLITE_TRANSIENT);
    }

    while (sqlite3_step (stmt) == SQLITE_ROW)
        out.push_back (readRow (stmt));

    sqlite3_finalize (stmt);
    return out;
}

std::vector<LibraryItem> LibraryDatabase::runItemQuery (const juce::String& whereAndOrder,
                                                        const juce::StringArray& textParams) const
{
    std::vector<LibraryItem> out;
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return out;

    const juce::String sql = "SELECT id, filename, path, file_type, category, bpm, bars, genre,"
                             " date_created, date_indexed, favorite, generation_type, generation_seed,"
                             " parent_source_ids, pattern_id, engine, generation_depth, exists_on_disk"
                             " FROM library WHERE exists_on_disk=1 " + whereAndOrder;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql.toRawUTF8(), -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    for (int i = 0; i < textParams.size(); ++i)
        sqlite3_bind_text (stmt, i + 1, textParams[i].toRawUTF8(), -1, SQLITE_TRANSIENT);

    while (sqlite3_step (stmt) == SQLITE_ROW)
        out.push_back (readRow (stmt));

    sqlite3_finalize (stmt);
    return out;
}

std::vector<LibraryItem> LibraryDatabase::queryUnderFolder (const juce::String& dirPath,
                                                            const juce::String& search,
                                                            int limit) const
{
    juce::String where;
    juce::StringArray params;

    if (dirPath.isNotEmpty())
    {
        // Prefix match on the folder plus a separator, so "Kicks" never
        // matches "Kicks_Old". substr() keeps this independent of LIKE's
        // wildcard and case rules, which differ from file-system paths.
        auto prefix = dirPath;
        if (! prefix.endsWithChar (juce::File::getSeparatorChar()))
            prefix += juce::File::getSeparatorString();

        params.add (prefix);
        where << " AND substr(path, 1, length(?1)) = ?1";
    }

    if (search.trim().isNotEmpty())
    {
        params.add (likePattern (search));
        where << " AND filename LIKE ?" << params.size() << " ESCAPE '\\'";
    }

    where << " ORDER BY filename COLLATE NOCASE";
    if (limit > 0)
        where << " LIMIT " << limit;

    return runItemQuery (where, params);
}

int LibraryDatabase::countUnderFolder (const juce::String& dirPath) const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr || dirPath.isEmpty())
        return 0;

    auto prefix = dirPath;
    if (! prefix.endsWithChar (juce::File::getSeparatorChar()))
        prefix += juce::File::getSeparatorString();

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "SELECT COUNT(*) FROM library WHERE exists_on_disk=1"
                                " AND substr(path, 1, length(?1)) = ?1;", -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_text (stmt, 1, prefix.toRawUTF8(), -1, SQLITE_TRANSIENT);
    int n = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        n = sqlite3_column_int (stmt, 0);
    sqlite3_finalize (stmt);
    return n;
}

int LibraryDatabase::countFavourites() const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "SELECT COUNT(*) FROM library WHERE exists_on_disk=1 AND favorite=1;",
                            -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int n = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        n = sqlite3_column_int (stmt, 0);
    sqlite3_finalize (stmt);
    return n;
}

std::vector<LibraryItem> LibraryDatabase::queryFavourites (const juce::String& search, int limit) const
{
    juce::String where = " AND favorite=1";
    juce::StringArray params;

    if (search.trim().isNotEmpty())
    {
        params.add (likePattern (search));
        where << " AND filename LIKE ?1 ESCAPE '\\'";
    }

    where << " ORDER BY filename COLLATE NOCASE";
    if (limit > 0)
        where << " LIMIT " << limit;

    return runItemQuery (where, params);
}

std::optional<LibraryItem> LibraryDatabase::findByFilename (const juce::String& filename) const
{
    auto rows = runItemQuery (" AND filename = ?1 LIMIT 1", juce::StringArray { filename });
    if (rows.empty())
        return {};
    return rows.front();
}

bool LibraryDatabase::loadThumb (const juce::String& path, juce::int64 modTime, juce::int64 size,
                                 std::vector<juce::uint8>& peaks, double& seconds) const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "SELECT mod_time, size, seconds, peaks FROM thumbs WHERE path=?1;",
                            -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text (stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    bool ok = false;

    if (sqlite3_step (stmt) == SQLITE_ROW
        && columnI64 (stmt, 0) == modTime && columnI64 (stmt, 1) == size)
    {
        seconds = sqlite3_column_double (stmt, 2);
        const auto* blob = static_cast<const juce::uint8*> (sqlite3_column_blob (stmt, 3));
        const int bytes = sqlite3_column_bytes (stmt, 3);
        if (blob != nullptr && bytes > 0)
        {
            peaks.assign (blob, blob + bytes);
            ok = true;
        }
    }

    sqlite3_finalize (stmt);
    return ok;
}

void LibraryDatabase::storeThumb (const juce::String& path, juce::int64 modTime, juce::int64 size,
                                  const std::vector<juce::uint8>& peaks, double seconds) const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr || peaks.empty())
        return;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "INSERT OR REPLACE INTO thumbs (path, mod_time, size, seconds, peaks)"
                                " VALUES (?1, ?2, ?3, ?4, ?5);", -1, &stmt, nullptr) != SQLITE_OK)
        return;

    sqlite3_bind_text  (stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, (sqlite3_int64) modTime);
    sqlite3_bind_int64 (stmt, 3, (sqlite3_int64) size);
    sqlite3_bind_double (stmt, 4, seconds);
    sqlite3_bind_blob  (stmt, 5, peaks.data(), (int) peaks.size(), SQLITE_TRANSIENT);
    sqlite3_step (stmt);
    sqlite3_finalize (stmt);
}

std::vector<LibraryItem> LibraryDatabase::queryByRole (FolderRole role, const juce::String& search) const
{
    const auto cat = categoryForRole (role);
    return query (cat, search, SortMode::Name, false);
}

std::optional<LibraryItem> LibraryDatabase::findByPath (const juce::String& path) const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return {};

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, filename, path, file_type, category, bpm, bars, genre,"
                      " date_created, date_indexed, favorite, generation_type, generation_seed,"
                      " parent_source_ids, pattern_id, engine, generation_depth, exists_on_disk"
                      " FROM library WHERE path=?1 LIMIT 1;";

    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    sqlite3_bind_text (stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    std::optional<LibraryItem> out;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        out = readRow (stmt);
    sqlite3_finalize (stmt);
    return out;
}

std::optional<LibraryItem> LibraryDatabase::findById (juce::int64 id) const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return {};

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, filename, path, file_type, category, bpm, bars, genre,"
                      " date_created, date_indexed, favorite, generation_type, generation_seed,"
                      " parent_source_ids, pattern_id, engine, generation_depth, exists_on_disk"
                      " FROM library WHERE id=?1 LIMIT 1;";

    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return {};

    sqlite3_bind_int64 (stmt, 1, (sqlite3_int64) id);
    std::optional<LibraryItem> out;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        out = readRow (stmt);
    sqlite3_finalize (stmt);
    return out;
}

int LibraryDatabase::countInCategory (Category c) const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "SELECT COUNT(*) FROM library WHERE exists_on_disk=1 AND category=?1;",
                            -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    const auto cat = categoryKey (c);
    sqlite3_bind_text (stmt, 1, cat.toRawUTF8(), -1, SQLITE_TRANSIENT);
    int n = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        n = sqlite3_column_int (stmt, 0);
    sqlite3_finalize (stmt);
    return n;
}

int LibraryDatabase::nextSequence (const juce::String& filenamePrefix) const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return 1;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "SELECT filename FROM library WHERE filename LIKE ?1;", -1, &stmt, nullptr) != SQLITE_OK)
        return 1;

    const auto like = filenamePrefix + "%";
    sqlite3_bind_text (stmt, 1, like.toRawUTF8(), -1, SQLITE_TRANSIENT);

    int maxN = 0;
    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        auto name = columnText (stmt, 0);
        name = name.upToLastOccurrenceOf (".", false, false);
        // Strip optional _FULL / _KICK / stem suffixes after the index.
        const auto after = name.fromFirstOccurrenceOf (filenamePrefix, false, false);
        int digits = 0;
        for (int i = 0; i < after.length(); ++i)
        {
            auto c = after[i];
            if (c >= '0' && c <= '9')
                digits = digits * 10 + (int) (c - '0');
            else
                break;
        }
        maxN = juce::jmax (maxN, digits);
    }

    sqlite3_finalize (stmt);
    return maxN + 1;
}

std::vector<juce::String> LibraryDatabase::allIndexedPaths() const
{
    std::vector<juce::String> out;
    const juce::ScopedLock sl (lock);
    if (db == nullptr)
        return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "SELECT path FROM library;", -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    while (sqlite3_step (stmt) == SQLITE_ROW)
        out.push_back (columnText (stmt, 0));

    sqlite3_finalize (stmt);
    return out;
}

} // namespace foundry
