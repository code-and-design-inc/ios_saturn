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
#include "UIDebugSCSP.h"
#include "CommonDialogs.h"

#include <QImageWriter>
#include <QGraphicsPixmapItem>
#include <QDebug>
#include <QIODevice>
#include <QTimer>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>

UIDebugSCSP::UIDebugSCSP( QWidget* p )
	: QDialog( p )
	, graphWidget(nullptr)
	, cbGraphMode(nullptr)
{
	// setup dialog
	setupUi( this );

   sbSlotNumber->setMinimum(0);
   sbSlotNumber->setMaximum(31);
   sbSlotNumber->setValue(31);
   sbSlotNumber->setValue(0);

   // Setup Common Control registers
   if (HighWram)
   {
      char tempstr[2048];
      ScspCommonControlRegisterDebugStats(tempstr);
      pteCommonControlRegisters->appendPlainText(tempstr);
      pteCommonControlRegisters->moveCursor(QTextCursor::Start);
   }

#ifdef HAVE_QT_MULTIMEDIA
	audioBufferTimer = new QTimer(this);
	//audioDeviceInfo = QMediaDevices::defaultAudioOutput();
	audioOutput = 0;
	slot_workbuf = 0;
	slot_buf = 0;
	initAudio();
#endif

   // Disable DSP Register display
   gbDSPControlRegisters->setVisible( false );

   // Real-time update timer (Issue #73)
   updateTimer = new QTimer(this);
   connect(updateTimer, SIGNAL(timeout()), this, SLOT(onUpdateTimer()));
   realtimeUpdateEnabled = false;
   memset(&prevSlotInfo, 0, sizeof(prevSlotInfo));

   // Graph widget (Issue #73)
   QGroupBox *gbGraph = new QGroupBox("Graph View", this);
   QVBoxLayout *graphLayout = new QVBoxLayout(gbGraph);

   // Graph mode selector
   QHBoxLayout *modeLayout = new QHBoxLayout();
   QLabel *modeLabel = new QLabel("Mode:", gbGraph);
   cbGraphMode = new QComboBox(gbGraph);
   cbGraphMode->addItem("Envelope", (int)SCSPGraphMode::Envelope);
   cbGraphMode->addItem("LFO", (int)SCSPGraphMode::LFO);
   cbGraphMode->addItem("Waveform", (int)SCSPGraphMode::Waveform);
   modeLayout->addWidget(modeLabel);
   modeLayout->addWidget(cbGraphMode);
   modeLayout->addStretch();
   graphLayout->addLayout(modeLayout);

   // Graph widget
   graphWidget = new UIDebugSCSPGraph(gbGraph);
   graphWidget->setMinimumHeight(180);
   graphLayout->addWidget(graphWidget);

   // Add to main layout (after the existing horizontal layout)
   QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
   if (mainLayout) {
      mainLayout->insertWidget(1, gbGraph);
   }

   connect(cbGraphMode, SIGNAL(currentIndexChanged(int)), this, SLOT(onGraphModeChanged(int)));

	// retranslate widgets
	QtYabause::retranslateWidget( this );
}

UIDebugSCSP::~UIDebugSCSP()
{
#ifdef HAVE_QT_MULTIMEDIA
	delete slot_workbuf;
	delete slot_buf;
#endif
}

