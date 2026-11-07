#include "vyra/integrations/DiscordPresence.h"

#include <discord_rpc.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <ctime>
#include <limits>

namespace vyra::integrations
{
namespace
{
constexpr char discordApplicationId[] = "1568306069007237163";
std::atomic<DiscordPresence*> activePresence { nullptr };

juce::String formatTime(double seconds)
{
    const auto wholeSeconds = static_cast<int>(std::clamp(
        seconds, 0.0, static_cast<double>(std::numeric_limits<int>::max())));
    return juce::String(wholeSeconds / 60).paddedLeft('0', 2)
        + ":" + juce::String(wholeSeconds % 60).paddedLeft('0', 2);
}
}

DiscordPresence::DiscordPresence()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Vyra";
    options.filenameSuffix = ".settings";
    options.folderName = "Vyra";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    preferences = std::make_unique<juce::PropertiesFile>(options);
    const auto shouldEnable = preferences->getBoolValue("discordRichPresence", true);

    activePresence.store(this, std::memory_order_release);
    enabled.store(false, std::memory_order_relaxed);
    setEnabled(shouldEnable);
    startTimerHz(1);
}

DiscordPresence::~DiscordPresence()
{
    stopTimer();
    activePresence.store(nullptr, std::memory_order_release);
    if (initialized)
    {
        Discord_ClearPresence();
        Discord_Shutdown();
    }
}

bool DiscordPresence::isEnabled() const noexcept
{
    return enabled.load(std::memory_order_relaxed);
}

void DiscordPresence::setEnabled(bool shouldEnable)
{
    const auto wasEnabled = enabled.exchange(shouldEnable, std::memory_order_relaxed);
    if (preferences != nullptr)
    {
        preferences->setValue("discordRichPresence", shouldEnable);
        preferences->saveIfNeeded();
    }

    if (shouldEnable == wasEnabled)
        return;

    if (!shouldEnable)
    {
        if (initialized)
        {
            Discord_ClearPresence();
            Discord_Shutdown();
            initialized = false;
        }
        lastTitle.clear();
        lastPositionSeconds = -1.0;
        setConnectionState(ConnectionState::disabled);
        return;
    }

    DiscordEventHandlers handlers {};
    handlers.ready = &DiscordPresence::handleReady;
    handlers.disconnected = &DiscordPresence::handleDisconnected;
    handlers.errored = &DiscordPresence::handleError;
    Discord_Initialize(discordApplicationId, &handlers, 0, nullptr);
    initialized = true;
    setConnectionState(ConnectionState::connecting);
}

void DiscordPresence::updateTrack(const juce::String& title, double positionSeconds,
                                  double durationSeconds, bool isPlaying)
{
    if (!isEnabled() || !initialized)
        return;

    if (title.isEmpty() || !std::isfinite(positionSeconds)
        || !std::isfinite(durationSeconds) || durationSeconds <= 0.0)
    {
        if (lastTitle.isNotEmpty())
        {
            Discord_ClearPresence();
            lastTitle.clear();
            lastPositionSeconds = -1.0;
        }
        return;
    }

    positionSeconds = std::clamp(positionSeconds, 0.0, durationSeconds);
    const auto roundedPosition = std::floor(positionSeconds);
    auto details = title;
    while (details.getNumBytesAsUTF8() > 128)
        details = details.dropLastCharacters(1);
    const auto nowMilliseconds = juce::Time::getMillisecondCounter();
    const auto elapsedMilliseconds = nowMilliseconds - lastUpdateMilliseconds;
    const auto positionChanged = std::abs(roundedPosition - lastPositionSeconds) >= 5.0;
    const auto activityChanged = details != lastTitle || isPlaying != lastWasPlaying
        || std::abs(durationSeconds - lastDurationSeconds) > 0.5;
    if (!activityChanged && (!positionChanged || elapsedMilliseconds < 5000U))
        return;

    const auto elapsedText = formatTime(positionSeconds);
    const auto durationText = formatTime(durationSeconds);
    auto state = juce::String(isPlaying ? "Playing" : "Paused")
        + " · " + elapsedText + " / " + durationText;

    DiscordRichPresence presence {};
    presence.details = details.toRawUTF8();
    presence.state = state.toRawUTF8();
    if (isPlaying)
    {
        const auto timestamp = static_cast<std::int64_t>(std::time(nullptr));
        presence.startTimestamp = timestamp
            - static_cast<std::int64_t>(roundedPosition);
        presence.endTimestamp = presence.startTimestamp
            + static_cast<std::int64_t>(std::ceil(durationSeconds));
    }

    Discord_UpdatePresence(&presence);
    lastTitle = std::move(details);
    lastPositionSeconds = roundedPosition;
    lastDurationSeconds = durationSeconds;
    lastWasPlaying = isPlaying;
    lastUpdateMilliseconds = nowMilliseconds;
}

juce::String DiscordPresence::getStatus() const
{
    const auto state = connectionState.load(std::memory_order_relaxed);
    std::lock_guard lock(statusMutex);
    switch (state)
    {
        case ConnectionState::disabled: return "Discord Rich Presence is disabled.";
        case ConnectionState::connecting: return "Connecting to the Discord desktop app...";
        case ConnectionState::connected: return "Connected to Discord.";
        case ConnectionState::disconnected:
        case ConnectionState::error:
            return connectionMessage.isNotEmpty()
                ? connectionMessage
                : juce::String("Discord is unavailable; Rich Presence will retry.");
    }
    return "Discord Rich Presence status unavailable.";
}

void DiscordPresence::handleReady(const DiscordUser*)
{
    if (auto* presence = activePresence.load(std::memory_order_acquire))
        presence->setConnectionState(ConnectionState::connected);
}

void DiscordPresence::handleDisconnected(int errorCode, const char* message)
{
    if (auto* presence = activePresence.load(std::memory_order_acquire))
        presence->setConnectionState(
            ConnectionState::disconnected,
            juce::String("Discord disconnected (")
                + juce::String(errorCode) + "): "
                + juce::String::fromUTF8(message != nullptr ? message : ""));
}

void DiscordPresence::handleError(int errorCode, const char* message)
{
    if (auto* presence = activePresence.load(std::memory_order_acquire))
        presence->setConnectionState(
            ConnectionState::error,
            juce::String("Discord Rich Presence error (")
                + juce::String(errorCode) + "): "
                + juce::String::fromUTF8(message != nullptr ? message : ""));
}

void DiscordPresence::setConnectionState(ConnectionState state, juce::String message)
{
    {
        std::lock_guard lock(statusMutex);
        connectionMessage = std::move(message);
    }
    connectionState.store(state, std::memory_order_relaxed);
}

void DiscordPresence::timerCallback()
{
    if (enabled.load(std::memory_order_relaxed) && initialized)
        Discord_RunCallbacks();
}
}
