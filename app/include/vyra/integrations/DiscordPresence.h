#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

struct DiscordUser;

namespace vyra::integrations
{
class DiscordPresence final : private juce::Timer
{
public:
    DiscordPresence();
    ~DiscordPresence() override;

    DiscordPresence(const DiscordPresence&) = delete;
    DiscordPresence& operator=(const DiscordPresence&) = delete;

    [[nodiscard]] bool isEnabled() const noexcept;
    void setEnabled(bool shouldEnable);
    void updateTrack(const juce::String& title, double positionSeconds,
                     double durationSeconds, bool isPlaying);
    [[nodiscard]] juce::String getStatus() const;

private:
    enum class ConnectionState
    {
        disabled,
        connecting,
        connected,
        disconnected,
        error
    };

    static void handleReady(const DiscordUser* user);
    static void handleDisconnected(int errorCode, const char* message);
    static void handleError(int errorCode, const char* message);
    void setConnectionState(ConnectionState state, juce::String message = {});
    void timerCallback() override;

    std::unique_ptr<juce::PropertiesFile> preferences;
    std::atomic<bool> enabled { true };
    std::atomic<ConnectionState> connectionState { ConnectionState::disabled };
    mutable std::mutex statusMutex;
    juce::String connectionMessage;
    juce::String lastTitle;
    double lastPositionSeconds = -1.0;
    double lastDurationSeconds = 0.0;
    std::uint32_t lastUpdateMilliseconds = 0;
    bool lastWasPlaying = false;
    bool initialized = false;
};
}
