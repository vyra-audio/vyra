#include "vyra/library/TrackLibrary.h"

#include "vyra/audio/BpmAnalysis.h"
#include "vyra/audio/KeyAnalysis.h"
#include "vyra/audio/Waveform.h"

#include <juce_events/juce_events.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace vyra::library
{
namespace
{
class Statement final
{
public:
    Statement(sqlite3* database, const char* sql)
    {
        result = sqlite3_prepare_v2(database, sql, -1, &statement, nullptr);
    }

    ~Statement()
    {
        sqlite3_finalize(statement);
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    [[nodiscard]] bool valid() const noexcept
    {
        return result == SQLITE_OK && statement != nullptr;
    }

    [[nodiscard]] sqlite3_stmt* get() const noexcept
    {
        return statement;
    }

private:
    sqlite3_stmt* statement = nullptr;
    int result = SQLITE_ERROR;
};

void bindText(sqlite3_stmt* statement, int index, const juce::String& value)
{
    const auto text = value.toRawUTF8();
    sqlite3_bind_text(statement, index, text, -1, SQLITE_TRANSIENT);
}

juce::String columnText(sqlite3_stmt* statement, int index)
{
    const auto* value = sqlite3_column_text(statement, index);
    return value == nullptr ? juce::String {} : juce::String::fromUTF8(
        reinterpret_cast<const char*>(value));
}

juce::String metadataValue(const juce::StringPairArray& values,
                           const juce::StringArray& keys)
{
    for (const auto& key : keys)
    {
        const auto value = values.getValue(key, {}).trim();
        if (value.isNotEmpty())
            return value;
    }

    return {};
}

int parseYear(const juce::String& value)
{
    return value.substring(0, 4).getIntValue();
}

double parseBpm(const juce::String& value)
{
    const auto parsed = value.getDoubleValue();
    return std::isfinite(parsed) && parsed >= 30.0 && parsed <= 300.0
        ? parsed : 0.0;
}
}

TrackLibrary::TrackLibrary(const juce::File& databaseFile)
{
    formats.registerBasicFormats();

    const auto parent = databaseFile.getParentDirectory();
    if (!parent.createDirectory())
        throw std::runtime_error("Could not create the Vyra library directory.");

    const auto path = databaseFile.getFullPathName().toRawUTF8();
    const auto openResult = sqlite3_open_v2(path, &database,
                                            SQLITE_OPEN_READWRITE
                                                | SQLITE_OPEN_CREATE
                                                | SQLITE_OPEN_FULLMUTEX,
                                            nullptr);
    if (openResult != SQLITE_OK)
    {
        const auto message = database != nullptr
            ? juce::String::fromUTF8(sqlite3_errmsg(database))
            : juce::String("SQLite could not open the library database.");
        if (database != nullptr)
            sqlite3_close(database);
        database = nullptr;
        throw std::runtime_error(message.toStdString());
    }

    sqlite3_busy_timeout(database, 5000);
    if (!execute("PRAGMA journal_mode=WAL;")
        || !execute("PRAGMA synchronous=NORMAL;")
        || !execute("PRAGMA foreign_keys=ON;")
        || !execute(
            "CREATE TABLE IF NOT EXISTS tracks ("
            "id INTEGER PRIMARY KEY,"
            "path TEXT NOT NULL UNIQUE,"
            "title TEXT NOT NULL,"
            "artist TEXT NOT NULL DEFAULT '',"
            "album TEXT NOT NULL DEFAULT '',"
            "genre TEXT NOT NULL DEFAULT '',"
            "musical_key TEXT NOT NULL DEFAULT '',"
            "year INTEGER NOT NULL DEFAULT 0,"
            "sample_rate INTEGER NOT NULL DEFAULT 0,"
            "bit_depth INTEGER NOT NULL DEFAULT 0,"
            "duration_seconds REAL NOT NULL DEFAULT 0,"
            "bpm REAL NOT NULL DEFAULT 0,"
            "waveform BLOB NOT NULL DEFAULT X''"
            ");")
        || !execute("CREATE INDEX IF NOT EXISTS idx_tracks_title ON tracks(title COLLATE NOCASE);")
        || !execute("CREATE INDEX IF NOT EXISTS idx_tracks_artist ON tracks(artist COLLATE NOCASE);")
        || !execute(
            "CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY,"
            "name TEXT NOT NULL UNIQUE COLLATE NOCASE"
            ");")
        || !execute(
            "CREATE TABLE IF NOT EXISTS playlist_tracks ("
            "playlist_id INTEGER NOT NULL REFERENCES playlists(id) ON DELETE CASCADE,"
            "track_id INTEGER NOT NULL REFERENCES tracks(id) ON DELETE CASCADE,"
            "position INTEGER NOT NULL DEFAULT 0,"
            "PRIMARY KEY(playlist_id,track_id)"
            ");")
        || !execute(
            "CREATE TABLE IF NOT EXISTS play_history ("
            "id INTEGER PRIMARY KEY,"
            "track_id INTEGER REFERENCES tracks(id) ON DELETE SET NULL,"
            "path TEXT NOT NULL,"
            "title TEXT NOT NULL,"
            "deck_index INTEGER NOT NULL CHECK(deck_index IN (0,1)),"
            "played_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now'))"
            ");")
        || !execute(
            "CREATE INDEX IF NOT EXISTS idx_play_history_played_at "
            "ON play_history(played_at DESC,id DESC);")
        || !execute(
            "CREATE TABLE IF NOT EXISTS track_performance ("
            "path TEXT PRIMARY KEY NOT NULL,"
            "hot_cue_1 REAL NOT NULL DEFAULT -1,"
            "hot_cue_2 REAL NOT NULL DEFAULT -1,"
            "hot_cue_3 REAL NOT NULL DEFAULT -1,"
            "loop_beats INTEGER NOT NULL DEFAULT 4 "
            "CHECK(loop_beats IN (1,2,4,8))"
            ");"))
    {
        const auto message = getLastError();
        sqlite3_close(database);
        database = nullptr;
        throw std::runtime_error(message.toStdString());
    }

    Statement columns(database, "PRAGMA table_info(tracks);");
    bool hasWaveform = false;
    while (columns.valid() && sqlite3_step(columns.get()) == SQLITE_ROW)
        hasWaveform = hasWaveform || columnText(columns.get(), 1) == "waveform";
    if (!hasWaveform && !execute(
            "ALTER TABLE tracks ADD COLUMN waveform BLOB NOT NULL DEFAULT X'';"))
    {
        const auto message = getLastError();
        sqlite3_close(database);
        database = nullptr;
        throw std::runtime_error(message.toStdString());
    }
}

TrackLibrary::~TrackLibrary()
{
    cancelScan();
    scanPool.removeAllJobs(true, -1);

    std::lock_guard lock(databaseMutex);
    if (database != nullptr)
    {
        sqlite3_close(database);
        database = nullptr;
    }
}

juce::String TrackLibrary::getLastError() const
{
    std::lock_guard lock(errorMutex);
    return lastError;
}

std::vector<Track> TrackLibrary::search(const juce::String& query, int limit) const
{
    return searchInternal({}, query, limit);
}

std::vector<Track> TrackLibrary::searchPlaylist(const juce::String& playlist,
                                                const juce::String& query,
                                                int limit) const
{
    if (playlist.trim().isEmpty())
        return search(query, limit);
    return searchInternal(playlist.trim(), query, limit);
}

std::vector<juce::String> TrackLibrary::getPlaylists() const
{
    std::vector<juce::String> playlists;
    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
        return playlists;

    Statement statement(database, "SELECT name FROM playlists ORDER BY name COLLATE NOCASE;");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return playlists;
    }

    for (;;)
    {
        const auto result = sqlite3_step(statement.get());
        if (result == SQLITE_DONE)
            break;
        if (result != SQLITE_ROW)
        {
            setLastError(sqlite3_errmsg(database));
            playlists.clear();
            return playlists;
        }
        playlists.push_back(columnText(statement.get(), 0));
    }

    setLastError({});
    return playlists;
}

std::vector<Track> TrackLibrary::searchRecentPlays(const juce::String& query,
                                                   int limit) const
{
    std::vector<Track> results;
    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
        return results;

    Statement statement(database,
        "SELECT COALESCE(t.id,0),h.path,COALESCE(NULLIF(t.title,''),h.title),"
        "COALESCE(t.artist,''),COALESCE(t.album,''),COALESCE(t.genre,''),"
        "COALESCE(t.musical_key,''),COALESCE(t.year,0),COALESCE(t.sample_rate,0),"
        "COALESCE(t.bit_depth,0),COALESCE(t.duration_seconds,0),COALESCE(t.bpm,0),"
        "COALESCE(t.waveform,X''),h.played_at,h.deck_index "
        "FROM play_history h LEFT JOIN tracks t ON t.id=h.track_id "
        "WHERE ?1='' OR instr(lower(COALESCE(t.title,h.title)),lower(?1))>0 "
        "OR instr(lower(COALESCE(t.artist,'')),lower(?1))>0 "
        "OR instr(lower(COALESCE(t.album,'')),lower(?1))>0 "
        "OR instr(lower(COALESCE(t.genre,'')),lower(?1))>0 "
        "OR instr(lower(h.path),lower(?1))>0 "
        "ORDER BY h.played_at DESC,h.id DESC LIMIT ?2;");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return results;
    }

    bindText(statement.get(), 1, query.trim());
    sqlite3_bind_int(statement.get(), 2, std::clamp(limit, 1, 10000));
    for (;;)
    {
        const auto stepResult = sqlite3_step(statement.get());
        if (stepResult == SQLITE_DONE)
            break;
        if (stepResult != SQLITE_ROW)
        {
            setLastError(sqlite3_errmsg(database));
            results.clear();
            return results;
        }

        Track track;
        track.id = sqlite3_column_int64(statement.get(), 0);
        track.path = columnText(statement.get(), 1);
        track.title = columnText(statement.get(), 2);
        track.artist = columnText(statement.get(), 3);
        track.album = columnText(statement.get(), 4);
        track.genre = columnText(statement.get(), 5);
        track.musicalKey = columnText(statement.get(), 6);
        track.year = sqlite3_column_int(statement.get(), 7);
        track.sampleRate = sqlite3_column_int(statement.get(), 8);
        track.bitDepth = sqlite3_column_int(statement.get(), 9);
        track.durationSeconds = sqlite3_column_double(statement.get(), 10);
        track.bpm = sqlite3_column_double(statement.get(), 11);
        const auto* waveformData = static_cast<const std::uint8_t*>(
            sqlite3_column_blob(statement.get(), 12));
        const auto waveformSize = sqlite3_column_bytes(statement.get(), 12);
        if (waveformData != nullptr && waveformSize > 0)
            track.waveform.assign(waveformData, waveformData + waveformSize);
        track.lastPlayedAt = columnText(statement.get(), 13);
        track.lastPlayedDeck = sqlite3_column_int(statement.get(), 14);
        results.push_back(std::move(track));
    }

    setLastError({});
    return results;
}

