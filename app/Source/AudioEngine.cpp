#include "AudioEngine.h"

AudioEngine::AudioEngine (Session& s) : session (s) {}

void AudioEngine::play()
{
    if (playing.exchange (true))
        return;
    if (position.load() >= session.lengthSeconds() + 1.0)
        position = 0.0;
    chasePending = true;
}

void AudioEngine::stop()
{
    playing = false;
    resetPending = true;
}

void AudioEngine::setPosition (double seconds)
{
    position = juce::jmax (0.0, seconds);
    resetPending = true;
    chasePending = true;
}

void AudioEngine::setPlayOriginal (bool original)
{
    if (playOriginal.exchange (original) != original)
    {
        resetPending = true;
        chasePending = true;
    }
}

void AudioEngine::prepare (juce::AudioPluginInstance& plugin)
{
    plugin.setPlayHead (this);
    plugin.setRateAndBufferSizeDetails (sampleRate.load(), blockSize.load());
    plugin.prepareToPlay (sampleRate.load(), blockSize.load());
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    blockSize = device->getCurrentBufferSizeSamples();
    scratch.setSize (32, juce::jmax (blockSize.load(), 4096));
    midi.ensureSize (8192);

    const juce::ScopedLock sl (session.audioLock);
    for (const auto& t : session.tracks())
        if (t->audio.plugin != nullptr)
            prepare (*t->audio.plugin);
}

void AudioEngine::audioDeviceStopped()
{
    const juce::ScopedLock sl (session.audioLock);
    for (const auto& t : session.tracks())
        if (t->audio.plugin != nullptr)
            t->audio.plugin->releaseResources();
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    const juce::ScopedTryLock sl (session.audioLock);
    if (! sl.isLocked())
        return; // the message thread is swapping something; skip one block

    const double sr = sampleRate.load();
    const bool isPlayingNow = playing.load();
    const double from = position.load();
    const double to = from + numSamples / sr;
    const bool reset = resetPending.exchange (false);
    const bool chase = chasePending.exchange (false);

    blockStartSeconds = from;
    blockStartSamples = (int64_t) std::llround (from * sr);
    blockPlaying = isPlayingNow;
    blockTempoSource = nullptr;
    for (const auto& t : session.tracks())
        if (t->audio.finessed != nullptr)
        {
            blockTempoSource = t->audio.finessed.get();
            break;
        }

    bool anySolo = false;
    for (const auto& t : session.tracks())
        anySolo |= t->audio.solo;

    for (const auto& t : session.tracks())
    {
        // Muted tracks still run (silently) so their notes and controllers stay in sync.
        const bool audible = ! t->audio.mute && (! anySolo || t->audio.solo);
        renderTrack (*t, isPlayingNow ? from : -1.0, to, numSamples, reset, chase && isPlayingNow,
                     audible ? outputChannelData : nullptr, numOutputChannels);
    }

    if (isPlayingNow)
    {
        position = to;
        if (to > session.lengthSeconds() + 3.0) // let release tails ring, then stop
        {
            playing = false;
            resetPending = true;
        }
    }
}

void AudioEngine::renderTrack (Track& t, double from, double to, int numSamples, bool reset, bool chase,
                               float* const* out, int numOut)
{
    auto* plugin = t.audio.plugin.get();
    const auto& seq = playOriginal.load() ? t.audio.original : t.audio.finessed;

    midi.clear();

    if (reset)
    {
        for (int ch = 0; ch < 16; ++ch)
        {
            for (int n = 0; n < 128; ++n)
                if (t.audio.sounding[(size_t) ch][(size_t) n])
                    midi.addEvent (juce::MidiMessage::noteOff (ch + 1, n), 0);
            if (t.audio.sounding[(size_t) ch].any())
                midi.addEvent (juce::MidiMessage::allNotesOff (ch + 1), 0);
            t.audio.sounding[(size_t) ch].reset();
        }
    }

    if (seq != nullptr && from >= 0.0)
    {
        if (chase)
            seq->chase (from, midi);
        seq->collect (from, to, sampleRate.load(), numSamples, midi);
    }

    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.getChannel() < 1)
            continue;
        auto& bits = t.audio.sounding[(size_t) (m.getChannel() - 1)];
        if (m.isNoteOn())
            bits.set ((size_t) m.getNoteNumber());
        else if (m.isNoteOff())
            bits.reset ((size_t) m.getNoteNumber());
    }

    if (plugin == nullptr)
        return;

    const int channels = juce::jmax (plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels(), 2);
    if (scratch.getNumChannels() < channels || scratch.getNumSamples() < numSamples)
        scratch.setSize (juce::jmax (channels, scratch.getNumChannels()), juce::jmax (numSamples, scratch.getNumSamples()), false, false, true);

    juce::AudioBuffer<float> buffer (scratch.getArrayOfWritePointers(), channels, numSamples);
    buffer.clear();

    if (plugin->isSuspended())
        return;

    {
        const juce::ScopedLock pluginLock (plugin->getCallbackLock());
        plugin->processBlock (buffer, midi);
    }

    if (out == nullptr)
        return;

    const int pluginOuts = plugin->getTotalNumOutputChannels();
    for (int ch = 0; ch < numOut; ++ch)
    {
        if (out[ch] == nullptr || pluginOuts == 0)
            continue;
        const int src = pluginOuts == 1 ? 0 : juce::jmin (ch, pluginOuts - 1);
        juce::FloatVectorOperations::addWithMultiply (out[ch], buffer.getReadPointer (src), t.audio.gain, numSamples);
    }
}

juce::Optional<juce::AudioPlayHead::PositionInfo> AudioEngine::getPosition() const
{
    PositionInfo info;
    info.setIsPlaying (blockPlaying);
    info.setTimeInSeconds (blockStartSeconds);
    info.setTimeInSamples (blockStartSamples);

    if (blockTempoSource != nullptr)
    {
        const auto& tempo = blockTempoSource->tempo();
        const double tick = tempo.tickAt (blockStartSeconds);
        info.setBpm (tempo.bpmAt (tick));
        info.setPpqPosition (tick / tempo.ppq());

        TimeSignature ts;
        double barStartTick = 0.0;
        for (const auto& m : blockTempoSource->timeSignatures())
            if ((double) m.tick <= tick)
            {
                ts.numerator = m.numerator;
                ts.denominator = m.denominator;
                barStartTick = (double) m.tick;
            }
        info.setTimeSignature (ts);
        const double barTicks = tempo.ppq() * 4.0 * ts.numerator / juce::jmax (1, ts.denominator);
        const double bars = std::floor ((tick - barStartTick) / barTicks);
        info.setPpqPositionOfLastBarStart ((barStartTick + bars * barTicks) / tempo.ppq());
    }
    else
    {
        info.setBpm (120.0);
        info.setPpqPosition (blockStartSeconds * 2.0);
    }
    return info;
}
