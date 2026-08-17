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
#ifndef UIDebugSCSPChan_H
#define UIDebugSCSPChan_H

#include "../QtYabause.h"
#include <QDialog>
#include <QCheckBox>
#include <QLabel>

extern "C" {
#include "scsp.h"
}

#ifdef HAVE_QT_MULTIMEDIA
#include <QAudioOutput>
#endif

// Issue #73: Extended SCSP Channel Debug
#define SCSP_DEBUG_CHANNEL_COUNT 32

// Forward declaration
class SCSPMeterWidget;

class UIDebugSCSPChan : public QDialog
{
   Q_OBJECT
private:
   QCheckBox *checkbox[SCSP_DEBUG_CHANNEL_COUNT];
   QTimer *timer;

   // Meter widget for graphical display
   SCSPMeterWidget *meterWidget;

public:
   UIDebugSCSPChan(QWidget* parent = 0);
   ~UIDebugSCSPChan();

protected slots:
   void update_window();
};

#endif // UIDebugSCSPChan_H
