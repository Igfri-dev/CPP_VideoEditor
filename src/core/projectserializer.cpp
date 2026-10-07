#include "projectserializer.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>

QString ProjectSerializer::toPortableRelativePath(const QString &targetPath, const QString &baseDir)
{
    if (targetPath.isEmpty()) return QString();
    QDir dir(baseDir);
    QString rel = dir.relativeFilePath(targetPath);
    return QDir::fromNativeSeparators(rel);
}

QString ProjectSerializer::resolvePortablePath(const QString &savedPath, const QString &baseDir)
{
    if (savedPath.isEmpty()) return QString();

    // 1. Direct absolute or relative existence check
    QFileInfo fi(savedPath);
    if (fi.exists() && fi.isFile()) {
        return fi.absoluteFilePath();
    }

    // 2. Relative to base project directory
    QDir base(baseDir);
    QString candidate = base.absoluteFilePath(savedPath);
    if (QFileInfo::exists(candidate)) {
        return QFileInfo(candidate).absoluteFilePath();
    }

    // 3. Just the file name in base project directory or subdirectories
    QString fileName = fi.fileName();
    if (!fileName.isEmpty()) {
        QString subMedia = base.absoluteFilePath(QString("media/%1").arg(fileName));
        if (QFileInfo::exists(subMedia)) return subMedia;

        QString subAssets = base.absoluteFilePath(QString("sample_assets/%1").arg(fileName));
        if (QFileInfo::exists(subAssets)) return subAssets;

        QString inBase = base.absoluteFilePath(fileName);
        if (QFileInfo::exists(inBase)) return inBase;
    }

    // Fallback to original saved path
    return savedPath;
}

QJsonObject ProjectSerializer::serializeColorAdjustments(const ColorAdjustments &adj)
{
    QJsonObject obj;
    obj["mode"] = (adj.mode == ColorGradeMode::Sliders) ? "Sliders" : "Curves";
    obj["brightness"] = adj.brightness;
    obj["luminosity"] = adj.luminosity;
    obj["red"] = adj.red;
    obj["green"] = adj.green;
    obj["blue"] = adj.blue;
    obj["timeOfDayEnabled"] = adj.timeOfDayEnabled;
    obj["timeOfDay"] = static_cast<double>(adj.timeOfDay);

    auto serializeCurve = [](const ColorCurve &c) -> QJsonArray {
        QJsonArray arr;
        for (const ColorCurvePoint &pt : c.points()) {
            QJsonObject pObj;
            pObj["x"] = pt.x;
            pObj["y"] = pt.y;
            arr.append(pObj);
        }
        return arr;
    };

    obj["lumaCurve"] = serializeCurve(adj.lumaCurve);
    obj["colorCurve"] = serializeCurve(adj.colorCurve);
    obj["redCurve"] = serializeCurve(adj.redCurve);
    obj["greenCurve"] = serializeCurve(adj.greenCurve);
    obj["blueCurve"] = serializeCurve(adj.blueCurve);
    return obj;
}

ColorAdjustments ProjectSerializer::deserializeColorAdjustments(const QJsonObject &obj)
{
    ColorAdjustments adj;
    if (obj.isEmpty()) return adj;

    QString modeStr = obj.value("mode").toString();
    adj.mode = (modeStr == "Curves") ? ColorGradeMode::Curves : ColorGradeMode::Sliders;
    adj.brightness = obj.value("brightness").toInt(0);
    adj.luminosity = obj.value("luminosity").toInt(0);
    adj.red = obj.value("red").toInt(0);
    adj.green = obj.value("green").toInt(0);
    adj.timeOfDayEnabled = obj.value("timeOfDayEnabled").toBool(false);
    float readTod = static_cast<float>(obj.value("timeOfDay").toDouble(0.60));
    // Backward compatibility: legacy Day default of 0.66 maps smoothly to new neutral Noon 0.60
    if (std::abs(readTod - 0.66f) < 0.005f) {
        readTod = 0.60f;
    }
    adj.timeOfDay = readTod;

    auto deserializeCurve = [](const QJsonArray &arr, CurveType type) -> ColorCurve {
        ColorCurve c(type);
        if (arr.isEmpty()) return c;
        QVector<ColorCurvePoint> pts;
        for (const QJsonValue &v : arr) {
            QJsonObject pObj = v.toObject();
            pts.append(ColorCurvePoint(pObj.value("x").toDouble(), pObj.value("y").toDouble()));
        }
        if (!pts.isEmpty()) {
            c.clearAndSetPoints(pts);
        }
        return c;
    };

    if (obj.contains("lumaCurve")) adj.lumaCurve = deserializeCurve(obj.value("lumaCurve").toArray(), CurveType::Luma);
    if (obj.contains("colorCurve")) adj.colorCurve = deserializeCurve(obj.value("colorCurve").toArray(), CurveType::ColorSpectrum);
    if (obj.contains("redCurve")) adj.redCurve = deserializeCurve(obj.value("redCurve").toArray(), CurveType::Red);
    if (obj.contains("greenCurve")) adj.greenCurve = deserializeCurve(obj.value("greenCurve").toArray(), CurveType::Green);
    if (obj.contains("blueCurve")) adj.blueCurve = deserializeCurve(obj.value("blueCurve").toArray(), CurveType::Blue);

    return adj;
}