bool TrackLibrary::recordPlay(const juce::String& path,
                              const juce::String& title,
                              int deckIndex)
{
    if (path.trim().isEmpty() || title.trim().isEmpty()
        || path.length() > 4096 || title.length() > 4096
        || deckIndex < 0 || deckIndex > 1)
    {
        setLastError("Playback history requires a valid path, title, and deck.");
        return false;
    }

    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
    {
        setLastError("The library database is not open.");
        return false;
    }

    Statement statement(database,
        "INSERT INTO play_history(track_id,path,title,deck_index) "
        "VALUES((SELECT id FROM tracks WHERE path=?1),?1,?2,?3);");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }
    bindText(statement.get(), 1, path);
    bindText(statement.get(), 2, title);
    sqlite3_bind_int(statement.get(), 3, deckIndex);
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }

    setLastError({});
    return true;
}

bool TrackLibrary::loadPerformanceState(const juce::String& path,
                                        TrackPerformanceState& state) const
{
    state = {};
    if (path.trim().isEmpty())
    {
        setLastError("A track path is required to load performance state.");
        return false;
    }

    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
    {
        setLastError("The library database is not open.");
        return false;
    }

    Statement statement(database,
        "SELECT hot_cue_1,hot_cue_2,hot_cue_3,loop_beats "
        "FROM track_performance WHERE path=?1;");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }
    bindText(statement.get(), 1, path);
    const auto result = sqlite3_step(statement.get());
    if (result == SQLITE_DONE)
    {
        setLastError({});
        return true;
    }
    if (result != SQLITE_ROW)
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }

    for (int cue = 0; cue < 3; ++cue)
    {
        const auto position = sqlite3_column_double(statement.get(), cue);
        state.hotCueSeconds[static_cast<std::size_t>(cue)] =
            std::isfinite(position) && position >= 0.0 ? position : -1.0;
    }
    const auto loopBeats = sqlite3_column_int(statement.get(), 3);
    state.loopBeats = loopBeats == 1 || loopBeats == 2 || loopBeats == 4 || loopBeats == 8
        ? loopBeats : 4;
    setLastError({});
    return true;
}

