#include "juce_audio_processors.h"
/*
  ==============================================================================

   This file is part of the JUCE framework.
   Copyright (c) Raw Material Software Limited

   JUCE is an open source framework subject to commercial or open source
   licensing.

   By downloading, installing, or using the JUCE framework, or combining the
   JUCE framework with any other source code, object code, content or any other
   copyrightable work, you agree to the terms of the JUCE End User Licence
   Agreement, and all incorporated terms including the JUCE Privacy Policy and
   the JUCE Website Terms of Service, as applicable, which will bind you. If you
   do not agree to the terms of these agreements, we will not license the JUCE
   framework to you, and you must discontinue the installation or download
   process and cease use of the JUCE framework.

   JUCE End User Licence Agreement: https://juce.com/legal/juce-8-licence/
   JUCE Privacy Policy: https://juce.com/juce-privacy-policy
   JUCE Website Terms of Service: https://juce.com/juce-website-terms-of-service/

   Or:

   You may also use this code under the terms of the AGPLv3:
   https://www.gnu.org/licenses/agpl-3.0.en.html

   THE JUCE FRAMEWORK IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL
   WARRANTIES, WHETHER EXPRESSED OR IMPLIED, INCLUDING WARRANTY OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

namespace juce
{

//==============================================================================

AudioProcessorValueTreeState::Parameter::Parameter (const ParameterID& parameterID,
                                                    const String& parameterName,
                                                    NormalisableRange<float> valueRange,
                                                    float defaultParameterValue,
                                                    const AudioProcessorValueTreeStateParameterAttributes& attributes)
    : AudioParameterFloat (parameterID,
                           parameterName,
                           valueRange,
                           defaultParameterValue,
                           attributes.getAudioParameterFloatAttributes()),
      unsnappedDefault (valueRange.convertTo0to1 (defaultParameterValue)),
      discrete (attributes.getDiscrete()),
      boolean (attributes.getBoolean())
{
}

float AudioProcessorValueTreeState::Parameter::getDefaultValue() const  { return unsnappedDefault; }
int AudioProcessorValueTreeState::Parameter::getNumSteps() const        { return RangedAudioParameter::getNumSteps(); }

bool AudioProcessorValueTreeState::Parameter::isDiscrete() const        { return discrete; }
bool AudioProcessorValueTreeState::Parameter::isBoolean() const         { return boolean; }

void AudioProcessorValueTreeState::Parameter::valueChanged (float newValue)
{
    if (approximatelyEqual ((float) lastValue, newValue))
        return;

    lastValue = newValue;
    NullCheckedInvocation::invoke (onValueChanged);
}

//==============================================================================
class AudioProcessorValueTreeState::ParameterAdapter final : private AudioProcessorParameter::Listener
{
private:
    using Listener = AudioProcessorValueTreeState::Listener;

public:
    explicit ParameterAdapter (RangedAudioParameter& parameterIn)
        : parameter (parameterIn),
          // For legacy reasons, the unnormalised value should *not* be snapped on construction
          unnormalisedValue (getRange().convertFrom0to1 (parameter.getDefaultValue()))
    {
        parameter.addListener (this);

        if (auto* ptr = dynamic_cast<Parameter*> (&parameter))
            ptr->onValueChanged = [this] { parameterValueChanged ({}, {}); };
    }

    ~ParameterAdapter() override        { parameter.removeListener (this); }

    void addListener (Listener* l)      { listeners.add (l); }
    void removeListener (Listener* l)   { listeners.remove (l); }

    RangedAudioParameter& getParameter()                { return parameter; }
    const RangedAudioParameter& getParameter() const    { return parameter; }

    const NormalisableRange<float>& getRange() const    { return parameter.getNormalisableRange(); }

    float getDenormalisedDefaultValue() const    { return denormalise (parameter.getDefaultValue()); }

    void setDenormalisedValue (float value)
    {
        if (! approximatelyEqual (value, (float) unnormalisedValue))
            setNormalisedValue (normalise (value));
    }

    float getDenormalisedValueForText (const String& text) const
    {
        return denormalise (parameter.getValueForText (text));
    }

    String getTextForDenormalisedValue (float value) const
    {
        return parameter.getText (normalise (value), 0);
    }

    float getDenormalisedValue() const                { return unnormalisedValue; }
    std::atomic<float>& getRawDenormalisedValue()     { return unnormalisedValue; }

    bool flushToTree (const Identifier& key, UndoManager* um)
    {
        auto needsUpdateTestValue = true;

        if (! needsUpdate.compare_exchange_strong (needsUpdateTestValue, false))
            return false;

        if (auto* valueProperty = tree.getPropertyPointer (key))
        {
            if (! approximatelyEqual ((float) *valueProperty, unnormalisedValue.load()))
            {
                ScopedValueSetter<bool> svs (ignoreParameterChangedCallbacks, true);
                tree.setProperty (key, unnormalisedValue.load(), um);
            }
        }
        else
        {
            tree.setProperty (key, unnormalisedValue.load(), nullptr);
        }

        return true;
    }

    ValueTree tree;

private:
    void parameterGestureChanged (int, bool) override {}

    void parameterValueChanged (int, float) override
    {
        const auto newValue = denormalise (parameter.getValue());

        if (! listenersNeedCalling && approximatelyEqual ((float) unnormalisedValue, newValue))
            return;

        unnormalisedValue = newValue;
        listeners.call ([this] (Listener& l) { l.parameterChanged (parameter.paramID, unnormalisedValue); });
        listenersNeedCalling = false;
        needsUpdate = true;
    }

    float denormalise (float normalised) const
    {
        return getParameter().convertFrom0to1 (normalised);
    }

    float normalise (float denormalised) const
    {
        return getParameter().convertTo0to1 (denormalised);
    }

    void setNormalisedValue (float value)
    {
        if (ignoreParameterChangedCallbacks)
            return;

        parameter.setValueNotifyingHost (value);
    }

    class LockedListeners
    {
    public:
        template <typename Fn>
        void call (Fn&& fn)
        {
            const CriticalSection::ScopedLockType lock (mutex);
            listeners.call (std::forward<Fn> (fn));
        }

        void add (Listener* l)
        {
            const CriticalSection::ScopedLockType lock (mutex);
            listeners.add (l);
        }

        void remove (Listener* l)
        {
            const CriticalSection::ScopedLockType lock (mutex);
            listeners.remove (l);
        }

    private:
        CriticalSection mutex;
        ListenerList<Listener> listeners;
    };

    RangedAudioParameter& parameter;
    LockedListeners listeners;
    std::atomic<float> unnormalisedValue { 0.0f };
    std::atomic<bool> needsUpdate { true }, listenersNeedCalling { true };
    bool ignoreParameterChangedCallbacks { false };
};

//==============================================================================
AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& processorToConnectTo,
                                                            UndoManager* undoManagerToUse,
                                                            const Identifier& valueTreeType,
                                                            ParameterLayout parameterLayout)
    : AudioProcessorValueTreeState (processorToConnectTo, undoManagerToUse)
{
    struct PushBackVisitor final : ParameterLayout::Visitor
    {
        explicit PushBackVisitor (AudioProcessorValueTreeState& stateIn)
            : state (&stateIn) {}

        void visit (std::unique_ptr<RangedAudioParameter> param) const override
        {
            if (param == nullptr)
            {
                jassertfalse;
                return;
            }

            state->addParameterAdapter (*param);
            state->processor.addParameter (param.release());
        }

        void visit (std::unique_ptr<AudioProcessorParameterGroup> group) const override
        {
            if (group == nullptr)
            {
                jassertfalse;
                return;
            }

            for (const auto param : group->getParameters (true))
            {
                if (const auto rangedParam = dynamic_cast<RangedAudioParameter*> (param))
                {
                    state->addParameterAdapter (*rangedParam);
                }
                else
                {
                    // If you hit this assertion then you are attempting to add a parameter that is
                    // not derived from RangedAudioParameter to the AudioProcessorValueTreeState.
                    jassertfalse;
                }
            }

            state->processor.addParameterGroup (std::move (group));
        }

        AudioProcessorValueTreeState* state;
    };

    for (auto& item : parameterLayout.parameters)
        item->accept (PushBackVisitor (*this));

    state = ValueTree (valueTreeType);
}

AudioProcessorValueTreeState::AudioProcessorValueTreeState (AudioProcessor& p, UndoManager* um)
    : processor (p), undoManager (um)
{
    startTimerHz (10);
    state.addListener (this);
}

AudioProcessorValueTreeState::~AudioProcessorValueTreeState()
{
    stopTimer();
}

//==============================================================================
RangedAudioParameter* AudioProcessorValueTreeState::createAndAddParameter (const String& paramID,
                                                                           const String& paramName,
                                                                           const String& labelText,
                                                                           NormalisableRange<float> range,
                                                                           float defaultVal,
                                                                           std::function<String (float)> valueToTextFunction,
                                                                           std::function<float (const String&)> textToValueFunction,
                                                                           bool isMetaParameter,
                                                                           bool isAutomatableParameter,
                                                                           bool isDiscreteParameter,
                                                                           AudioProcessorParameter::Category category,
                                                                           bool isBooleanParameter)
{
    auto attributes = AudioProcessorValueTreeStateParameterAttributes()
                          .withLabel (labelText)
                          .withStringFromValueFunction ([fn = std::move (valueToTextFunction)] (float v, int) { return fn (v); })
                          .withValueFromStringFunction (std::move (textToValueFunction))
                          .withMeta (isMetaParameter)
                          .withAutomatable (isAutomatableParameter)
                          .withDiscrete (isDiscreteParameter)
                          .withCategory (category)
                          .withBoolean (isBooleanParameter);

    return createAndAddParameter (std::make_unique<Parameter> (paramID,
                                                               paramName,
                                                               range,
                                                               defaultVal,
                                                               std::move (attributes)));
}

RangedAudioParameter* AudioProcessorValueTreeState::createAndAddParameter (std::unique_ptr<RangedAudioParameter> param)
{
    if (param == nullptr)
        return nullptr;

    // All parameters must be created before giving this manager a ValueTree state!
    jassert (! state.isValid());

    if (getParameter (param->paramID) != nullptr)
        return nullptr;

    addParameterAdapter (*param);

    processor.addParameter (param.get());

    return param.release();
}

//==============================================================================
void AudioProcessorValueTreeState::addParameterAdapter (RangedAudioParameter& param)
{
    adapterTable.emplace (param.paramID, std::make_unique<ParameterAdapter> (param));
}

AudioProcessorValueTreeState::ParameterAdapter* AudioProcessorValueTreeState::getParameterAdapter (StringRef paramID) const
{
    auto it = adapterTable.find (paramID);
    return it == adapterTable.end() ? nullptr : it->second.get();
}

void AudioProcessorValueTreeState::addParameterListener (StringRef paramID, Listener* listener)
{
    if (auto* p = getParameterAdapter (paramID))
        p->addListener (listener);
}

void AudioProcessorValueTreeState::removeParameterListener (StringRef paramID, Listener* listener)
{
    if (auto* p = getParameterAdapter (paramID))
        p->removeListener (listener);
}

Value AudioProcessorValueTreeState::getParameterAsValue (StringRef paramID) const
{
    if (auto* adapter = getParameterAdapter (paramID))
        if (adapter->tree.isValid())
            return adapter->tree.getPropertyAsValue (valuePropertyID, undoManager);

    return {};
}

NormalisableRange<float> AudioProcessorValueTreeState::getParameterRange (StringRef paramID) const noexcept
{
    if (auto* p = getParameterAdapter (paramID))
        return p->getRange();

    return {};
}

RangedAudioParameter* AudioProcessorValueTreeState::getParameter (StringRef paramID) const noexcept
{
    if (auto adapter = getParameterAdapter (paramID))
        return &adapter->getParameter();

    return nullptr;
}

std::atomic<float>* AudioProcessorValueTreeState::getRawParameterValue (StringRef paramID) const noexcept
{
    if (auto* p = getParameterAdapter (paramID))
        return &p->getRawDenormalisedValue();

    return nullptr;
}

ValueTree AudioProcessorValueTreeState::copyState()
{
    ScopedLock lock (valueTreeChanging);
    flushParameterValuesToValueTree();
    return state.createCopy();
}

void AudioProcessorValueTreeState::replaceState (const ValueTree& newState)
{
    ScopedLock lock (valueTreeChanging);

    state = newState;

    if (undoManager != nullptr)
        undoManager->clearUndoHistory();
}

void AudioProcessorValueTreeState::setNewState (ValueTree vt)
{
    jassert (vt.getParent() == state);

    if (auto* p = getParameterAdapter (vt.getProperty (idPropertyID).toString()))
    {
        p->tree = vt;
        p->setDenormalisedValue (p->tree.getProperty (valuePropertyID, p->getDenormalisedDefaultValue()));
    }
}

void AudioProcessorValueTreeState::updateParameterConnectionsToChildTrees()
{
    ScopedLock lock (valueTreeChanging);

    for (auto& p : adapterTable)
        p.second->tree = ValueTree();

    for (const auto& child : state)
        setNewState (child);

    for (auto& p : adapterTable)
    {
        auto& adapter = *p.second;

        if (! adapter.tree.isValid())
        {
            adapter.tree = ValueTree (valueType);
            adapter.tree.setProperty (idPropertyID, adapter.getParameter().paramID, nullptr);
            state.appendChild (adapter.tree, nullptr);
        }
    }

    flushParameterValuesToValueTree();
}

void AudioProcessorValueTreeState::valueTreePropertyChanged (ValueTree& tree, const Identifier&)
{
    if (tree.hasType (valueType) && tree.getParent() == state)
        setNewState (tree);
}

void AudioProcessorValueTreeState::valueTreeChildAdded (ValueTree& parent, ValueTree& tree)
{
    if (parent == state && tree.hasType (valueType))
        setNewState (tree);
}

void AudioProcessorValueTreeState::valueTreeRedirected (ValueTree& v)
{
    if (v == state)
        updateParameterConnectionsToChildTrees();
}

bool AudioProcessorValueTreeState::flushParameterValuesToValueTree()
{
    ScopedLock lock (valueTreeChanging);

    bool anyUpdated = false;

    for (auto& p : adapterTable)
        anyUpdated |= p.second->flushToTree (valuePropertyID, undoManager);

    return anyUpdated;
}

void AudioProcessorValueTreeState::timerCallback()
{
    auto anythingUpdated = flushParameterValuesToValueTree();

    startTimer (anythingUpdated ? 1000 / 50
                                : jlimit (50, 500, getTimerInterval() + 20));
}

} // namespace juce