QJsonObject ProjectSerializer::serializeMotionPath(const MotionPath &path)
{
    QJsonObject obj;
    obj["preset"] = motionPresetToString(path.preset());
    obj["easing"] = motionEasingToString(path.easing());

    QJsonArray wpArr;
    for (int i = 0; i < path.waypointCount(); ++i) {
        MotionWaypoint wp = path.waypoint(i);
        QJsonObject wObj;
        wObj["x"] = wp.pos.x();
        wObj["y"] = wp.pos.y();
        wObj["curved"] = wp.isCurved;
        wObj["hxIn"] = wp.handleIn.x();
        wObj["hyIn"] = wp.handleIn.y();
        wObj["hxOut"] = wp.handleOut.x();
        wObj["hyOut"] = wp.handleOut.y();
        wpArr.append(wObj);
    }
    obj["waypoints"] = wpArr;
    return obj;
}

MotionPath ProjectSerializer::deserializeMotionPath(const QJsonObject &obj)
{
    MotionPath path;
    if (obj.isEmpty()) return path;

    path.setPreset(stringToMotionPreset(obj.value("preset").toString()));
    path.setEasing(stringToMotionEasing(obj.value("easing").toString()));

    QJsonArray wpArr = obj.value("waypoints").toArray();
    QVector<MotionWaypoint> waypoints;
    for (const QJsonValue &v : wpArr) {
        QJsonObject wObj = v.toObject();
        MotionWaypoint wp;
        wp.pos = QPointF(wObj.value("x").toDouble(), wObj.value("y").toDouble());
        wp.isCurved = wObj.value("curved").toBool(false);
        wp.handleIn = QPointF(wObj.value("hxIn").toDouble(), wObj.value("hyIn").toDouble());
        wp.handleOut = QPointF(wObj.value("hxOut").toDouble(), wObj.value("hyOut").toDouble());
        waypoints.append(wp);
    }
    if (!waypoints.isEmpty()) {
        path.setWaypoints(waypoints);
    }
    return path;
}

