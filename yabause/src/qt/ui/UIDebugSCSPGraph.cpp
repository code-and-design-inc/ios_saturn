/*  Copyright 2024 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
// Issue #73: SCSP Debug Graph Widget with offscreen rendering
#include "UIDebugSCSPGraph.h"
#include <QPainter>
#include <QPainterPath>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrentRun>

UIDebugSCSPGraph::UIDebugSCSPGraph(QWidget *parent)
    : QWidget(parent)
    , m_slot(0)
    , m_mode(SCSPGraphMode::Envelope)
    , m_running(false)
    , m_historyPos(0)
    , m_rendering(false)
{
    // Initialize history buffers
    m_envelopeHistory.resize(HISTORY_SIZE, 0);
    m_envelopeStateHistory.resize(HISTORY_SIZE, 3);  // RELEASE state
    m_lfoHistory.resize(HISTORY_SIZE, 0);
    m_waveformBuffer.resize(1024, 0);

    // Envelope state colors: ATTACK=red, DECAY1=green, DECAY2=blue, RELEASE=gray
    m_stateColors[0] = QColor(255, 80, 80);    // ATTACK - red
    m_stateColors[1] = QColor(80, 255, 80);    // DECAY1 - green
    m_stateColors[2] = QColor(80, 80, 255);    // DECAY2 - blue
    m_stateColors[3] = QColor(128, 128, 128);  // RELEASE - gray

    // Initialize offscreen image
    m_offscreenImage = QImage(300, 150, QImage::Format_RGB32);
    m_offscreenImage.fill(Qt::black);

    // Setup update timer
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &UIDebugSCSPGraph::startBackgroundRender);

    setMinimumSize(300, 150);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
}

UIDebugSCSPGraph::~UIDebugSCSPGraph()
{
    if (m_renderFuture.isRunning()) {
        m_renderFuture.waitForFinished();
    }
    delete m_timer;
}

void UIDebugSCSPGraph::setSlot(int slot)
{
    if (slot >= 0 && slot < 32) {
        m_slot = slot;
        clear();
    }
}

void UIDebugSCSPGraph::setMode(SCSPGraphMode mode)
{
    m_mode = mode;
    clear();
    update();
}

void UIDebugSCSPGraph::setRunning(bool running)
{
    m_running = running;
    if (running) {
        m_timer->start(50);  // ~20 FPS for performance
    } else {
        m_timer->stop();
    }
}

void UIDebugSCSPGraph::clear()
{
    std::fill(m_envelopeHistory.begin(), m_envelopeHistory.end(), 0);
    std::fill(m_envelopeStateHistory.begin(), m_envelopeStateHistory.end(), 3);
    std::fill(m_lfoHistory.begin(), m_lfoHistory.end(), 0);
    std::fill(m_waveformBuffer.begin(), m_waveformBuffer.end(), 0);
    m_historyPos = 0;
    update();
}

void UIDebugSCSPGraph::startBackgroundRender()
{
    if (m_rendering) return;

    m_rendering = true;

    // Collect data from SCSP into local variables (avoid nested locks)
    ScspSlotDebugInfo info;
    std::vector<s16> tempWaveform;

    ScspLockThread();
    scsp_debug_get_slot_state(m_slot, &info);
    if (m_mode == SCSPGraphMode::Waveform) {
        tempWaveform.resize(m_waveformBuffer.size());
        scsp_debug_get_sample_buffer(m_slot, tempWaveform.data(), (int)tempWaveform.size());
    }
    ScspUnLockThread();

    // Now update member variables with m_dataMutex (no ScspLock held)
    {
        QMutexLocker dataLocker(&m_dataMutex);
        m_envelopeHistory[m_historyPos] = info.envelope_level;
        m_envelopeStateHistory[m_historyPos] = info.envelope_state;
        m_lfoHistory[m_historyPos] = info.lfo_position;
        if (!tempWaveform.empty()) {
            m_waveformBuffer = std::move(tempWaveform);
        }
        m_historyPos = (m_historyPos + 1) % HISTORY_SIZE;
    }

    // Start background rendering
    m_renderFuture = QtConcurrent::run([this]() {
        renderToImage();
    });
}

void UIDebugSCSPGraph::renderToImage()
{
    int w = width();
    int h = height();
    if (w <= 0 || h <= 0) {
        m_rendering = false;
        return;
    }

    QImage tempImage(w, h, QImage::Format_RGB32);
    tempImage.fill(Qt::black);

    QPainter painter(&tempImage);

    // Draw background grid
    drawGrid(painter, w, h);

    // Draw the appropriate graph (with data lock to prevent race condition)
    {
        QMutexLocker dataLocker(&m_dataMutex);
        switch (m_mode) {
        case SCSPGraphMode::Envelope:
            drawEnvelopeGraph(painter, w, h);
            break;
        case SCSPGraphMode::LFO:
            drawLFOGraph(painter, w, h);
            break;
        case SCSPGraphMode::Waveform:
            drawWaveformGraph(painter, w, h);
            break;
        }
    }

    // Draw legend
    drawLegend(painter, w, h);

    painter.end();

    // Copy to offscreen buffer (thread-safe)
    {
        QMutexLocker locker(&m_imageMutex);
        m_offscreenImage = tempImage;
    }

    m_rendering = false;

    // Schedule update on main thread
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void UIDebugSCSPGraph::drawGrid(QPainter &painter, int w, int h)
{
    painter.setPen(QPen(QColor(40, 40, 40), 1));

    // Vertical grid lines
    for (int i = 1; i < 8; i++) {
        int x = w * i / 8;
        painter.drawLine(x, 0, x, h);
    }

    // Horizontal grid lines
    for (int i = 1; i < 4; i++) {
        int y = h * i / 4;
        painter.drawLine(0, y, w, y);
    }
}

void UIDebugSCSPGraph::drawLegend(QPainter &painter, int w, int h)
{
    Q_UNUSED(w);
    int y = 15;
    int x = 10;

    painter.setFont(QFont("Monospace", 8));

    switch (m_mode) {
    case SCSPGraphMode::Envelope:
        painter.setPen(Qt::white);
        painter.drawText(x, y, QString("Slot %1 - Envelope").arg(m_slot));

        y = h - 10;
        painter.setPen(m_stateColors[0]);
        painter.drawText(x, y, "ATK");
        x += 35;
        painter.setPen(m_stateColors[1]);
        painter.drawText(x, y, "D1");
        x += 30;
        painter.setPen(m_stateColors[2]);
        painter.drawText(x, y, "D2");
        x += 30;
        painter.setPen(m_stateColors[3]);
        painter.drawText(x, y, "REL");
        break;

    case SCSPGraphMode::LFO:
        painter.setPen(Qt::white);
        painter.drawText(x, y, QString("Slot %1 - LFO Position").arg(m_slot));
        break;

    case SCSPGraphMode::Waveform:
        painter.setPen(Qt::white);
        painter.drawText(x, y, QString("Slot %1 - Waveform").arg(m_slot));
        break;
    }
}

void UIDebugSCSPGraph::drawEnvelopeGraph(QPainter &painter, int w, int h)
{
    int graphH = h - 30;  // Leave space for legend
    int marginTop = 20;

    if (w <= 0 || graphH <= 0) return;

    // Draw envelope curve
    QPainterPath path;
    bool pathStarted = false;

    for (int i = 0; i < HISTORY_SIZE; i++) {
        int idx = (m_historyPos + i) % HISTORY_SIZE;
        int x = w * i / HISTORY_SIZE;

        double level = 1.0 - (m_envelopeHistory[idx] / 1023.0);
        int y = marginTop + (int)(graphH * (1.0 - level));

        if (!pathStarted) {
            path.moveTo(x, y);
            pathStarted = true;
        } else {
            path.lineTo(x, y);
        }
    }

    painter.setPen(QPen(Qt::cyan, 2));
    painter.drawPath(path);

    // Draw state indicator bars at bottom
    int barHeight = 4;
    int barY = graphH + marginTop + 2;
    for (int i = 0; i < HISTORY_SIZE; i++) {
        int idx = (m_historyPos + i) % HISTORY_SIZE;
        int x = w * i / HISTORY_SIZE;
        int nextX = w * (i + 1) / HISTORY_SIZE;

        painter.fillRect(x, barY, nextX - x, barHeight, m_stateColors[m_envelopeStateHistory[idx] & 3]);
    }

    // Draw current level indicator
    int currentIdx = (m_historyPos + HISTORY_SIZE - 1) % HISTORY_SIZE;
    double currentLevel = 1.0 - (m_envelopeHistory[currentIdx] / 1023.0);
    int currentY = marginTop + (int)(graphH * (1.0 - currentLevel));

    painter.setPen(Qt::yellow);
    painter.drawLine(w - 5, currentY, w, currentY);

    // Draw level text
    painter.setPen(Qt::white);
    painter.drawText(w - 60, marginTop + 15,
                     QString("%1").arg(m_envelopeHistory[currentIdx], 4, 16, QChar('0')).toUpper());
}

void UIDebugSCSPGraph::drawLFOGraph(QPainter &painter, int w, int h)
{
    int graphH = h - 30;
    int marginTop = 20;

    if (w <= 0 || graphH <= 0) return;

    // Draw LFO position curve
    QPainterPath path;
    bool pathStarted = false;

    for (int i = 0; i < HISTORY_SIZE; i++) {
        int idx = (m_historyPos + i) % HISTORY_SIZE;
        int x = w * i / HISTORY_SIZE;

        double pos = (m_lfoHistory[idx] & 0xFF) / 255.0;
        int y = marginTop + (int)(graphH * (1.0 - pos));

        if (!pathStarted) {
            path.moveTo(x, y);
            pathStarted = true;
        } else {
            path.lineTo(x, y);
        }
    }

    painter.setPen(QPen(QColor(255, 128, 0), 2));  // Orange for LFO
    painter.drawPath(path);
}

void UIDebugSCSPGraph::drawWaveformGraph(QPainter &painter, int w, int h)
{
    int graphH = h - 30;
    int marginTop = 20;
    int centerY = marginTop + graphH / 2;

    if (w <= 0 || graphH <= 0) return;

    // Draw center line
    painter.setPen(QPen(QColor(60, 60, 60), 1));
    painter.drawLine(0, centerY, w, centerY);

    // Draw waveform
    QPainterPath path;
    int sampleCount = (int)m_waveformBuffer.size();

    for (int i = 0; i < w && i < sampleCount; i++) {
        int sampleIdx = i * sampleCount / w;
        double sample = m_waveformBuffer[sampleIdx] / 32768.0;
        int y = centerY - (int)(sample * graphH / 2);

        if (i == 0) {
            path.moveTo(i, y);
        } else {
            path.lineTo(i, y);
        }
    }

    painter.setPen(QPen(Qt::green, 1));
    painter.drawPath(path);
}

void UIDebugSCSPGraph::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);

    // Just draw the pre-rendered image
    QMutexLocker locker(&m_imageMutex);
    painter.drawImage(0, 0, m_offscreenImage);
}