bool TrackLibrary::savePerformanceState(const juce::String& path,
                                       const TrackPerformanceState& state)
{
    if (path.trim().isEmpty()
        || std::any_of(state.hotCueSeconds.begin(), state.hotCueSeconds.end(),
                       [](double position)
                       {
                           return !std::isfinite(position) || position < -1.0;
                       })
        || (state.loopBeats != 1 && state.loopBeats != 2
            && state.loopBeats != 4 && state.loopBeats != 8))
    {
        setLastError("Track performance state contains an invalid path, cue, or loop.");
        return false;
    }

    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
    {
        setLastError("The library database is not open.");
        return false;
    }

    Statement statement(database,
        "INSERT INTO track_performance(path,hot_cue_1,hot_cue_2,hot_cue_3,loop_beats) "
        "VALUES(?1,?2,?3,?4,?5) ON CONFLICT(path) DO UPDATE SET "
        "hot_cue_1=excluded.hot_cue_1,hot_cue_2=excluded.hot_cue_2,"
        "hot_cue_3=excluded.hot_cue_3,loop_beats=excluded.loop_beats;");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }
    bindText(statement.get(), 1, path);
    for (int cue = 0; cue < 3; ++cue)
        sqlite3_bind_double(statement.get(), cue + 2,
                            state.hotCueSeconds[static_cast<std::size_t>(cue)]);
    sqlite3_bind_int(statement.get(), 5, state.loopBeats);
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }

    setLastError({});
    return true;
}