#ifdef HAVE_QT_MULTIMEDIA
void UIDebugSCSP::initAudio()
{
	connect(audioBufferTimer, SIGNAL(timeout()), SLOT(audioBufferRefill()));

	isPlaying = true;

#if QT_VERSION < 0x040700
	audioFormat.setFrequency(44100);
	audioFormat.setChannels(2);
#else
	audioFormat.setSampleRate(44100);
	audioFormat.setChannelCount(2);
#endif
	//audioFormat.setSampleSize(16);
	//audioFormat.setCodec("audio/pcm");
	//audioFormat.setByteOrder(QAudioFormat::LittleEndian);
	//audioFormat.setSampleType(QAudioFormat::SignedInt);

	//QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
	//if (!info.isFormatSupported(audioFormat)) 
	//{
	//	qWarning() << "Normal format not available, trying alternative";
	//	audioFormat = info.nearestFormat(audioFormat);
	//}

	//delete audioOutput;
	//audioOutput = 0;
	//audioOutput = new QAudioOutput(audioDeviceInfo, audioFormat, this);
	//connect(audioOutput, SIGNAL(notify()), SLOT(notified()));
	//connect(audioOutput, SIGNAL(stateChanged(QAudio::State)), SLOT(stateChanged(QAudio::State)));

	ScspSlotResetDebug(sbSlotNumber->value());
}

void UIDebugSCSP::notified()
{
	if (audioOutput) {
		qint64 bytesFree = audioOutput->bytesFree(); // Get buffer free space
		qint64 elapsedUSecs = audioOutput->elapsedUSecs(); // Elapsed time since playback started (microseconds)
		qint64 processedUSecs = audioOutput->processedUSecs(); // Processed microseconds
		int periodSize = audioOutput->bufferSize(); // Get periodSize as buffer size

		qWarning() << "bytesFree =" << bytesFree
			<< ", elapsedUSecs =" << elapsedUSecs
			<< ", processedUSecs =" << processedUSecs
			<< ", periodSize =" << periodSize;
	}
}

void UIDebugSCSP::audioBufferRefill()
{
/*
	if (audioOutput && audioOutput->state() != QAudio::StoppedState) 
	{
		int chunks = audioOutput->bytesFree()/audioOutput->periodSize();
		while (chunks) 
		{
			int len=audioOutput->periodSize();
			len = (len/2) + (len%2);
			if (ScspSlotDebugAudio (slot_workbuf, slot_buf, len) == 0)
				break;
			outputDevice->write((char *)slot_buf, audioOutput->periodSize());
			--chunks;
		}
	}
*/
}

void UIDebugSCSP::stateChanged(QAudio::State state)
{
/*
	if (state == QAudio::IdleState)
	{
		delete slot_workbuf;
		delete slot_buf;
		slot_workbuf = 0;
		slot_buf = 0;
		notified();
		slot_workbuf = new u32[audioOutput->periodSize()*4];
		slot_buf = new s16[audioOutput->periodSize()];
	}
*/
}
#endif

void UIDebugSCSP::on_sbSlotNumber_valueChanged ( int i )
{
   // Update Sound Slot Info
   char tempstr[2048];
   if (HighWram)
   {
      ScspSlotDebugStats(i, tempstr);
      pteSlotInfo->clear();
      pteSlotInfo->appendPlainText(tempstr);
      pteSlotInfo->moveCursor(QTextCursor::Start);
      pbSaveAsWav->setEnabled(true);
      pbSaveSlotRegisters->setEnabled(true);
   }
   else
   {
      pbSaveAsWav->setEnabled(false);
      pbSaveSlotRegisters->setEnabled(false);
   }

   // Update graph slot (Issue #73)
   if (graphWidget) {
      graphWidget->setSlot(i);
   }
}

#ifdef HAVE_QT_MULTIMEDIA
void UIDebugSCSP::on_pbPlaySlot_clicked ()
{
	audioBufferTimer->stop();
	//audioOutput->stop();

	if (isPlaying) 
	{
		ScspSlotResetDebug(sbSlotNumber->value());
		pbPlaySlot->setText(QtYabause::translate("Stop Slot"));
		//outputDevice = audioOutput->start();
		isPlaying = false;
		audioBufferTimer->start(20);
	} 
	else 
	{
		pbPlaySlot->setText(QtYabause::translate("Play Slot"));
		isPlaying = true;
	}
}
#endif