QJsonObject ProjectSerializer::serializeClip(const TimelineClip &clip, const QString &projectDir)
{
    QJsonObject obj;
    obj["id"] = static_cast<qint64>(clip.id());
    obj["trackId"] = static_cast<qint64>(clip.trackId());
    obj["name"] = clip.name();
    obj["filePath"] = clip.filePath();
    obj["relativePath"] = toPortableRelativePath(clip.filePath(), projectDir);
    obj["type"] = clipTypeToString(clip.type());

    obj["timelineInMs"] = static_cast<qint64>(clip.timelineInMs());
    obj["timelineOutMs"] = static_cast<qint64>(clip.timelineOutMs());
    obj["sourceInMs"] = static_cast<qint64>(clip.sourceInMs());
    obj["sourceOutMs"] = static_cast<qint64>(clip.sourceOutMs());
    obj["sourceDurationMs"] = static_cast<qint64>(clip.sourceDurationMs());

    obj["speed"] = clip.speed();
    obj["volume"] = clip.volume();
    obj["opacity"] = clip.opacity();
    obj["fadeInMs"] = static_cast<qint64>(clip.fadeInMs());
    obj["fadeOutMs"] = static_cast<qint64>(clip.fadeOutMs());
    obj["transitionIn"] = transitionTypeToString(clip.transitionIn());
    obj["transitionInDurationMs"] = static_cast<qint64>(clip.transitionInDurationMs());
    obj["transitionOut"] = transitionTypeToString(clip.transitionOut());
    obj["transitionOutDurationMs"] = static_cast<qint64>(clip.transitionOutDurationMs());

    obj["linkedClipId"] = static_cast<qint64>(clip.linkedClipId());
    obj["color"] = clip.color().name(QColor::HexArgb);
    obj["isAudioMuted"] = clip.isAudioMuted();

    // Transforms
    obj["posX"] = clip.posX();
    obj["posY"] = clip.posY();
    obj["scaleX"] = clip.scaleX();
    obj["scaleY"] = clip.scaleY();
    obj["rotation"] = clip.rotation();
    obj["scaleMode"] = clipScaleModeToString(clip.scaleMode());

    // Filters
    QJsonArray filterArr;
    for (VisualFilter f : clip.filterStack()) {
        filterArr.append(visualFilterToString(f));
    }
    obj["filters"] = filterArr;

    // Color Adjustments
    obj["colorAdjustments"] = serializeColorAdjustments(clip.colorAdjustments());

    // Motion Path
    if (clip.hasMotionPath()) {
        obj["motionPath"] = serializeMotionPath(clip.motionPath());
    }

    // Text Clip properties
    if (clip.type() == ClipType::Text) {
        QJsonObject tObj;
        tObj["textContent"] = clip.textContent();
        tObj["richTextHtml"] = clip.richTextHtml();
        tObj["fontFamily"] = clip.fontFamily();
        tObj["fontSize"] = clip.fontSize();
        tObj["isBold"] = clip.isBold();
        tObj["isItalic"] = clip.isItalic();
        tObj["isUnderline"] = clip.isUnderline();
        tObj["textColor"] = clip.textColor().name(QColor::HexArgb);
        tObj["backgroundColor"] = clip.backgroundColor().name(QColor::HexArgb);
        tObj["textAlignment"] = clip.textAlignment();
        tObj["textBoxWidth"] = clip.textBoxWidth();
        tObj["textBoxHeight"] = clip.textBoxHeight();
        obj["text"] = tObj;
    }

    return obj;
}

TimelineClip ProjectSerializer::deserializeClip(const QJsonObject &obj, const QString &projectDir)
{
    TimelineClip clip;
    clip.setId(obj.value("id").toVariant().toLongLong());
    clip.setTrackId(obj.value("trackId").toVariant().toLongLong());
    clip.setName(obj.value("name").toString());

    QString rawPath = obj.value("filePath").toString();
    QString relPath = obj.value("relativePath").toString();
    QString resolved = resolvePortablePath(!relPath.isEmpty() ? relPath : rawPath, projectDir);
    clip.setFilePath(resolved);

    QString typeStr = obj.value("type").toString();
    if (typeStr == "Audio") clip.setType(ClipType::Audio);
    else if (typeStr == "Image") clip.setType(ClipType::Image);
    else if (typeStr == "Text") clip.setType(ClipType::Text);
    else clip.setType(ClipType::Video);

    clip.setTimelineInMs(obj.value("timelineInMs").toVariant().toLongLong());
    clip.setTimelineOutMs(obj.value("timelineOutMs").toVariant().toLongLong());
    clip.setSourceInMs(obj.value("sourceInMs").toVariant().toLongLong());
    clip.setSourceOutMs(obj.value("sourceOutMs").toVariant().toLongLong());
    clip.setSourceDurationMs(obj.value("sourceDurationMs").toVariant().toLongLong());

    clip.setSpeed(obj.value("speed").toDouble(1.0));
    clip.setVolume(obj.value("volume").toDouble(1.0));
    clip.setOpacity(obj.value("opacity").toDouble(1.0));
    clip.setFadeInMs(obj.value("fadeInMs").toVariant().toLongLong());
    clip.setFadeOutMs(obj.value("fadeOutMs").toVariant().toLongLong());
    clip.setTransitionIn(stringToTransitionType(obj.value("transitionIn").toString()),
                         obj.value("transitionInDurationMs").toVariant().toLongLong());
    clip.setTransitionOut(stringToTransitionType(obj.value("transitionOut").toString()),
                          obj.value("transitionOutDurationMs").toVariant().toLongLong());

    clip.setLinkedClipId(obj.value("linkedClipId").toVariant().toLongLong());
    if (obj.contains("color")) clip.setColor(QColor(obj.value("color").toString()));
    clip.setAudioMuted(obj.value("isAudioMuted").toBool(false));

    clip.setPosX(obj.value("posX").toDouble(0.0));
    clip.setPosY(obj.value("posY").toDouble(0.0));
    clip.setScaleX(obj.value("scaleX").toDouble(1.0));
    clip.setScaleY(obj.value("scaleY").toDouble(1.0));
    clip.setRotation(obj.value("rotation").toDouble(0.0));
    if (obj.contains("scaleMode")) {
        clip.setScaleMode(stringToClipScaleMode(obj.value("scaleMode").toString()));
    } else {
        clip.setScaleMode(ClipScaleMode::FillCrop);
    }

    // Filters
    QJsonArray filterArr = obj.value("filters").toArray();
    QVector<VisualFilter> stack;
    for (const QJsonValue &v : filterArr) {
        stack.append(stringToVisualFilter(v.toString()));
    }
    clip.setFilterStack(stack);

    // Color Adjustments
    if (obj.contains("colorAdjustments")) {
        clip.setColorAdjustments(deserializeColorAdjustments(obj.value("colorAdjustments").toObject()));
    }

    // Motion Path
    if (obj.contains("motionPath")) {
        clip.setMotionPath(deserializeMotionPath(obj.value("motionPath").toObject()));
    }

    // Text Clip
    if (obj.contains("text")) {
        QJsonObject tObj = obj.value("text").toObject();
        clip.setTextContent(tObj.value("textContent").toString());
        clip.setRichTextHtml(tObj.value("richTextHtml").toString());
        clip.setFontFamily(tObj.value("fontFamily").toString("Arial"));
        clip.setFontSize(tObj.value("fontSize").toInt(48));
        clip.setBold(tObj.value("isBold").toBool(false));
        clip.setItalic(tObj.value("isItalic").toBool(false));
        clip.setUnderline(tObj.value("isUnderline").toBool(false));
        if (tObj.contains("textColor")) clip.setTextColor(QColor(tObj.value("textColor").toString()));
        if (tObj.contains("backgroundColor")) clip.setBackgroundColor(QColor(tObj.value("backgroundColor").toString()));
        clip.setTextAlignment(tObj.value("textAlignment").toInt(0x0004 | 0x0080));
        clip.setTextBoxWidth(tObj.value("textBoxWidth").toInt(0));
        clip.setTextBoxHeight(tObj.value("textBoxHeight").toInt(0));
    }

    return clip;
}