bool TrackLibrary::createPlaylist(const juce::String& name)
{
    const auto normalizedName = name.trim();
    if (normalizedName.isEmpty() || normalizedName.length() > 100)
    {
        setLastError("Playlist names must contain between 1 and 100 characters.");
        return false;
    }

    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
    {
        setLastError("The library database is not open.");
        return false;
    }

    Statement statement(database, "INSERT INTO playlists(name) VALUES(?1);");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }
    bindText(statement.get(), 1, normalizedName);
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }

    setLastError({});
    return true;
}

bool TrackLibrary::addTrackToPlaylist(const juce::String& playlist,
                                     std::int64_t trackId)
{
    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
    {
        setLastError("The library database is not open.");
        return false;
    }

    Statement statement(database,
        "INSERT OR IGNORE INTO playlist_tracks(playlist_id,track_id,position) "
        "SELECT id,?2,COALESCE((SELECT MAX(position)+1 FROM playlist_tracks "
        "WHERE playlist_id=playlists.id),0) FROM playlists WHERE name=?1;");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }
    bindText(statement.get(), 1, playlist.trim());
    sqlite3_bind_int64(statement.get(), 2, trackId);
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }

    if (sqlite3_changes(database) == 0)
    {
        Statement existing(database,
            "SELECT 1 FROM playlists p JOIN playlist_tracks pt ON pt.playlist_id=p.id "
            "JOIN tracks t ON t.id=pt.track_id WHERE p.name=?1 AND t.id=?2;");
        if (existing.valid())
        {
            bindText(existing.get(), 1, playlist.trim());
            sqlite3_bind_int64(existing.get(), 2, trackId);
            if (sqlite3_step(existing.get()) == SQLITE_ROW)
            {
                setLastError({});
                return true;
            }
        }
        setLastError("The playlist or track could not be found.");
        return false;
    }

    setLastError({});
    return true;
}