void UIDebugSCSP::on_pbSaveAsWav_clicked ()
{
	// request a file to save to to user
  QString text;
   
	text = QString("channel%1.wav").arg(sbSlotNumber->value(), 2, 10, QChar('0'));
	const QString s = CommonDialogs::getSaveFileName(text, QtYabause::translate( "Choose a location for your wav file" ), QtYabause::translate( "WAV Files (*.wav)" ) );
	
	// write image if ok
	if ( !s.isEmpty() )
		if (ScspSlotDebugAudioSaveWav(sbSlotNumber->value(), s.toLatin1()) != 0)
			CommonDialogs::information( QtYabause::translate( "An error occured while writing file." ) );                  
}

void UIDebugSCSP::on_pbSaveSlotRegisters_clicked ()
{
	const QString s = CommonDialogs::getSaveFileName( QString(), QtYabause::translate( "Choose a location for your binary file" ), QtYabause::translate( "Binary Files (*.bin)" ) );
	if ( !s.isEmpty() )
      if (ScspSlotDebugSaveRegisters(sbSlotNumber->value(), s.toLatin1()) != 0)
			CommonDialogs::information( QtYabause::translate( "An error occured while writing file." ) );
}

// Real-time update functions (Issue #73)
void UIDebugSCSP::on_cbRealtimeUpdate_toggled(bool checked)
{
   realtimeUpdateEnabled = checked;
   if (checked) {
      scsp_debug_set_mode(1);  // Enable debug mode for waveform recording
      updateTimer->start(33);  // ~30 FPS update rate
      graphWidget->setRunning(true);
   } else {
      updateTimer->stop();
      graphWidget->setRunning(false);
      scsp_debug_set_mode(0);  // Disable debug mode
   }
}

void UIDebugSCSP::onGraphModeChanged(int index)
{
   SCSPGraphMode mode = static_cast<SCSPGraphMode>(cbGraphMode->itemData(index).toInt());
   graphWidget->setMode(mode);
}

void UIDebugSCSP::onUpdateTimer()
{
   if (!HighWram || !realtimeUpdateEnabled)
      return;

   updateSlotInfo();
}

QString UIDebugSCSP::getEnvelopeStateName(int state)
{
   switch (state) {
      case SCSP_ENV_ATTACK:  return "ATTACK";
      case SCSP_ENV_DECAY1:  return "DECAY1";
      case SCSP_ENV_DECAY2:  return "DECAY2";
      case SCSP_ENV_RELEASE: return "RELEASE";
      default: return "UNKNOWN";
   }
}

