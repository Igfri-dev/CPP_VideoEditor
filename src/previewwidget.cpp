#include "previewwidget.h"
#include "engine/videocompositor.h"
#include "engine/videoframedecoder.h"
#include "engine/gpuaccelerationmanager.h"
#include "engine/gpusettingsdialog.h"
#include "filters/TimeOfDayFilter.h"
#include "medialibrary/mediaitem.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStyle>
#include <QMenu>
#include <QContextMenuEvent>
#include <cmath>

// ---------------------------------------------------------------------------
// MonitorWidget Implementation
// ---------------------------------------------------------------------------

MonitorWidget::MonitorWidget(TimelineModel *model, QWidget *parent)
    : QOpenGLWidget(parent)
    , m_model(model)
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);

    setMouseTracking(true);
    setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(320, 180);
}

MonitorWidget::~MonitorWidget()
{
    makeCurrent();
    if (m_videoTexture) {
        if (m_videoTexture->isCreated()) {
            m_videoTexture->destroy();
        }
        delete m_videoTexture;
        m_videoTexture = nullptr;
    }
    if (m_lutTexture3D) {
        if (m_lutTexture3D->isCreated()) {
            m_lutTexture3D->destroy();
        }
        delete m_lutTexture3D;
        m_lutTexture3D = nullptr;
    }
    if (m_vao.isCreated()) {
        m_vao.destroy();
    }
    if (m_vbo.isCreated()) {
        m_vbo.destroy();
    }
    delete m_shaderProgram;
    m_shaderProgram = nullptr;
    doneCurrent();
}

void MonitorWidget::setFrame(const QImage &frame)
{
    m_currentFrame = frame;
    update();
}

void MonitorWidget::setSelectedClipId(qint64 clipId)
{
    if (m_selectedClipId != clipId) {
        m_selectedClipId = clipId;
        update();
    }
}

void MonitorWidget::setCurrentPositionMs(qint64 posMs)
{
    m_currentPositionMs = posMs;
    update();
}

void MonitorWidget::setIsPlaying(bool playing)
{
    if (m_isPlaying != playing) {
        m_isPlaying = playing;
        update();
    }
}

static QSize getClipBaseSize(const TimelineClip *clip, const QSize &canvas = QSize(1920, 1080))
{
    if (!clip) return canvas;
    if (clip->type() == ClipType::Text) {
        QImage textImg = clip->renderTextImage(canvas);
        return textImg.isNull() ? QSize(400, 100) : textImg.size();
    }
    QSize mediaSize = VideoFrameDecoder::instance().getVideoDimensions(clip->filePath());
    if (mediaSize.isEmpty() || !mediaSize.isValid()) {
        mediaSize = canvas;
    }
    if (clip->scaleMode() == ClipScaleMode::FitLetterbox) {
        return mediaSize.scaled(canvas, Qt::KeepAspectRatio);
    } else if (clip->scaleMode() == ClipScaleMode::Stretch) {
        return canvas;
    } else {
        return mediaSize.scaled(canvas, Qt::KeepAspectRatioByExpanding);
    }
}

QPointF MonitorWidget::canvasToScreen(const QPointF &canvasOffset, const QRect &dRect) const
{
    QSize canvasBase = m_model ? m_model->canvasSize() : QSize(1920, 1080);
    double cw = canvasBase.width();
    double ch = canvasBase.height();
    double canvasX = (cw / 2.0) + canvasOffset.x();
    double canvasY = (ch / 2.0) + canvasOffset.y();
    double sx = dRect.left() + (canvasX / cw) * dRect.width();
    double sy = dRect.top() + (canvasY / ch) * dRect.height();
    return QPointF(sx, sy);
}

QPointF MonitorWidget::screenToCanvas(const QPointF &screenPoint, const QRect &dRect) const
{
    if (dRect.width() <= 0 || dRect.height() <= 0) return QPointF(0, 0);
    QSize canvasBase = m_model ? m_model->canvasSize() : QSize(1920, 1080);
    double cw = canvasBase.width();
    double ch = canvasBase.height();
    double canvasX = ((screenPoint.x() - dRect.left()) / static_cast<double>(dRect.width())) * cw;
    double canvasY = ((screenPoint.y() - dRect.top()) / static_cast<double>(dRect.height())) * ch;
    return QPointF(canvasX - (cw / 2.0), canvasY - (ch / 2.0));
}

MonitorWidget::GizmoGeometry MonitorWidget::calculateGizmoGeometry() const
{
    GizmoGeometry geom;
    if (!m_model || m_selectedClipId <= 0) {
        return geom;
    }

    TimelineClip *clip = m_model->findClip(m_selectedClipId);
    if (!clip || clip->type() == ClipType::Audio) {
        return geom;
    }

    // Only show if clip is active at current timeline position
    if (m_currentPositionMs < clip->timelineInMs() || m_currentPositionMs > clip->timelineOutMs()) {
        return geom;
    }

    // Check track visibility
    TimelineTrack *track = m_model->findTrackForClip(m_selectedClipId);
    if (track && !track->isVisible()) {
        return geom;
    }

    // Canvas display rectangle inside this widget maintaining project aspect ratio
    QSize canvasBase = m_model ? m_model->canvasSize() : QSize(1920, 1080);
    QSize scaledCanvas = canvasBase.scaled(size(), Qt::KeepAspectRatio);
    geom.displayRect = QRect(
        (width() - scaledCanvas.width()) / 2,
        (height() - scaledCanvas.height()) / 2,
        scaledCanvas.width(),
        scaledCanvas.height()
    );

    geom.scaleScreenX = geom.displayRect.width() / static_cast<double>(canvasBase.width());
    geom.scaleScreenY = geom.displayRect.height() / static_cast<double>(canvasBase.height());

    QSize baseSize = getClipBaseSize(clip, canvasBase);
    double baseW = baseSize.width();
    double baseH = baseSize.height();

    // Center in canvas coordinates (0, 0 is center)
    double cx_canvas = (canvasBase.width() / 2.0) + clip->posXAt(m_currentPositionMs);
    double cy_canvas = (canvasBase.height() / 2.0) + clip->posYAt(m_currentPositionMs);

    geom.centerScreen = QPointF(
        geom.displayRect.left() + cx_canvas * geom.scaleScreenX,
        geom.displayRect.top() + cy_canvas * geom.scaleScreenY
    );

    double curScaleX = clip->scaleXAt(m_currentPositionMs);
    double curScaleY = clip->scaleYAt(m_currentPositionMs);
    double halfW = (baseW * curScaleX / 2.0) * geom.scaleScreenX;
    double halfH = (baseH * curScaleY / 2.0) * geom.scaleScreenY;

    double curRot = clip->rotationAt(m_currentPositionMs);
    double rad = qDegreesToRadians(curRot);
    double cosR = std::cos(rad);
    double sinR = std::sin(rad);

    auto transformLocal = [&](double lx, double ly) -> QPointF {
        double rx = lx * cosR - ly * sinR;
        double ry = lx * sinR + ly * cosR;
        return QPointF(geom.centerScreen.x() + rx, geom.centerScreen.y() + ry);
    };

    geom.pTL = transformLocal(-halfW, -halfH);
    geom.pTR = transformLocal(halfW, -halfH);
    geom.pBR = transformLocal(halfW, halfH);
    geom.pBL = transformLocal(-halfW, halfH);
    geom.pTC = (geom.pTL + geom.pTR) / 2.0;
    geom.pML = (geom.pTL + geom.pBL) / 2.0;
    geom.pMR = (geom.pTR + geom.pBR) / 2.0;

    // Rotation handle 28px above top-center in local orientation
    geom.pRot = transformLocal(0, -halfH - 28.0);
    // Keyframe diamond badge at top-right of bounding box
    geom.kfBadgeRect = QRectF(geom.pTR.x() + 4, geom.pTR.y() - 22, 22, 18);
    geom.isTextClip = (clip->type() == ClipType::Text);
    geom.isValid = true;
    return geom;
}

MonitorWidget::GizmoAction MonitorWidget::hitTest(const QPointF &pos, const GizmoGeometry &geom) const
{
    if (!geom.isValid) return GizmoAction::None;

    auto dist = [](const QPointF &a, const QPointF &b) {
        return std::hypot(a.x() - b.x(), a.y() - b.y());
    };

    auto distToSegment = [](const QPointF &p, const QPointF &a, const QPointF &b) -> double {
        QPointF ab = b - a;
        double l2 = ab.x() * ab.x() + ab.y() * ab.y();
        if (l2 < 1e-6) return std::hypot(p.x() - a.x(), p.y() - a.y());
        double t = qBound(0.0, ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / l2, 1.0);
        QPointF proj = a + t * ab;
        return std::hypot(p.x() - proj.x(), p.y() - proj.y());
    };

    // Keyframe diamond badge check
    if (geom.kfBadgeRect.contains(pos)) {
        return GizmoAction::ToggleKeyframe;
    }

    // Rotation handle check (radius ~14)
    if (dist(pos, geom.pRot) <= 14.0) {
        return GizmoAction::Rotate;
    }

    // Corner handles check (radius ~12)
    if (dist(pos, geom.pTL) <= 12.0) return GizmoAction::ResizeTL;
    if (dist(pos, geom.pTR) <= 12.0) return GizmoAction::ResizeTR;
    if (dist(pos, geom.pBR) <= 12.0) return GizmoAction::ResizeBR;
    if (dist(pos, geom.pBL) <= 12.0) return GizmoAction::ResizeBL;

    // Side handles / border edges check for text clips (width & line breaks)
    if (geom.isTextClip) {
        if (dist(pos, geom.pML) <= 14.0 || distToSegment(pos, geom.pTL, geom.pBL) <= 8.0) {
            return GizmoAction::ResizeLeft;
        }
        if (dist(pos, geom.pMR) <= 14.0 || distToSegment(pos, geom.pTR, geom.pBR) <= 8.0) {
            return GizmoAction::ResizeRight;
        }
    }

    // Body check (inside polygon)
    QPolygonF poly;
    poly << geom.pTL << geom.pTR << geom.pBR << geom.pBL;
    if (poly.containsPoint(pos, Qt::OddEvenFill)) {
        return GizmoAction::Move;
    }

    return GizmoAction::None;
}

