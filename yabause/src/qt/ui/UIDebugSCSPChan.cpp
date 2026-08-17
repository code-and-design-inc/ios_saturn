/*	Copyright 2012 Theo Berkau <cwx@cyberwarriorx.com>

This file is part of Yabause.

Yabause is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Yabause is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Yabause; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include "UIDebugSCSPChan.h"
#include "CommonDialogs.h"

#include <QImageWriter>
#include <QGraphicsPixmapItem>
#include <QDebug>
#include <QIODevice>
#include <QTimer>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPainter>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrentRun>
#include <cstring>

// Issue #73: Separate widget for graphical meter display with offscreen rendering
class SCSPMeterWidget : public QWidget
{
   Q_OBJECT

private:
   // Forward declaration of SlotData struct (needed before functions that use it)
   struct SlotData {
      int env;
      int state;
      s16 levelL;
      s16 levelR;
      s16 peakL;
      s16 peakR;
      bool isActive;
   };

public:
   SCSPMeterWidget(QWidget *parent = nullptr) : QWidget(parent), m_rendering(false)
   {
      // Envelope state colors: ATTACK=red, DECAY1=green, DECAY2=blue, RELEASE=cyan
      envelope_colors[0] = Qt::red;
      envelope_colors[1] = Qt::green;
      envelope_colors[2] = Qt::blue;
      envelope_colors[3] = Qt::cyan;

      // Initialize slot data
      for (int i = 0; i < SCSP_DEBUG_CHANNEL_COUNT; i++) {
         m_slotData[i] = {0, 0, 0, 0, 0, 0, false};
         peakLevelL[i] = 0;
         peakLevelR[i] = 0;
         peakHoldCount[i] = 0;
      }

      setMinimumSize(350, 520);
      setAutoFillBackground(true);
      QPalette pal = palette();
      pal.setColor(QPalette::Window, QColor(32, 32, 32));
      setPalette(pal);

      // Initialize offscreen image
      m_offscreenImage = QImage(400, 520, QImage::Format_RGB32);
      m_offscreenImage.fill(QColor(32, 32, 32));

      // Setup timer for background rendering
      m_timer = new QTimer(this);
      connect(m_timer, &QTimer::timeout, this, &SCSPMeterWidget::startBackgroundRender);
      m_timer->start(33);  // ~30 FPS
   }

   ~SCSPMeterWidget()
   {
      // Wait for any pending render to complete
      if (m_renderFuture.isRunning()) {
         m_renderFuture.waitForFinished();
      }
   }

public slots:
   void startBackgroundRender()
   {
      // Don't start new render if one is already running
      if (m_rendering) return;

      m_rendering = true;

      // Collect data from SCSP into local arrays (avoid nested locks)
      SlotData tempSlotData[SCSP_DEBUG_CHANNEL_COUNT];

      ScspLockThread();
      for (int i = 0; i < SCSP_DEBUG_CHANNEL_COUNT; i++) {
         int env = 0, state = 0;
         scsp_debug_get_envelope(i, &env, &state);

         s16 levelL = 0, levelR = 0;
         scsp_debug_get_output_level(i, &levelL, &levelR);

         bool isActive = scsp_debug_is_slot_active(i);

         tempSlotData[i].env = env;
         tempSlotData[i].state = state;
         tempSlotData[i].levelL = levelL;
         tempSlotData[i].levelR = levelR;
         tempSlotData[i].isActive = isActive;
      }
      ScspUnLockThread();

      // Now update member variables with m_dataMutex (no ScspLock held)
      {
         QMutexLocker dataLocker(&m_dataMutex);
         for (int i = 0; i < SCSP_DEBUG_CHANNEL_COUNT; i++) {
            m_slotData[i].env = tempSlotData[i].env;
            m_slotData[i].state = tempSlotData[i].state;
            m_slotData[i].levelL = tempSlotData[i].levelL;
            m_slotData[i].levelR = tempSlotData[i].levelR;
            m_slotData[i].isActive = tempSlotData[i].isActive;

            // Update peak hold
            if (qAbs(tempSlotData[i].levelL) > qAbs(peakLevelL[i])) {
               peakLevelL[i] = tempSlotData[i].levelL;
               peakHoldCount[i] = 15;  // Hold for ~0.5 seconds at 30 FPS
            }
            if (qAbs(tempSlotData[i].levelR) > qAbs(peakLevelR[i])) {
               peakLevelR[i] = tempSlotData[i].levelR;
               peakHoldCount[i] = 15;
            }
            if (peakHoldCount[i] > 0) {
               peakHoldCount[i]--;
            } else {
               peakLevelL[i] = (s16)(peakLevelL[i] * 0.95);
               peakLevelR[i] = (s16)(peakLevelR[i] * 0.95);
            }
            m_slotData[i].peakL = peakLevelL[i];
            m_slotData[i].peakR = peakLevelR[i];
         }
      }

      // Start background rendering
      m_renderFuture = QtConcurrent::run([this]() {
         renderToImage();
      });
   }

private:
   void renderToImage()
   {
      QImage tempImage(400, 520, QImage::Format_RGB32);
      tempImage.fill(QColor(32, 32, 32));

      QPainter painter(&tempImage);

      // Layout: 8 columns x 4 rows for 32 slots
      int slotWidth = 24;
      int slotHeight = 100;
      int spacerX = 8;
      int spacerY = 20;
      int startX = 10;
      int startY = 30;

      // Draw title
      painter.setPen(Qt::white);
      QFont titleFont = painter.font();
      titleFont.setPointSize(10);
      titleFont.setBold(true);
      painter.setFont(titleFont);
      painter.drawText(startX, 20, "Envelope & Level Meters");

      // Draw legend
      int legendX = startX + 8 * (slotWidth + spacerX) + 20;
      int legendY = startY;
      painter.setPen(Qt::white);
      QFont legendFont = painter.font();
      legendFont.setPointSize(8);
      legendFont.setBold(false);
      painter.setFont(legendFont);
      painter.drawText(legendX, legendY, "States:");
      legendY += 15;
      painter.setPen(envelope_colors[0]);
      painter.drawText(legendX, legendY, "ATTACK");
      legendY += 12;
      painter.setPen(envelope_colors[1]);
      painter.drawText(legendX, legendY, "DECAY1");
      legendY += 12;
      painter.setPen(envelope_colors[2]);
      painter.drawText(legendX, legendY, "DECAY2");
      legendY += 12;
      painter.setPen(envelope_colors[3]);
      painter.drawText(legendX, legendY, "RELEASE");

      // Copy slot data with lock to avoid race condition
      SlotData localSlotData[SCSP_DEBUG_CHANNEL_COUNT];
      {
         QMutexLocker dataLocker(&m_dataMutex);
         std::memcpy(localSlotData, m_slotData, sizeof(localSlotData));
      }

      // Draw 32 slot meters in 8x4 grid
      int activeCount = 0;
      for (int row = 0; row < 4; row++) {
         for (int col = 0; col < 8; col++) {
            int slot = row * 8 + col;
            int x = startX + col * (slotWidth + spacerX);
            int y = startY + row * (slotHeight + spacerY);
            drawSlotMeterLocal(painter, localSlotData[slot], slot, x, y, slotWidth, slotHeight);
            if (localSlotData[slot].isActive) activeCount++;
         }
      }

      // Draw status bar
      painter.setPen(Qt::white);
      painter.drawText(startX, startY + 4 * (slotHeight + spacerY) + 10,
                       QString("Active Slots: %1 / %2").arg(activeCount).arg(SCSP_DEBUG_CHANNEL_COUNT));

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

   void drawLevelMeter(QPainter &painter, int x, int y, int width, int height,
                       s16 levelL, s16 levelR, s16 peakL, s16 peakR)
   {
      double ratioL = qAbs(levelL) / 32767.0;
      double ratioR = qAbs(levelR) / 32767.0;
      double peakRatioL = qAbs(peakL) / 32767.0;
      double peakRatioR = qAbs(peakR) / 32767.0;

      int halfWidth = width / 2 - 1;
      int meterHeight = (int)(height * ratioL);
      int meterHeightR = (int)(height * ratioR);

      painter.fillRect(x, y, width, height, Qt::darkGray);

      QColor levelColor;
      if (ratioL > 0.9)
         levelColor = Qt::red;
      else if (ratioL > 0.7)
         levelColor = Qt::yellow;
      else
         levelColor = Qt::green;

      painter.fillRect(x, y + height - meterHeight, halfWidth, meterHeight, levelColor);

      if (ratioR > 0.9)
         levelColor = Qt::red;
      else if (ratioR > 0.7)
         levelColor = Qt::yellow;
      else
         levelColor = Qt::green;

      painter.fillRect(x + halfWidth + 2, y + height - meterHeightR, halfWidth, meterHeightR, levelColor);

      int peakY_L = y + height - (int)(height * peakRatioL);
      int peakY_R = y + height - (int)(height * peakRatioR);
      painter.setPen(Qt::white);
      painter.drawLine(x, peakY_L, x + halfWidth - 1, peakY_L);
      painter.drawLine(x + halfWidth + 2, peakY_R, x + width - 1, peakY_R);
   }

   void drawSlotMeter(QPainter &painter, int slot, int x, int y, int width, int height)
   {
      const SlotData &data = m_slotData[slot];
      drawSlotMeterLocal(painter, data, slot, x, y, width, height);
   }

   // Thread-safe version that uses a local copy of SlotData
   void drawSlotMeterLocal(QPainter &painter, const SlotData &data, int slot, int x, int y, int width, int height)
   {
      // Draw slot number
      painter.setPen(data.isActive ? Qt::white : Qt::gray);
      QFont font = painter.font();
      font.setPointSize(8);
      painter.setFont(font);
      painter.drawText(x, y - 2, QString::number(slot));

      // Draw envelope bar
      int envHeight = height / 2;
      double env_ratio = (1023.0 - data.env) / 1023.0;
      int barHeight = (int)(envHeight * env_ratio);

      QRect envRect(x, y, width, envHeight);
      painter.fillRect(envRect, Qt::black);

      QRect barRect(x, y + envHeight - barHeight, width, barHeight);
      painter.fillRect(barRect, envelope_colors[data.state & 3]);

      if (data.isActive) {
         painter.setPen(QPen(Qt::yellow, 2));
      } else {
         painter.setPen(Qt::darkGray);
      }
      painter.drawRect(envRect);

      // Draw level meter below envelope
      int meterY = y + envHeight + 4;
      int meterHeight = height / 2 - 4;
      drawLevelMeter(painter, x, meterY, width, meterHeight,
                     data.levelL, data.levelR, data.peakL, data.peakR);
   }

protected:
   void paintEvent(QPaintEvent *event) override
   {
      Q_UNUSED(event);
      QPainter painter(this);

      // Just draw the pre-rendered image
      QMutexLocker locker(&m_imageMutex);
      painter.drawImage(0, 0, m_offscreenImage);
   }

private:
   QColor envelope_colors[4];
   SlotData m_slotData[SCSP_DEBUG_CHANNEL_COUNT];
   s16 peakLevelL[SCSP_DEBUG_CHANNEL_COUNT];
   s16 peakLevelR[SCSP_DEBUG_CHANNEL_COUNT];
   int peakHoldCount[SCSP_DEBUG_CHANNEL_COUNT];

   QImage m_offscreenImage;
   QMutex m_imageMutex;
   QMutex m_dataMutex;  // Protects m_slotData, peakLevelL/R, peakHoldCount
   QTimer *m_timer;
   QFuture<void> m_renderFuture;
   std::atomic<bool> m_rendering;
};

// Include moc file for Q_OBJECT in local class
#include "UIDebugSCSPChan.moc"

UIDebugSCSPChan::UIDebugSCSPChan(QWidget* p)
   : QDialog(p)
{
   scsp_debug_set_mode(1);
   scsp_debug_instrument_clear();

   // Main layout
   QVBoxLayout *mainLayout = new QVBoxLayout(this);

   // Upper: Graphical meter widget
   meterWidget = new SCSPMeterWidget(this);
   mainLayout->addWidget(meterWidget);

   // Lower: Mute checkboxes in 8-column grid
   QGroupBox *muteGroup = new QGroupBox("Mute Controls", this);
   QGridLayout *checkLayout = new QGridLayout(muteGroup);
   checkLayout->setSpacing(4);

   for (int i = 0; i < SCSP_DEBUG_CHANNEL_COUNT; i++)
   {
      checkbox[i] = new QCheckBox(QString("Slot %1").arg(i, 2));
      checkbox[i]->setChecked(false);
      checkbox[i]->setToolTip(QString("Mute slot %1").arg(i));
      checkLayout->addWidget(checkbox[i], i / 8, i % 8);
   }
   mainLayout->addWidget(muteGroup);

   resize(400, 700);
   setWindowTitle("SCSP Channel Monitor");

   timer = new QTimer(this);
   timer->setInterval(33);  // ~30 FPS for checkbox updates
   timer->start();

   connect(timer, SIGNAL(timeout()), SLOT(update_window()));

   QtYabause::retranslateWidget(this);
}

UIDebugSCSPChan::~UIDebugSCSPChan()
{
   scsp_debug_set_mode(0);

   delete timer;
   for (int i = 0; i < SCSP_DEBUG_CHANNEL_COUNT; i++)
      delete checkbox[i];
}

void UIDebugSCSPChan::update_window()
{
   ScspLockThread();

   for (int i = 0; i < SCSP_DEBUG_CHANNEL_COUNT; i++)
   {
      u32 sa = 0;
      int muted = 0;
      scsp_debug_instrument_get_data(i, &sa, &muted);

      bool isActive = scsp_debug_is_slot_active(i);

      QString label = QString("%1").arg(i, 2);
      if (isActive) {
         label += "*";
      }

      checkbox[i]->setText(label);

      if (checkbox[i]->isChecked())
         scsp_debug_instrument_set_mute(sa, 1);
      else
         scsp_debug_instrument_set_mute(sa, 0);
   }

   ScspUnLockThread();

   // Note: meterWidget updates itself via its own timer
}
