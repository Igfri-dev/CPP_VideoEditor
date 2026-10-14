#pragma once

#include <QString>
#include <QVector3D>
#include <QMatrix3x3>
#include <QColor>
#include <QImage>
#include <algorithm>
#include <cmath>

enum class ColorSpaceStandard {
    sRGB = 0,
    Rec709 = 1,
    DisplayP3 = 2,
    DciP3 = 3,
    Rec2020 = 4
};

enum class TransferFunction {
    sRGB = 0,
    Gamma22 = 1,
    Gamma24 = 2,     // BT.1886
    Linear = 3,
    PQ = 4,          // SMPTE ST 2084 (HDR10)
    HLG = 5          // ARIB STD-B67 (Hybrid Log-Gamma)
};

QString colorSpaceStandardToString(ColorSpaceStandard cs);
ColorSpaceStandard stringToColorSpaceStandard(const QString &str);
QString transferFunctionToString(TransferFunction tf);
TransferFunction stringToTransferFunction(const QString &str);

class ColorSpaceManager {
public:
    static ColorSpaceManager& instance();

    ColorSpaceStandard projectWorkingSpace() const { return m_workingSpace; }
    void setProjectWorkingSpace(ColorSpaceStandard cs) { m_workingSpace = cs; }

    TransferFunction projectTransferFunction() const { return m_transferFunction; }
    void setProjectTransferFunction(TransferFunction tf) { m_transferFunction = tf; }

    bool isHdrEnabled() const {
        return m_transferFunction == TransferFunction::PQ || m_transferFunction == TransferFunction::HLG;
    }

    // High precision color conversion functions
    static QVector3D decodeToLinear(const QVector3D &encodedRgb, TransferFunction tf);
    static QVector3D encodeFromLinear(const QVector3D &linearRgb, TransferFunction tf);

    static QVector3D convertColorSpace(const QVector3D &linearRgb,
                                       ColorSpaceStandard fromSpace,
                                       ColorSpaceStandard toSpace);

    static QImage processFrameColorSpace(const QImage &source,
                                         ColorSpaceStandard inputSpace,
                                         TransferFunction inputTf,
                                         ColorSpaceStandard outputSpace,
                                         TransferFunction outputTf);

    // Gamut mapping & tone reproduction
    static QVector3D toneMapAces(const QVector3D &hdrLinear);

private:
    ColorSpaceManager();
    ColorSpaceStandard m_workingSpace = ColorSpaceStandard::Rec709;
    TransferFunction m_transferFunction = TransferFunction::sRGB;
};