bool ProjectSerializer::saveProject(const QString &filePath, const TimelineModel *model, const QStringList &mediaFiles)
{
    if (!model || filePath.isEmpty()) return false;

    QString projectDir = QFileInfo(filePath).absolutePath();

    QJsonObject root;
    root["format"] = "CPP_VideoEditor_Project";
    root["version"] = 1;

    // Project metadata
    QJsonObject projMeta;
    projMeta["name"] = QFileInfo(filePath).completeBaseName();
    projMeta["aspectRatio"] = projectAspectRatioToString(model->aspectRatio());
    projMeta["canvasWidth"] = model->canvasSize().width();
    projMeta["canvasHeight"] = model->canvasSize().height();
    projMeta["customDurationMs"] = static_cast<qint64>(model->customDurationMs());
    projMeta["totalDurationMs"] = static_cast<qint64>(model->totalDurationMs());
    projMeta["globalColorAdjustments"] = serializeColorAdjustments(model->globalColorAdjustments());
    root["project"] = projMeta;

    // Media library files
    QJsonArray mediaArr;
    for (const QString &mPath : mediaFiles) {
        QJsonObject mObj;
        mObj["absolutePath"] = mPath;
        mObj["relativePath"] = toPortableRelativePath(mPath, projectDir);
        mObj["fileName"] = QFileInfo(mPath).fileName();
        mediaArr.append(mObj);
    }
    root["mediaLibrary"] = mediaArr;

    // Markers
    QJsonArray markerArr;
    for (const TimelineMarker &m : model->markers()) {
        QJsonObject mObj;
        mObj["id"] = static_cast<qint64>(m.id);
        mObj["timeMs"] = static_cast<qint64>(m.timeMs);
        mObj["name"] = m.name;
        mObj["comment"] = m.comment;
        mObj["color"] = m.color.name(QColor::HexArgb);
        markerArr.append(mObj);
    }
    root["markers"] = markerArr;

    // Tracks & Clips
    auto serializeTracks = [&](const QVector<TimelineTrack> &tracks) -> QJsonArray {
        QJsonArray tArr;
        for (const TimelineTrack &t : tracks) {
            QJsonObject tObj;
            tObj["id"] = static_cast<qint64>(t.id());
            tObj["name"] = t.name();
            tObj["type"] = (t.type() == TrackType::Video) ? "Video" : "Audio";
            tObj["isVisible"] = t.isVisible();
            tObj["isMuted"] = t.isMuted();
            tObj["isSolo"] = t.isSolo();
            tObj["isLocked"] = t.isLocked();
            tObj["volume"] = t.volume();

            QJsonArray clipsArr;
            for (const TimelineClip &c : t.clips()) {
                clipsArr.append(serializeClip(c, projectDir));
            }
            tObj["clips"] = clipsArr;
            tArr.append(tObj);
        }
        return tArr;
    };

    root["videoTracks"] = serializeTracks(model->videoTracks());
    root["audioTracks"] = serializeTracks(model->audioTracks());

    // Write file safely with QSaveFile to prevent corruption during writes
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    return file.commit();
}

