#include "clip.h"
#include <QtMath>
#include <QTextDocument>
#include <QPainter>
#include <QFont>
#include <QTextOption>
#include <QRegularExpression>

QString clipTypeToString(ClipType type)
{
    switch (type) {
    case ClipType::Video: return "Video";
    case ClipType::Audio: return "Audio";
    case ClipType::Image: return "Imagen";
    case ClipType::Text:  return "Texto";
    }
    return "Desconocido";
}

QList<VisualFilterInfo> allVisualFilters()
{
    static const QList<VisualFilterInfo> filters = {
        // Ninguno
        { VisualFilter::None, "Ninguno", "General" },

        // Color & Tonalidad
        { VisualFilter::Grayscale, "Escala de grises", "🎨 Color y Tonalidad" },
        { VisualFilter::Sepia, "Sepia", "🎨 Color y Tonalidad" },
        { VisualFilter::Invert, "Invertir colores", "🎨 Color y Tonalidad" },
        { VisualFilter::HighContrast, "Alto contraste", "🎨 Color y Tonalidad" },
        { VisualFilter::Brightness, "Brillo extra", "🎨 Color y Tonalidad" },
        { VisualFilter::Warm, "Calidez (Golden Hour)", "🎨 Color y Tonalidad" },
        { VisualFilter::Cool, "Tono frío (Cine Teal)", "🎨 Color y Tonalidad" },
        { VisualFilter::Vibrant, "Saturación intensa (Pop Art)", "🎨 Color y Tonalidad" },
        { VisualFilter::Desaturate, "Desaturado suave", "🎨 Color y Tonalidad" },

        // Estilizado y Cine
        { VisualFilter::Vignette, "Viñeta cinematográfica", "🎬 Estilizado y Cine" },
        { VisualFilter::VintageFilm, "Película Vintage 90s", "🎬 Estilizado y Cine" },
        { VisualFilter::Cyberpunk, "Ciberpunk (Neon Synthwave)", "🎬 Estilizado y Cine" },
        { VisualFilter::NightVision, "Visión nocturna (Fósforo verde)", "🎬 Estilizado y Cine" },
        { VisualFilter::Noir, "Cine Negro (B&W Noir)", "🎬 Estilizado y Cine" },

        // Desenfoque y Mosaico
        { VisualFilter::Blur, "Desenfoque suave", "🌫️ Desenfoque y Mosaico" },
        { VisualFilter::HeavyBlur, "Desenfoque intenso", "🌫️ Desenfoque y Mosaico" },
        { VisualFilter::Pixelate, "Pixelado / Mosaico 8-bit", "🌫️ Desenfoque y Mosaico" },

        // Glitch y Artísticos
        { VisualFilter::ChromaticAberration, "Aberración cromática (RGB Glitch)", "⚡ Glitch y Artísticos" },
        { VisualFilter::EdgeDetect, "Detección de bordes (Cómic)", "⚡ Glitch y Artísticos" },
        { VisualFilter::Posterize, "Posterizado", "⚡ Glitch y Artísticos" },
        { VisualFilter::Solarize, "Solarización (Sabattier)", "⚡ Glitch y Artísticos" },
        { VisualFilter::MirrorH, "Espejo horizontal", "⚡ Glitch y Artísticos" },
        { VisualFilter::MirrorV, "Espejo vertical", "⚡ Glitch y Artísticos" }
    };
    return filters;
}

QString visualFilterToString(VisualFilter filter)
{
    for (const VisualFilterInfo &info : allVisualFilters()) {
        if (info.filter == filter) {
            return info.name;
        }
    }
    return "Ninguno";
}

