/*	Copyright 2008 Filipe Azevedo <pasnox@gmail.com>
   Copyright 2013 Theo Berkau <cwx@cyberwarriorx.com>

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
#ifndef UICONTROLLERSETTING_H
#define UICONTROLLERSETTING_H

#include <QDialog>
#include <QLabel>
#include <QToolButton>
#include "QtYabause.h"

#include <QMap>

class QComboBox;

class QTimer;

class UIControllerSetting : public QDialog
{
	Q_OBJECT

public:
	UIControllerSetting( PerInterface_struct* core, uint port, uint pad, uint perType, QWidget* parent = 0 );
	virtual ~UIControllerSetting();
	void setInfos(QLabel *lInfos);
	// Call after setupUi(): inserts the physical input device selector at the top
	// of the dialog. A Saturn port is driven by exactly one physical device, so
	// this is where that device is chosen.
	void installDeviceSelector();

protected:
	PerInterface_struct* mCore;
	uint mPort;
	uint mPad;
	u8 mPadKey;
	uint mPerType;
	QTimer* mTimer;
	QMap<QToolButton*, u8> mButtons;
	QMap<u8, QString> mNames;
	QMap<u8, u32> mScanMasks;
	QLabel *mlInfos;
	u32 scanFlags;
	QToolButton * curTb;
	QComboBox* mDeviceCombo;
	QTimer* mDeviceTimer;
	int mDeviceGeneration;

	// (Re)fill the device list, keeping the current selection. Used both when
	// the dialog opens and when a device is plugged in or unplugged while it
	// is open.
	void populateDeviceCombo();

	// Point the peripheral core's scan at the currently selected device so a
	// button press on another pad cannot be bound here by accident.
	void applyScanDevice();
	// Repaint the "assigned" marks from what is currently stored.
	void refreshPadIcons();
	// The device the selector is currently showing.
	QString selectedDeviceId() const;
	// True when this port is driven by input Qt delivers (keyboard, and the
	// mouse for the Saturn mouse and gun). False for an SDL device, in which
	// case a key or mouse press must not be bindable here - the port is driven
	// by exactly one physical device.
	bool bindsHostInput() const;

	void keyPressEvent( QKeyEvent* event );
	void mouseMoveEvent(QMouseEvent * event);
	void mousePressEvent(QMouseEvent * event);
	void setPadKey( u32 key );
	void loadPadSettings();
	void setScanFlags(u32 scanMask);

	virtual bool eventFilter( QObject* object, QEvent* event );

protected slots:
	void tbButton_clicked();
	void deviceCombo_currentIndexChanged( int index );
	void deviceTimer_timeout();
	void timer_timeout();
};

#endif // UICONTROLLERSETTING_H
