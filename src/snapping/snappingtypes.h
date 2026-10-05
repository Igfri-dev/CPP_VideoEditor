#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <QString>
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Snapping {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float _x, float _y) : x(_x), y(_y) {}
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    Rect() = default;
    Rect(float _x, float _y, float _w, float _h) : x(_x), y(_y), width(_w), height(_h) {}

    float left() const { return x; }
    float right() const { return x + width; }
    float top() const { return y; }
    float bottom() const { return y + height; }

    float centerX() const { return x + width * 0.5f; }
    float centerY() const { return y + height * 0.5f; }

    void setLeft(float l) { x = l; }
    void setRight(float r) { x = r - width; }
    void setTop(float t) { y = t; }
    void setBottom(float b) { y = b - height; }
    void setCenterX(float cx) { x = cx - width * 0.5f; }
    void setCenterY(float cy) { y = cy - height * 0.5f; }
};

struct RotatedRect {
    float centerX = 0.0f;
    float centerY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float rotation = 0.0f; // degrees

    RotatedRect() = default;
    RotatedRect(float cx, float cy, float w, float h, float rot = 0.0f)
        : centerX(cx), centerY(cy), width(w), height(h), rotation(rot) {}

    RotatedRect(const Rect &rect)
        : centerX(rect.centerX()), centerY(rect.centerY()), width(rect.width), height(rect.height), rotation(0.0f) {}

    Rect boundingRect() const {
        if (std::abs(rotation) < 0.001f) {
            return Rect(centerX - width * 0.5f, centerY - height * 0.5f, width, height);
        }
        double rad = rotation * (M_PI / 180.0);
        double cosR = std::abs(std::cos(rad));
        double sinR = std::abs(std::sin(rad));
        float bbW = static_cast<float>(width * cosR + height * sinR);
        float bbH = static_cast<float>(width * sinR + height * cosR);
        return Rect(centerX - bbW * 0.5f, centerY - bbH * 0.5f, bbW, bbH);
    }

    std::vector<Vec2> corners() const {
        double rad = rotation * (M_PI / 180.0);
        double cosR = std::cos(rad);
        double sinR = std::sin(rad);
        float hw = width * 0.5f;
        float hh = height * 0.5f;
        auto rot = [&](float lx, float ly) -> Vec2 {
            return Vec2(
                centerX + static_cast<float>(lx * cosR - ly * sinR),
                centerY + static_cast<float>(lx * sinR + ly * cosR)
            );
        };
        return {
            rot(-hw, -hh), // TL
            rot( hw, -hh), // TR
            rot( hw,  hh), // BR
            rot(-hw,  hh)  // BL
        };
    }
};

enum class SnapType {
    CanvasCenter,
    CanvasEdge,
    ObjectCenter,
    ObjectEdge,
    EqualSpacing,
    SameSize,
    SafeArea,
    CustomGuide,
    Rotation
};

enum class GuideOrientation {
    Horizontal,
    Vertical
};

struct SmartGuide {
    GuideOrientation orientation = GuideOrientation::Vertical;
    float position = 0.0f; // Canvas coordinate
    SnapType reason = SnapType::CanvasCenter;
    QString label;
};

struct SnapCandidate {
    float position = 0.0f;
    float distance = 0.0f;
    int priority = 0; // lower number = higher priority
    SnapType type = SnapType::CanvasCenter;
    QString label;
};

struct SnapResult {
    Vec2 position; // Snapped top-left of bounding box
    Vec2 snappedCenter; // Snapped center of the element
    bool snappedX = false;
    bool snappedY = false;
    std::vector<SmartGuide> guides;
};

struct RotationSnapResult {
    double snappedAngle = 0.0;
    bool snapped = false;
    double snapTarget = 0.0;
    QString label;
};

struct ResizeSnapResult {
    Rect rect;
    bool snappedW = false;
    bool snappedH = false;
    std::vector<SmartGuide> guides;
};

struct SnapSettings {
    bool enabled = true;
    bool canvasCenter = true;
    bool canvasEdges = true;
    bool objectCenters = true;
    bool objectEdges = true;
    bool equalSpacing = true;
    bool sameSize = true;
    bool safeAreas = true;
    bool snapRotation = true;
    float thresholdScreenPx = 8.0f;
    float rotationThresholdDeg = 4.0f;
};

enum class TimelineSnapType {
    None,
    ClipStart,
    ClipEnd,
    Playhead,
    Marker,
    OtherTrackCut,
    Frame
};

struct TimelineSnapCandidate {
    qint64 timeMs = 0;
    qint64 distanceMs = 0;
    int priority = 0;
    TimelineSnapType type = TimelineSnapType::None;
};

struct TimelineSnapResult {
    qint64 timeMs = 0;
    bool snapped = false;
    TimelineSnapType type = TimelineSnapType::None;
};

struct TimelineSnapSettings {
    bool enabled = true;
    bool clipEdges = true;
    bool playhead = true;
    bool otherTrackCuts = true;
    bool frameSnap = true;
    double fps = 30.0;
    double thresholdPixels = 8.0;
};

} // namespace Snapping
