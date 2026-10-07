# CPP VideoEditor 🎬

![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Qt 6](https://img.shields.io/badge/Qt-6.x-41CD52?logo=qt)
![FFmpeg](https://img.shields.io/badge/FFmpeg-5%20%7C%206%20%7C%207-007808?logo=ffmpeg)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake)
![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux%20%7C%20Windows-blue)
![License](https://img.shields.io/badge/License-Proprietary%20%2F%20All%20Rights%20Reserved-red)

Un editor de video no lineal (**NLE**) moderno, fluido y de alto rendimiento construido desde cero en **C++17**, **Qt 6** y las librerías nativas de **FFmpeg** (`libavformat`, `libavcodec`, `libswscale`, `libswresample`).

Diseñado con una arquitectura modular orientada a creadores de contenido contemporáneos, soportando formatos horizontales (16:9), verticales para redes sociales (9:16), formatos cuadrados y cine ultrawide, con aceleración por hardware en Apple Silicon, Windows y Linux.

---

## 📑 Tabla de Contenidos

- [Características Principales](#-características-principales)
  - [1. Línea de Tiempo Multipista](#1-línea-de-tiempo-multipista)
  - [2. Separación Total de Audio y Video](#2-separación-total-de-audio-y-video)
  - [3. Relaciones de Aspecto y Modo Vertical (9:16)](#3-relaciones-de-aspecto-y-modo-vertical-916)
  - [4. Animación Vectorial y Rutas de Movimiento (Motion Paths)](#4-animación-vectorial-y-rutas-de-movimiento-motion-paths)
  - [5. Corrección de Color y Curvas Spline](#5-corrección-de-color-y-curvas-spline)
  - [6. Pila de Efectos Visuales y Transiciones](#6-pila-de-efectos-visuales-y-transiciones)
  - [7. Sistema de Títulos y Texto Enriquecido](#7-sistema-de-títulos-y-texto-enriquecido)
  - [8. Motores de Magnetismo Inteligente (Snapping)](#8-motores-de-magnetismo-inteligente-snapping)
  - [9. Exportación Multiformato y Aceleración por Hardware](#9-exportación-multiformato-y-aceleración-por-hardware)
  - [10. Guardado de Proyectos (.veproj)](#10-guardado-de-proyectos-veproj)
- [Arquitectura del Código](#-arquitectura-del-código)
- [Aceleración por Hardware Multiplataforma](#-aceleración-por-hardware-multiplataforma)
- [Guía de Compilación e Instalación](#-guía-de-compilación-e-instalación)
  - [🍎 macOS](#-macos-apple-silicon--intel)
  - [🐧 Linux](#-linux-ubuntu-debian-fedora-arch)
  - [🪟 Windows](#-windows-msvc-2022--mingw)
- [Ejecución y Pruebas Unitarias](#-ejecución-y-pruebas-unitarias)
- [Atajos de Teclado](#-atajos-de-teclado)
- [Licencia y Propiedad Intelectual](#-licencia-y-propiedad-intelectual)

---

## 🌟 Características Principales

### 1. Línea de Tiempo Multipista
- **Pistas ilimitadas**: Gestión organizada de pistas de Video (`V1`, `V2`...), Audio (`A1`, `A2`...) y Texto.
- **Herramienta de Cuchilla / División (Split)**: Corte de clips con precisión de milisegundo mediante atajo (`Ctrl+K` / `Cmd+K`) o división multiselección a través de todas las pistas simultáneamente.
- **Edición en extremos (Trimming)**: Manijas interactivas de recorte en los puntos In / Out de cada clip.
- **Formas de onda en tiempo real**: Extracción y renderizado de la envolvente de picos reales de pistas de audio y video.
- **Eliminación y Rizado (Ripple Delete)**: Opción de eliminar clips dejando el espacio vacío o cerrando automáticamente el hueco (`Shift+Delete`).
- **Unión y cicatrización de cortes (Join Clips)**: Reconstitución de clips contiguos divididos previamente.
- **Zoom y navegación temporal**: Zoom con `Cmd/Ctrl + Rueda` y desplazamiento suave con regla estática fijada.
- **Marcadores de Escena (Markers)**: Marcadores de tiempo con nombres, comentarios y colores para indexar tomas.

### 2. Separación Total de Audio y Video
- Desvinculación en un clic (`Ctrl+U` / `Cmd+U` o menú contextual): convierte la pista de audio de un video en un elemento completamente autónomo en las pistas de audio (`A1`, `A2`).
- Permite mover, silenciar, recortar, reemplazar o aplicar efectos al audio sin afectar el cuadro de video.
- Posibilidad de volver a vincular clips en cualquier momento.

### 3. Relaciones de Aspecto y Modo Vertical (9:16)
- Preajustes inmediatos de lienzo para cualquier plataforma:
  - **16:9 Panorámico** (1920×1080) — YouTube, TV, Desktop.
  - **9:16 Vertical** (1080×1920) — TikTok, Instagram Reels, YouTube Shorts.
  - **1:1 Cuadrado** (1080×1080) — Feed de Instagram.
  - **4:3 Clásico** (1440×1080) — Retro / Documentales.
  - **21:9 Cine UltraWide** (2560×1080) — Producciones cinematográficas.
- **Modos de Adaptación al Lienzo (Scale Modes)**:
  - **Llenar lienzo (Cortar desborde) / FillCrop**: Preserva la relación de aspecto 1:1 original del clip; el video llena la altura del lienzo vertical y corta limpiamente los laterales desbordados sin comprimir ni deformar los píxeles.
  - **Ajustar al lienzo (Con bandas) / FitLetterbox**: Ajusta el clip completo añadiendo bandas de fondo.
  - **Estirar (Sin proporción) / Stretch**: Escala anamórfica exacta a los límites del lienzo.

### 4. Animación Vectorial y Rutas de Movimiento (Motion Paths)
- **Keyframing visual**: Animación de posición y desplazamiento en el lienzo.
- **Preajustes de trayectoria**: Izquierda a Derecha, Derecha a Izquierda, Arriba a Abajo, Diagonal, Zoom y Rebote.
- **Puntos vectoriales (Waypoints)**: Adición, reordenamiento y edición manual de puntos de control.
- **Curvas Bézier con Tiradores**: Suavizado de trayectoria mediante splines cúbicos y tiradores de tangencia.
- **Modos de Suavizado (Easing)**: `Linear`, `EaseInOut`, `EaseIn`, `EaseOut`.
- Inversión de trayectoria y auto-suavizado con un clic.

### 5. Corrección de Color y Curvas Spline
- **Modo Deslizadores**:
  - Brillo (-100 a +100).
  - Luminosidad / Contraste perceptual (-100 a +100).
  - Balance de color RGB independiente (Canal Rojo, Verde y Azul).
- **Modo Curvas (Spline Editor)**:
  - Curva de Luminancia (Luma).
  - Curvas de canales individuales R, G, B.
  - Espectro Tonal / Gradiente de Saturación.
  - Interpolación continua Monotone Hermite Splines con evaluación precisa de LUTs de 256 niveles.
- **Gradación Atmosférica: Hora del Día (Time of Day)**:
  - Control mediante deslizador continuo interactivo (`0.0f` a `1.0f`) o botones de preajuste:
    - `0.00f` **Noche**: Exposición -2.5 EV, tinte azul medianoche profundo, mezcla celeste completa y desplazamiento espectral de Purkinje (visión escotópica).
    - `0.33f` **Mañana**: Exposición -0.5 EV, tinte ámbar pastel cálido y gradiente matutino.
    - `0.66f` **Día**: Exposición 0.0 EV, balance neutro puro y cielo natural (atajo de rendimiento 0 ms).
    - `1.00f` **Atardecer**: Exposición -0.8 EV, tinte carmesí/naranja atardecer y gradiente cálido de horizonte.
  - Detección procedural de cielo mediante heurísticas de luminancia y dominancia cromática sin requerir modelos pesados de IA.
  - Procesamiento ultra-rápido en CPU mediante tablas de búsqueda directa (LUTs), bandas de escaneo multihilo y shader GLSL multiplataforma (`TimeOfDay.frag`).
  - Aplicable tanto a clips individuales como a nivel global del proyecto con soporte completo de Deshacer/Rehacer (`Ctrl+Z` / `Ctrl+Y`) y serialización JSON.
- Ajustes tanto a nivel de clip individual como a nivel global del proyecto (Master Color).

### 6. Pila de Efectos Visuales y Transiciones
- **Catálogo de 24 filtros en tiempo real**:
  - *Color*: Escala de Grises, Sepia, Invertir, Alto Contraste, Brillo, Negativo Térmico.
  - *Estilizado*: Pixelado, Posterizado, Detección de Bordes (Sobel), Viñeta, Cyberpunk, Glow.
  - *Desenfoque / Nitidez*: Desenfoque Gaussiano, Desenfoque de Movimiento, Enfoque (Sharpen).
  - *Artístico*: Dibujo a Lápiz, Pintura al Óleo, Halftone, Pop Art.
  - *Transformación*: Espejo Horizontal, Espejo Vertical, Zoom Rápido.
- **Pila de Efectos (Effects Stack)**: Aplicación secuencial con jerarquía matemática (los efectos superiores procesan sobre los inferiores), reordenamiento Drag & Drop y gestión por capas.
- **Transiciones entre clips**: Fundido Encadenado (*Cross Dissolve*), Fundido a Negro (*Dip to Black*), Fundido a Blanco (*Dip to White*), Barridos (*Wipe Left/Right/Up/Down*), Deslizamientos (*Slide Left/Right*) y *Zoom In* con control de duración.

### 7. Sistema de Títulos y Texto Enriquecido
- Generación de clips de texto y subtítulos directamente en la línea de tiempo.
- Edición de texto enriquecido (HTML/RichText): familia de fuentes, tamaño en puntos con incrementos de ±1 pt, negrita, cursiva, subrayado, alineación (izquierda, centro, derecha).
- Selector de color de texto y color de fondo de caja con transparencia alfa.
- Gizmo interactivo para reposicionar y redimensionar la caja de texto en el monitor.

### 8. Motores de Magnetismo Inteligente (Snapping)
- **Canvas Snapping**:
  - Guías inteligentes magnéticas al centro del lienzo (eje X e Y).
  - Alineación a bordes del lienzo y áreas de seguridad (Title Safe 90% / Action Safe 93%).
  - Alineación de bordes y centros con respecto a otros elementos en escena.
  - Coincidencia de dimensiones (Same Size Snapping).
  - Magnetismo angular en rotación (0°, 45°, 90°, 135°, 180°).
  - Desactivación rápida manteniendo presionada la tecla `Alt`.
- **Timeline Snapping**:
  - Ajuste magnético a extremos de clips contiguos, puntos de corte, cabezal de reproducción y origen (0 ms).
  - Ajuste a la cuadrícula temporal según la tasa de cuadros (FPS snapping: 30 fps, 60 fps).

### 9. Exportación Multiformato y Aceleración por Hardware
- Renderizado final de alta velocidad utilizando FFmpeg:
  - **MP4**: H.264 / AAC (con compatibilidad universal).
  - **WebM**: VP9 / Opus (optimizado para web moderna).
  - **QuickTime MOV**: Apple ProRes 422 / ProRes 4444 (flujos de trabajo profesionales sin pérdida).
  - **GIF Animado**: Generación de paleta en dos pasadas (`palettegen` / `paletteuse`) para GIFs sin banding.
  - **Audio Únicamente**: MP3 o WAV estéreo sin compresión.
- Selección de resolución (4K, 1080p, 720p, 480p o resolución nativa del proyecto) y tasa de cuadros (24, 30, 60 fps).
- Exportación de rango personalizado en la línea de tiempo.

### 10. Guardado de Proyectos (.veproj)
- Guardado y carga completa de sesiones en formato `.veproj` (JSON estructurado).
- Serialización de pistas, clips, transformaciones, rutas de animación, curvas de color, pila de efectos, marcadores y ajustes de proyecto.
- Rutas relativas para portabilidad de proyectos entre diferentes computadoras.
- Sistema de autoguardado y protección contra fallos.

---

## 🏛 Arquitectura del Código

El proyecto está diseñado bajo patrones de diseño sólidos (MVC, Memento para Undo/Redo, Observer y Pipeline de renderizado por capas):

```
CPP_VideoEditor/
├── CMakeLists.txt              # Configuración de compilación CMake
├── LICENSE                     # Licencia propietaria (All Rights Reserved)
├── README.md                   # Documentación principal
├── resources/                  # Recursos embebidos Qt (iconos, qrc)
├── sample_assets/              # Recursos de prueba para desarrollo y tests
├── src/
│   ├── main.cpp                # Punto de entrada de la aplicación
│   ├── mainwindow.h/.cpp       # Ventana principal, menús y distribución de docks
│   ├── previewwidget.h/.cpp    # Monitor de previsualización con Gizmo y guías
│   ├── timelinewidget.h/.cpp   # Línea de tiempo multipista interactiva
│   ├── core/
│   │   ├── clip.h/.cpp         # Entidad TimelineClip, transformaciones y propiedades
│   │   ├── track.h/.cpp        # Estructura de pistas de audio y video
│   │   ├── timelinemodel.h/.cpp# Modelo central y sistema de Undo/Redo (Memento)
│   │   ├── colorcurve.h/.cpp   # Splines Hermite y evaluación de curvas LUT
│   │   ├── marker.h            # Marcadores de tiempo
│   │   └── projectserializer.h/.cpp # Guardado y carga de archivos .veproj
│   ├── engine/
│   │   ├── videoframedecoder.h/.cpp # Decodificación multihilo FFmpeg con caché
│   │   ├── videocompositor.h/.cpp   # Compositor multicapa, efectos y filtros
│   │   ├── audioengine.h/.cpp       # Motor de reproducción de audio PCM (QAudioSink)
│   │   ├── waveformgenerator.h/.cpp # Extracción de envolvente de picos de audio
│   │   ├── videoexporter.h/.cpp     # Pipeline de exportación acelerada FFmpeg
│   │   └── exportdialog.h/.cpp      # Interfaz de usuario para exportación
│   ├── inspector/
│   │   ├── inspectorwidget.h/.cpp   # Panel de propiedades, efectos y curvas
│   │   └── curveeditorwidget.h/.cpp # Control gráfico de curvas de color
│   ├── medialibrary/
│   │   ├── mediaitem.h/.cpp         # Metadatos y análisis multimedia
│   │   └── medialibrarywidget.h/.cpp# Panel Media Pool y búsqueda
│   └── snapping/
│       ├── canvassnappingengine.h/.cpp  # Magnetismo de lienzo y guías
│       └── timelinesnappingengine.h/.cpp# Magnetismo de línea de tiempo
└── tests/
    └── test_editor.cpp         # Suite completa de 28 pruebas unitarias y de sistema
```

---

## ⚡ Aceleración por Hardware Multiplataforma

El motor detecta dinámicamente los decodificadores y codificadores acelerados por hardware disponibles en cada plataforma:

| Plataforma | API / Backend HW | Codificadores H.264 / HEVC / ProRes |
|---|---|---|
| **macOS** | **Apple Silicon (VideoToolbox)** | `h264_videotoolbox`, `hevc_videotoolbox`, `prores_videotoolbox` |
| **Windows** | **NVIDIA NVENC / Intel QSV / AMD AMF** | `h264_nvenc`, `hevc_nvenc`, `h264_qsv`, `h264_amf` |
| **Linux** | **VA-API / NVENC** | `h264_vaapi`, `hevc_vaapi`, `h264_nvenc` |

---

## 🛠 Guía de Compilación e Instalación

### 🍎 macOS (Apple Silicon & Intel)

1. Instalar dependencias mediante [Homebrew](https://brew.sh/):
   ```bash
   brew install qt@6 ffmpeg pkg-config cmake
   ```

2. Configurar y compilar:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(sysctl -n hw.ncpu)
   ```

---

### 🐧 Linux (Ubuntu, Debian, Fedora, Arch)

1. Instalar dependencias del sistema:
   * **Ubuntu / Debian**:
     ```bash
     sudo apt update
     sudo apt install -y build-essential cmake pkg-config \
         qt6-base-dev qt6-multimedia-dev libqt6multimediawidgets6 \
         libavformat-dev libavcodec-dev libswscale-dev libswresample-dev libavutil-dev \
         ffmpeg
     ```
   * **Fedora**:
     ```bash
     sudo dnf install -y gcc-c++ cmake pkgconfig \
         qt6-qtbase-devel qt6-qtmultimedia-devel \
         ffmpeg-free-devel ffmpeg
     ```
   * **Arch Linux / Manjaro**:
     ```bash
     sudo pacman -S --needed base-devel cmake pkgconf qt6-base qt6-multimedia ffmpeg
     ```

2. Configurar y compilar:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   ```

---

### 🪟 Windows (MSVC 2022 / MinGW)

1. Instalar **Qt 6** (mediante Qt Online Installer seleccionando MSVC 2022 o MinGW) o vía `vcpkg`:
   ```cmd
   vcpkg install qtbase:x64-windows qtmultimedia:x64-windows ffmpeg:x64-windows
   ```
2. Si utilizas binarios de FFmpeg compartidos desde [gyan.dev](https://www.gyan.dev/ffmpeg/builds/):
   - Establece la variable de entorno `FFMPEG_DIR` apuntando a la carpeta de FFmpeg.
3. Compilar:
   ```cmd
   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
*(Nota: Para crear una versión portátil sin dependencias en el PATH, puedes copiar `ffmpeg.exe` en la misma carpeta del ejecutable `CPP_VideoEditor.exe`).*

---

## 🧪 Ejecución y Pruebas Unitarias

### Ejecutar la Aplicación:
```bash
./build/CPP_VideoEditor
```

### Ejecutar la Suite de Pruebas:
El proyecto incluye una suite automatizada de pruebas exhaustivas que validan la decodificación, el motor de composición, la precisión de audio, las transformaciones, los filtros, las curvas de color, el guardado de proyectos y la preservación de relaciones de aspecto:

```bash
./build/test_editor
```
*(Todas las 28 pruebas pasan con un 100% de éxito en integración continua).*

---

## ⌨️ Atajos de Teclado

| Atajo | Acción |
|---|---|
| `Espacio` | Reproducir / Pausar |
| `Flecha Izquierda` | Retroceder 1 fotograma |
| `Flecha Derecha` | Avanzar 1 fotograma |
| `Home` | Ir al inicio del proyecto |
| `End` | Ir al final del proyecto |
| `Ctrl+K` / `Cmd+K` | Dividir clip en la posición del cabezal |
| `Ctrl+U` / `Cmd+U` | Separar audio del video seleccionado |
| `Ctrl+D` / `Cmd+D` | Duplicar clip |
| `Supr` / `Backspace` | Eliminar clip seleccionado |
| `Shift + Supr` | Eliminar clip con rizado (*Ripple Delete*) |
| `Ctrl+S` / `Cmd+S` | Guardar proyecto (.veproj) |
| `Ctrl+O` / `Cmd+O` | Abrir proyecto existente |
| `Ctrl+Z` / `Cmd+Z` | Deshacer acción anterior |
| `Ctrl+Y` / `Cmd+Y` | Rehacer acción |
| `Ctrl+E` / `Cmd+E` | Abrir diálogo de exportación de video |
| `Ctrl + Rueda` | Zoom in / Zoom out en la línea de tiempo |
| `Alt (mantener)` | Desactivar magnetismo (Snapping Bypass) durante el arrastre |

---

## ⚖️ Licencia y Propiedad Intelectual

**Copyright (c) 2026 Igfri-dev. Todos los derechos reservados / All Rights Reserved.**

Este proyecto y su código fuente son de **acceso público exclusivamente con fines de lectura, referencia técnica, revisión de código y evaluación personal**.

> **IMPORTANTE — NO ES SOFTWARE DE CÓDIGO ABIERTO (NOT OPEN SOURCE):**  
> Este software **NO es de código abierto** y **NO está licenciado** bajo licencias permisivas o copyleft aprobadas por la OSI (como MIT, Apache, GPL o BSD). Queda expresamente prohibida la redistribución, modificación para fines comerciales, sublicenciamiento o uso en productos con fines de lucro sin la autorización previa, expresa y por escrito del autor.

Para los términos completos, consulta el archivo [LICENSE](LICENSE). Para consultas o solicitudes de licencias comerciales, contacta a través de [GitHub @Igfri-dev](https://github.com/Igfri-dev).