#include "canvassnappingengine.h"
#include <cmath>
#include <limits>
#include <vector>

namespace Snapping {

void CanvasSnappingEngine::resetHysteresis()
{
    m_hasActiveSnapX = false;
    m_activeTargetX = 0.0f;
    m_hasActiveSnapY = false;
    m_activeTargetY = 0.0f;
}

std::optional<CanvasSnappingEngine::EvaluatedSnap> CanvasSnappingEngine::evaluateSnapX(
    const RotatedRect &movingElement,
    const Rect &canvasRect,
    const std::vector<RotatedRect> &otherElements,
    const SnapSettings &settings,
    float threshold)
{
    std::vector<EvaluatedSnap> candidates;
    Rect movingBounds = movingElement.boundingRect();
    std::vector<Vec2> movingCorners = movingElement.corners();
    float cx = movingElement.centerX;
    bool isRotated = std::abs(movingElement.rotation) > 0.01f;

    auto addCandidate = [&](float target, float current, int priority, SnapType type, const QString &label) {
        float delta = target - current;
        if (std::abs(delta) <= threshold) {
            candidates.push_back({delta, target, priority, type, label});
        }
    };

    // 1. Canvas Center
    if (settings.canvasCenter) {
        float ccX = canvasRect.centerX();
        addCandidate(ccX, cx, 1, SnapType::CanvasCenter, "Centro lienzo");
        addCandidate(ccX, movingBounds.left(), 3, SnapType::CanvasCenter, "Borde izquierdo al centro");
        addCandidate(ccX, movingBounds.right(), 3, SnapType::CanvasCenter, "Borde derecho al centro");
        if (isRotated) {
            for (const auto &c : movingCorners) {
                addCandidate(ccX, c.x, 3, SnapType::CanvasCenter, "Vértice al centro lienzo");
            }
        }
    }

    // 2. Canvas Edges
    if (settings.canvasEdges) {
        addCandidate(canvasRect.left(), movingBounds.left(), 2, SnapType::CanvasEdge, "Borde izquierdo lienzo");
        addCandidate(canvasRect.right(), movingBounds.right(), 2, SnapType::CanvasEdge, "Borde derecho lienzo");
        if (isRotated) {
            for (const auto &c : movingCorners) {
                addCandidate(canvasRect.left(), c.x, 3, SnapType::CanvasEdge, "Vértice a borde izquierdo");
                addCandidate(canvasRect.right(), c.x, 3, SnapType::CanvasEdge, "Vértice a borde derecho");
            }
        }
    }

    // 3. Safe Areas (Title Safe 90%, Action Safe 95%)
    if (settings.safeAreas) {
        float cw = canvasRect.width;
        float titleSafeL = canvasRect.left() + cw * 0.10f;
        float titleSafeR = canvasRect.right() - cw * 0.10f;
        float actionSafeL = canvasRect.left() + cw * 0.05f;
        float actionSafeR = canvasRect.right() - cw * 0.05f;

        addCandidate(titleSafeL, movingBounds.left(), 5, SnapType::SafeArea, "Title Safe 90%");
        addCandidate(titleSafeR, movingBounds.right(), 5, SnapType::SafeArea, "Title Safe 90%");
        addCandidate(actionSafeL, movingBounds.left(), 5, SnapType::SafeArea, "Action Safe 95%");
        addCandidate(actionSafeR, movingBounds.right(), 5, SnapType::SafeArea, "Action Safe 95%");

        if (isRotated) {
            for (const auto &c : movingCorners) {
                addCandidate(titleSafeL, c.x, 5, SnapType::SafeArea, "Vértice a Title Safe");
                addCandidate(titleSafeR, c.x, 5, SnapType::SafeArea, "Vértice a Title Safe");
                addCandidate(actionSafeL, c.x, 5, SnapType::SafeArea, "Vértice a Action Safe");
                addCandidate(actionSafeR, c.x, 5, SnapType::SafeArea, "Vértice a Action Safe");
            }
        }
    }

    // 4. Other Elements
    for (const RotatedRect &other : otherElements) {
        Rect otherBounds = other.boundingRect();
        auto otherCorners = other.corners();
        bool otherRotated = std::abs(other.rotation) > 0.01f;

        if (settings.objectCenters) {
            addCandidate(other.centerX, cx, 3, SnapType::ObjectCenter, "Centro a centro");
            addCandidate(other.centerX, movingBounds.left(), 5, SnapType::ObjectCenter, "Izquierda a centro");
            addCandidate(other.centerX, movingBounds.right(), 5, SnapType::ObjectCenter, "Derecha a centro");
            addCandidate(otherBounds.left(), cx, 5, SnapType::ObjectEdge, "Centro a izquierda");
            addCandidate(otherBounds.right(), cx, 5, SnapType::ObjectEdge, "Centro a derecha");
        }

        if (settings.objectEdges) {
            addCandidate(otherBounds.left(), movingBounds.left(), 4, SnapType::ObjectEdge, "Alineado a la izquierda");
            addCandidate(otherBounds.right(), movingBounds.right(), 4, SnapType::ObjectEdge, "Alineado a la derecha");
            addCandidate(otherBounds.right(), movingBounds.left(), 4, SnapType::ObjectEdge, "Acoplado lateral");
            addCandidate(otherBounds.left(), movingBounds.right(), 4, SnapType::ObjectEdge, "Acoplado lateral");

            if (isRotated || otherRotated) {
                for (const auto &mc : movingCorners) {
                    addCandidate(other.centerX, mc.x, 5, SnapType::ObjectCenter, "Vértice a centro objeto");
                    addCandidate(otherBounds.left(), mc.x, 5, SnapType::ObjectEdge, "Vértice a borde objeto");
                    addCandidate(otherBounds.right(), mc.x, 5, SnapType::ObjectEdge, "Vértice a borde objeto");
                    for (const auto &oc : otherCorners) {
                        addCandidate(oc.x, mc.x, 5, SnapType::ObjectEdge, "Vértice a vértice");
                    }
                }
            }
        }
    }

    // 5. Equal Spacing (between two existing elements and moving element)
    if (settings.equalSpacing && otherElements.size() >= 2) {
        for (size_t i = 0; i < otherElements.size(); ++i) {
            Rect A_box = otherElements[i].boundingRect();
            for (size_t j = i + 1; j < otherElements.size(); ++j) {
                Rect B_box = otherElements[j].boundingRect();
                const Rect &A = A_box.left() < B_box.left() ? A_box : B_box;
                const Rect &B = A_box.left() < B_box.left() ? B_box : A_box;

                float gapAB = B.left() - A.right();
                if (gapAB > 0.0f) {
                    float targetRightOfB = B.right() + gapAB;
                    addCandidate(targetRightOfB, movingBounds.left(), 6, SnapType::EqualSpacing, "Espaciado uniforme");

                    float targetLeftOfA = A.left() - gapAB - movingBounds.width;
                    addCandidate(targetLeftOfA, movingBounds.left(), 6, SnapType::EqualSpacing, "Espaciado uniforme");
                }
            }
        }
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    // Pick closest, using priority for tie-break
    auto best = candidates[0];
    for (size_t i = 1; i < candidates.size(); ++i) {
        float d1 = std::abs(candidates[i].delta);
        float dBest = std::abs(best.delta);
        if (d1 < dBest - 0.001f) {
            best = candidates[i];
        } else if (std::abs(d1 - dBest) <= 0.001f && candidates[i].priority < best.priority) {
            best = candidates[i];
        }
    }

    return best;
}

std::optional<CanvasSnappingEngine::EvaluatedSnap> CanvasSnappingEngine::evaluateSnapY(
    const RotatedRect &movingElement,
    const Rect &canvasRect,
    const std::vector<RotatedRect> &otherElements,
    const SnapSettings &settings,
    float threshold)
{
    std::vector<EvaluatedSnap> candidates;
    Rect movingBounds = movingElement.boundingRect();
    std::vector<Vec2> movingCorners = movingElement.corners();
    float cy = movingElement.centerY;
    bool isRotated = std::abs(movingElement.rotation) > 0.01f;

    auto addCandidate = [&](float target, float current, int priority, SnapType type, const QString &label) {
        float delta = target - current;
        if (std::abs(delta) <= threshold) {
            candidates.push_back({delta, target, priority, type, label});
        }
    };

    // 1. Canvas Center
    if (settings.canvasCenter) {
        float ccY = canvasRect.centerY();
        addCandidate(ccY, cy, 1, SnapType::CanvasCenter, "Centro lienzo");
        addCandidate(ccY, movingBounds.top(), 3, SnapType::CanvasCenter, "Borde superior al centro");
        addCandidate(ccY, movingBounds.bottom(), 3, SnapType::CanvasCenter, "Borde inferior al centro");
        if (isRotated) {
            for (const auto &c : movingCorners) {
                addCandidate(ccY, c.y, 3, SnapType::CanvasCenter, "Vértice al centro lienzo");
            }
        }
    }

    // 2. Canvas Edges
    if (settings.canvasEdges) {
        addCandidate(canvasRect.top(), movingBounds.top(), 2, SnapType::CanvasEdge, "Borde superior lienzo");
        addCandidate(canvasRect.bottom(), movingBounds.bottom(), 2, SnapType::CanvasEdge, "Borde inferior lienzo");
        if (isRotated) {
            for (const auto &c : movingCorners) {
                addCandidate(canvasRect.top(), c.y, 3, SnapType::CanvasEdge, "Vértice a borde superior");
                addCandidate(canvasRect.bottom(), c.y, 3, SnapType::CanvasEdge, "Vértice a borde inferior");
            }
        }
    }

    // 3. Safe Areas (Title Safe 90%, Action Safe 95%)
    if (settings.safeAreas) {
        float ch = canvasRect.height;
        float titleSafeT = canvasRect.top() + ch * 0.10f;
        float titleSafeB = canvasRect.bottom() - ch * 0.10f;
        float actionSafeT = canvasRect.top() + ch * 0.05f;
        float actionSafeB = canvasRect.bottom() - ch * 0.05f;

        addCandidate(titleSafeT, movingBounds.top(), 5, SnapType::SafeArea, "Title Safe 90%");
        addCandidate(titleSafeB, movingBounds.bottom(), 5, SnapType::SafeArea, "Title Safe 90%");
        addCandidate(actionSafeT, movingBounds.top(), 5, SnapType::SafeArea, "Action Safe 95%");
        addCandidate(actionSafeB, movingBounds.bottom(), 5, SnapType::SafeArea, "Action Safe 95%");

        if (isRotated) {
            for (const auto &c : movingCorners) {
                addCandidate(titleSafeT, c.y, 5, SnapType::SafeArea, "Vértice a Title Safe");
                addCandidate(titleSafeB, c.y, 5, SnapType::SafeArea, "Vértice a Title Safe");
                addCandidate(actionSafeT, c.y, 5, SnapType::SafeArea, "Vértice a Action Safe");
                addCandidate(actionSafeB, c.y, 5, SnapType::SafeArea, "Vértice a Action Safe");
            }
        }
    }

    // 4. Other Elements
    for (const RotatedRect &other : otherElements) {
        Rect otherBounds = other.boundingRect();
        auto otherCorners = other.corners();
        bool otherRotated = std::abs(other.rotation) > 0.01f;

        if (settings.objectCenters) {
            addCandidate(other.centerY, cy, 3, SnapType::ObjectCenter, "Centro a centro");
            addCandidate(other.centerY, movingBounds.top(), 5, SnapType::ObjectCenter, "Superior a centro");
            addCandidate(other.centerY, movingBounds.bottom(), 5, SnapType::ObjectCenter, "Inferior a centro");
            addCandidate(otherBounds.top(), cy, 5, SnapType::ObjectEdge, "Centro a superior");
            addCandidate(otherBounds.bottom(), cy, 5, SnapType::ObjectEdge, "Centro a inferior");
        }

        if (settings.objectEdges) {
            addCandidate(otherBounds.top(), movingBounds.top(), 4, SnapType::ObjectEdge, "Alineado superior");
            addCandidate(otherBounds.bottom(), movingBounds.bottom(), 4, SnapType::ObjectEdge, "Alineado inferior");
            addCandidate(otherBounds.bottom(), movingBounds.top(), 4, SnapType::ObjectEdge, "Acoplado vertical");
            addCandidate(otherBounds.top(), movingBounds.bottom(), 4, SnapType::ObjectEdge, "Acoplado vertical");

            if (isRotated || otherRotated) {
                for (const auto &mc : movingCorners) {
                    addCandidate(other.centerY, mc.y, 5, SnapType::ObjectCenter, "Vértice a centro objeto");
                    addCandidate(otherBounds.top(), mc.y, 5, SnapType::ObjectEdge, "Vértice a borde objeto");
                    addCandidate(otherBounds.bottom(), mc.y, 5, SnapType::ObjectEdge, "Vértice a borde objeto");
                    for (const auto &oc : otherCorners) {
                        addCandidate(oc.y, mc.y, 5, SnapType::ObjectEdge, "Vértice a vértice");
                    }
                }
            }
        }
    }

    // 5. Equal Spacing vertically
    if (settings.equalSpacing && otherElements.size() >= 2) {
        for (size_t i = 0; i < otherElements.size(); ++i) {
            Rect A_box = otherElements[i].boundingRect();
            for (size_t j = i + 1; j < otherElements.size(); ++j) {
                Rect B_box = otherElements[j].boundingRect();
                const Rect &A = A_box.top() < B_box.top() ? A_box : B_box;
                const Rect &B = A_box.top() < B_box.top() ? B_box : A_box;

                float gapAB = B.top() - A.bottom();
                if (gapAB > 0.0f) {
                    float targetBottomB = B.bottom() + gapAB;
                    addCandidate(targetBottomB, movingBounds.top(), 6, SnapType::EqualSpacing, "Espaciado vertical");

                    float targetTopA = A.top() - gapAB - movingBounds.height;
                    addCandidate(targetTopA, movingBounds.top(), 6, SnapType::EqualSpacing, "Espaciado vertical");
                }
            }
        }
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    auto best = candidates[0];
    for (size_t i = 1; i < candidates.size(); ++i) {
        float d1 = std::abs(candidates[i].delta);
        float dBest = std::abs(best.delta);
        if (d1 < dBest - 0.001f) {
            best = candidates[i];
        } else if (std::abs(d1 - dBest) <= 0.001f && candidates[i].priority < best.priority) {
            best = candidates[i];
        }
    }

    return best;
}

SnapResult CanvasSnappingEngine::snapMove(
    const RotatedRect &movingElement,
    const Rect &canvasRect,
    const std::vector<RotatedRect> &otherElements,
    const SnapSettings &settings,
    float scaleScreen)
{
    Rect movingBounds = movingElement.boundingRect();
    SnapResult result;
    result.position = Vec2(movingBounds.x, movingBounds.y);
    result.snappedCenter = Vec2(movingElement.centerX, movingElement.centerY);

    if (!settings.enabled) {
        resetHysteresis();
        return result;
    }

    float scale = (scaleScreen > 0.001f) ? scaleScreen : 1.0f;
    float baseThreshold = settings.thresholdScreenPx / scale;

    // Use hysteresis: if already snapped, threshold to exit is 1.5x larger
    float thresholdX = m_hasActiveSnapX ? (baseThreshold * 1.5f) : baseThreshold;
    float thresholdY = m_hasActiveSnapY ? (baseThreshold * 1.5f) : baseThreshold;

    auto optSnapX = evaluateSnapX(movingElement, canvasRect, otherElements, settings, thresholdX);
    if (optSnapX) {
        result.position.x = movingBounds.x + optSnapX->delta;
        result.snappedCenter.x = movingElement.centerX + optSnapX->delta;
        result.snappedX = true;
        m_hasActiveSnapX = true;
        m_activeTargetX = optSnapX->guidePos;

        SmartGuide g;
        g.orientation = GuideOrientation::Vertical;
        g.position = optSnapX->guidePos;
        g.reason = optSnapX->type;
        g.label = optSnapX->label;
        result.guides.push_back(g);
    } else {
        m_hasActiveSnapX = false;
    }

    auto optSnapY = evaluateSnapY(movingElement, canvasRect, otherElements, settings, thresholdY);
    if (optSnapY) {
        result.position.y = movingBounds.y + optSnapY->delta;
        result.snappedCenter.y = movingElement.centerY + optSnapY->delta;
        result.snappedY = true;
        m_hasActiveSnapY = true;
        m_activeTargetY = optSnapY->guidePos;

        SmartGuide g;
        g.orientation = GuideOrientation::Horizontal;
        g.position = optSnapY->guidePos;
        g.reason = optSnapY->type;
        g.label = optSnapY->label;
        result.guides.push_back(g);
    } else {
        m_hasActiveSnapY = false;
    }

    return result;
}

SnapResult CanvasSnappingEngine::snapMove(
    const Rect &movingBounds,
    const Rect &canvasRect,
    const std::vector<Rect> &otherElements,
    const SnapSettings &settings,
    float scaleScreen)
{
    RotatedRect movingRot(movingBounds);
    std::vector<RotatedRect> othersRot;
    othersRot.reserve(otherElements.size());
    for (const auto &r : otherElements) {
        othersRot.emplace_back(r);
    }
    return snapMove(movingRot, canvasRect, othersRot, settings, scaleScreen);
}

RotationSnapResult CanvasSnappingEngine::snapRotation(
    double desiredAngleDeg,
    const SnapSettings &settings,
    bool isShiftPressed)
{
    RotationSnapResult result;
    result.snappedAngle = desiredAngleDeg;
    result.snapped = false;

    if (!settings.enabled || !settings.snapRotation) {
        return result;
    }

    if (isShiftPressed) {
        double step = 15.0;
        double snapped = std::round(desiredAngleDeg / step) * step;
        result.snappedAngle = snapped;
        result.snapped = true;
        result.snapTarget = snapped;
        int displayAngle = static_cast<int>(std::round(snapped)) % 360;
        if (displayAngle < 0) displayAngle += 360;
        result.label = QString("Ángulo: %1°").arg(displayAngle);
        return result;
    }

    double step = 45.0;
    double nearestMultiple = std::round(desiredAngleDeg / step) * step;
    double diff = std::abs(desiredAngleDeg - nearestMultiple);

    if (diff <= settings.rotationThresholdDeg) {
        result.snapped = true;
        result.snappedAngle = nearestMultiple;
        result.snapTarget = nearestMultiple;
        int displayAngle = static_cast<int>(std::round(nearestMultiple)) % 360;
        if (displayAngle < 0) displayAngle += 360;
        result.label = QString("Ángulo: %1°").arg(displayAngle);
    }

    return result;
}

ResizeSnapResult CanvasSnappingEngine::snapResize(
    const Rect &resizingBounds,
    const Rect &canvasRect,
    const std::vector<Rect> &otherElements,
    const SnapSettings &settings,
    float scaleScreen)
{
    ResizeSnapResult result;
    result.rect = resizingBounds;

    if (!settings.enabled) {
        return result;
    }

    float scale = (scaleScreen > 0.001f) ? scaleScreen : 1.0f;
    float threshold = settings.thresholdScreenPx / scale;

    if (settings.sameSize) {
        for (const Rect &other : otherElements) {
            // Check same width
            if (!result.snappedW && std::abs(resizingBounds.width - other.width) <= threshold) {
                result.rect.width = other.width;
                result.snappedW = true;

                SmartGuide g;
                g.orientation = GuideOrientation::Vertical;
                g.position = result.rect.right();
                g.reason = SnapType::SameSize;
                g.label = QString("Mismo ancho (%1 px)").arg(qRound(other.width));
                result.guides.push_back(g);
            }

            // Check same height
            if (!result.snappedH && std::abs(resizingBounds.height - other.height) <= threshold) {
                result.rect.height = other.height;
                result.snappedH = true;

                SmartGuide g;
                g.orientation = GuideOrientation::Horizontal;
                g.position = result.rect.bottom();
                g.reason = SnapType::SameSize;
                g.label = QString("Mismo alto (%1 px)").arg(qRound(other.height));
                result.guides.push_back(g);
            }
        }
    }

    return result;
}

} // namespace Snapping