VisualFilter stringToVisualFilter(const QString &str)
{
    QString s = str.trimmed();
    for (const VisualFilterInfo &info : allVisualFilters()) {
        if (s.compare(info.name, Qt::CaseInsensitive) == 0) {
            return info.filter;
        }
    }

    // Direct identifier aliases
    if (s.compare("Grayscale", Qt::CaseInsensitive) == 0) return VisualFilter::Grayscale;
    if (s.compare("Sepia", Qt::CaseInsensitive) == 0) return VisualFilter::Sepia;
    if (s.compare("Invert", Qt::CaseInsensitive) == 0) return VisualFilter::Invert;
    if (s.compare("HighContrast", Qt::CaseInsensitive) == 0) return VisualFilter::HighContrast;
    if (s.compare("Brightness", Qt::CaseInsensitive) == 0) return VisualFilter::Brightness;
    if (s.compare("Warm", Qt::CaseInsensitive) == 0) return VisualFilter::Warm;
    if (s.compare("Cool", Qt::CaseInsensitive) == 0) return VisualFilter::Cool;
    if (s.compare("Vibrant", Qt::CaseInsensitive) == 0) return VisualFilter::Vibrant;
    if (s.compare("Desaturate", Qt::CaseInsensitive) == 0) return VisualFilter::Desaturate;
    if (s.compare("Vignette", Qt::CaseInsensitive) == 0) return VisualFilter::Vignette;
    if (s.compare("VintageFilm", Qt::CaseInsensitive) == 0 || s.compare("Vintage", Qt::CaseInsensitive) == 0) return VisualFilter::VintageFilm;
    if (s.compare("Cyberpunk", Qt::CaseInsensitive) == 0) return VisualFilter::Cyberpunk;
    if (s.compare("NightVision", Qt::CaseInsensitive) == 0) return VisualFilter::NightVision;
    if (s.compare("Noir", Qt::CaseInsensitive) == 0) return VisualFilter::Noir;
    if (s.compare("Blur", Qt::CaseInsensitive) == 0) return VisualFilter::Blur;
    if (s.compare("HeavyBlur", Qt::CaseInsensitive) == 0) return VisualFilter::HeavyBlur;
    if (s.compare("Pixelate", Qt::CaseInsensitive) == 0) return VisualFilter::Pixelate;
    if (s.compare("ChromaticAberration", Qt::CaseInsensitive) == 0 || s.compare("Glitch", Qt::CaseInsensitive) == 0) return VisualFilter::ChromaticAberration;
    if (s.compare("EdgeDetect", Qt::CaseInsensitive) == 0) return VisualFilter::EdgeDetect;
    if (s.compare("Posterize", Qt::CaseInsensitive) == 0) return VisualFilter::Posterize;
    if (s.compare("Solarize", Qt::CaseInsensitive) == 0) return VisualFilter::Solarize;
    if (s.compare("MirrorH", Qt::CaseInsensitive) == 0) return VisualFilter::MirrorH;
    if (s.compare("MirrorV", Qt::CaseInsensitive) == 0) return VisualFilter::MirrorV;

    return VisualFilter::None;
}

QList<TransitionType> allTransitionTypes()
{
    return {
        TransitionType::None,
        TransitionType::DipToBlack,
        TransitionType::DipToWhite,
        TransitionType::CrossDissolve,
        TransitionType::WipeLeft,
        TransitionType::WipeRight,
        TransitionType::WipeUp,
        TransitionType::WipeDown,
        TransitionType::SlideLeft,
        TransitionType::SlideRight,
        TransitionType::ZoomIn
    };
}

QString transitionTypeToString(TransitionType type)
{
    switch (type) {
    case TransitionType::None: return "None";
    case TransitionType::DipToBlack: return "DipToBlack";
    case TransitionType::DipToWhite: return "DipToWhite";
    case TransitionType::CrossDissolve: return "CrossDissolve";
    case TransitionType::WipeLeft: return "WipeLeft";
    case TransitionType::WipeRight: return "WipeRight";
    case TransitionType::WipeUp: return "WipeUp";
    case TransitionType::WipeDown: return "WipeDown";
    case TransitionType::SlideLeft: return "SlideLeft";
    case TransitionType::SlideRight: return "SlideRight";
    case TransitionType::ZoomIn: return "ZoomIn";
    }
    return "None";
}

QString transitionTypeDisplayName(TransitionType type)
{
    switch (type) {
    case TransitionType::None: return "Ninguna";
    case TransitionType::DipToBlack: return "Fundido a Negro (Dip to Black)";
    case TransitionType::DipToWhite: return "Fundido a Blanco (Dip to White)";
    case TransitionType::CrossDissolve: return "Disolución Cruzada (Cross Dissolve)";
    case TransitionType::WipeLeft: return "Cortinilla Izquierda (Wipe Left)";
    case TransitionType::WipeRight: return "Cortinilla Derecha (Wipe Right)";
    case TransitionType::WipeUp: return "Cortinilla Arriba (Wipe Up)";
    case TransitionType::WipeDown: return "Cortinilla Abajo (Wipe Down)";
    case TransitionType::SlideLeft: return "Deslizamiento Izq (Slide Left)";
    case TransitionType::SlideRight: return "Deslizamiento Der (Slide Right)";
    case TransitionType::ZoomIn: return "Zoom Suave (Zoom Transition)";
    }
    return "Ninguna";
}

