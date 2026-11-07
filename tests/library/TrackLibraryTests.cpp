#include "vyra/library/TrackLibrary.h"

#include <sqlite3.h>

#include <cstdlib>
#include <iostream>
#include <numbers>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main()
{
    const auto databaseFile = juce::File::getSpecialLocation(
        juce::File::tempDirectory).getChildFile("vyra-library-test.sqlite3");
    databaseFile.deleteFile();

    {
        vyra::library::TrackLibrary library(databaseFile);
        vyra::library::Track track;
        track.path = "C:/music/daft punk/one more time.wav";
        track.title = "One More Time";
        track.artist = "Daft Punk";
        track.album = "Discovery";
        track.genre = "Electronic";
        track.musicalKey = "F#m";
        track.year = 2001;
        track.sampleRate = 44100;
        track.bitDepth = 16;
        track.durationSeconds = 320.0;
        track.bpm = 123.0;
        track.waveform = { 0, 64, 128, 255 };

        require(library.saveTrack(track), "inserting track should succeed");
        require(library.search("").size() == 1, "empty query should return all tracks");
        require(library.search("daft").size() == 1, "search should be case-insensitive");
        require(library.search("discovery").front().musicalKey == "F#m",
                "track metadata should round-trip");
        require(library.search("discovery").front().waveform == track.waveform,
                "waveform data should round-trip");
        require(library.search("missing").empty(), "non-matching query should return no tracks");
        require(library.recordPlay(track.path, "Daft Punk — One More Time", 0),
                "deck one playback should be recorded");
        require(library.recordPlay(track.path, "Daft Punk — One More Time", 1),
                "deck two playback should be recorded");
        const auto recentPlays = library.searchRecentPlays("daft");
        require(recentPlays.size() == 2, "recent-play search should retain each play event");
        require(recentPlays.front().title == "One More Time"
                    && recentPlays.front().lastPlayedDeck == 1
                    && recentPlays.front().lastPlayedAt.isNotEmpty(),
                "history should include track metadata, deck, and timestamp");
        require(library.searchRecentPlays("not in history").empty(),
                "recent-play search should filter unmatched entries");
        require(library.recordPlay("C:/music/unindexed.wav", "Unindexed track", 0),
                "directly loaded unindexed tracks should be recordable");
        const auto unindexedHistory = library.searchRecentPlays("unindexed");
        require(unindexedHistory.size() == 1 && unindexedHistory.front().id == 0
                    && unindexedHistory.front().title == "Unindexed track",
                "history should retain tracks that are not in the library");
        require(!library.recordPlay({}, "Invalid", 0)
                    && !library.recordPlay("C:/music/invalid.wav", "Invalid", 2),
                "invalid history entries should fail");
        vyra::library::TrackPerformanceState performanceState;
        require(library.loadPerformanceState(track.path, performanceState)
                    && performanceState.loopBeats == 4
                    && performanceState.hotCueSeconds[0] == -1.0,
                "tracks without saved performance state should get safe defaults");
        performanceState.hotCueSeconds = { 12.5, -1.0, 98.75 };
        performanceState.loopBeats = 8;
        require(library.savePerformanceState(track.path, performanceState),
                "track performance state should save");
        vyra::library::TrackPerformanceState invalidPerformanceState;
        invalidPerformanceState.hotCueSeconds[1] = -2.0;
        require(!library.savePerformanceState(track.path, invalidPerformanceState),
                "invalid saved cue positions should fail");
        invalidPerformanceState = {};
        invalidPerformanceState.loopBeats = 3;
        require(!library.savePerformanceState(track.path, invalidPerformanceState),
                "unsupported saved loop lengths should fail");
        require(library.createPlaylist("Warm-up"), "playlist should be created");
        require(!library.createPlaylist("warm-up"),
                "duplicate playlist names should be rejected case-insensitively");
        const auto savedTrack = library.search("discovery").front();
        require(library.addTrackToPlaylist("Warm-up", savedTrack.id),
                "track should be added to a playlist");
        require(library.addTrackToPlaylist("warm-up", savedTrack.id),
                "adding a track twice should be idempotent");
        require(library.searchPlaylist("Warm-up", "daft").size() == 1,
                "playlist search should filter tracks by metadata");
        require(library.searchPlaylist("Warm-up", "unmatched").empty(),
                "playlist text search should return no unmatched tracks");
        require(library.getPlaylists().size() == 1,
                "playlist names should be persisted");

        track.title = "One More Time (Remaster)";
        require(library.saveTrack(track), "updating a track path should succeed");
        require(library.search("").size() == 1, "upsert should not create a duplicate path");
        require(library.search("remaster").front().year == 2001,
                "upsert should preserve updated metadata");
        require(library.searchPlaylist("Warm-up", "").size() == 1,
                "playlist membership should remain after track metadata updates");
        require(library.removeTrackFromPlaylist("Warm-up", savedTrack.id),
                "track should be removable from a playlist");
        require(library.searchPlaylist("Warm-up", "").empty(),
                "removed track should no longer appear in its playlist");
        require(!library.removeTrackFromPlaylist("Warm-up", savedTrack.id),
                "removing a missing playlist entry should report failure");

        track.path = "C:/music/invalid.wav";
        track.title.clear();
        require(!library.saveTrack(track), "invalid metadata should fail explicitly");
        require(library.getLastError().isNotEmpty(), "invalid metadata should report an error");

        const auto waveFile = databaseFile.getSiblingFile("vyra-waveform-test.wav");
        waveFile.deleteFile();
        juce::WavAudioFormat wavFormat;
        auto outputStream = waveFile.createOutputStream();
        require(outputStream != nullptr, "synthetic waveform test file should open");
        auto* writerRaw = wavFormat.createWriterFor(outputStream.get(), 44100.0,
                                                     1, 16, {}, 0);
        require(writerRaw != nullptr, "synthetic waveform writer should initialize");
        outputStream.release();
        std::unique_ptr<juce::AudioFormatWriter> writer(writerRaw);
        juce::AudioBuffer<float> samples(1, 44100);
        for (int sample = 0; sample < samples.getNumSamples(); ++sample)
        {
            const auto amplitude = sample < samples.getNumSamples() / 2 ? 0.1f : 0.8f;
            const auto value = amplitude * std::sin(
                2.0f * std::numbers::pi_v<float> * 440.0f
                * static_cast<float>(sample) / 44100.0f);
            samples.setSample(0, sample, value);
        }
        require(writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()),
                "synthetic waveform samples should be written");
        writer.reset();
        require(library.indexFile(waveFile), "audio file should be indexed");
        const auto analyzed = library.search("vyra-waveform-test");
        require(analyzed.size() == 1, "indexed test track should be searchable");
        require(analyzed.front().waveform.size() == 512,
                "analysis should persist the 512-bin waveform");
        require(analyzed.front().waveform.front() > 15
                    && analyzed.front().waveform.front() < 40,
                "waveform should reflect the quieter first half");
        require(analyzed.front().waveform.back() > 150
                    && analyzed.front().waveform.back() < 240,
                "waveform should reflect the louder second half");
        waveFile.deleteFile();

        const auto beatFile = databaseFile.getSiblingFile("vyra-bpm-test.wav");
        beatFile.deleteFile();
        auto beatStream = beatFile.createOutputStream();
        require(beatStream != nullptr, "synthetic BPM test file should open");
        auto* beatWriterRaw = wavFormat.createWriterFor(beatStream.get(), 44100.0,
                                                         1, 16, {}, 0);
        require(beatWriterRaw != nullptr, "synthetic BPM writer should initialize");
        beatStream.release();
        std::unique_ptr<juce::AudioFormatWriter> beatWriter(beatWriterRaw);
        constexpr int beatSampleRate = 44100;
        constexpr int beatDurationSeconds = 30;
        constexpr int beatSamples = beatSampleRate * beatDurationSeconds;
        juce::AudioBuffer<float> beatSamplesBuffer(1, beatSamples);
        beatSamplesBuffer.clear();
        for (int sample = 0; sample < beatSamples; ++sample)
        {
            const auto phase = sample % (beatSampleRate / 2);
            if (phase < beatSampleRate / 20)
            {
                const auto decay = std::exp(-static_cast<float>(phase) / 700.0f);
                beatSamplesBuffer.setSample(0, sample, decay * std::sin(
                    2.0f * std::numbers::pi_v<float> * 100.0f
                    * static_cast<float>(phase) / beatSampleRate));
            }
        }
        require(beatWriter->writeFromAudioSampleBuffer(
                    beatSamplesBuffer, 0, beatSamplesBuffer.getNumSamples()),
                "synthetic beat samples should be written");
        beatWriter.reset();
        require(library.indexFile(beatFile), "beat file should be indexed");
        const auto bpmTrack = library.search("vyra-bpm-test");
        require(bpmTrack.size() == 1, "analyzed BPM track should be searchable");
        require(bpmTrack.front().bpm > 118.0 && bpmTrack.front().bpm < 122.0,
                "BPM analysis should detect the 120 BPM test beat");
        beatFile.deleteFile();

        const auto keyFile = databaseFile.getSiblingFile("vyra-key-test.wav");
        keyFile.deleteFile();
        auto keyStream = keyFile.createOutputStream();
        require(keyStream != nullptr, "synthetic key test file should open");
        auto* keyWriterRaw = wavFormat.createWriterFor(keyStream.get(), 44100.0,
                                                        1, 16, {}, 0);
        require(keyWriterRaw != nullptr, "synthetic key writer should initialize");
        keyStream.release();
        std::unique_ptr<juce::AudioFormatWriter> keyWriter(keyWriterRaw);
        constexpr int keyDurationSeconds = 12;
        constexpr int keySamples = beatSampleRate * keyDurationSeconds;
        juce::AudioBuffer<float> chord(1, keySamples);
        for (int sample = 0; sample < keySamples; ++sample)
        {
            const auto time = static_cast<float>(sample) / beatSampleRate;
            const auto value = (std::sin(2.0f * std::numbers::pi_v<float> * 220.0f * time)
                                + std::sin(2.0f * std::numbers::pi_v<float> * 277.18f * time)
                                + std::sin(2.0f * std::numbers::pi_v<float> * 329.63f * time))
                / 6.0f;
            chord.setSample(0, sample, value);
        }
        require(keyWriter->writeFromAudioSampleBuffer(chord, 0, keySamples),
                "synthetic chord should be written");
        keyWriter.reset();
        require(library.indexFile(keyFile), "chord file should be indexed");
        const auto keyTrack = library.search("vyra-key-test");
        require(keyTrack.size() == 1, "analyzed key track should be searchable");
        require(keyTrack.front().musicalKey == "A major"
                    || keyTrack.front().musicalKey == "F# minor",
                "key analysis should recognize the A major pitch collection");
        keyFile.deleteFile();
    }

    {
        vyra::library::TrackLibrary reopenedLibrary(databaseFile);
        const auto playlists = reopenedLibrary.getPlaylists();
        require(playlists.size() == 1 && playlists.front() == "Warm-up",
                "playlists should persist after reopening the database");
        require(reopenedLibrary.searchRecentPlays("").size() == 3,
                "playback history should persist after reopening the database");
        vyra::library::TrackPerformanceState performanceState;
        require(reopenedLibrary.loadPerformanceState(
                    "C:/music/daft punk/one more time.wav", performanceState)
                    && performanceState.hotCueSeconds[0] == 12.5
                    && performanceState.hotCueSeconds[2] == 98.75
                    && performanceState.loopBeats == 8,
                "hot cues and loop length should persist across database reopen");
    }

    databaseFile.deleteFile();

    const auto legacyDatabaseFile = databaseFile.getSiblingFile("vyra-library-legacy-test.sqlite3");
    legacyDatabaseFile.deleteFile();
    sqlite3* legacyDatabase = nullptr;
    require(sqlite3_open(legacyDatabaseFile.getFullPathName().toRawUTF8(),
                         &legacyDatabase) == SQLITE_OK,
            "legacy library database should open");
    constexpr auto legacySchema =
        "CREATE TABLE tracks ("
        "id INTEGER PRIMARY KEY,path TEXT NOT NULL UNIQUE,title TEXT NOT NULL,"
        "artist TEXT NOT NULL DEFAULT '',album TEXT NOT NULL DEFAULT '',"
        "genre TEXT NOT NULL DEFAULT '',musical_key TEXT NOT NULL DEFAULT '',"
        "year INTEGER NOT NULL DEFAULT 0,sample_rate INTEGER NOT NULL DEFAULT 0,"
        "bit_depth INTEGER NOT NULL DEFAULT 0,duration_seconds REAL NOT NULL DEFAULT 0,"
        "bpm REAL NOT NULL DEFAULT 0);";
    require(sqlite3_exec(legacyDatabase, legacySchema, nullptr, nullptr, nullptr) == SQLITE_OK,
            "legacy library schema should initialize");
    sqlite3_close(legacyDatabase);
    {
        vyra::library::TrackLibrary migratedLibrary(legacyDatabaseFile);
        vyra::library::Track migratedTrack;
        migratedTrack.path = "C:/music/migrated.wav";
        migratedTrack.title = "Migrated track";
        require(migratedLibrary.saveTrack(migratedTrack),
                "legacy schema should migrate to waveform-capable storage");
        require(migratedLibrary.search("migrated").size() == 1,
                "migrated library should remain searchable");
        require(migratedLibrary.recordPlay("C:/music/migrated.wav", "Migrated track", 0)
                    && migratedLibrary.searchRecentPlays("").size() == 1,
                "legacy libraries should initialize playback history");
    }
    legacyDatabaseFile.deleteFile();
    std::cout << "Track library tests passed\n";
    return EXIT_SUCCESS;
}
