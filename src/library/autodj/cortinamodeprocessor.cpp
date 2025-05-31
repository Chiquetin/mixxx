Class TransitionManager {
    calculateTransition() {
    }

    m_channelFader;
    m_pCOCrossfader;
}

Class SocialDanceTM : TransitionManager {
    m_xFadeTransitionProgress; // is used to measure transition progress
    m_xFadeTransitionStart;    // play position at which fromDeck starts x-fading
}

double TransitionManager::getFirstSoundSecond(DeckAttributes* pDeck) {
    TrackPointer pTrack = pDeck->getLoadedTrack();
    if (!pTrack) {
        return 0.0;
    }
    CuePointer pFromTrackN60dBSound = pTrack->findCueByType(mixxx::CueType::N60dBSound);
    if (pFromTrackN60dBSound) {
        const mixxx::audio::FramePos firstSound = pFromTrackN60dBSound->getPosition();
        if (firstSound.isValid()) {
            const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
            if (firstSound <= trackEndPosition) {
                return framePositionToSeconds(firstSound, pDeck);
            } else {
                qWarning() << "-60 dB Sound Cue starts after track end in:"
                           << pTrack->getLocation()
                           << "Using the first sample instead.";
            }
        }
    }
    return 0.0;
}

double TransitionManager::getIntroStartSecond(DeckAttributes* pDeck) {
    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    const mixxx::audio::FramePos introStartPosition = pDeck->introStartPosition();
    const mixxx::audio::FramePos introEndPosition = pDeck->introEndPosition();
    if (!introStartPosition.isValid() || introStartPosition > trackEndPosition) {
        double firstSoundSecond = getFirstSoundSecond(pDeck);
        if (!introEndPosition.isValid() || introEndPosition > trackEndPosition) {
            // No intro start and intro end set, use First Sound.
            return firstSoundSecond;
        }
        double introEndSecond = framePositionToSeconds(introEndPosition, pDeck);
        if (m_transitionTime >= 0) {
            return introEndSecond - m_transitionTime;
        }
        return introEndSecond;
    }
    return framePositionToSeconds(introStartPosition, pDeck);
}

CortinaModeProcessor::calculateTrackMarkers(DeckAttributes* pDeck) {
    // calculates the effective start and end of the intro and outro
    // based on the rules for the transition

    // The advantage of using the FramePos values here is that the validity can be checked
    const mixxx::audio::FramePos introStartPosition = pDeck->introStartPosition();
    const mixxx::audio::FramePos introEndPosition = pDeck->introEndPosition();
    const mixxx::audio::FramePos outroStartPosition = pDeck->outroStartPosition();
    const mixxx::audio::FramePos outroEndPosition = pDeck->outroEndPosition();
    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();

    const trackDuration = framePositionToSeconds(trackEndPosition);

    double m_trackIntroStartRel = 0.0;
    double m_trackIntroEndRel = kTransitionFadeTime / trackDuration;
    double m_trackOutroStartRel = 0.0;
    double m_trackOutroEndRel = 1.0;

    if (introStartPosition.isValid()) {
        m_trackIntroStartRel = introStartPosition / trackEndPosition;
    }
    if (introEndPosition.isValid()) {
        m_trackIntroEndRel = introEndPosition / trackEndPosition;
        if (!introStartPosition.isValid()) {
            qWarning() << "Intro end marker is used without intro start marker";
            // check if fading should start at 0.0 or later
            if (framePositionToSeconds(introEndPosition) > kTransitionFadeTime) {
                m_trackIntroStartRel =
                        m_trackIntroEndRel - kTransitionFadeTime / trackDuration;
            }
        }
    }
    if (outroEndPosition.isValid()) {
        m_trackOutroEndRel = outroEndPosition / trackEndPosition;
    }
    if (outroStartPosition.isValid()) {
        m_trackOutroEndRel = outroEndPosition / trackEndPosition;
    }

    const double trackDuration = getEndSecond(pDeck);
    const double introStartSecond = getIntroStartSecond(pDeck);
    const double introEndSecond = getIntroEndSecond(pDeck);
    const double outroStartSecond = getOutroStartSecond(pDeck);
    const double outroEndSecond = getOutroEndSecond(pDeck);
}
