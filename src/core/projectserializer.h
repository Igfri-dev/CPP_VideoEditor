#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include "timelinemodel.h"

class ProjectSerializer {
public:
    static bool saveProject(const QString &filePath, const TimelineModel *model, const QStringList &mediaFiles = QStringList());
    static bool loadProject(const QString &filePath, TimelineModel *model, QStringList &outMediaFiles);

    static QString toPortableRelativePath(const QString &targetPath, const QString &baseDir);
    static QString resolvePortablePath(const QString &savedPath, const QString &baseDir);

    static QJsonObject serializeColorAdjustments(const ColorAdjustments &adj);
    static ColorAdjustments deserializeColorAdjustments(const QJsonObject &obj);

    static QJsonObject serializeMotionPath(const MotionPath &path);
    static MotionPath deserializeMotionPath(const QJsonObject &obj);

private:
    static QJsonObject serializeClip(const TimelineClip &clip, const QString &projectDir);
    static TimelineClip deserializeClip(const QJsonObject &obj, const QString &projectDir);
};