bool ProjectSerializer::loadProject(const QString &filePath, TimelineModel *model, QStringList &outMediaFiles)
{
    if (!model || filePath.isEmpty()) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();
    QString projectDir = QFileInfo(filePath).absolutePath();

    model->resetProject();
    model->videoTracks().clear();
    model->audioTracks().clear();

    // 1. Project metadata
    if (root.contains("project")) {
        QJsonObject projMeta = root.value("project").toObject();
        QString arStr = projMeta.value("aspectRatio").toString("16:9");
        model->setAspectRatio(stringToProjectAspectRatio(arStr), false);

        qint64 cDur = projMeta.value("customDurationMs").toVariant().toLongLong();
        if (cDur > 0) {
            model->setCustomDurationMs(cDur);
        }

        if (projMeta.contains("globalColorAdjustments")) {
            model->setGlobalColorAdjustments(deserializeColorAdjustments(projMeta.value("globalColorAdjustments").toObject()), false);
        }
    }

    // 2. Media library
    outMediaFiles.clear();
    QJsonArray mediaArr = root.value("mediaLibrary").toArray();
    for (const QJsonValue &v : mediaArr) {
        QJsonObject mObj = v.toObject();
        QString rel = mObj.value("relativePath").toString();
        QString abs = mObj.value("absolutePath").toString();
        QString resolved = resolvePortablePath(!rel.isEmpty() ? rel : abs, projectDir);
        if (!resolved.isEmpty() && QFileInfo::exists(resolved)) {
            outMediaFiles.append(resolved);
        }
    }

    // 3. Markers
    QVector<TimelineMarker> markers;
    QJsonArray markerArr = root.value("markers").toArray();
    for (const QJsonValue &v : markerArr) {
        QJsonObject mObj = v.toObject();
        TimelineMarker m;
        m.id = mObj.value("id").toVariant().toLongLong();
        m.timeMs = mObj.value("timeMs").toVariant().toLongLong();
        m.name = mObj.value("name").toString();
        m.comment = mObj.value("comment").toString();
        if (mObj.contains("color")) m.color = QColor(mObj.value("color").toString());
        markers.append(m);
    }
    model->setMarkers(markers);

    // 4. Tracks & Clips
    auto deserializeTracks = [&](const QJsonArray &tArr, TrackType type) {
        for (const QJsonValue &tv : tArr) {
            QJsonObject tObj = tv.toObject();
            qint64 trackId = tObj.value("id").toVariant().toLongLong();
            QString name = tObj.value("name").toString();
            TimelineTrack track(trackId, name, type);
            track.setVisible(tObj.value("isVisible").toBool(true));
            track.setMuted(tObj.value("isMuted").toBool(false));
            track.setSolo(tObj.value("isSolo").toBool(false));
            track.setLocked(tObj.value("isLocked").toBool(false));
            track.setVolume(tObj.value("volume").toDouble(1.0));

            QJsonArray clipsArr = tObj.value("clips").toArray();
            for (const QJsonValue &cv : clipsArr) {
                TimelineClip c = deserializeClip(cv.toObject(), projectDir);
                track.addClip(c);
            }

            if (type == TrackType::Video) {
                model->videoTracks().append(track);
            } else {
                model->audioTracks().append(track);
            }
        }
    };

    deserializeTracks(root.value("videoTracks").toArray(), TrackType::Video);
    deserializeTracks(root.value("audioTracks").toArray(), TrackType::Audio);

    // If project had no tracks, provide standard defaults
    if (model->videoTracks().isEmpty()) {
        model->addTrack(TrackType::Video, "V1");
    }
    if (model->audioTracks().isEmpty()) {
        model->addTrack(TrackType::Audio, "A1");
    }

    model->clearHistory();
    model->notifyChange();
    return true;
}