bool TrackLibrary::removeTrackFromPlaylist(const juce::String& playlist,
                                          std::int64_t trackId)
{
    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
    {
        setLastError("The library database is not open.");
        return false;
    }

    Statement statement(database,
        "DELETE FROM playlist_tracks WHERE track_id=?2 AND playlist_id="
        "(SELECT id FROM playlists WHERE name=?1);");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }
    bindText(statement.get(), 1, playlist.trim());
    sqlite3_bind_int64(statement.get(), 2, trackId);
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }
    if (sqlite3_changes(database) == 0)
    {
        setLastError("The track is not in this playlist.");
        return false;
    }

    setLastError({});
    return true;
}

std::vector<Track> TrackLibrary::searchInternal(const juce::String& playlist,
                                                const juce::String& query,
                                                int limit) const
{
    std::vector<Track> results;
    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
        return results;

    const auto isPlaylistSearch = playlist.isNotEmpty();
    Statement statement(database, isPlaylistSearch
        ? "SELECT t.id,t.path,t.title,t.artist,t.album,t.genre,t.musical_key,t.year,"
          "t.sample_rate,t.bit_depth,t.duration_seconds,t.bpm,t.waveform "
          "FROM tracks t JOIN playlist_tracks pt ON pt.track_id=t.id "
          "JOIN playlists p ON p.id=pt.playlist_id "
          "WHERE p.name=?1 COLLATE NOCASE AND (?2='' "
          "OR instr(lower(t.title),lower(?2))>0 "
          "OR instr(lower(t.artist),lower(?2))>0 "
          "OR instr(lower(t.album),lower(?2))>0 "
          "OR instr(lower(t.genre),lower(?2))>0 "
          "OR instr(lower(t.path),lower(?2))>0) "
          "ORDER BY pt.position,t.title COLLATE NOCASE LIMIT ?3;"
        : "SELECT id,path,title,artist,album,genre,musical_key,year,sample_rate,"
          "bit_depth,duration_seconds,bpm,waveform FROM tracks "
          "WHERE ?1 = '' OR instr(lower(title),lower(?1)) > 0 "
          "OR instr(lower(artist),lower(?1)) > 0 "
          "OR instr(lower(album),lower(?1)) > 0 "
          "OR instr(lower(genre),lower(?1)) > 0 "
          "OR instr(lower(path),lower(?1)) > 0 "
          "ORDER BY title COLLATE NOCASE, artist COLLATE NOCASE LIMIT ?2;");

    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return results;
    }

    if (isPlaylistSearch)
    {
        bindText(statement.get(), 1, playlist);
        bindText(statement.get(), 2, query.trim());
        sqlite3_bind_int(statement.get(), 3, std::clamp(limit, 1, 10000));
    }
    else
    {
        bindText(statement.get(), 1, query.trim());
        sqlite3_bind_int(statement.get(), 2, std::clamp(limit, 1, 10000));
    }

    for (;;)
    {
        const auto stepResult = sqlite3_step(statement.get());
        if (stepResult == SQLITE_DONE)
            break;
        if (stepResult != SQLITE_ROW)
        {
            setLastError(sqlite3_errmsg(database));
            results.clear();
            return results;
        }

        Track track;
        track.id = sqlite3_column_int64(statement.get(), 0);
        track.path = columnText(statement.get(), 1);
        track.title = columnText(statement.get(), 2);
        track.artist = columnText(statement.get(), 3);
        track.album = columnText(statement.get(), 4);
        track.genre = columnText(statement.get(), 5);
        track.musicalKey = columnText(statement.get(), 6);
        track.year = sqlite3_column_int(statement.get(), 7);
        track.sampleRate = sqlite3_column_int(statement.get(), 8);
        track.bitDepth = sqlite3_column_int(statement.get(), 9);
        track.durationSeconds = sqlite3_column_double(statement.get(), 10);
        track.bpm = sqlite3_column_double(statement.get(), 11);
        const auto* waveformData = static_cast<const std::uint8_t*>(
            sqlite3_column_blob(statement.get(), 12));
        const auto waveformSize = sqlite3_column_bytes(statement.get(), 12);
        if (waveformData != nullptr && waveformSize > 0)
            track.waveform.assign(waveformData, waveformData + waveformSize);
        results.push_back(std::move(track));
    }

    setLastError({});
    return results;
}

