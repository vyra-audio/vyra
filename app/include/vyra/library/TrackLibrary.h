#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

struct sqlite3;

namespace vyra::library
{
struct Track
{
    std::int64_t id = 0;
    juce::String path;
    juce::String title;
    juce::String artist;
    juce::String album;
    juce::String genre;
    juce::String musicalKey;
    int year = 0;
    int sampleRate = 0;
    int bitDepth = 0;
    double durationSeconds = 0.0;
    double bpm = 0.0;
    juce::String lastPlayedAt;
    int lastPlayedDeck = 0;
    std::vector<std::uint8_t> waveform;
};

struct TrackPerformanceState
{
    std::array<double, 3> hotCueSeconds { -1.0, -1.0, -1.0 };
    int loopBeats = 4;
};

class TrackLibrary final
{
public:
    struct ScanCallbacks
    {
        std::function<void(int indexed, int failures)> progress;
        std::function<void(juce::String error)> finished;
    };

    explicit TrackLibrary(const juce::File& databaseFile);
    ~TrackLibrary();

    TrackLibrary(const TrackLibrary&) = delete;
    TrackLibrary& operator=(const TrackLibrary&) = delete;

    [[nodiscard]] juce::String getLastError() const;
    [[nodiscard]] std::vector<Track> search(const juce::String& query,
                                            int limit = 500) const;
    [[nodiscard]] std::vector<Track> searchPlaylist(const juce::String& playlist,
                                                    const juce::String& query,
                                                    int limit = 500) const;
    [[nodiscard]] std::vector<Track> searchRecentPlays(const juce::String& query,
                                                       int limit = 500) const;
    [[nodiscard]] bool recordPlay(const juce::String& path,
                                  const juce::String& title,
                                  int deckIndex);
    [[nodiscard]] bool loadPerformanceState(const juce::String& path,
                                            TrackPerformanceState& state) const;
    [[nodiscard]] bool savePerformanceState(const juce::String& path,
                                            const TrackPerformanceState& state);
    [[nodiscard]] std::vector<juce::String> getPlaylists() const;
    [[nodiscard]] bool createPlaylist(const juce::String& name);
    [[nodiscard]] bool addTrackToPlaylist(const juce::String& playlist,
                                          std::int64_t trackId);
    [[nodiscard]] bool removeTrackFromPlaylist(const juce::String& playlist,
                                               std::int64_t trackId);
    [[nodiscard]] bool saveTrack(const Track& track);
    [[nodiscard]] bool indexFile(const juce::File& file);
    [[nodiscard]] bool scanDirectoryAsync(const juce::File& directory,
                                          ScanCallbacks callbacks);
    void cancelScan() noexcept;
    [[nodiscard]] bool isScanning() const noexcept;

private:
    [[nodiscard]] std::vector<Track> searchInternal(const juce::String& playlist,
                                                    const juce::String& query,
                                                    int limit) const;
    [[nodiscard]] bool execute(const char* sql) const;
    [[nodiscard]] bool indexFileOnWorker(const juce::File& file);
    void setLastError(juce::String error) const;

    sqlite3* database = nullptr;
    mutable std::mutex databaseMutex;
    mutable std::mutex errorMutex;
    mutable juce::String lastError;
    juce::AudioFormatManager formats;
    juce::ThreadPool scanPool { 1 };
    std::atomic<bool> scanActive { false };
    std::atomic<bool> scanCancelled { false };
};
}