TransitionType stringToTransitionType(const QString &str)
{
    QString s = str.trimmed();
    if (s.compare("DipToBlack", Qt::CaseInsensitive) == 0 || s.contains("Negro", Qt::CaseInsensitive)) return TransitionType::DipToBlack;
    if (s.compare("DipToWhite", Qt::CaseInsensitive) == 0 || s.contains("Blanco", Qt::CaseInsensitive)) return TransitionType::DipToWhite;
    if (s.compare("CrossDissolve", Qt::CaseInsensitive) == 0 || s.contains("Disol", Qt::CaseInsensitive)) return TransitionType::CrossDissolve;
    if (s.compare("WipeLeft", Qt::CaseInsensitive) == 0) return TransitionType::WipeLeft;
    if (s.compare("WipeRight", Qt::CaseInsensitive) == 0) return TransitionType::WipeRight;
    if (s.compare("WipeUp", Qt::CaseInsensitive) == 0) return TransitionType::WipeUp;
    if (s.compare("WipeDown", Qt::CaseInsensitive) == 0) return TransitionType::WipeDown;
    if (s.compare("SlideLeft", Qt::CaseInsensitive) == 0) return TransitionType::SlideLeft;
    if (s.compare("SlideRight", Qt::CaseInsensitive) == 0) return TransitionType::SlideRight;
    if (s.compare("ZoomIn", Qt::CaseInsensitive) == 0 || s.contains("Zoom", Qt::CaseInsensitive)) return TransitionType::ZoomIn;
    return TransitionType::None;
}

QString clipScaleModeToString(ClipScaleMode mode)
{
    switch (mode) {
    case ClipScaleMode::FillCrop: return "FillCrop";
    case ClipScaleMode::FitLetterbox: return "FitLetterbox";
    case ClipScaleMode::Stretch: return "Stretch";
    }
    return "FillCrop";
}

ClipScaleMode stringToClipScaleMode(const QString &str)
{
    QString s = str.trimmed();
    if (s.compare("FitLetterbox", Qt::CaseInsensitive) == 0 || s.compare("Fit", Qt::CaseInsensitive) == 0) {
        return ClipScaleMode::FitLetterbox;
    }
    if (s.compare("Stretch", Qt::CaseInsensitive) == 0) {
        return ClipScaleMode::Stretch;
    }
    return ClipScaleMode::FillCrop;
}

QString clipScaleModeDisplayName(ClipScaleMode mode)
{
    switch (mode) {
    case ClipScaleMode::FillCrop: return "Llenar lienzo (Cortar desborde)";
    case ClipScaleMode::FitLetterbox: return "Ajustar al lienzo (Con bandas)";
    case ClipScaleMode::Stretch: return "Estirar (Sin proporción)";
    }
    return "Llenar lienzo (Cortar desborde)";
}

TimelineClip::TimelineClip()
    : m_id(0)
    , m_trackId(0)
{
}

TimelineClip::TimelineClip(qint64 id, qint64 trackId, const QString &name, const QString &filePath, ClipType type)
    : m_id(id)
    , m_trackId(trackId)
    , m_name(name)
    , m_filePath(filePath)
    , m_type(type)
{
    switch (type) {
    case ClipType::Video:
        m_color = QColor("#2f81f7"); // Modern Blue
        break;
    case ClipType::Audio:
        m_color = QColor("#d29922"); // Warm Amber
        break;
    case ClipType::Image:
        m_color = QColor("#8957e5"); // Distinctive Royal Purple for Images/Overlays
        m_timelineOutMs = m_timelineInMs + 5000;
        m_sourceDurationMs = 5000;
        m_sourceOutMs = 5000;
        break;
    case ClipType::Text:
        m_color = QColor("#d29922"); // Warm Golden Amber for Titles/Text
        m_timelineOutMs = m_timelineInMs + 5000;
        m_sourceDurationMs = 5000;
        m_sourceOutMs = 5000;
        m_textContent = name.isEmpty() ? "Texto de ejemplo" : name;
        break;
    }
}

qint64 TimelineClip::mapTimelineToSourceMs(qint64 timelineMs) const
{
    if (timelineMs < m_timelineInMs) {
        return m_sourceInMs;
    }
    if (timelineMs > m_timelineOutMs) {
        return m_sourceOutMs;
    }
    const qint64 offsetOnTimeline = timelineMs - m_timelineInMs;
    const qint64 sourceOffset = qRound64(offsetOnTimeline * m_speed);
    return m_sourceInMs + sourceOffset;
}

