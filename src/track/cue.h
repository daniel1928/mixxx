#ifndef MIXXX_CUE_H
#define MIXXX_CUE_H

#include <QColor>
#include <QMutex>
#include <QObject>

#include "track/cueinfo.h"
#include "track/trackid.h"
#include "util/audiosignal.h"
#include "util/color/predefinedcolor.h"
#include "util/memory.h"

class CuePosition;
class CueDAO;
class Track;

class Cue : public QObject {
    Q_OBJECT

  public:
    static constexpr double kNoPosition = -1.0;
    static constexpr int kNoHotCue = -1;

    ~Cue() override = default;

    bool isDirty() const;
    int getId() const;
    TrackId getTrackId() const;

    mixxx::CueType getType() const;
    void setType(mixxx::CueType type);

    double getPosition() const;
    void setStartPosition(double samplePosition);
    void setEndPosition(double samplePosition);

    double getLength() const;

    int getHotCue() const;
    void setHotCue(int hotCue);

    QString getLabel() const;
    void setLabel(QString label);

    PredefinedColorPointer getColor() const;
    void setColor(PredefinedColorPointer color);

    double getEndPosition() const;

  signals:
    void updated();

  private:
    explicit Cue(TrackId trackId);
    explicit Cue(TrackId trackId, mixxx::AudioSignal::SampleRate sampleRate, const mixxx::CueInfo& cueInfo);
    Cue(int id, TrackId trackId, mixxx::CueType type, double position, double length, int hotCue, QString label, PredefinedColorPointer color);
    void setDirty(bool dirty);
    void setId(int id);
    void setTrackId(TrackId trackId);

    mutable QMutex m_mutex;

    bool m_bDirty;
    int m_iId;
    TrackId m_trackId;
    mixxx::CueType m_type;
    double m_sampleStartPosition;
    double m_sampleEndPosition;
    int m_iHotCue;
    QString m_label;
    PredefinedColorPointer m_color;

    friend class Track;
    friend class CueDAO;
};

class CuePointer: public std::shared_ptr<Cue> {
  public:
    CuePointer() = default;
    explicit CuePointer(Cue* pCue)
          : std::shared_ptr<Cue>(pCue, deleteLater) {
    }

  private:
    static void deleteLater(Cue* pCue);
};

class CuePosition {
  public:
    CuePosition()
        : m_position(0.0) {}
    CuePosition(double position)
        : m_position(position) {}

    double getPosition() const {
        return m_position;
    }

    void setPosition(double position) {
        m_position = position;
    }

    void set(double position) {
        m_position = position;
    }

    void reset() {
        m_position = 0.0;
    }

  private:
    double m_position;
};

bool operator==(const CuePosition& lhs, const CuePosition& rhs);

inline
bool operator!=(const CuePosition& lhs, const CuePosition& rhs) {
    return !(lhs == rhs);
}

inline
QDebug operator<<(QDebug dbg, const CuePosition& arg) {
    return dbg << "position =" << arg.getPosition();
}

#endif // MIXXX_CUE_H
