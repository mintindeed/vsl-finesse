#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Session.h"

#include <atomic>

// Plays every track's sequence through its plugin and mixes them to the
// output. Also the play head the hosted plugins see.
class AudioEngine : public juce::AudioIODeviceCallback,
                    public juce::AudioPlayHead
{
public:
    explicit AudioEngine (Session&);

    // Transport (message thread).
    void play();
    void stop();
    void togglePlay() { isPlaying() ? stop() : play(); }
    bool isPlaying() const { return playing.load(); }
    void setPosition (double seconds);
    double getPositionSeconds() const { return position.load(); }

    // A/B: play the untouched original instead of the finessed part.
    void setPlayOriginal (bool);
    bool isPlayingOriginal() const { return playOriginal.load(); }

    // Gets a newly created plugin ready for this device.
    void prepare (juce::AudioPluginInstance&);

    double getSampleRate() const { return sampleRate.load(); }
    int getBlockSize() const { return blockSize.load(); }

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override;

    // AudioPlayHead
    juce::Optional<PositionInfo> getPosition() const override;

private:
    void renderTrack (Track&, double from, double to, int numSamples, bool reset, bool chase,
                      float* const* out, int numOut);

    Session& session;

    std::atomic<bool> playing { false };
    std::atomic<bool> playOriginal { false };
    std::atomic<double> position { 0.0 };
    std::atomic<bool> resetPending { false }; // all notes off
    std::atomic<bool> chasePending { false }; // resend keyswitch/CC state

    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> blockSize { 512 };

    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midi;

    // Snapshot for getPosition(), written on the audio thread before plugins run.
    double blockStartSeconds = 0.0;
    int64_t blockStartSamples = 0;
    bool blockPlaying = false;
    const PlaybackSequence* blockTempoSource = nullptr;
};
