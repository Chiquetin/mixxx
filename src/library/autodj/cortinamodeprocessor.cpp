/*
 * The AutoDJ Processor has two main functions
 * - Managing the playlist used to supply tracks for the AutoDJ
 * - Managing the transitions between tracks when AutoDJ is active
 */

#include <QScopedPointer>

class TransitionManager;
class IdleModeProcessor;
class CortinaModeProcessor;
class FullOutroIntroProcessor;

class TransitionManagerContext : public QObject {
    enum TransitionMode {
        ADJ_DISABLED,
        ADJ_FULL_INTRO_OUTRO_FADE,
        ADJ_CORTINA_MODE
    };

    enum AutoDJState {
        ADJ_LEFT_DECK_FADE_IN,
        ADJ_LEFT_DECK_PLAYING,
        ADJ_LEFT_DECK_FADE_OUT,
        ADJ_LEFT_DECK_XFADING,
        ADJ_RIGHT_DECK_FADE_IN,
        ADJ_RIGHT_DECK_PLAYING,
        ADJ_RIGHT_DECK_FADE_OUT,
        ADJ_RIGHT_DECK_XFADING
    };

  private:
    QScopedPointer<TransitionManager> m_pTransitionManager;
    int currentTransitionMode;

    // Manage state transitions
    QScopedPointer<TransitionManager> changeState(
            TransitionMode newTransitionMode) {
        switch (currentStateIndex) {
        case TransitionMode::ADJ_IDLE:
            return QScopedPointer<IdleModeProcessor>.create();
        case TransitionMode::ADJ_CORTINA_MODE:
            return QScopedPointer<CortinaModeProcessor>.create();
        case TransitionMode::ADJ_FULL_INTRO_OUTRO_FADE:
            return QScopedPointer<FullOutroIntroProcessor>.create();
        }
    }

  public:
    TransitionManagerContext()
            : currentTransitionMode(TransitionMode::ADJ_DISABLED) {
        m_pTransitionManager = QScopedPointer<IdleModeProcessor>.create();
    }
}

/*
 * Transition manager is the base class for any mode of the AutoDJ
 * it provides the interface to the AutoDJProcessor
 */
class TransitionManager : public QObject {
    void calculateTransition();

    // Helper functions
    double getFirstSoundSecond(DeckAttributes* pDeck);
    double getIntroStartSecond(DeckAttributes* pDeck);

    m_channelFader;
    m_pCOCrossfader;
}

class CortinaModeProcessor : TransitionManager {
    m_xFadeTransitionProgress; // is used to measure transition progress
    m_xFadeTransitionStart;    // play position at which fromDeck starts x-fading

    void updateTransitionMarkers(DeckAttributes* pFromDeck, DeckAttributes* pToDeck);
    void updateTrackMarkers(DeckAttributes* pDeck);
}

double
TransitionManager::getFirstSoundSecond(
        DeckAttributes* pDeck) {
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

/*
 * updateTransitionMarkers updates all markers used to manage the transition
 */
void CortinaModeProcessor::updateTransitionMarkers(
        DeckAttributes* pFromDeck, DeckAttributes* pToDeck) {
}

/*
 * updateTrackMarkers updates the markers used to control the cross fader and
 * channel volume faders during fade-in / fade-out / fade-over
 */
void CortinaModeProcessor::updateTrackMarkers(DeckAttributes* pDeck) {
    // calculates the effective start and end of the intro and outro
    // based on the rules for the transition

    const mixxx::audio::FramePos trackEndPosition = pDeck->trackEndPosition();
    const trackDuration = framePositionToSeconds(trackEndPosition, pDeck);

    // assign default behaviour for fade-in / fade-out
    double m_trackIntroStartRel = 0.0;
    double m_trackIntroEndRel = kTrackFadeInTime / trackDuration;
    double m_trackOutroStartRel = 1.0 - kTrackFadeOutTime / trackDuration;
    double m_trackOutroEndRel = 1.0;

    // modify track markers when intro markers have been set
    // The advantage of using the FramePos values here is that the validity can be checked
    const mixxx::audio::FramePos introStartPosition = pDeck->introStartPosition();
    const mixxx::audio::FramePos introEndPosition = pDeck->introEndPosition();
    if (introStartPosition.isValid()) {
        m_trackIntroStartRel = introStartPosition / trackEndPosition;
    }
    if (introEndPosition.isValid()) {
        m_trackIntroEndRel = introEndPosition / trackEndPosition;
        if (!introStartPosition.isValid()) {
            qWarning() << "No intro-start cue set: fade out to end of track";
            // check if fading should start at 0.0 or later
            const double introEndSecond = framePositionToSeconds(introEndPosition, pDeck);
            if (introEndSecond > kTransitionFadeTime) {
                m_trackIntroStartRel =
                        m_trackIntroEndRel - kTransitionFadeTime / trackDuration;
            }
        }
    }

    // modify track marker when outro markers have been set
    // The advantage of using the FramePos values here is that the validity can be checked
    const mixxx::audio::FramePos outroStartPosition = pDeck->outroStartPosition();
    const mixxx::audio::FramePos outroEndPosition = pDeck->outroEndPosition();
    if (outroStartPosition.isValid()) {
        m_trackOutroStartRel = outroStartPosition / trackEndPosition;
    }
    if (outroEndPosition.isValid()) {
        m_trackOutroEndRel = outroEndPosition / trackEndPosition;
    } else if (outroStartPosition.isValid()) {
        qWarning() << "No outro-end cue set: fade out to end of track";
    }
}

class PlaylistManager : public QObject {
}