bool TrackLibrary::saveTrack(const Track& track)
{
    if (track.path.isEmpty() || track.title.isEmpty()
        || !std::isfinite(track.durationSeconds) || track.durationSeconds < 0.0
        || !std::isfinite(track.bpm) || track.bpm < 0.0 || track.bpm > 300.0
        || track.waveform.size() > 4096)
    {
        setLastError("Track metadata contains an invalid path, title, duration, or BPM.");
        return false;
    }

    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
    {
        setLastError("The library database is not open.");
        return false;
    }

    Statement statement(database,
        "INSERT INTO tracks(path,title,artist,album,genre,musical_key,year,"
        "sample_rate,bit_depth,duration_seconds,bpm,waveform) "
        "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12) "
        "ON CONFLICT(path) DO UPDATE SET title=excluded.title,artist=excluded.artist,"
        "album=excluded.album,genre=excluded.genre,musical_key=excluded.musical_key,"
        "year=excluded.year,sample_rate=excluded.sample_rate,bit_depth=excluded.bit_depth,"
        "duration_seconds=excluded.duration_seconds,bpm=excluded.bpm,"
        "waveform=CASE WHEN length(excluded.waveform)>0 "
        "THEN excluded.waveform ELSE tracks.waveform END;");
    if (!statement.valid())
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }

    bindText(statement.get(), 1, track.path);
    bindText(statement.get(), 2, track.title);
    bindText(statement.get(), 3, track.artist);
    bindText(statement.get(), 4, track.album);
    bindText(statement.get(), 5, track.genre);
    bindText(statement.get(), 6, track.musicalKey);
    sqlite3_bind_int(statement.get(), 7, std::max(0, track.year));
    sqlite3_bind_int(statement.get(), 8, std::max(0, track.sampleRate));
    sqlite3_bind_int(statement.get(), 9, std::max(0, track.bitDepth));
    sqlite3_bind_double(statement.get(), 10, track.durationSeconds);
    sqlite3_bind_double(statement.get(), 11, track.bpm);
    if (track.waveform.empty())
        sqlite3_bind_zeroblob(statement.get(), 12, 0);
    else
        sqlite3_bind_blob(statement.get(), 12, track.waveform.data(),
                          static_cast<int>(track.waveform.size()), SQLITE_TRANSIENT);

    if (sqlite3_step(statement.get()) != SQLITE_DONE)
    {
        setLastError(sqlite3_errmsg(database));
        return false;
    }

    setLastError({});
    return true;
}

bool TrackLibrary::indexFile(const juce::File& file)
{
    return indexFileOnWorker(file);
}