void MonitorWidget::initializeGL()
{
    initializeOpenGLFunctions();

    const char *vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char *renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    if (vendor && renderer) {
        GpuAccelerationManager::instance().updateFromGlInfo(
            QString::fromUtf8(vendor), QString::fromUtf8(renderer));
    }

    m_shaderProgram = new QOpenGLShaderProgram(this);

    const char *vShaderSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec2 a_pos;\n"
        "layout(location = 1) in vec2 a_tex;\n"
        "out vec2 TexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
        "    TexCoord = a_tex;\n"
        "}\n";

    std::string fShaderStr = TimeOfDayFilter::GetFragmentShaderSource();
    if (fShaderStr.empty()) {
        fShaderStr =
            "#version 330 core\n"
            "in vec2 TexCoord;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D u_texture;\n"
            "void main() {\n"
            "    FragColor = texture(u_texture, TexCoord);\n"
            "}\n";
    }

    bool ok = m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vShaderSrc) &&
              m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fShaderStr.c_str()) &&
              m_shaderProgram->link();

    if (!ok) {
        // Fallback for legacy OpenGL 2.1 / Compatibility contexts
        delete m_shaderProgram;
        m_shaderProgram = new QOpenGLShaderProgram(this);

        const char *vShaderSrc120 =
            "#version 120\n"
            "attribute vec2 a_pos;\n"
            "attribute vec2 a_tex;\n"
            "varying vec2 TexCoord;\n"
            "void main() {\n"
            "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
            "    TexCoord = a_tex;\n"
            "}\n";

        std::string f120 = fShaderStr;
        size_t verPos = f120.find("#version 330 core");
        if (verPos != std::string::npos) {
            f120.replace(verPos, 17, "#version 120");
        }
        size_t inPos = f120.find("in vec2 TexCoord;");
        if (inPos != std::string::npos) {
            f120.replace(inPos, 17, "varying vec2 TexCoord;");
        }
        size_t outPos = f120.find("out vec4 FragColor;");
        if (outPos != std::string::npos) {
            f120.replace(outPos, 19, "// out vec4 FragColor;");
        }
        size_t fcPos = 0;
        while ((fcPos = f120.find("FragColor", fcPos)) != std::string::npos) {
            f120.replace(fcPos, 9, "gl_FragColor");
            fcPos += 12;
        }

        ok = m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vShaderSrc120) &&
             m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, f120.c_str()) &&
             m_shaderProgram->link();
    }

    if (ok) {
        // Create VAO and VBO for screen-aligned rendering quad
        if (!m_vao.isCreated()) {
            m_vao.create();
        }
        m_vao.bind();

        if (!m_vbo.isCreated()) {
            m_vbo.create();
        }
        m_vbo.bind();

        static const float vertices[] = {
            -1.0f,  1.0f, 0.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 1.0f
        };
        m_vbo.allocate(vertices, sizeof(vertices));

        m_shaderProgram->enableAttributeArray(0);
        m_shaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));

        m_shaderProgram->enableAttributeArray(1);
        m_shaderProgram->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));

        m_vbo.release();
        m_vao.release();

        // Create neutral 3D LUT texture to satisfy sampler3D u_lutTexture binding on unit 1
        if (!m_lutTexture3D) {
            m_lutTexture3D = new QOpenGLTexture(QOpenGLTexture::Target3D);
            m_lutTexture3D->setSize(2, 2, 2);
            m_lutTexture3D->setFormat(QOpenGLTexture::RGB8_UNorm);
            m_lutTexture3D->allocateStorage(QOpenGLTexture::RGB, QOpenGLTexture::UInt8);
            static const uint8_t neutralLut[2 * 2 * 2 * 3] = {
                0,   0,   0,     255, 0,   0,
                0,   255, 0,     255, 255, 0,
                0,   0,   255,   255, 0,   255,
                0,   255, 255,   255, 255, 255
            };
            m_lutTexture3D->setData(QOpenGLTexture::RGB, QOpenGLTexture::UInt8, neutralLut);
            m_lutTexture3D->setMinificationFilter(QOpenGLTexture::Linear);
            m_lutTexture3D->setMagnificationFilter(QOpenGLTexture::Linear);
            m_lutTexture3D->setWrapMode(QOpenGLTexture::ClampToEdge);
        }

        m_glInitialized = true;
    } else {
        qWarning() << "Failed to compile MonitorWidget shaders:" << m_shaderProgram->log();
        delete m_shaderProgram;
        m_shaderProgram = nullptr;
        m_glInitialized = false;
    }
}

void MonitorWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

bool MonitorWidget::renderGpuFrame(const QRect &dRect, const ColorAdjustments &adj)
{
    if (dRect.width() <= 0 || dRect.height() <= 0 || m_currentFrame.isNull() || !m_glInitialized || !m_shaderProgram) {
        return false;
    }

    if (!m_videoTexture || m_videoTexture->width() != m_currentFrame.width() || m_videoTexture->height() != m_currentFrame.height()) {
        if (m_videoTexture) {
            if (m_videoTexture->isCreated()) {
                m_videoTexture->destroy();
            }
            delete m_videoTexture;
            m_videoTexture = nullptr;
        }
        m_videoTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
        m_videoTexture->setSize(m_currentFrame.width(), m_currentFrame.height());
        m_videoTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        m_videoTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
        m_videoTexture->setMinificationFilter(QOpenGLTexture::Linear);
        m_videoTexture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_videoTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    }

    QImage glImg = (m_currentFrame.format() == QImage::Format_RGBA8888 || m_currentFrame.format() == QImage::Format_RGBA8888_Premultiplied)
                   ? m_currentFrame
                   : m_currentFrame.convertToFormat(QImage::Format_RGBA8888);
    m_videoTexture->setData(0, QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, glImg.constBits());

    qreal dpr = devicePixelRatioF();
    int vx = qRound(dRect.left() * dpr);
    int vy = qRound((height() - dRect.bottom()) * dpr);
    int vw = qRound(dRect.width() * dpr);
    int vh = qRound(dRect.height() * dpr);
    glViewport(vx, vy, vw, vh);

    if (!m_shaderProgram->bind()) {
        return false;
    }

    // Bind 2D video frame to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    m_videoTexture->bind(0);
    m_shaderProgram->setUniformValue("u_texture", 0);

    // Bind 3D LUT texture to unit 1 to prevent GL_INVALID_OPERATION from sampler type mismatch on unit 0
    if (m_lutTexture3D) {
        glActiveTexture(GL_TEXTURE1);
        m_lutTexture3D->bind(1);
        m_shaderProgram->setUniformValue("u_lutTexture", 1);
    }

    float targetTime = adj.timeOfDay;
    float sourceTime = (adj.timeOfDaySourceMode == TimeOfDaySourceMode::Auto) ? 0.60f : adj.timeOfDaySourceTime;
    RelativeSettings settings;
    settings.intensity = adj.timeOfDayIntensity;
    settings.skinProtectionFactor = adj.timeOfDaySkinProtection;
    settings.skyInfluenceFactor = adj.timeOfDaySkyInfluence;
    settings.highlightWarmthBias = adj.timeOfDayHighlightWarmth;
    settings.shadowCoolnessBias = adj.timeOfDayShadowCoolness;
    settings.exposureBias = adj.timeOfDayExposureBias;
    settings.lutStrengthFactor = adj.timeOfDayLutStrength;

    TimeProfile prof = TimeOfDayFilter::CalculateRelativeProfile(sourceTime, targetTime, settings);
    auto gains = TimeOfDayFilter::KelvinToRGB(prof.temperature, prof.tint);
    auto shadowG = TimeOfDayFilter::KelvinToRGB(prof.shadowTemperature, prof.shadowTint);
    auto midG = TimeOfDayFilter::KelvinToRGB(prof.midtoneTemperature, 0.0f);
    auto highG = TimeOfDayFilter::KelvinToRGB(prof.highlightTemperature, 0.0f);

    m_shaderProgram->setUniformValue("u_exposureEV", prof.exposureEV);
    m_shaderProgram->setUniformValue("u_whiteBalanceGains", gains[0], gains[1], gains[2]);
    m_shaderProgram->setUniformValue("u_shadowGains", shadowG[0], shadowG[1], shadowG[2]);
    m_shaderProgram->setUniformValue("u_midtoneGains", midG[0], midG[1], midG[2]);
    m_shaderProgram->setUniformValue("u_highlightGains", highG[0], highG[1], highG[2]);
    m_shaderProgram->setUniformValue("u_contrast", prof.contrast);
    m_shaderProgram->setUniformValue("u_saturation", prof.saturation);
    m_shaderProgram->setUniformValue("u_shadowLift", prof.shadowLift);
    m_shaderProgram->setUniformValue("u_midtoneGain", prof.midtoneGain);
    m_shaderProgram->setUniformValue("u_highlightGain", prof.highlightGain);
    m_shaderProgram->setUniformValue("u_highlightRolloff", prof.highlightRolloff);
    m_shaderProgram->setUniformValue("u_purkinjeStrength", prof.purkinjeStrength);
    m_shaderProgram->setUniformValue("u_skyExposureDrop", prof.skyExposureDrop);
    m_shaderProgram->setUniformValue("u_skinProtection", prof.skinProtection);
    m_shaderProgram->setUniformValue("u_lutStrength", 0.0f); // Analytical model calibrated, avoid uninitialized 3D LUT sampling
    m_shaderProgram->setUniformValue("u_timeOfDay", targetTime);
    m_shaderProgram->setUniformValue("u_intensity", adj.timeOfDayEnabled ? adj.timeOfDayIntensity : 0.0f);

    if (m_vao.isCreated()) {
        m_vao.bind();
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_vao.release();
    } else {
        m_vbo.bind();
        m_shaderProgram->enableAttributeArray(0);
        m_shaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));
        m_shaderProgram->enableAttributeArray(1);
        m_shaderProgram->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_shaderProgram->disableAttributeArray(0);
        m_shaderProgram->disableAttributeArray(1);
        m_vbo.release();
    }

    if (m_lutTexture3D) {
        glActiveTexture(GL_TEXTURE1);
        m_lutTexture3D->release();
    }
    glActiveTexture(GL_TEXTURE0);
    m_videoTexture->release();
    m_shaderProgram->release();

    glViewport(0, 0, qRound(width() * dpr), qRound(height() * dpr));
    return true;
}

