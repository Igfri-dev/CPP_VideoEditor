#pragma once

#include "snappingtypes.h"
#include <vector>
#include <optional>

namespace Snapping {

class CanvasSnappingEngine {
public:
    CanvasSnappingEngine() = default;

    SnapResult snapMove(
        const RotatedRect &movingElement,
        const Rect &canvasRect,
        const std::vector<RotatedRect> &otherElements,
        const SnapSettings &settings,
        float scaleScreen = 1.0f
    );

    SnapResult snapMove(
        const Rect &movingBounds,
        const Rect &canvasRect,
        const std::vector<Rect> &otherElements,
        const SnapSettings &settings,
        float scaleScreen = 1.0f
    );

    ResizeSnapResult snapResize(
        const Rect &resizingBounds,
        const Rect &canvasRect,
        const std::vector<Rect> &otherElements,
        const SnapSettings &settings,
        float scaleScreen = 1.0f
    );

    RotationSnapResult snapRotation(
        double desiredAngleDeg,
        const SnapSettings &settings,
        bool isShiftPressed = false
    );

    void resetHysteresis();

private:
    struct EvaluatedSnap {
        float delta = 0.0f;
        float guidePos = 0.0f;
        int priority = 100;
        SnapType type = SnapType::CanvasCenter;
        QString label;
    };

    std::optional<EvaluatedSnap> evaluateSnapX(
        const RotatedRect &movingElement,
        const Rect &canvasRect,
        const std::vector<RotatedRect> &otherElements,
        const SnapSettings &settings,
        float threshold
    );

    std::optional<EvaluatedSnap> evaluateSnapY(
        const RotatedRect &movingElement,
        const Rect &canvasRect,
        const std::vector<RotatedRect> &otherElements,
        const SnapSettings &settings,
        float threshold
    );

    bool m_hasActiveSnapX = false;
    float m_activeTargetX = 0.0f;
    bool m_hasActiveSnapY = false;
    float m_activeTargetY = 0.0f;
};

} // namespace Snapping
