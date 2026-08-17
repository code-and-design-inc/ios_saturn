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
#ifndef UIDEBUGSCSPGRAPH_H
#define UIDEBUGSCSPGRAPH_H

#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QMutex>
#include <QFuture>
#include <vector>
#include <atomic>

extern "C" {
#include "scsp.h"
}

// Graph display modes
enum class SCSPGraphMode {
    Envelope,    // ADSR envelope over time
    LFO,         // LFO waveform
    Waveform     // Audio waveform oscilloscope
};

class UIDebugSCSPGraph : public QWidget
{
    Q_OBJECT

public:
    explicit UIDebugSCSPGraph(QWidget *parent = nullptr);
    ~UIDebugSCSPGraph();

    void setSlot(int slot);
    int slot() const { return m_slot; }

    void setMode(SCSPGraphMode mode);
    SCSPGraphMode mode() const { return m_mode; }

    void setRunning(bool running);
    bool isRunning() const { return m_running; }

    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void startBackgroundRender();

private:
    void renderToImage();
    void drawEnvelopeGraph(QPainter &painter, int w, int h);
    void drawLFOGraph(QPainter &painter, int w, int h);
    void drawWaveformGraph(QPainter &painter, int w, int h);
    void drawGrid(QPainter &painter, int w, int h);
    void drawLegend(QPainter &painter, int w, int h);

    int m_slot;
    SCSPGraphMode m_mode;
    bool m_running;
    QTimer *m_timer;

    // History buffers
    static const int HISTORY_SIZE = 256;
    std::vector<u16> m_envelopeHistory;
    std::vector<u8> m_envelopeStateHistory;
    std::vector<u32> m_lfoHistory;
    std::vector<s16> m_waveformBuffer;

    int m_historyPos;

    // Colors for envelope states
    QColor m_stateColors[4];

    // Offscreen rendering
    QImage m_offscreenImage;
    QMutex m_imageMutex;
    QMutex m_dataMutex;  // Protects history buffers and m_historyPos
    QFuture<void> m_renderFuture;
    std::atomic<bool> m_rendering;
};

#endif // UIDEBUGSCSPGRAPH_H