void MonitorWidget::paintGL()
{
    glClearColor(0.043f, 0.051f, 0.071f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    GizmoGeometry geom = calculateGizmoGeometry();
    QRect dRect = geom.isValid ? geom.displayRect : QRect();
    if (dRect.isEmpty()) {
        QSize canvasBase = m_model ? m_model->canvasSize() : QSize(1920, 1080);
        QSize scaledCanvas = canvasBase.scaled(size(), Qt::KeepAspectRatio);
        dRect = QRect(
            (width() - scaledCanvas.width()) / 2,
            (height() - scaledCanvas.height()) / 2,
            scaledCanvas.width(),
            scaledCanvas.height()
        );
    }

    ColorAdjustments adj;
    if (m_model) {
        adj = m_model->globalColorAdjustments();
        if (!adj.timeOfDayEnabled || adj.timeOfDayIntensity <= 0.001f) {
            if (m_selectedClipId > 0) {
                if (TimelineClip *selClip = m_model->findClip(m_selectedClipId)) {
                    ColorAdjustments clipAdj = selClip->colorAdjustmentsAt(m_currentPositionMs);
                    if (clipAdj.timeOfDayEnabled && clipAdj.timeOfDayIntensity > 0.001f) {
                        adj = clipAdj;
                    }
                }
            } else {
                for (const auto &track : m_model->videoTracks()) {
                    for (const auto &c : track.clips()) {
                        if (m_currentPositionMs >= c.timelineInMs() && m_currentPositionMs < c.timelineOutMs()) {
                            ColorAdjustments clipAdj = c.colorAdjustmentsAt(m_currentPositionMs);
                            if (clipAdj.timeOfDayEnabled && clipAdj.timeOfDayIntensity > 0.001f) {
                                adj = clipAdj;
                                break;
                            }
                        }
                    }
                    if (adj.timeOfDayEnabled) break;
                }
            }
        }
    }

    // VideoCompositor already faithfully renders all visual tracks, adjustment layers, filters, and color adjustments into m_currentFrame
    bool needGpuShader = false;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    bool gpuRendered = false;
    if (needGpuShader) {
        p.beginNativePainting();
        gpuRendered = renderGpuFrame(dRect, adj);
        p.endNativePainting();
    }

    // Canvas frame black backdrop & fallback image if GPU render was not executed
    if (!gpuRendered) {
        p.fillRect(dRect, Qt::black);
        if (!m_currentFrame.isNull()) {
            p.drawImage(dRect, m_currentFrame);
        }
    }

    // Canvas border
    p.setPen(QPen(QColor("#30363d"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(dRect);

    // Draw Safe Areas if enabled or toggled
    if (m_showSafeAreas) {
        // Title Safe (10% margins)
        QRectF titleSafeRect(
            dRect.left() + dRect.width() * 0.10,
            dRect.top() + dRect.height() * 0.10,
            dRect.width() * 0.80,
            dRect.height() * 0.80
        );
        p.setPen(QPen(QColor(227, 179, 65, 140), 1, Qt::DashLine));
        p.drawRect(titleSafeRect);

        // Action Safe (5% margins)
        QRectF actionSafeRect(
            dRect.left() + dRect.width() * 0.05,
            dRect.top() + dRect.height() * 0.05,
            dRect.width() * 0.90,
            dRect.height() * 0.90
        );
        p.setPen(QPen(QColor(88, 166, 255, 120), 1, Qt::DotLine));
        p.drawRect(actionSafeRect);
    }

    // Draw Smart Guides
    for (const auto &g : m_activeGuides) {
        QColor guideColor;
        switch (g.reason) {
        case Snapping::SnapType::CanvasCenter:
            guideColor = QColor("#00d2ff"); // Vibrant Cyan
            break;
        case Snapping::SnapType::CanvasEdge:
            guideColor = QColor("#58a6ff"); // Blue
            break;
        case Snapping::SnapType::ObjectCenter:
        case Snapping::SnapType::ObjectEdge:
            guideColor = QColor("#ff007f"); // Vibrant Magenta
            break;
        case Snapping::SnapType::SafeArea:
            guideColor = QColor("#e3b341"); // Golden Yellow
            break;
        case Snapping::SnapType::EqualSpacing:
            guideColor = QColor("#2ea043"); // Green
            break;
        case Snapping::SnapType::SameSize:
            guideColor = QColor("#f0883e"); // Orange
            break;
        default:
            guideColor = QColor("#00d2ff");
            break;
        }

        p.setPen(QPen(guideColor, 1.5, Qt::DashLine));
        QSize canvasBase = m_model ? m_model->canvasSize() : QSize(1920, 1080);
        double cw = canvasBase.width();
        double ch = canvasBase.height();
        if (g.orientation == Snapping::GuideOrientation::Vertical) {
            double screenX = dRect.left() + (g.position / cw) * dRect.width();
            p.drawLine(QPointF(screenX, dRect.top()), QPointF(screenX, dRect.bottom()));

            if (!g.label.isEmpty()) {
                QFont gf = p.font();
                gf.setPixelSize(10);
                p.setFont(gf);
                QFontMetrics fm(gf);
                int tw = fm.horizontalAdvance(g.label) + 8;
                QRectF badge(screenX + 4, dRect.top() + 8, tw, 18);
                if (badge.right() > dRect.right()) badge.moveLeft(screenX - tw - 4);
                p.fillRect(badge, QColor(20, 23, 29, 210));
                p.setPen(guideColor);
                p.drawRect(badge);
                p.setPen(Qt::white);
                p.drawText(badge, Qt::AlignCenter, g.label);
            }
        } else {
            double screenY = dRect.top() + (g.position / ch) * dRect.height();
            p.drawLine(QPointF(dRect.left(), screenY), QPointF(dRect.right(), screenY));

            if (!g.label.isEmpty()) {
                QFont gf = p.font();
                gf.setPixelSize(10);
                p.setFont(gf);
                QFontMetrics fm(gf);
                int tw = fm.horizontalAdvance(g.label) + 8;
                QRectF badge(dRect.left() + 8, screenY + 4, tw, 18);
                if (badge.bottom() > dRect.bottom()) badge.moveTop(screenY - 22);
                p.fillRect(badge, QColor(20, 23, 29, 210));
                p.setPen(guideColor);
                p.drawRect(badge);
                p.setPen(Qt::white);
                p.drawText(badge, Qt::AlignCenter, g.label);
            }
        }
    }

    // Draw Motion Path (Ruta de Desplazamiento) if clip is selected and has motion path
    TimelineClip *selectedClip = (m_model && m_selectedClipId > 0) ? m_model->findClip(m_selectedClipId) : nullptr;
    if (selectedClip && selectedClip->hasMotionPath()) {
        const MotionPath &mPath = selectedClip->motionPath();
        const auto &wps = mPath.waypoints();
        if (wps.size() >= 2) {
            // 0. Render Ghost Previews of the actual clip content at Start, End, and any actively dragged waypoint
            QImage ghostImg;
            if (!m_isPlaying) {
                if (selectedClip->type() == ClipType::Text) {
                    ghostImg = selectedClip->renderTextImage(QSize(1920, 1080));
                } else if (!selectedClip->filePath().isEmpty()) {
                    if (m_cachedGhostClipId == selectedClip->id() && m_cachedGhostSourceInMs == selectedClip->sourceInMs() && !m_cachedGhostImg.isNull()) {
                        ghostImg = m_cachedGhostImg;
                    } else {
                        m_cachedGhostImg = VideoFrameDecoder::instance().getFrame(selectedClip->filePath(), selectedClip->sourceInMs(), QSize(1920, 1080));
                        m_cachedGhostClipId = selectedClip->id();
                        m_cachedGhostSourceInMs = selectedClip->sourceInMs();
                        ghostImg = m_cachedGhostImg;
                    }
                }
            } else {
                if (m_cachedGhostClipId == selectedClip->id()) {
                    ghostImg = m_cachedGhostImg;
                }
            }

            QSize baseSize = getClipBaseSize(selectedClip, QSize(1920, 1080));
            double sx = dRect.width() / 1920.0;
            double sy = dRect.height() / 1080.0;
            double bw = baseSize.width() * selectedClip->scaleX() * sx;
            double bh = baseSize.height() * selectedClip->scaleY() * sy;

            for (int i = 0; i < wps.size(); ++i) {
                bool isDragged = (m_currentAction == GizmoAction::DragMotionPoint && m_activeMotionPointIndex == i);
                if (i == 0 || i == wps.size() - 1 || isDragged) {
                    QPointF sPos = canvasToScreen(wps[i].pos, dRect);
                    p.save();
                    p.translate(sPos);
                    if (qAbs(selectedClip->rotation()) > 0.001) {
                        p.rotate(selectedClip->rotation());
                    }

                    if (!ghostImg.isNull()) {
                        p.setOpacity(isDragged ? 0.65 : 0.40);
                        p.drawImage(QRectF(-bw / 2.0, -bh / 2.0, bw, bh), ghostImg);
                    }

                    QColor outlineColor = (i == 0) ? QColor("#3fb950") : (i == wps.size() - 1 ? QColor("#ff7b72") : QColor("#a371f7"));
                    p.setOpacity(isDragged ? 0.90 : 0.65);
                    p.setPen(QPen(outlineColor, isDragged ? 2.0 : 1.2, Qt::DashLine));
                    p.setBrush(QColor(outlineColor.red(), outlineColor.green(), outlineColor.blue(), 25));
                    p.drawRoundedRect(QRectF(-bw / 2.0, -bh / 2.0, bw, bh), 2, 2);
                    p.restore();
                }
            }

            // 1. Draw Trajectory Path Curve
            QPainterPath pathCurve;
            const int sampleSteps = 80;
            for (int s = 0; s <= sampleSteps; ++s) {
                double u = s / static_cast<double>(sampleSteps);
                QPointF cPos = mPath.evaluate(u);
                QPointF sPos = canvasToScreen(cPos, dRect);
                if (s == 0) pathCurve.moveTo(sPos);
                else pathCurve.lineTo(sPos);
            }

            // Glow underlay
            p.setPen(QPen(QColor(0, 229, 255, 60), 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(pathCurve);

            // Dotted/Dashed trajectory line
            p.setPen(QPen(QColor("#00e5ff"), 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPath(pathCurve);

            // Draw directional motion dots/arrows at 25%, 50%, 75%
            for (double tArrow : {0.25, 0.50, 0.75}) {
                QPointF pA = canvasToScreen(mPath.evaluate(tArrow - 0.02), dRect);
                QPointF pB = canvasToScreen(mPath.evaluate(tArrow + 0.02), dRect);
                QPointF dir = pB - pA;
                double len = std::hypot(dir.x(), dir.y());
                if (len > 1.0) {
                    dir /= len;
                    QPointF norm(-dir.y(), dir.x());
                    QPointF mid = canvasToScreen(mPath.evaluate(tArrow), dRect);
                    QPolygonF arrowPoly;
                    arrowPoly << mid + dir * 6.0 << mid - dir * 4.0 + norm * 4.0 << mid - dir * 4.0 - norm * 4.0;
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor("#00e5ff"));
                    p.drawPolygon(arrowPoly);
                }
            }

            // 2. Draw Current Position along path at current playhead
            if (selectedClip->durationMs() > 0) {
                double currU = qBound(0.0, static_cast<double>(m_currentPositionMs - selectedClip->timelineInMs()) / selectedClip->durationMs(), 1.0);
                QPointF currPos = canvasToScreen(mPath.evaluate(currU), dRect);
                p.setPen(QPen(QColor("#00e5ff"), 2));
                p.setBrush(Qt::white);
                p.drawEllipse(currPos, 6, 6);
                p.setPen(QPen(QColor(0, 229, 255, 120), 1.5));
                p.setBrush(Qt::NoBrush);
                p.drawEllipse(currPos, 10, 10);
            }

            // 3. Draw Bézier Tangent Handles (Photoshop style)
            for (int i = 0; i < wps.size(); ++i) {
                const auto &wp = wps[i];
                if (wp.isCurved || !wp.handleIn.isNull() || !wp.handleOut.isNull()) {
                    QPointF sPos = canvasToScreen(wp.pos, dRect);

                    if (!wp.handleIn.isNull()) {
                        QPointF sHandleIn = canvasToScreen(wp.pos + wp.handleIn, dRect);
                        p.setPen(QPen(QColor("#58a6ff"), 1.2, Qt::SolidLine));
                        p.drawLine(sPos, sHandleIn);
                        p.setPen(QPen(Qt::white, 1.2));
                        p.setBrush(QColor("#58a6ff"));
                        p.drawEllipse(sHandleIn, 4, 4);
                    }
                    if (!wp.handleOut.isNull()) {
                        QPointF sHandleOut = canvasToScreen(wp.pos + wp.handleOut, dRect);
                        p.setPen(QPen(QColor("#58a6ff"), 1.2, Qt::SolidLine));
                        p.drawLine(sPos, sHandleOut);
                        p.setPen(QPen(Qt::white, 1.2));
                        p.setBrush(QColor("#58a6ff"));
                        p.drawEllipse(sHandleOut, 4, 4);
                    }
                }
            }

            // 4. Draw Waypoints with Badges
            for (int i = 0; i < wps.size(); ++i) {
                const auto &wp = wps[i];
                QPointF sPos = canvasToScreen(wp.pos, dRect);

                if (i == 0) {
                    // Start point (Green)
                    p.setPen(QPen(Qt::white, 2));
                    p.setBrush(QColor("#3fb950"));
                    p.drawEllipse(sPos, 7, 7);

                    QFont bf = p.font();
                    bf.setPixelSize(10);
                    bf.setBold(true);
                    p.setFont(bf);
                    QRectF badge(sPos.x() + 9, sPos.y() - 18, 56, 16);
                    p.fillRect(badge, QColor(20, 23, 29, 220));
                    p.setPen(QColor("#3fb950"));
                    p.drawRoundedRect(badge, 3, 3);
                    p.setPen(Qt::white);
                    p.drawText(badge, Qt::AlignCenter, "▶ INICIO");
                } else if (i == wps.size() - 1) {
                    // End point (Coral/Red)
                    p.setPen(QPen(Qt::white, 2));
                    p.setBrush(QColor("#ff7b72"));
                    p.drawEllipse(sPos, 7, 7);

                    QFont bf = p.font();
                    bf.setPixelSize(10);
                    bf.setBold(true);
                    p.setFont(bf);
                    QRectF badge(sPos.x() + 9, sPos.y() - 18, 48, 16);
                    p.fillRect(badge, QColor(20, 23, 29, 220));
                    p.setPen(QColor("#ff7b72"));
                    p.drawRoundedRect(badge, 3, 3);
                    p.setPen(Qt::white);
                    p.drawText(badge, Qt::AlignCenter, "⏹ FIN");
                } else {
                    // Intermediate Waypoint (Purple/Cyan)
                    p.setPen(QPen(Qt::white, 1.5));
                    p.setBrush(QColor("#a371f7"));
                    p.drawEllipse(sPos, 6, 6);

                    QFont bf = p.font();
                    bf.setPixelSize(9);
                    bf.setBold(true);
                    p.setFont(bf);
                    QString pLabel = QString("P%1").arg(i + 1);
                    QRectF badge(sPos.x() + 8, sPos.y() - 16, 26, 14);
                    p.fillRect(badge, QColor(20, 23, 29, 220));
                    p.setPen(QColor("#a371f7"));
                    p.drawRoundedRect(badge, 2, 2);
                    p.setPen(Qt::white);
                    p.drawText(badge, Qt::AlignCenter, pLabel);
                }
            }
        }
    }

    // Draw Gizmo if valid selection
    if (geom.isValid) {
        // Outline
        p.setPen(QPen(QColor("#58a6ff"), 2, Qt::SolidLine));
        QPolygonF poly;
        poly << geom.pTL << geom.pTR << geom.pBR << geom.pBL;
        p.drawPolygon(poly);

        // Rotation connector line
        p.setPen(QPen(QColor("#58a6ff"), 1.5, Qt::DashLine));
        p.drawLine(geom.pTC, geom.pRot);

        // Rotation handle
        p.setPen(QPen(Qt::white, 1.5));
        p.setBrush(m_activeRotationSnap ? QColor("#00d2ff") : QColor("#388bfd"));
        p.drawEllipse(geom.pRot, 6, 6);

        // Rotation snap ray and badge if rotating
        if (m_isRotating && !m_activeRotationLabel.isEmpty()) {
            if (m_activeRotationSnap) {
                p.setPen(QPen(QColor("#00d2ff"), 1.5, Qt::DashLine));
                QPointF dir = geom.pRot - geom.centerScreen;
                p.drawLine(geom.centerScreen - dir * 1.5, geom.centerScreen + dir * 1.5);
            }

            QFont rf = p.font();
            rf.setPixelSize(11);
            rf.setBold(true);
            p.setFont(rf);
            QFontMetrics fm(rf);
            int bw = fm.horizontalAdvance(m_activeRotationLabel) + 14;
            QRectF badgeRect(geom.pRot.x() + 12, geom.pRot.y() - 10, bw, 20);
            p.fillRect(badgeRect, QColor(20, 23, 29, 230));
            p.setPen(m_activeRotationSnap ? QColor("#00d2ff") : QColor("#58a6ff"));
            p.drawRect(badgeRect);
            p.setPen(Qt::white);
            p.drawText(badgeRect, Qt::AlignCenter, m_activeRotationLabel);
        }

        // Corner handles
        auto drawHandle = [&](const QPointF &pt) {
            p.setPen(QPen(QColor("#1f6feb"), 1.5));
            p.setBrush(Qt::white);
            p.drawRect(QRectF(pt.x() - 4.5, pt.y() - 4.5, 9.0, 9.0));
        };
        drawHandle(geom.pTL);
        drawHandle(geom.pTR);
        drawHandle(geom.pBR);
        drawHandle(geom.pBL);

        // Draw Side handles (Middle-Left and Middle-Right) and borders for text clips
        if (geom.isTextClip) {
            TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
            double deg = clip ? clip->rotation() : 0.0;

            bool leftActive = (m_currentAction == GizmoAction::ResizeLeft);
            bool rightActive = (m_currentAction == GizmoAction::ResizeRight);

            // Left/Right border glow or highlight when resizing width
            if (leftActive) {
                p.setPen(QPen(QColor("#00d2ff"), 3.0, Qt::SolidLine));
                p.drawLine(geom.pTL, geom.pBL);
            }
            if (rightActive) {
                p.setPen(QPen(QColor("#00d2ff"), 3.0, Qt::SolidLine));
                p.drawLine(geom.pTR, geom.pBR);
            }

            auto drawSidePill = [&](const QPointF &pt, bool isActive) {
                p.save();
                p.translate(pt);
                p.rotate(deg);
                QRectF pillRect(-3.5, -9.0, 7.0, 18.0);
                p.setPen(QPen(isActive ? QColor("#00d2ff") : QColor("#1f6feb"), 1.5));
                p.setBrush(isActive ? QColor("#00d2ff") : Qt::white);
                p.drawRoundedRect(pillRect, 3.5, 3.5);
                p.restore();
            };

            drawSidePill(geom.pML, leftActive);
            drawSidePill(geom.pMR, rightActive);

            if (m_isResizingWidth) {
                QPointF handlePos = leftActive ? geom.pML : geom.pMR;
                QString widthLabel = QString("↔ Ancho: %1 px").arg(m_activeWidthValue);

                QFont wf = p.font();
                wf.setPixelSize(11);
                wf.setBold(true);
                p.setFont(wf);
                QFontMetrics fm(wf);
                int bw = fm.horizontalAdvance(widthLabel) + 16;
                QRectF badgeRect(handlePos.x() + 14, handlePos.y() - 11, bw, 22);
                if (badgeRect.right() > dRect.right()) {
                    badgeRect.moveLeft(handlePos.x() - bw - 14);
                }
                p.fillRect(badgeRect, QColor(20, 23, 29, 230));
                p.setPen(QColor("#00d2ff"));
                p.drawRect(badgeRect);
                p.setPen(Qt::white);
                p.drawText(badgeRect, Qt::AlignCenter, widthLabel);
            }
        }

        // Draw Keyframe Badge at top-right of gizmo
        TimelineClip *selectedClip = (m_model && m_selectedClipId > 0) ? m_model->findClip(m_selectedClipId) : nullptr;
        if (selectedClip) {
            const qint64 offset = m_currentPositionMs - selectedClip->timelineInMs();
            bool hasKfNow = selectedClip->animPosition().hasKeyframeAt(offset, 15) ||
                            selectedClip->animRotation().hasKeyframeAt(offset, 15) ||
                            selectedClip->animScaleX().hasKeyframeAt(offset, 15);
            bool isAnim = selectedClip->animPosition().isAnimated() ||
                          selectedClip->animRotation().isAnimated() ||
                          selectedClip->animScaleX().isAnimated();

            p.save();
            p.setRenderHint(QPainter::Antialiasing, true);
            QRectF kfBadge = geom.kfBadgeRect;
            p.setPen(QPen(hasKfNow ? QColor("#f1e05a") : (isAnim ? QColor("#58a6ff") : QColor("#30363d")), 1.5));
            p.setBrush(hasKfNow ? QColor("#f1e05a") : QColor(22, 27, 34, 230));
            p.drawRoundedRect(kfBadge, 3, 3);

            p.setPen(hasKfNow ? QColor("#0d1117") : (isAnim ? QColor("#58a6ff") : QColor("#8b949e")));
            QFont kfFont = p.font();
            kfFont.setBold(true);
            kfFont.setPixelSize(11);
            p.setFont(kfFont);
            p.drawText(kfBadge, Qt::AlignCenter, "◆");
            p.restore();
        }
    }
}

void MonitorWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    TimelineClip *motionClip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (motionClip && motionClip->hasMotionPath()) {
        const MotionPath &mPath = motionClip->motionPath();
        const auto &wps = mPath.waypoints();
        QSize canvas169(1920, 1080);
        QSize scaledCanvas = canvas169.scaled(size(), Qt::KeepAspectRatio);
        QRect dRect((width() - scaledCanvas.width()) / 2, (height() - scaledCanvas.height()) / 2, scaledCanvas.width(), scaledCanvas.height());

        // Check if clicked on Bézier handles first (radius ~9px)
        for (int i = 0; i < wps.size(); ++i) {
            const auto &wp = wps[i];
            if (wp.isCurved || !wp.handleOut.isNull()) {
                QPointF sHandleOut = canvasToScreen(wp.pos + wp.handleOut, dRect);
                if (std::hypot(event->position().x() - sHandleOut.x(), event->position().y() - sHandleOut.y()) <= 9.0) {
                    m_currentAction = GizmoAction::DragMotionHandleOut;
                    m_activeMotionPointIndex = i;
                    m_initialMousePos = event->position();
                    m_initialMotionPath = mPath;
                    return;
                }
            }
            if (wp.isCurved || !wp.handleIn.isNull()) {
                QPointF sHandleIn = canvasToScreen(wp.pos + wp.handleIn, dRect);
                if (std::hypot(event->position().x() - sHandleIn.x(), event->position().y() - sHandleIn.y()) <= 9.0) {
                    m_currentAction = GizmoAction::DragMotionHandleIn;
                    m_activeMotionPointIndex = i;
                    m_initialMousePos = event->position();
                    m_initialMotionPath = mPath;
                    return;
                }
            }
        }

        // Check if clicked on Waypoints (Start, End, Intermediate - radius ~12px or badge)
        for (int i = 0; i < wps.size(); ++i) {
            QPointF sPos = canvasToScreen(wps[i].pos, dRect);
            QRectF badgeRect(sPos.x() + 7, sPos.y() - 20, 60, 20);
            if (std::hypot(event->position().x() - sPos.x(), event->position().y() - sPos.y()) <= 12.0 ||
                badgeRect.contains(event->position())) {
                m_currentAction = GizmoAction::DragMotionPoint;
                m_activeMotionPointIndex = i;
                m_initialMousePos = event->position();
                m_initialMotionPath = mPath;
                return;
            }
        }
    }

    GizmoGeometry geom = calculateGizmoGeometry();
    GizmoAction action = hitTest(event->position(), geom);

    if (action == GizmoAction::ToggleKeyframe && geom.isValid) {
        TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
        if (clip) {
            const qint64 offset = m_currentPositionMs - clip->timelineInMs();
            if (offset >= 0 && offset <= clip->durationMs()) {
                bool hasKfNow = clip->animPosition().hasKeyframeAt(offset, 15) ||
                                clip->animRotation().hasKeyframeAt(offset, 15) ||
                                clip->animScaleX().hasKeyframeAt(offset, 15);
                if (hasKfNow) {
                    clip->animPosition().removeKeyframeAt(offset, 15);
                    clip->animRotation().removeKeyframeAt(offset, 15);
                    clip->animScaleX().removeKeyframeAt(offset, 15);
                    clip->animScaleY().removeKeyframeAt(offset, 15);
                } else {
                    clip->animPosition().setKeyframe(offset, clip->positionAt(m_currentPositionMs));
                    clip->animRotation().setKeyframe(offset, clip->rotationAt(m_currentPositionMs));
                    clip->animScaleX().setKeyframe(offset, clip->scaleXAt(m_currentPositionMs));
                    clip->animScaleY().setKeyframe(offset, clip->scaleYAt(m_currentPositionMs));
                }
                emit clipTransformChanged(clip->id());
                update();
            }
        }
        return;
    }

    if (action != GizmoAction::None && geom.isValid) {
        TimelineClip *clip = m_model->findClip(m_selectedClipId);
        if (clip) {
            m_currentAction = action;
            m_initialMousePos = event->position();
            m_initialClipPosX = clip->posXAt(m_currentPositionMs);
            m_initialClipPosY = clip->posYAt(m_currentPositionMs);
            m_initialMouseDist = std::hypot(event->position().x() - geom.centerScreen.x(),
                                            event->position().y() - geom.centerScreen.y());
            if (m_initialMouseDist < 1.0) m_initialMouseDist = 1.0;
            m_initialClipScaleX = clip->scaleXAt(m_currentPositionMs);
            m_initialClipScaleY = clip->scaleYAt(m_currentPositionMs);
            m_initialMouseAngle = qRadiansToDegrees(std::atan2(event->position().y() - geom.centerScreen.y(),
                                                              event->position().x() - geom.centerScreen.x()));
            m_initialClipRotation = clip->rotationAt(m_currentPositionMs);

            QSize canvas169(1920, 1080);
            QSize baseSize = getClipBaseSize(clip, canvas169);
            m_initialTextBoxWidth = (clip->textBoxWidth() > 50) ? clip->textBoxWidth() : baseSize.width();
            m_isResizingWidth = (action == GizmoAction::ResizeLeft || action == GizmoAction::ResizeRight);
            m_activeWidthValue = qRound(m_initialTextBoxWidth);
            return;
        }
    }

    // If clicked inside monitor, check if clicked on any visible clip
    if (m_model) {
        const auto &vTracks = m_model->videoTracks();
        for (const TimelineTrack &track : vTracks) {
            if (!track.isVisible()) continue;
            const TimelineClip *c = track.clipAtTime(m_currentPositionMs);
            if (c) {
                m_selectedClipId = c->id();
                emit clipSelected(c->id());
                update();
                return;
            }
        }
    }

    QWidget::mousePressEvent(event);
}

void MonitorWidget::mouseMoveEvent(QMouseEvent *event)
{
    GizmoGeometry geom = calculateGizmoGeometry();

    if (m_currentAction == GizmoAction::None) {
        TimelineClip *motionClip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
        if (motionClip && motionClip->hasMotionPath()) {
            QSize canvas169(1920, 1080);
            QSize scaledCanvas = canvas169.scaled(size(), Qt::KeepAspectRatio);
            QRect dRect((width() - scaledCanvas.width()) / 2, (height() - scaledCanvas.height()) / 2, scaledCanvas.width(), scaledCanvas.height());
            const auto &wps = motionClip->motionPath().waypoints();
            for (int i = 0; i < wps.size(); ++i) {
                if (!wps[i].handleOut.isNull() || wps[i].isCurved) {
                    QPointF sH = canvasToScreen(wps[i].pos + wps[i].handleOut, dRect);
                    if (std::hypot(event->position().x() - sH.x(), event->position().y() - sH.y()) <= 9.0) {
                        setCursor(Qt::PointingHandCursor);
                        setToolTip("Tirador Bézier (Arrastra para ajustar curvatura, Alt para ángulo libre)");
                        return;
                    }
                }
                if (!wps[i].handleIn.isNull() || wps[i].isCurved) {
                    QPointF sH = canvasToScreen(wps[i].pos + wps[i].handleIn, dRect);
                    if (std::hypot(event->position().x() - sH.x(), event->position().y() - sH.y()) <= 9.0) {
                        setCursor(Qt::PointingHandCursor);
                        setToolTip("Tirador Bézier (Arrastra para ajustar curvatura, Alt para ángulo libre)");
                        return;
                    }
                }
                QPointF sPos = canvasToScreen(wps[i].pos, dRect);
                QRectF badgeRect(sPos.x() + 7, sPos.y() - 20, 60, 20);
                if (std::hypot(event->position().x() - sPos.x(), event->position().y() - sPos.y()) <= 12.0 ||
                    badgeRect.contains(event->position())) {
                    setCursor(Qt::SizeAllCursor);
                    if (i == 0) setToolTip("Punto de INICIO (Arrastra para mover con Snap; mantén Alt para movimiento libre)");
                    else if (i == wps.size() - 1) setToolTip("Punto de FIN (Arrastra para mover con Snap; mantén Alt para movimiento libre)");
                    else setToolTip(QString("Punto %1 (Arrastra para mover con Snap, doble clic para alternar curvatura)").arg(i + 1));
                    return;
                }
            }
        }

        // Update cursor on hover
        GizmoAction hoverAction = hitTest(event->position(), geom);
        TimelineClip *hoverClip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
        double rotDeg = hoverClip ? hoverClip->rotation() : 0.0;

        auto getBorderCursor = [](double rotationDeg) -> QCursor {
            double angle = std::fmod(std::fmod(rotationDeg, 180.0) + 180.0, 180.0);
            if (angle < 22.5 || angle >= 157.5) return Qt::SizeHorCursor;
            if (angle >= 22.5 && angle < 67.5) return Qt::SizeFDiagCursor;
            if (angle >= 67.5 && angle < 112.5) return Qt::SizeVerCursor;
            return Qt::SizeBDiagCursor;
        };

        switch (hoverAction) {
        case GizmoAction::ToggleKeyframe:
            setCursor(Qt::PointingHandCursor);
            setToolTip("Alternar fotograma clave de transformación en el cabezal");
            break;
        case GizmoAction::Rotate:
            setCursor(Qt::PointingHandCursor);
            break;
        case GizmoAction::ResizeTL:
        case GizmoAction::ResizeBR:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case GizmoAction::ResizeTR:
        case GizmoAction::ResizeBL:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case GizmoAction::ResizeLeft:
        case GizmoAction::ResizeRight:
            setCursor(getBorderCursor(rotDeg));
            break;
        case GizmoAction::Move:
            setCursor(Qt::SizeAllCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
        }
        return;
    }

    if (m_currentAction == GizmoAction::DragMotionPoint) {
        TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
        if (clip && m_activeMotionPointIndex >= 0 && m_activeMotionPointIndex < clip->motionPath().waypointCount()) {
            QSize canvas169(1920, 1080);
            QSize scaledCanvas = canvas169.scaled(size(), Qt::KeepAspectRatio);
            QRect dRect((width() - scaledCanvas.width()) / 2, (height() - scaledCanvas.height()) / 2, scaledCanvas.width(), scaledCanvas.height());
            QPointF newCanvasPos = screenToCanvas(event->position(), dRect);

            QSize baseSize = getClipBaseSize(clip, canvas169);
            float w = baseSize.width() * clip->scaleX();
            float h = baseSize.height() * clip->scaleY();

            float desiredCenterX = 960.0f + newCanvasPos.x();
            float desiredCenterY = 540.0f + newCanvasPos.y();

            Snapping::RotatedRect movingElement(desiredCenterX, desiredCenterY, w, h, clip->rotation());
            Snapping::Rect canvasRect(0.0f, 0.0f, 1920.0f, 1080.0f);

            std::vector<Snapping::RotatedRect> otherElements;
            if (m_model) {
                for (const TimelineTrack &track : m_model->videoTracks()) {
                    if (!track.isVisible()) continue;
                    const TimelineClip *oc = track.clipAtTime(m_currentPositionMs);
                    if (oc && oc->id() != clip->id()) {
                        QSize oBase = getClipBaseSize(oc, canvas169);
                        float ow = oBase.width() * oc->scaleX();
                        float oh = oBase.height() * oc->scaleY();
                        float ocx = 960.0f + oc->posX();
                        float ocy = 540.0f + oc->posY();
                        otherElements.emplace_back(ocx, ocy, ow, oh, oc->rotation());
                    }
                }
            }

            MotionPath path = clip->motionPath();
            auto wps = path.waypoints();
            for (int j = 0; j < wps.size(); ++j) {
                if (j == m_activeMotionPointIndex) continue;
                float otherWpCenterX = 960.0f + wps[j].pos.x();
                float otherWpCenterY = 540.0f + wps[j].pos.y();
                otherElements.emplace_back(otherWpCenterX, otherWpCenterY, w, h, clip->rotation());
            }

            bool isAltPressed = (event->modifiers() & Qt::AltModifier);
            Snapping::SnapSettings effectiveSettings = m_snapSettings;
            if (isAltPressed) effectiveSettings.enabled = false;

            float scaleFactor = static_cast<float>(dRect.width()) / 1920.0f;
            Snapping::SnapResult snapRes = m_snapEngine.snapMove(
                movingElement, canvasRect, otherElements, effectiveSettings, scaleFactor
            );

            m_activeGuides = snapRes.guides;

            QPointF snappedCanvasPos(snapRes.snappedCenter.x - 960.0f, snapRes.snappedCenter.y - 540.0f);
            wps[m_activeMotionPointIndex].pos = snappedCanvasPos;
            path.setWaypoints(wps);
            clip->setMotionPath(path);
            emit clipTransformChanged(m_selectedClipId);
            update();
        }
        return;
    }
    if (m_currentAction == GizmoAction::DragMotionHandleOut) {
        TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
        if (clip && m_activeMotionPointIndex >= 0 && m_activeMotionPointIndex < clip->motionPath().waypointCount()) {
            QSize canvas169(1920, 1080);
            QSize scaledCanvas = canvas169.scaled(size(), Qt::KeepAspectRatio);
            QRect dRect((width() - scaledCanvas.width()) / 2, (height() - scaledCanvas.height()) / 2, scaledCanvas.width(), scaledCanvas.height());
            QPointF mouseCanvas = screenToCanvas(event->position(), dRect);
            MotionPath path = clip->motionPath();
            auto wps = path.waypoints();
            QPointF delta = mouseCanvas - wps[m_activeMotionPointIndex].pos;
            wps[m_activeMotionPointIndex].handleOut = delta;
            wps[m_activeMotionPointIndex].isCurved = true;
            if (!(event->modifiers() & Qt::AltModifier)) {
                wps[m_activeMotionPointIndex].handleIn = -delta;
            }
            path.setWaypoints(wps);
            clip->setMotionPath(path);
            emit clipTransformChanged(m_selectedClipId);
            update();
        }
        return;
    }
    if (m_currentAction == GizmoAction::DragMotionHandleIn) {
        TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
        if (clip && m_activeMotionPointIndex >= 0 && m_activeMotionPointIndex < clip->motionPath().waypointCount()) {
            QSize canvas169(1920, 1080);
            QSize scaledCanvas = canvas169.scaled(size(), Qt::KeepAspectRatio);
            QRect dRect((width() - scaledCanvas.width()) / 2, (height() - scaledCanvas.height()) / 2, scaledCanvas.width(), scaledCanvas.height());
            QPointF mouseCanvas = screenToCanvas(event->position(), dRect);
            MotionPath path = clip->motionPath();
            auto wps = path.waypoints();
            QPointF delta = mouseCanvas - wps[m_activeMotionPointIndex].pos;
            wps[m_activeMotionPointIndex].handleIn = delta;
            wps[m_activeMotionPointIndex].isCurved = true;
            if (!(event->modifiers() & Qt::AltModifier)) {
                wps[m_activeMotionPointIndex].handleOut = -delta;
            }
            path.setWaypoints(wps);
            clip->setMotionPath(path);
            emit clipTransformChanged(m_selectedClipId);
            update();
        }
        return;
    }

    TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (!clip || !geom.isValid) return;

    if (m_currentAction == GizmoAction::Move) {
        double deltaScreenX = event->position().x() - m_initialMousePos.x();
        double deltaScreenY = event->position().y() - m_initialMousePos.y();
        double deltaCanvasX = deltaScreenX / (geom.scaleScreenX > 0 ? geom.scaleScreenX : 1.0);
        double deltaCanvasY = deltaScreenY / (geom.scaleScreenY > 0 ? geom.scaleScreenY : 1.0);

        QSize canvas169(1920, 1080);
        QSize baseSize = getClipBaseSize(clip, canvas169);
        float w = baseSize.width() * clip->scaleX();
        float h = baseSize.height() * clip->scaleY();

        float desiredCenterX = 960.0f + m_initialClipPosX + deltaCanvasX;
        float desiredCenterY = 540.0f + m_initialClipPosY + deltaCanvasY;

        Snapping::RotatedRect movingElement(desiredCenterX, desiredCenterY, w, h, clip->rotation());
        Snapping::Rect canvasRect(0.0f, 0.0f, 1920.0f, 1080.0f);

        std::vector<Snapping::RotatedRect> otherElements;
        if (m_model) {
            for (const TimelineTrack &track : m_model->videoTracks()) {
                if (!track.isVisible()) continue;
                const TimelineClip *oc = track.clipAtTime(m_currentPositionMs);
                if (oc && oc->id() != clip->id()) {
                    QSize oBase = getClipBaseSize(oc, canvas169);
                    float ow = oBase.width() * oc->scaleX();
                    float oh = oBase.height() * oc->scaleY();
                    float ocx = 960.0f + oc->posX();
                    float ocy = 540.0f + oc->posY();
                    otherElements.emplace_back(ocx, ocy, ow, oh, oc->rotation());
                }
            }
        }

        bool isAltPressed = (event->modifiers() & Qt::AltModifier);
        Snapping::SnapSettings effectiveSettings = m_snapSettings;
        if (isAltPressed) effectiveSettings.enabled = false;

        Snapping::SnapResult snapRes = m_snapEngine.snapMove(
            movingElement, canvasRect, otherElements, effectiveSettings, geom.scaleScreenX
        );

        m_activeGuides = snapRes.guides;

        float finalPosX = snapRes.snappedCenter.x - 960.0f;
        float finalPosY = snapRes.snappedCenter.y - 540.0f;
        clip->setPosX(finalPosX);
        clip->setPosY(finalPosY);

        const qint64 offset = qBound(0LL, m_currentPositionMs - clip->timelineInMs(), clip->durationMs());
        if (m_autoKeyframe || clip->animPosition().isAnimated()) {
            if (!clip->animPosition().isAnimated() && offset > 0) {
                clip->animPosition().setKeyframe(0, QPointF(m_initialClipPosX, m_initialClipPosY));
            }
            clip->animPosition().setKeyframe(offset, QPointF(finalPosX, finalPosY));
        }

        emit clipTransformChanged(clip->id());
        update();
    } else if (m_currentAction == GizmoAction::Rotate) {
        double currentAngle = qRadiansToDegrees(std::atan2(event->position().y() - geom.centerScreen.y(),
                                                          event->position().x() - geom.centerScreen.x()));
        double deltaAngle = currentAngle - m_initialMouseAngle;
        double rawRotation = m_initialClipRotation + deltaAngle;

        bool isAltPressed = (event->modifiers() & Qt::AltModifier);
        bool isShiftPressed = (event->modifiers() & Qt::ShiftModifier);
        Snapping::SnapSettings effectiveSettings = m_snapSettings;
        if (isAltPressed) effectiveSettings.enabled = false;

        auto rotRes = m_snapEngine.snapRotation(rawRotation, effectiveSettings, isShiftPressed);
        float finalRot = rotRes.snappedAngle;
        clip->setRotation(finalRot);

        const qint64 offset = qBound(0LL, m_currentPositionMs - clip->timelineInMs(), clip->durationMs());
        if (m_autoKeyframe || clip->animRotation().isAnimated()) {
            if (!clip->animRotation().isAnimated() && offset > 0) {
                clip->animRotation().setKeyframe(0, m_initialClipRotation);
            }
            clip->animRotation().setKeyframe(offset, finalRot);
        }

        m_activeGuides.clear();
        m_isRotating = true;
        m_activeRotationSnap = rotRes.snapped;
        m_activeRotationAngle = rotRes.snappedAngle;
        int displayAngle = static_cast<int>(std::round(rotRes.snappedAngle)) % 360;
        if (displayAngle < 0) displayAngle += 360;
        m_activeRotationLabel = rotRes.snapped ? rotRes.label : QString("Ángulo: %1°").arg(displayAngle);

        emit clipTransformChanged(clip->id());
        update();
    } else if (m_currentAction == GizmoAction::ResizeLeft || m_currentAction == GizmoAction::ResizeRight) {
        double deltaScreenX = event->position().x() - m_initialMousePos.x();
        double deltaScreenY = event->position().y() - m_initialMousePos.y();
        double deltaCanvasX = deltaScreenX / (geom.scaleScreenX > 0 ? geom.scaleScreenX : 1.0);
        double deltaCanvasY = deltaScreenY / (geom.scaleScreenY > 0 ? geom.scaleScreenY : 1.0);

        double rad = qDegreesToRadians(clip->rotation());
        double cosR = std::cos(rad);
        double sinR = std::sin(rad);

        // Project mouse displacement onto the local width axis of the box
        double deltaLocalX = deltaCanvasX * cosR + deltaCanvasY * sinR;
        double scaleX = clip->scaleX() > 0.05 ? clip->scaleX() : 1.0;
        double deltaTextBoxW = deltaLocalX / scaleX;

        bool isAltPressed = (event->modifiers() & Qt::AltModifier);

        double newWidth = m_initialTextBoxWidth;
        double deltaW = 0.0;

        if (m_currentAction == GizmoAction::ResizeRight) {
            if (isAltPressed) {
                // Symmetric expansion from center
                newWidth = qMax(80.0, m_initialTextBoxWidth + 2.0 * deltaTextBoxW);
                clip->setPosX(m_initialClipPosX);
                clip->setPosY(m_initialClipPosY);
            } else {
                // Left edge remains pinned in place
                newWidth = qMax(80.0, m_initialTextBoxWidth + deltaTextBoxW);
                deltaW = newWidth - m_initialTextBoxWidth;
                double shiftLocalX = (deltaW * scaleX) / 2.0;
                clip->setPosX(m_initialClipPosX + shiftLocalX * cosR);
                clip->setPosY(m_initialClipPosY + shiftLocalX * sinR);
            }
        } else { // ResizeLeft
            if (isAltPressed) {
                // Symmetric expansion from center
                newWidth = qMax(80.0, m_initialTextBoxWidth - 2.0 * deltaTextBoxW);
                clip->setPosX(m_initialClipPosX);
                clip->setPosY(m_initialClipPosY);
            } else {
                // Right edge remains pinned in place
                newWidth = qMax(80.0, m_initialTextBoxWidth - deltaTextBoxW);
                deltaW = newWidth - m_initialTextBoxWidth;
                double shiftLocalX = -(deltaW * scaleX) / 2.0;
                clip->setPosX(m_initialClipPosX + shiftLocalX * cosR);
                clip->setPosY(m_initialClipPosY + shiftLocalX * sinR);
            }
        }

        int finalWidth = qRound(newWidth);
        clip->setTextBoxWidth(finalWidth);

        const qint64 offset = qBound(0LL, m_currentPositionMs - clip->timelineInMs(), clip->durationMs());
        if (m_autoKeyframe || clip->animPosition().isAnimated()) {
            if (!clip->animPosition().isAnimated() && offset > 0) {
                clip->animPosition().setKeyframe(0, QPointF(m_initialClipPosX, m_initialClipPosY));
            }
            clip->animPosition().setKeyframe(offset, QPointF(clip->posX(), clip->posY()));
        }

        m_activeWidthValue = finalWidth;
        m_isResizingWidth = true;

        emit clipTransformChanged(clip->id());
        update();
    } else { // Resizing corners
        double currentDist = std::hypot(event->position().x() - geom.centerScreen.x(),
                                        event->position().y() - geom.centerScreen.y());
        double ratio = currentDist / m_initialMouseDist;
        float newScaleX = qBound(0.05, m_initialClipScaleX * ratio, 10.0);
        float newScaleY = qBound(0.05, m_initialClipScaleY * ratio, 10.0);

        QSize canvas169(1920, 1080);
        QSize baseSize = getClipBaseSize(clip, canvas169);

        float w = baseSize.width() * newScaleX;
        float h = baseSize.height() * newScaleY;
        float cx = 960.0f + clip->posX();
        float cy = 540.0f + clip->posY();

        Snapping::Rect resizingBounds(cx - w * 0.5f, cy - h * 0.5f, w, h);
        Snapping::Rect canvasRect(0.0f, 0.0f, 1920.0f, 1080.0f);

        std::vector<Snapping::Rect> otherElements;
        if (m_model) {
            for (const TimelineTrack &track : m_model->videoTracks()) {
                if (!track.isVisible()) continue;
                const TimelineClip *oc = track.clipAtTime(m_currentPositionMs);
                if (oc && oc->id() != clip->id()) {
                    QSize oBase = getClipBaseSize(oc, canvas169);
                    float ow = oBase.width() * oc->scaleX();
                    float oh = oBase.height() * oc->scaleY();
                    float ocx = 960.0f + oc->posX();
                    float ocy = 540.0f + oc->posY();
                    otherElements.push_back(Snapping::RotatedRect(ocx, ocy, ow, oh, oc->rotation()).boundingRect());
                }
            }
        }

        bool isAltPressed = (event->modifiers() & Qt::AltModifier);
        Snapping::SnapSettings effectiveSettings = m_snapSettings;
        if (isAltPressed) effectiveSettings.enabled = false;

        auto resizeRes = m_snapEngine.snapResize(resizingBounds, canvasRect, otherElements, effectiveSettings, geom.scaleScreenX);
        m_activeGuides = resizeRes.guides;

        if (resizeRes.snappedW && baseSize.width() > 0) {
            newScaleX = resizeRes.rect.width / baseSize.width();
            newScaleY = newScaleX;
        }
        if (resizeRes.snappedH && baseSize.height() > 0) {
            newScaleY = resizeRes.rect.height / baseSize.height();
            newScaleX = newScaleY;
        }

        clip->setScaleX(newScaleX);
        clip->setScaleY(newScaleY);

        const qint64 offset = qBound(0LL, m_currentPositionMs - clip->timelineInMs(), clip->durationMs());
        if (m_autoKeyframe || clip->animScaleX().isAnimated()) {
            if (!clip->animScaleX().isAnimated() && offset > 0) {
                clip->animScaleX().setKeyframe(0, m_initialClipScaleX);
                clip->animScaleY().setKeyframe(0, m_initialClipScaleY);
            }
            clip->animScaleX().setKeyframe(offset, newScaleX);
            clip->animScaleY().setKeyframe(offset, newScaleY);
        }

        emit clipTransformChanged(clip->id());
        update();
    }
}

void MonitorWidget::mouseReleaseEvent(QMouseEvent *event)
{
    m_activeGuides.clear();
    m_snapEngine.resetHysteresis();
    m_isRotating = false;
    m_activeRotationSnap = false;
    m_activeRotationLabel.clear();
    m_isResizingWidth = false;

    if (m_currentAction == GizmoAction::DragMotionPoint ||
        m_currentAction == GizmoAction::DragMotionHandleIn ||
        m_currentAction == GizmoAction::DragMotionHandleOut) {
        TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
        if (clip && m_model) {
            MotionPath finalPath = clip->motionPath();
            clip->setMotionPath(m_initialMotionPath);
            m_model->setClipMotionPath(m_selectedClipId, finalPath);
        }
        m_currentAction = GizmoAction::None;
        m_activeMotionPointIndex = -1;
        update();
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (m_currentAction != GizmoAction::None) {
        m_currentAction = GizmoAction::None;
        if (m_model) {
            m_model->notifyChange();
        }
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void MonitorWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (clip && clip->hasMotionPath()) {
        QSize canvas169(1920, 1080);
        QSize scaledCanvas = canvas169.scaled(size(), Qt::KeepAspectRatio);
        QRect dRect((width() - scaledCanvas.width()) / 2, (height() - scaledCanvas.height()) / 2, scaledCanvas.width(), scaledCanvas.height());

        MotionPath path = clip->motionPath();
        const auto &wps = path.waypoints();

        // 1. Check if double-clicked on existing waypoint -> toggle curvature (Linear <-> Curved)
        for (int i = 0; i < wps.size(); ++i) {
            QPointF sPos = canvasToScreen(wps[i].pos, dRect);
            if (std::hypot(event->position().x() - sPos.x(), event->position().y() - sPos.y()) <= 12.0) {
                auto newWps = wps;
                newWps[i].isCurved = !newWps[i].isCurved;
                if (newWps[i].isCurved) {
                    QPointF dir(80, 0);
                    if (i > 0 && i < wps.size() - 1) {
                        dir = (wps[i + 1].pos - wps[i - 1].pos) / 4.0;
                    }
                    newWps[i].handleOut = dir;
                    newWps[i].handleIn = -dir;
                } else {
                    newWps[i].handleIn = QPointF(0, 0);
                    newWps[i].handleOut = QPointF(0, 0);
                }
                path.setWaypoints(newWps);
                m_model->setClipMotionPath(m_selectedClipId, path);
                emit clipTransformChanged(m_selectedClipId);
                update();
                return;
            }
        }

        // 2. Check if double-clicked near path curve -> insert new waypoint at clicked location!
        QPointF clickPos = event->position();
        double bestDist = 1e9;
        double bestU = 0.5;
        for (int s = 0; s <= 100; ++s) {
            double u = s / 100.0;
            QPointF sPos = canvasToScreen(path.evaluate(u), dRect);
            double dist = std::hypot(clickPos.x() - sPos.x(), clickPos.y() - sPos.y());
            if (dist < bestDist) {
                bestDist = dist;
                bestU = u;
            }
        }

        if (bestDist <= 25.0) {
            QPointF newCanvasPos = screenToCanvas(clickPos, dRect);
            int numSegs = wps.size() - 1;
            int insertIndex = qBound(1, static_cast<int>(bestU * numSegs) + 1, wps.size() - 1);
            MotionWaypoint newWp(newCanvasPos, true, QPointF(-60, 0), QPointF(60, 0));
            path.insertWaypoint(insertIndex, newWp);
            m_model->setClipMotionPath(m_selectedClipId, path);
            emit clipTransformChanged(m_selectedClipId);
            update();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void MonitorWidget::contextMenuEvent(QContextMenuEvent *event)
{
    TimelineClip *clip = m_model ? m_model->findClip(m_selectedClipId) : nullptr;
    if (!clip) {
        QWidget::contextMenuEvent(event);
        return;
    }

    QMenu menu(this);
    const qint64 offset = m_currentPositionMs - clip->timelineInMs();
    bool inRange = (offset >= 0 && offset <= clip->durationMs());

    if (inRange) {
        bool hasKf = clip->animPosition().hasKeyframeAt(offset, 15) ||
                     clip->animRotation().hasKeyframeAt(offset, 15) ||
                     clip->animScaleX().hasKeyframeAt(offset, 15);
        if (hasKf) {
            QAction *delKf = menu.addAction("Eliminar fotograma clave de transformación aquí");
            connect(delKf, &QAction::triggered, this, [this, clip, offset]() {
                clip->animPosition().removeKeyframeAt(offset, 15);
                clip->animRotation().removeKeyframeAt(offset, 15);
                clip->animScaleX().removeKeyframeAt(offset, 15);
                clip->animScaleY().removeKeyframeAt(offset, 15);
                emit clipTransformChanged(clip->id());
                update();
            });
        } else {
            QAction *addKf = menu.addAction("Insertar fotograma clave de transformación aquí");
            connect(addKf, &QAction::triggered, this, [this, clip, offset]() {
                clip->animPosition().setKeyframe(offset, clip->positionAt(m_currentPositionMs));
                clip->animRotation().setKeyframe(offset, clip->rotationAt(m_currentPositionMs));
                clip->animScaleX().setKeyframe(offset, clip->scaleXAt(m_currentPositionMs));
                clip->animScaleY().setKeyframe(offset, clip->scaleYAt(m_currentPositionMs));
                emit clipTransformChanged(clip->id());
                update();
            });
        }

        if (clip->animPosition().isAnimated() || clip->animRotation().isAnimated() || clip->animScaleX().isAnimated()) {
            QAction *clearAnim = menu.addAction("Limpiar animaciones de transformación");
            connect(clearAnim, &QAction::triggered, this, [this, clip]() {
                clip->animPosition().clearKeyframes();
                clip->animRotation().clearKeyframes();
                clip->animScaleX().clearKeyframes();
                clip->animScaleY().clearKeyframes();
                emit clipTransformChanged(clip->id());
                update();
            });
        }
    }

    menu.addSeparator();
    QAction *autoKfAction = menu.addAction("Auto-Keyframe");
    autoKfAction->setCheckable(true);
    autoKfAction->setChecked(m_autoKeyframe);
    connect(autoKfAction, &QAction::toggled, this, [this](bool checked) {
        setAutoKeyframe(checked);
    });

    menu.addSeparator();
    QAction *centerAction = menu.addAction("Centrar en el lienzo");
    connect(centerAction, &QAction::triggered, this, [this, clip, offset, inRange]() {
        clip->setPosX(0.0f);
        clip->setPosY(0.0f);
        if (inRange && (m_autoKeyframe || clip->animPosition().isAnimated())) {
            clip->animPosition().setKeyframe(offset, QPointF(0.0, 0.0));
        }
        emit clipTransformChanged(clip->id());
        update();
    });

    QAction *resetAction = menu.addAction("Restablecer transformación (100%, 0°, Centro)");
    connect(resetAction, &QAction::triggered, this, [this, clip, offset, inRange]() {
        clip->setPosX(0.0f);
        clip->setPosY(0.0f);
        clip->setScaleX(1.0f);
        clip->setScaleY(1.0f);
        clip->setRotation(0.0f);
        if (inRange && (m_autoKeyframe || clip->animPosition().isAnimated())) {
            clip->animPosition().setKeyframe(offset, QPointF(0.0, 0.0));
            clip->animScaleX().setKeyframe(offset, 1.0f);
            clip->animScaleY().setKeyframe(offset, 1.0f);
            clip->animRotation().setKeyframe(offset, 0.0f);
        }
        emit clipTransformChanged(clip->id());
        update();
    });

    menu.exec(event->globalPos());
}

void MonitorWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MonitorWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MonitorWidget::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QStringList paths;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                paths.append(url.toLocalFile());
            }
        }
        if (!paths.isEmpty()) {
            emit filesDropped(paths);
            event->acceptProposedAction();
        }
    }
}

// ---------------------------------------------------------------------------
// PreviewWidget Implementation
// ---------------------------------------------------------------------------

PreviewWidget::PreviewWidget(TimelineModel *model, AudioEngine *audioEngine, QWidget *parent)
    : QWidget(parent)
    , m_model(model)
    , m_audioEngine(audioEngine)
{
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Interactive 16:9 Canvas monitor widget
    m_monitorWidget = new MonitorWidget(m_model, this);
    mainLayout->addWidget(m_monitorWidget, 1);

    connect(m_monitorWidget, &MonitorWidget::clipTransformChanged, this, [this](qint64 clipId) {
        renderCurrentFrame();
        emit clipTransformChanged(clipId);
    });
    connect(m_monitorWidget, &MonitorWidget::clipSelected, this, [this](qint64 clipId) {
        emit clipSelected(clipId);
    });
    connect(m_monitorWidget, &MonitorWidget::filesDropped, this, &PreviewWidget::onMonitorFilesDropped);

    // Transport Bar
    QWidget *transportBar = new QWidget(this);
    QHBoxLayout *transportLayout = new QHBoxLayout(transportBar);
    transportLayout->setContentsMargins(0, 4, 0, 0);
    transportLayout->setSpacing(6);

    // Timecode
    m_timecodeLabel = new QLabel("00:00:00.00 / 00:00:10.00", this);
    m_timecodeLabel->setStyleSheet("font-family: monospace; font-size: 12px; font-weight: bold; color: #58a6ff; min-width: 170px;");
    transportLayout->addWidget(m_timecodeLabel);

    transportLayout->addStretch();

    auto createTransportBtn = [this](const QString &text, const QString &tooltip) {
        QPushButton *btn = new QPushButton(text, this);
        btn->setToolTip(tooltip);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(32, 28);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; font-weight: bold;"
            "}"
            "QPushButton:hover { background-color: #30363d; color: white; }"
            "QPushButton:pressed { background-color: #161b22; }"
        );
        return btn;
    };

    m_jumpStartBtn = createTransportBtn("|<", "Ir al inicio (Home)");
    m_stepBackBtn = createTransportBtn("<", "Retroceder 1 cuadro (Flecha Izq)");
    m_playPauseBtn = createTransportBtn("▶", "Reproducir / Pausar (Espacio)");
    m_playPauseBtn->setFixedSize(48, 28);
    m_playPauseBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636; color: white; border-radius: 4px; font-weight: bold; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6b2c; }"
    );
    m_stepForwardBtn = createTransportBtn(">", "Avanzar 1 cuadro (Flecha Der)");
    m_jumpEndBtn = createTransportBtn(">|", "Ir al final (End)");

    connect(m_jumpStartBtn, &QPushButton::clicked, this, &PreviewWidget::jumpToStart);
    connect(m_stepBackBtn, &QPushButton::clicked, this, &PreviewWidget::stepBackward);
    connect(m_playPauseBtn, &QPushButton::clicked, this, &PreviewWidget::togglePlayPause);
    connect(m_stepForwardBtn, &QPushButton::clicked, this, &PreviewWidget::stepForward);
    connect(m_jumpEndBtn, &QPushButton::clicked, this, &PreviewWidget::jumpToEnd);

    transportLayout->addWidget(m_jumpStartBtn);
    transportLayout->addWidget(m_stepBackBtn);
    transportLayout->addWidget(m_playPauseBtn);
    transportLayout->addWidget(m_stepForwardBtn);
    transportLayout->addWidget(m_jumpEndBtn);

    m_autoKeyframeBtn = new QPushButton("◆ Auto", this);
    m_autoKeyframeBtn->setCheckable(true);
    m_autoKeyframeBtn->setChecked(false);
    m_autoKeyframeBtn->setToolTip("Auto-Keyframe: Generar o actualizar fotogramas clave al mover o rotar en el visor");
    m_autoKeyframeBtn->setCursor(Qt::PointingHandCursor);
    m_autoKeyframeBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #21262d; color: #8b949e; border: 1px solid #30363d; border-radius: 4px; font-weight: bold; font-size: 11px; padding: 2px 8px; min-width: 50px; min-height: 24px;"
        "}"
        "QPushButton:hover { background-color: #30363d; color: #e6edf3; }"
        "QPushButton:checked { background-color: #f1e05a; color: #0d1117; border-color: #d29922; }"
    );
    connect(m_autoKeyframeBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (m_monitorWidget) {
            m_monitorWidget->setAutoKeyframe(checked);
        }
    });
    transportLayout->addWidget(m_autoKeyframeBtn);

    transportLayout->addStretch();

    // Preview Quality / Resolution selector
    m_resolutionCombo = new QComboBox(this);
    m_resolutionCombo->setToolTip("Resolución de previsualización en el monitor (Optimización de RAM)");
    m_resolutionCombo->addItem("100% Completa", static_cast<int>(PreviewResolution::Full));
    m_resolutionCombo->addItem("50% Media", static_cast<int>(PreviewResolution::Half));
    m_resolutionCombo->addItem("25% Cuarto", static_cast<int>(PreviewResolution::Quarter));
    m_resolutionCombo->addItem("12.5% Borrador", static_cast<int>(PreviewResolution::Draft));
    int curResIdx = m_resolutionCombo->findData(static_cast<int>(GpuAccelerationManager::instance().previewResolution()));
    if (curResIdx >= 0) m_resolutionCombo->setCurrentIndex(curResIdx);
    m_resolutionCombo->setStyleSheet(
        "QComboBox { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 4px; padding: 2px 6px; font-size: 11px; font-weight: bold; min-height: 22px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #161b22; color: #c9d1d9; selection-background-color: #1f6feb; selection-color: white; }"
    );
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PreviewWidget::onResolutionComboChanged);
    transportLayout->addWidget(m_resolutionCombo);

    // Performance & GPU dialog button
    m_perfSettingsBtn = new QPushButton("⚙️ GPU", this);
    m_perfSettingsBtn->setToolTip("Configuración de Aceleración por GPU y Límites de Memoria RAM");
    m_perfSettingsBtn->setCursor(Qt::PointingHandCursor);
    m_perfSettingsBtn->setStyleSheet(
        "QPushButton { background-color: #21262d; color: #7ee787; border: 1px solid #30363d; border-radius: 4px; font-size: 11px; font-weight: bold; padding: 2px 8px; min-height: 22px; }"
        "QPushButton:hover { background-color: #30363d; color: #a6e3a1; border-color: #2ea043; }"
        "QPushButton:pressed { background-color: #161b22; }"
    );
    connect(m_perfSettingsBtn, &QPushButton::clicked, this, &PreviewWidget::openPerformanceDialog);
    transportLayout->addWidget(m_perfSettingsBtn);

    connect(&GpuAccelerationManager::instance(), &GpuAccelerationManager::previewResolutionChanged, this, [this](PreviewResolution res) {
        int idx = m_resolutionCombo->findData(static_cast<int>(res));
        if (idx >= 0 && m_resolutionCombo->currentIndex() != idx) {
            m_resolutionCombo->blockSignals(true);
            m_resolutionCombo->setCurrentIndex(idx);
            m_resolutionCombo->blockSignals(false);
        }
        renderCurrentFrame();
    });

    connect(&GpuAccelerationManager::instance(), &GpuAccelerationManager::gpuRenderingChanged, this, [this](bool) {
        renderCurrentFrame();
    });

    // Volume slider
    QLabel *volIcon = new QLabel("🔊", this);
    volIcon->setStyleSheet("color: #8b949e;");
    transportLayout->addWidget(volIcon);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setToolTip("Volumen Maestro");
    connect(m_volumeSlider, &QSlider::valueChanged, this, &PreviewWidget::setMasterVolume);
    transportLayout->addWidget(m_volumeSlider);

    // Stereo Audio VU Meter
    m_audioMeter = new AudioMeterWidget(this);
    transportLayout->addWidget(m_audioMeter);

    mainLayout->addWidget(transportBar);

    // Timer for video frame updates during playback
    m_videoTimer.setInterval(16); // ~60 fps smooth updates
    connect(&m_videoTimer, &QTimer::timeout, this, &PreviewWidget::onVideoTimerTick);

    if (m_audioEngine) {
        connect(m_audioEngine, &AudioEngine::positionAdvanced, this, &PreviewWidget::onAudioPositionAdvanced);
        connect(m_audioEngine, &AudioEngine::audioLevelsChanged, m_audioMeter, &AudioMeterWidget::setLevels);
    }

    if (m_model) {
        connect(m_model, &TimelineModel::timelineChanged, this, &PreviewWidget::updatePreview);
        connect(m_model, &TimelineModel::aspectRatioChanged, this, [this]() {
            updatePreview();
        });
        connect(m_model, &TimelineModel::durationChanged, this, [this]() {
            updateTimecodeLabel();
        });
    }

    renderCurrentFrame();
    updateTimecodeLabel();
}

PreviewWidget::~PreviewWidget()
{
    pause();
}

void PreviewWidget::updatePreview()
{
    if (!m_updatePreviewPending) {
        m_updatePreviewPending = true;
        QTimer::singleShot(0, this, [this]() {
            m_updatePreviewPending = false;
            renderCurrentFrame();
            updateTimecodeLabel();
        });
    }
}

void PreviewWidget::setPosition(qint64 positionMs)
{
    qint64 total = m_model ? m_model->totalDurationMs() : 10000;
    m_currentPositionMs = qBound<qint64>(0, positionMs, total);
    m_playbackStartMs = m_currentPositionMs;
    if (m_isPlaying) {
        m_playbackClock.restart();
    }
    if (m_audioEngine) {
        m_audioEngine->setPosition(m_currentPositionMs);
    }
    renderCurrentFrame();
    updateTimecodeLabel();
    emit playheadMoved(m_currentPositionMs);
}

void PreviewWidget::play()
{
    if (m_isPlaying) return;
    qint64 total = m_model ? m_model->totalDurationMs() : 10000;
    if (m_currentPositionMs >= total) {
        m_currentPositionMs = 0;
    }

    m_isPlaying = true;
    m_playbackStartMs = m_currentPositionMs;
    m_playbackClock.start();
    if (m_monitorWidget) {
        m_monitorWidget->setIsPlaying(true);
    }
    m_playPauseBtn->setText("❚❚");
    m_playPauseBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #da3633; color: white; border-radius: 4px; font-weight: bold; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #f85149; }"
        "QPushButton:pressed { background-color: #b62324; }"
    );

    if (m_audioEngine) {
        m_audioEngine->startPlayback(m_currentPositionMs);
    }
    m_videoTimer.start();
    emit playbackStateChanged(true);
}

void PreviewWidget::pause()
{
    if (!m_isPlaying) return;
    m_isPlaying = false;
    if (m_monitorWidget) {
        m_monitorWidget->setIsPlaying(false);
    }
    m_playPauseBtn->setText("▶");
    m_playPauseBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #238636; color: white; border-radius: 4px; font-weight: bold; font-size: 14px;"
        "}"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #1a6b2c; }"
    );

    if (m_audioEngine) {
        m_audioEngine->pausePlayback();
    }
    m_videoTimer.stop();
    emit playbackStateChanged(false);
}

