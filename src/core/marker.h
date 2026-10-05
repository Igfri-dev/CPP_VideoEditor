#pragma once

#include <QString>
#include <QColor>
#include <QtGlobal>

struct TimelineMarker {
    qint64 id = 0;
    qint64 timeMs = 0;
    QString name = "Marcador";
    QString comment;
    QColor color = QColor("#58a6ff");

    bool operator==(const TimelineMarker &o) const {
        return id == o.id && timeMs == o.timeMs && name == o.name && comment == o.comment && color == o.color;
    }
    bool operator!=(const TimelineMarker &o) const {
        return !(*this == o);
    }
};
