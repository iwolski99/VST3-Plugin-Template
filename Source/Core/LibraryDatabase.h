#pragma once

#include "Types.h"

struct sqlite3;
struct sqlite3_stmt;

namespace foundry
{

class LibraryDatabase
{
public:
    LibraryDatabase();
    ~LibraryDatabase();

    LibraryDatabase (const LibraryDatabase&) = delete;
    LibraryDatabase& operator= (const LibraryDatabase&) = delete;

    bool open (const juce::File& file, juce::String& error);
    void close();
    bool isOpen() const noexcept { return db != nullptr; }
    juce::File file() const { return dbFile; }

    bool rebuild (juce::String& error);

    bool upsert (const LibraryItem& item, juce::String& error);
    bool removeByPath (const juce::String& path, juce::String& error);
    bool removeById (juce::int64 id, juce::String& error);
    bool setFavorite (juce::int64 id, bool favorite, juce::String& error);
    bool markMissing (const juce::String& path);

    std::vector<LibraryItem> query (Category category,
                                    const juce::String& search,
                                    SortMode sort,
                                    bool favoritesOnly) const;

    std::vector<LibraryItem> queryByRole (FolderRole role,
                                          const juce::String& search) const;

    /** Every indexed file at or below `dirPath` (any depth), optionally
        filtered by a filename search. An empty dirPath means the whole
        library. Ordered by filename; `limit` <= 0 means no limit. */
    std::vector<LibraryItem> queryUnderFolder (const juce::String& dirPath,
                                               const juce::String& search,
                                               int limit = 0) const;

    int countUnderFolder (const juce::String& dirPath) const;
    int countFavourites() const;

    std::vector<LibraryItem> queryFavourites (const juce::String& search, int limit = 0) const;

    /** First indexed file with this exact filename - used to relink a riff
        whose sample was moved to a different folder. */
    std::optional<LibraryItem> findByFilename (const juce::String& filename) const;

    /** Cached waveform thumbnail for a file, valid only while the file's size
        and modification time are unchanged. */
    bool loadThumb (const juce::String& path, juce::int64 modTime, juce::int64 size,
                    std::vector<juce::uint8>& peaks, double& seconds) const;
    void storeThumb (const juce::String& path, juce::int64 modTime, juce::int64 size,
                     const std::vector<juce::uint8>& peaks, double seconds) const;

    std::optional<LibraryItem> findByPath (const juce::String& path) const;
    std::optional<LibraryItem> findById (juce::int64 id) const;

    int countInCategory (Category c) const;
    int nextSequence (const juce::String& filenamePrefix) const;

    std::vector<juce::String> allIndexedPaths() const;

private:
    bool exec (const char* sql, juce::String& error) const;
    bool createSchema (juce::String& error);
    LibraryItem readRow (sqlite3_stmt* stmt) const;
    static juce::String likePattern (const juce::String& raw);
    std::vector<LibraryItem> runItemQuery (const juce::String& whereAndOrder,
                                           const juce::StringArray& textParams) const;

    sqlite3* db = nullptr;
    juce::File dbFile;
    mutable juce::CriticalSection lock;
};

} // namespace foundry