QString UIDebugSCSP::formatSlotInfo(const ScspSlotDebugInfo &info)
{
   QString result;

   // Active status with color indicator
   QString activeStatus = info.is_active ? "[ACTIVE]" : "[INACTIVE]";

   result += QString("=== Slot Status: %1 ===\n\n").arg(activeStatus);

   // Key state
   result += QString("Key On: %1, Key Bit: %2\n")
      .arg(info.key_on).arg(info.key_bit);

   // Source settings
   QString sourceType;
   switch (info.source_select) {
      case 0: sourceType = "External DRAM"; break;
      case 1: sourceType = "Noise"; break;
      case 2: sourceType = "Zero"; break;
      default: sourceType = "Unknown"; break;
   }
   QString loopType;
   switch (info.loop_ctrl) {
      case 0: loopType = "None"; break;
      case 1: loopType = "Normal"; break;
      case 2: loopType = "Reverse"; break;
      case 3: loopType = "Alternating"; break;
      default: loopType = "Unknown"; break;
   }
   result += QString("Source: %1, Loop: %2, PCM: %3-bit\n")
      .arg(sourceType).arg(loopType).arg(info.pcm_8bit ? 8 : 16);

   // Address info
   result += QString("\n--- Address ---\n");
   result += QString("Start: 0x%1, Loop Start: 0x%2, Loop End: 0x%3\n")
      .arg(info.start_addr, 6, 16, QChar('0'))
      .arg(info.loop_start, 4, 16, QChar('0'))
      .arg(info.loop_end, 4, 16, QChar('0'));
   result += QString("Current: 0x%1\n")
      .arg(info.current_addr, 6, 16, QChar('0'));

   // Envelope (ADSR)
   result += QString("\n--- Envelope (ADSR) ---\n");
   result += QString("State: %1, Level: %2 (0x%3)\n")
      .arg(getEnvelopeStateName(info.envelope_state))
      .arg(info.envelope_level)
      .arg(info.envelope_level, 3, 16, QChar('0'));
   result += QString("AR: %1, D1R: %2, D2R: %3, RR: %4\n")
      .arg(info.attack_rate).arg(info.decay1_rate)
      .arg(info.decay2_rate).arg(info.release_rate);
   result += QString("DL: %1, KRS: %2\n")
      .arg(info.decay_level).arg(info.key_rate_scale);

   // Pitch
   result += QString("\n--- Pitch ---\n");
   result += QString("Octave: %1, FNS: %2 (0x%3)\n")
      .arg((int8_t)(info.octave ^ 8) - 8)  // Convert to signed
      .arg(info.frequency_num)
      .arg(info.frequency_num, 3, 16, QChar('0'));
   result += QString("Phase: 0x%1\n")
      .arg(info.phase_value, 8, 16, QChar('0'));

   // LFO
   result += QString("\n--- LFO ---\n");
   QString pitchWave, ampWave;
   const char* waveNames[] = {"Saw", "Square", "Triangle", "Noise"};
   pitchWave = waveNames[info.pitch_lfo_wave & 3];
   ampWave = waveNames[info.amp_lfo_wave & 3];
   result += QString("Frequency: %1\n").arg(info.lfo_frequency);
   result += QString("Pitch LFO: Wave=%1, Shift=%2\n")
      .arg(pitchWave).arg(info.pitch_lfo_shift);
   result += QString("Amp LFO: Wave=%1, Shift=%2\n")
      .arg(ampWave).arg(info.amp_lfo_shift);
   result += QString("Position: 0x%1\n")
      .arg(info.lfo_position, 8, 16, QChar('0'));

   // Level
   result += QString("\n--- Level ---\n");
   result += QString("Total Level: %1 (0x%2)\n")
      .arg(info.total_level).arg(info.total_level, 3, 16, QChar('0'));
   result += QString("Direct: Level=%1, Pan=%2\n")
      .arg(info.direct_level).arg(info.direct_pan);
   result += QString("Effect: Level=%1, Pan=%2\n")
      .arg(info.effect_level).arg(info.effect_pan);

   // Modulation
   result += QString("\n--- Modulation ---\n");
   result += QString("MDL: %1, MDXSL: %2, MDYSL: %3\n")
      .arg(info.mod_level).arg(info.mod_x_select).arg(info.mod_y_select);

   // Output
   result += QString("\n--- Output ---\n");
   result += QString("Current Output: %1, Muted: %2\n")
      .arg(info.current_output).arg(info.is_muted ? "Yes" : "No");

   return result;
}

void UIDebugSCSP::updateSlotInfo()
{
   int slotNum = sbSlotNumber->value();
   ScspSlotDebugInfo info;

   ScspLockThread();
   scsp_debug_get_slot_state(slotNum, &info);
   ScspUnLockThread();

   // Format and display the info
   QString infoText = formatSlotInfo(info);
   pteSlotInfo->clear();
   pteSlotInfo->appendPlainText(infoText);
   pteSlotInfo->moveCursor(QTextCursor::Start);

   // Update window title with active status
   if (info.is_active) {
      setWindowTitle(QString("SCSP Debug - Slot %1 [ACTIVE]").arg(slotNum));
   } else {
      setWindowTitle(QString("SCSP Debug - Slot %1").arg(slotNum));
   }

   prevSlotInfo = info;
}