void PreviewWidget::togglePlayPause()
{
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void PreviewWidget::stepForward()
{
    pause();
    setPosition(m_currentPositionMs + 33);
}

void PreviewWidget::stepBackward()
{
    pause();
    setPosition(m_currentPositionMs - 33);
}

void PreviewWidget::jumpToStart()
{
    pause();
    setPosition(0);
}

void PreviewWidget::jumpToEnd()
{
    pause();
    if (m_model) {
        setPosition(m_model->totalDurationMs());
    }
}

void PreviewWidget::setMasterVolume(int volumePercent)
{
    if (m_audioEngine) {
        m_audioEngine->setMasterVolume(volumePercent / 100.0);
    }
}

void PreviewWidget::onAudioPositionAdvanced(qint64 positionMs)
{
    if (!m_isPlaying) return;

    // Ignore audio position while hardware DAC is still priming its buffer
    if (positionMs <= m_playbackStartMs && m_playbackClock.elapsed() < 120) {
        return;
    }

    // Soft drift correction: lock wall-clock to audio clock when audio is active and healthy
    qint64 currentWallMs = m_playbackStartMs + m_playbackClock.elapsed();
    qint64 drift = positionMs - currentWallMs;

    // Within 45ms, lip-sync is imperceptible according to broadcast standards
    if (qAbs(drift) > 45 && qAbs(drift) < 200) {
        m_playbackStartMs += (drift > 0 ? 1 : -1);
    } else if (drift >= 200) {
        // Audio has jumped forward by 200ms+; catch video up forward
        m_playbackStartMs = positionMs;
        m_playbackClock.restart();
    }
    // If drift <= -200, audio is temporarily lagging/buffering: video continues forward
    // without rewinding, allowing AudioEngine's feed timer to catch up smoothly.
}

void PreviewWidget::onVideoTimerTick()
{
    if (!m_isPlaying) return;

    qint64 elapsedMs = m_playbackClock.elapsed();
    qint64 targetMs = m_playbackStartMs + elapsedMs;

    // Enforce strictly monotonic forward playback: playhead must NEVER retrocede during active playback
    if (targetMs < m_currentPositionMs) {
        targetMs = m_currentPositionMs;
    }

    qint64 total = m_model ? m_model->totalDurationMs() : 10000;
    if (targetMs >= total) {
        m_currentPositionMs = total;
        pause();
        emit playheadMoved(m_currentPositionMs);
        updateTimecodeLabel();
        renderCurrentFrame();
        return;
    }

    m_currentPositionMs = targetMs;
    emit playheadMoved(m_currentPositionMs);
    updateTimecodeLabel();
    renderCurrentFrame();
}

void PreviewWidget::setSelectedClipId(qint64 clipId)
{
    if (m_monitorWidget) {
        m_monitorWidget->setSelectedClipId(clipId);
    }
}

void PreviewWidget::onMonitorFilesDropped(const QStringList &filePaths)
{
    if (!m_model) return;
    qint64 lastClipId = -1;
    for (const QString &fp : filePaths) {
        MediaItem item(fp);
        qint64 dur = item.durationMs() > 0 ? item.durationMs() : 5000;
        qint64 targetTrackId = -1;
        qint64 clipId = m_model->addMediaClip(fp, item.type(), targetTrackId, m_currentPositionMs, dur, false);
        if (clipId > 0) {
            lastClipId = clipId;
        }
    }
    if (lastClipId > 0) {
        setSelectedClipId(lastClipId);
        emit clipSelected(lastClipId);
    }
    renderCurrentFrame();
}

void PreviewWidget::renderCurrentFrame()
{
    if (!m_monitorWidget) return;

    QSize canvasBase = m_model ? m_model->canvasSize() : QSize(1920, 1080);
    QSize targetPreviewSize = GpuAccelerationManager::instance().calculatePreviewSize(canvasBase);

    // Render composited frame from model
    QImage frame = VideoCompositor::renderFrame(m_model, m_currentPositionMs, targetPreviewSize, /*forPreview=*/true);
    if (frame.isNull()) {
        frame = QImage(targetPreviewSize, QImage::Format_RGB32);
        frame.fill(Qt::black);
    }

    m_monitorWidget->setCurrentPositionMs(m_currentPositionMs);
    m_monitorWidget->setFrame(frame);
    emit frameRendered(frame);
}

void PreviewWidget::onResolutionComboChanged(int index)
{
    if (index < 0 || !m_resolutionCombo) return;
    int data = m_resolutionCombo->itemData(index).toInt();
    GpuAccelerationManager::instance().setPreviewResolution(static_cast<PreviewResolution>(data));
    renderCurrentFrame();
}

void PreviewWidget::openPerformanceDialog()
{
    GpuSettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        renderCurrentFrame();
    }
}

void PreviewWidget::updateTimecodeLabel()
{
    if (!m_timecodeLabel) return;
    qint64 total = m_model ? m_model->totalDurationMs() : 10000;
    m_timecodeLabel->setText(QString("%1 / %2").arg(formatTimecode(m_currentPositionMs)).arg(formatTimecode(total)));
}

QString PreviewWidget::formatTimecode(qint64 ms) const
{
    qint64 totalSecs = ms / 1000;
    qint64 hours = totalSecs / 3600;
    qint64 mins = (totalSecs % 3600) / 60;
    qint64 secs = totalSecs % 60;
    qint64 frames = (ms % 1000) / 33; // 30 fps frames
    return QString("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(frames, 2, 10, QChar('0'));
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    renderCurrentFrame();
}

void PreviewWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space) {
        togglePlayPause();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Left) {
        stepBackward();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Right) {
        stepForward();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Home) {
        jumpToStart();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_End) {
        jumpToEnd();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}
