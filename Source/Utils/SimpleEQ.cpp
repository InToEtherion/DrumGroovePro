#include "SimpleEQ.h"

SimpleEQ::SimpleEQ()
{
    // Initialize with flat response (0 dB gain on all bands)
    for (int i = 0; i < NumBands; ++i)
    {
        gains[i] = 0.0f;
    }
}

void SimpleEQ::prepareToPlay(double sampleRate, int maximumBlockSize)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(maximumBlockSize);
    spec.numChannels = 2;

    for (int i = 0; i < NumBands; ++i)
    {
        filters[i].prepare(spec);
        updateFilterCoefficients(static_cast<Band>(i));
    }
}

void SimpleEQ::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    // Process through all three bands
    for (int i = 0; i < NumBands; ++i)
    {
        filters[i].process(context);
    }
}

void SimpleEQ::reset()
{
    for (auto& filter : filters)
    {
        filter.reset();
    }
}

void SimpleEQ::setGain(Band band, float gainDb)
{
    if (band >= 0 && band < NumBands)
    {
        gains[band] = juce::jlimit(-12.0f, 12.0f, gainDb);
        updateFilterCoefficients(band);
    }
}

float SimpleEQ::getGain(Band band) const
{
    if (band >= 0 && band < NumBands)
        return gains[band];
    return 0.0f;
}

float SimpleEQ::getFrequency(Band band)
{
    switch (band)
    {
        case Low:  return lowFreq;
        case Mid:  return midFreq;
        case High: return highFreq;
        default:   return 1000.0f;
    }
}

juce::String SimpleEQ::getBandName(Band band)
{
    switch (band)
    {
        case Low:  return "Low";
        case Mid:  return "Mid";
        case High: return "High";
        default:   return "Unknown";
    }
}

void SimpleEQ::updateFilterCoefficients(Band band)
{
    if (currentSampleRate <= 0.0)
        return;

    float gainLinear = juce::Decibels::decibelsToGain(gains[band]);

    juce::ReferenceCountedObjectPtr<Coefficients> coeffs;

    switch (band)
    {
        case Low:
            coeffs = Coefficients::makeLowShelf(currentSampleRate, lowFreq, 0.707f, gainLinear);
            break;

        case Mid:
            coeffs = Coefficients::makePeakFilter(currentSampleRate, midFreq, midQ, gainLinear);
            break;

        case High:
            coeffs = Coefficients::makeHighShelf(currentSampleRate, highFreq, 0.707f, gainLinear);
            break;

        default:
            return;
    }

    *filters[band].state = *coeffs;
}