double TimelineClip::opacityAt(qint64 timelineMs) const
{
    if (timelineMs < m_timelineInMs || timelineMs > m_timelineOutMs) {
        return 0.0;
    }
    double factor = 1.0;
    const qint64 offset = timelineMs - m_timelineInMs;
    const qint64 duration = durationMs();

    if (m_fadeInMs > 0 && offset < m_fadeInMs) {
        factor = qMin(factor, static_cast<double>(offset) / static_cast<double>(m_fadeInMs));
    }
    if (m_fadeOutMs > 0 && offset > (duration - m_fadeOutMs)) {
        const qint64 remaining = duration - offset;
        factor = qMin(factor, static_cast<double>(remaining) / static_cast<double>(m_fadeOutMs));
    }
    return qBound(0.0, m_opacity * factor, 1.0);
}

double TimelineClip::volumeAt(qint64 timelineMs) const
{
    if (m_isAudioMuted || timelineMs < m_timelineInMs || timelineMs > m_timelineOutMs) {
        return 0.0;
    }
    double factor = 1.0;
    const qint64 offset = timelineMs - m_timelineInMs;
    const qint64 duration = durationMs();

    if (m_fadeInMs > 0 && offset < m_fadeInMs) {
        factor = qMin(factor, static_cast<double>(offset) / static_cast<double>(m_fadeInMs));
    }
    if (m_fadeOutMs > 0 && offset > (duration - m_fadeOutMs)) {
        const qint64 remaining = duration - offset;
        factor = qMin(factor, static_cast<double>(remaining) / static_cast<double>(m_fadeOutMs));
    }
    return qBound(0.0, m_volume * factor, 2.0);
}

QImage TimelineClip::renderTextImage(const QSize &canvasSize) const
{
    if (m_type != ClipType::Text) return QImage();

    QTextDocument doc;
    doc.setDocumentMargin(16);

    int targetSize = m_fontSize > 0 ? m_fontSize : 48;
    QString targetFamily = m_fontFamily.isEmpty() ? "Arial" : m_fontFamily;

    QFont f(targetFamily, targetSize);
    f.setBold(m_isBold);
    f.setItalic(m_isItalic);
    f.setUnderline(m_isUnderline);
    doc.setDefaultFont(f);

    if (!m_richTextHtml.trimmed().isEmpty()) {
        QString html = m_richTextHtml;
        if (html.contains("font-size:", Qt::CaseInsensitive)) {
            html.replace(QRegularExpression("font-size:\\s*\\d+(?:\\.\\d+)?(?:pt|px)?", QRegularExpression::CaseInsensitiveOption), QString("font-size:%1pt").arg(targetSize));
        } else {
            html = QString("<div style=\"font-size:%1pt;\">%2</div>").arg(targetSize).arg(html);
        }
        doc.setHtml(html);
    } else {
        QString alignStr = "center";
        if ((m_textAlignment & Qt::AlignHorizontal_Mask) == Qt::AlignLeft) alignStr = "left";
        else if ((m_textAlignment & Qt::AlignHorizontal_Mask) == Qt::AlignRight) alignStr = "right";

        QString weightStr = m_isBold ? "bold" : "normal";
        QString styleStr = m_isItalic ? "italic" : "normal";
        QString decorStr = m_isUnderline ? "underline" : "none";

        QString text = m_textContent.isEmpty() ? "Texto de ejemplo" : m_textContent;
        QString html = QString(
            "<div style=\"text-align:%1; font-family:'%2'; font-size:%3pt; color:%4; font-weight:%5; font-style:%6; text-decoration:%7;\">%8</div>"
        ).arg(alignStr)
         .arg(targetFamily)
         .arg(targetSize)
         .arg(m_textColor.name())
         .arg(weightStr)
         .arg(styleStr)
         .arg(decorStr)
         .arg(text.toHtmlEscaped().replace("\n", "<br/>"));
        doc.setHtml(html);
    }

    if (m_textBoxWidth > 50) {
        doc.setTextWidth(m_textBoxWidth);
    } else {
        doc.adjustSize();
    }

    QSizeF docSize = doc.size();
    int imgW = qMax(80, qCeil(docSize.width()));
    int imgH = qMax(40, qCeil(docSize.height()));
    if (m_textBoxHeight > imgH) {
        imgH = m_textBoxHeight;
    }

    QImage img(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    img.fill(m_backgroundColor);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    doc.drawContents(&p);
    p.end();

    return img;
}
