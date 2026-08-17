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
#ifndef UIDEBUGSCSP_H
#define UIDEBUGSCSP_H

#include "ui_UIDebugSCSP.h"
#include "../QtYabause.h"
#include "UIDebugSCSPGraph.h"

extern "C" {
#include "scsp.h"
}

#ifdef HAVE_QT_MULTIMEDIA
#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioFormat>
#endif

class QComboBox;

class UIDebugSCSP : public QDialog, public Ui::UIDebugSCSP
{
	Q_OBJECT
private:
#ifdef HAVE_QT_MULTIMEDIA
	QTimer *audioBufferTimer;

	QMediaDevices audioDeviceInfo;
	QAudioSink*audioOutput;
	QIODevice *outputDevice;
	QAudioFormat audioFormat;
	bool isPlaying;

	u32 *slot_workbuf;
	s16 *slot_buf;
#endif

	// Real-time update timer (Issue #73)
	QTimer *updateTimer;
	bool realtimeUpdateEnabled;
	ScspSlotDebugInfo prevSlotInfo;  // For change detection

	// Graph widget (Issue #73)
	UIDebugSCSPGraph *graphWidget;
	QComboBox *cbGraphMode;

public:
	UIDebugSCSP( QWidget* parent = 0 );
	~UIDebugSCSP();

#ifdef HAVE_QT_MULTIMEDIA
protected:
	void initAudio();
#endif

protected:
	void updateSlotInfo();
	QString formatSlotInfo(const ScspSlotDebugInfo &info);
	QString getEnvelopeStateName(int state);

protected slots:
   void on_sbSlotNumber_valueChanged ( int i );
   void on_pbSaveAsWav_clicked ();
   void on_pbSaveSlotRegisters_clicked ();
   void onUpdateTimer();
   void on_cbRealtimeUpdate_toggled(bool checked);
   void onGraphModeChanged(int index);
#ifdef HAVE_QT_MULTIMEDIA
	void on_pbPlaySlot_clicked ();
	void notified();
	void audioBufferRefill();
	void stateChanged(QAudio::State state);
#endif
};

#endif // UIDEBUGSCSP_H
