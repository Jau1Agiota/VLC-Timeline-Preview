/* Read-only foreground progress observations, not stall diagnosis or image PTS.
 * GPL-2.0-or-later; see COPYING. GUI thread only. */
#ifndef VLC_QT_PLAYBACKDIAGNOSTICS_HPP
#define VLC_QT_PLAYBACKDIAGNOSTICS_HPP
#include <QString>

namespace PlaybackStability {
struct Sample {
    bool playing = false, available = false;
    qint64 timeMs = -1, decoded = 0, displayed = 0, lost = 0, audio = 0, playedAudio = 0;
};
class Progress {
public:
    void reset() { valid = false; ++generation; }
    QString observe(const Sample &s, qint64 intervalMs) {
        QString observation = QStringLiteral("baseline");
        qint64 decoded = 0, displayed = 0, lost = 0, audio = 0, played = 0;
        if(!s.available) observation = QStringLiteral("stats-unavailable");
        else if(!s.playing) observation = QStringLiteral("not-playing");
        else if(valid && s.timeMs >= previous.timeMs && s.decoded >= previous.decoded &&
                s.displayed >= previous.displayed && s.lost >= previous.lost &&
                s.audio >= previous.audio && s.playedAudio >= previous.playedAudio) {
            decoded = s.decoded - previous.decoded; displayed = s.displayed - previous.displayed;
            lost = s.lost - previous.lost; audio = s.audio - previous.audio;
            played = s.playedAudio - previous.playedAudio;
            observation = displayed > 0 ? QStringLiteral("display-counter-progress") :
                decoded > 0 ? QStringLiteral("decode-progress-no-display") :
                audio > 0 || played > 0 ? QStringLiteral("audio-progress-video-static") :
                QStringLiteral("no-counter-progress");
        }
        else if(valid) observation = QStringLiteral("seek-or-counter-reset");
        previous = s; valid = s.available && s.playing;
        return QStringLiteral("generation=%1 interval_ms=%2 gui_gap=%3 stats=%4 observation=%5 "
            "decoded=%6 displayed=%7 lost=%8 audio_decoded=%9 audio_played=%10 "
            "d_decoded=%11 d_displayed=%12 d_lost=%13 d_audio=%14 d_played=%15")
            .arg(generation).arg(intervalMs).arg(intervalMs > 2500).arg(s.available).arg(observation)
            .arg(s.decoded).arg(s.displayed).arg(s.lost).arg(s.audio).arg(s.playedAudio)
            .arg(decoded).arg(displayed).arg(lost).arg(audio).arg(played);
    }
private:
    bool valid = false;
    quint64 generation = 0;
    Sample previous;
};
}
#endif