bool TrackLibrary::scanDirectoryAsync(const juce::File& directory,
                                      ScanCallbacks callbacks)
{
    if (!directory.isDirectory())
    {
        setLastError("Select an existing folder to scan.");
        if (callbacks.finished)
            callbacks.finished(getLastError());
        return false;
    }

    bool expected = false;
    if (!scanActive.compare_exchange_strong(expected, true))
    {
        setLastError("A library scan is already running.");
        if (callbacks.finished)
            callbacks.finished(getLastError());
        return false;
    }

    scanCancelled.store(false, std::memory_order_relaxed);
    scanPool.addJob([this, directory, callbacks = std::move(callbacks)]() mutable
    {
        int indexed = 0;
        int failures = 0;
        juce::String scanError;
        const auto wildcard = formats.getWildcardForAllFormats();

        for (const auto& entry : juce::RangedDirectoryIterator(
                 directory, true, wildcard, juce::File::findFiles,
                 juce::File::FollowSymlinks::no))
        {
            if (scanCancelled.load(std::memory_order_relaxed))
                break;

            if (indexFileOnWorker(entry.getFile()))
                ++indexed;
            else
                ++failures;

            if (callbacks.progress && (indexed + failures) % 25 == 0)
            {
                juce::MessageManager::callAsync(
                    [progress = callbacks.progress, indexed, failures]() mutable
                    {
                        progress(indexed, failures);
                    });
            }
        }

        if (scanCancelled.load(std::memory_order_relaxed))
            scanError = "Library scan cancelled.";
        else if (failures > 0)
            scanError = juce::String(failures) + " file(s) could not be indexed.";

        scanActive.store(false, std::memory_order_relaxed);
        if (callbacks.finished)
        {
            juce::MessageManager::callAsync(
                [finished = std::move(callbacks.finished), scanError = std::move(scanError)]() mutable
                {
                    finished(std::move(scanError));
                });
        }
    });

    return true;
}

void TrackLibrary::cancelScan() noexcept
{
    scanCancelled.store(true, std::memory_order_relaxed);
}

bool TrackLibrary::isScanning() const noexcept
{
    return scanActive.load(std::memory_order_relaxed);
}

bool TrackLibrary::execute(const char* sql) const
{
    std::lock_guard lock(databaseMutex);
    if (database == nullptr)
        return false;

    char* errorMessage = nullptr;
    const auto result = sqlite3_exec(database, sql, nullptr, nullptr, &errorMessage);
    if (result != SQLITE_OK)
    {
        setLastError(errorMessage != nullptr ? juce::String::fromUTF8(errorMessage)
                                             : juce::String::fromUTF8(sqlite3_errmsg(database)));
        sqlite3_free(errorMessage);
        return false;
    }

    sqlite3_free(errorMessage);
    setLastError({});
    return true;
}

bool TrackLibrary::indexFileOnWorker(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
    {
        setLastError("Could not decode audio file: " + file.getFileName());
        return false;
    }

    const auto& metadata = reader->metadataValues;
    Track track;
    track.path = file.getFullPathName();
    track.title = metadataValue(metadata, { "title", "TIT2", "INAM" });
    if (track.title.isEmpty())
        track.title = file.getFileNameWithoutExtension();
    track.artist = metadataValue(metadata, { "artist", "TPE1", "IART" });
    track.album = metadataValue(metadata, { "album", "TALB", "IPRD" });
    track.genre = metadataValue(metadata, { "genre", "TCON", "IGNR" });
    track.musicalKey = metadataValue(metadata, { "key", "initialkey", "TKEY" });
    if (track.musicalKey.isEmpty())
        track.musicalKey = audio::analyzeMusicalKey(*reader);
    track.year = parseYear(metadataValue(metadata, { "date", "year", "TYER", "ICRD" }));
    track.bpm = parseBpm(metadataValue(metadata, { "bpm", "TBPM", "IBPM" }));
    if (track.bpm <= 0.0)
        track.bpm = audio::analyzeBpm(*reader);
    track.sampleRate = static_cast<int>(std::clamp(reader->sampleRate, 0.0,
                                                   static_cast<double>(std::numeric_limits<int>::max())));
    track.bitDepth = reader->bitsPerSample > 0
        ? static_cast<int>(reader->bitsPerSample) : 0;
    track.durationSeconds = reader->sampleRate > 0.0
        ? static_cast<double>(reader->lengthInSamples) / reader->sampleRate : 0.0;
    track.waveform = audio::analyzeWaveform(*reader);

    return saveTrack(track);
}

void TrackLibrary::setLastError(juce::String error) const
{
    std::lock_guard lock(errorMutex);
    lastError = std::move(error);
}
}
