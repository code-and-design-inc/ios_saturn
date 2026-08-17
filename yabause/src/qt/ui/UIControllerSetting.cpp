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
#include "UIPadSetting.h"
#include "UIPortManager.h"
#include "../Settings.h"
#include "../InputPortConfig.h"
#include "InputDeviceCombo.h"

#include <QKeyEvent>
#include <QTimer>
#include <QStringList>
#ifdef HAVE_LIBSDL
#include "../../persdljoy.h"
#endif

#include <QComboBox>
#include <QLabel>
#include <QStylePainter>
#include <QStyleOptionToolButton>

// Make a parent class for all controller setting classes


UIControllerSetting::UIControllerSetting( PerInterface_struct* core, uint port, uint pad, uint perType, QWidget* parent )
	: QDialog( parent )
{
	Q_ASSERT( core );
	
	mCore = core;
	mPort = port;
	mPad = pad;
	mPerType = perType;
	mTimer = new QTimer( this );
	mTimer->setInterval( 25 );
	curTb = NULL;
	mDeviceCombo = NULL;
	mDeviceTimer = NULL;
	mDeviceGeneration = 0;
	mPadKey = 0;
	mlInfos = NULL;
	scanFlags = PERSF_ALL;
	QtYabause::retranslateWidget( this );
}

UIControllerSetting::~UIControllerSetting()
{
#ifdef HAVE_LIBSDL
	// Leave the core scanning every device again for the next dialog.
	PERSDLJoySetScanDeviceIndex( PERSDL_SCAN_ANY_DEVICE );
#endif
}

void UIControllerSetting::setInfos(QLabel *lInfos)
{
   mlInfos = lInfos;
}

void UIControllerSetting::setScanFlags(u32 scanMask)
{
	switch (mPerType)
	{
		case PERPAD:
			scanFlags = PERSF_KEY | PERSF_BUTTON | PERSF_HAT | PERSF_AXIS;
			break;
		case PERWHEEL:
		case PERMISSIONSTICK:
		case PER3DPAD:
		case PERTWINSTICKS:
			scanFlags = PERSF_KEY | PERSF_BUTTON | PERSF_HAT | PERSF_AXIS;
			break;
		case PERGUN:
			scanFlags = PERSF_KEY | PERSF_BUTTON | PERSF_MOUSEMOVE;
			break;
		case PERKEYBOARD:
			scanFlags = PERSF_KEY;
			break;
		case PERMOUSE:
			scanFlags = PERSF_KEY | PERSF_BUTTON | PERSF_HAT | PERSF_MOUSEMOVE;
			break;
		default:
			scanFlags = PERSF_ALL;
			break;
	}

	scanFlags &= scanMask;
	setMouseTracking(scanFlags & PERSF_MOUSEMOVE ? true : false);
}

// Deliberately NOT delegated to InputDeviceCombo::selectedDeviceId(), despite
// the matching name: with no combo yet this reports the keyboard, so a dialog
// built before installDeviceSelector() ran still binds host input, while the
// shared helper reports an empty string. Delegating would be a silent change
// of behaviour here, not a cleanup.
QString UIControllerSetting::selectedDeviceId() const
{
	if ( !mDeviceCombo )
		return InputPortConfig::KeyboardDeviceId;
	return mDeviceCombo->itemData( mDeviceCombo->currentIndex() ).toString();
}

bool UIControllerSetting::bindsHostInput() const
{
	if ( !mDeviceCombo )
		return true;
	return InputPortConfig::bindsHostInput( selectedDeviceId() );
}

//////////////////////////////////////////////////////////////////////////////

void UIControllerSetting::keyPressEvent( QKeyEvent* e )
{
	if ( mTimer->isActive() )
	{
		if ( e->key() != Qt::Key_Escape )
		{
			if ( bindsHostInput() )
				setPadKey( e->key() );
			else
				e->ignore();
		}
		else
		{
			e->ignore();
			mButtons.key( mPadKey )->setChecked( false );
			mlInfos->clear();
			mTimer->stop();
			curTb->setAttribute(Qt::WA_TransparentForMouseEvents, false);
		}
	}
	else if ( e->key() == Qt::Key_Escape )
	{
		reject();
	}
	else
	{
		QWidget::keyPressEvent( e );
	}
}

void UIControllerSetting::mouseMoveEvent( QMouseEvent * e )
{
	if ( mTimer->isActive() )
	{
		if ( bindsHostInput() && (scanFlags & PERSF_MOUSEMOVE) )
			setPadKey((1 << 30));
	}
	else
		QWidget::mouseMoveEvent( e );
}

void UIControllerSetting::mousePressEvent( QMouseEvent * e )
{
	if ( mTimer->isActive() )
	{
		if ( bindsHostInput() && (scanFlags & PERSF_BUTTON) )
			setPadKey( (1 << 31) | e->button() );
	}
	else
		QWidget::mousePressEvent( e );
}

void UIControllerSetting::setPadKey( u32 key )
{
	Q_ASSERT( mlInfos );

	const QString settingsKey = QString( UIPortManager::mSettingsKey )
		.arg( mPort )
		.arg( mPad )
		.arg( mPerType )
		.arg( mPadKey );
	
	QtYabause::settings()->setValue( settingsKey, (quint32)key );
	mButtons.key( mPadKey )->setIcon( QIcon( ":/actions/icons/actions/button_ok.png" ) );
	mButtons.key( mPadKey )->setChecked( false );
	mlInfos->clear();
	mTimer->stop();
	if (curTb)
	   curTb->setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

void UIControllerSetting::loadPadSettings()
{
	Settings* settings = QtYabause::settings();
	
	foreach ( const u8& name, mNames.keys() )
	{
		mPadKey = name;
		const QString settingsKey = QString( UIPortManager::mSettingsKey )
			.arg( mPort )
			.arg( mPad )
			.arg( mPerType )
			.arg( mPadKey );
		
		if ( settings->contains( settingsKey ) )
		{
			setPadKey( settings->value( settingsKey ).toUInt() );
		}
	}
}

bool UIControllerSetting::eventFilter( QObject* object, QEvent* event )
{
	if ( event->type() == QEvent::Paint )
	{
		QToolButton* tb = qobject_cast<QToolButton*>( object );
		
		if ( tb )
		{
			if ( tb->isChecked() )
			{
				QStylePainter sp( tb );
				QStyleOptionToolButton options;
				
				options.initFrom( tb );
				options.arrowType = Qt::NoArrow;
				options.features = QStyleOptionToolButton::None;
				options.icon = tb->icon();
				options.iconSize = tb->iconSize();
				options.state = QStyle::State_Enabled | QStyle::State_HasFocus | QStyle::State_On | QStyle::State_AutoRaise;
				
				sp.drawComplexControl( QStyle::CC_ToolButton, options );
				
				return true;
			}
		}
	}
	
	return false;
}

void UIControllerSetting::tbButton_clicked()
{
	QToolButton* tb = qobject_cast<QToolButton*>( sender() );
	
	if ( !mTimer->isActive() )
	{
		tb->setChecked( true );
		mPadKey = mButtons[ tb ];
	
		QString text1 = QtYabause::translate(QString("Awaiting input for"));
		QString text2 = QtYabause::translate(mNames[ mPadKey ]);
		QString text3 = QtYabause::translate(QString("Press Esc key to cancel"));

		mlInfos->setText( text1 + QString(": %1\n").arg(text2) + text3 );
		setScanFlags(mScanMasks[mPadKey]);
		mCore->Flush();
		curTb=tb;
		tb->setAttribute(Qt::WA_TransparentForMouseEvents);
		mTimer->start();
	}
	else
	{
		tb->setChecked( tb == mButtons.key( mPadKey ) );
	}
}

void UIControllerSetting::timer_timeout()
{
	u32 key = 0;
	key = mCore->Scan(scanFlags);
	
	if ( key != 0 )
	{
		setPadKey( key );
	}
}

//////////////////////////////////////////////////////////////////////////////

void UIControllerSetting::populateDeviceCombo()
{
	Settings* settings = QtYabause::settings();
	const QString current = InputPortConfig::configuredDevice( settings, mPort, mPad );
	const QString storedName = settings->value(
		InputPortConfig::deviceNameKey( mPort, mPad ) ).toString();

	InputPortConfig::SdlDeviceSource source;
	const QList<InputPortConfig::Choice> choices =
		InputPortConfig::choicesForPort( source, mPerType, current, storedName );

	InputDeviceCombo::fill( mDeviceCombo, mPerType, choices, current );
}

//////////////////////////////////////////////////////////////////////////////

void UIControllerSetting::deviceTimer_timeout()
{
#ifdef HAVE_LIBSDL
	const int generation = PERSDLJoyRefreshDevices();
	if ( generation == mDeviceGeneration )
		return;

	mDeviceGeneration = generation;
	populateDeviceCombo();
	applyScanDevice();
#endif
}

//////////////////////////////////////////////////////////////////////////////

void UIControllerSetting::installDeviceSelector()
{
	QLabel* caption = new QLabel( QtYabause::translate( "Input device" ), this );
	mDeviceCombo = new QComboBox( this );
#ifdef HAVE_LIBSDL
	mDeviceGeneration = PERSDLJoyRefreshDevices();
#endif
	populateDeviceCombo();
	// These dialogs place a picture of the controller and its buttons at absolute
	// coordinates, so there is no layout to insert into: make room at the top by
	// moving everything down instead.
	const int rowHeight = qMax( caption->sizeHint().height(), mDeviceCombo->sizeHint().height() );
	const int shift = rowHeight + 8;
	foreach ( QWidget* w, findChildren<QWidget*>() )
	{
		if ( w == caption || w == mDeviceCombo || w->parentWidget() != this )
			continue;
		w->move( w->x(), w->y() + shift );
	}
	resize( width(), height() + shift );

	const int captionWidth = caption->sizeHint().width();
	caption->setGeometry( 6, 4, captionWidth, rowHeight );
	mDeviceCombo->setGeometry( captionWidth + 12, 4, qMax( 200, width() - captionWidth - 24 ), rowHeight );

	applyScanDevice();
	connect( mDeviceCombo, SIGNAL( currentIndexChanged( int ) ),
	         this, SLOT( deviceCombo_currentIndexChanged( int ) ) );

	// Watch for pads being plugged in or unplugged while the dialog is open.
	mDeviceTimer = new QTimer( this );
	mDeviceTimer->setInterval( 1000 );
	connect( mDeviceTimer, SIGNAL( timeout() ), this, SLOT( deviceTimer_timeout() ) );
	mDeviceTimer->start();
}

//////////////////////////////////////////////////////////////////////////////

void UIControllerSetting::applyScanDevice()
{
#ifdef HAVE_LIBSDL
	if ( !mDeviceCombo )
		return;

	const QString id = selectedDeviceId();
	if ( !InputPortConfig::isPhysicalDevice( id ) )
	{
		PERSDLJoySetScanDeviceIndex( PERSDL_SCAN_NO_DEVICE );
		return;
	}
	// An unplugged device resolves to -1, which would mean "any device"; keep it
	// disabled instead so nothing gets bound to the wrong pad.
	const int index = PERSDLJoyGetDeviceIndexForId( id.toLatin1().constData() );
	PERSDLJoySetScanDeviceIndex( index < 0 ? PERSDL_SCAN_NO_DEVICE : index );
#endif
}

//////////////////////////////////////////////////////////////////////////////

void UIControllerSetting::refreshPadIcons()
{
	Settings* settings = QtYabause::settings();

	foreach ( const u8& name, mNames.keys() )
	{
		QToolButton* tb = mButtons.key( name );
		if ( !tb )
			continue;
		const bool assigned = settings->contains( QString( UIPortManager::mSettingsKey )
			.arg( mPort ).arg( mPad ).arg( mPerType ).arg( name ) );
		tb->setIcon( assigned ? QIcon( ":/actions/icons/actions/button_ok.png" ) : QIcon() );
		tb->setChecked( false );
	}
}

//////////////////////////////////////////////////////////////////////////////

void UIControllerSetting::deviceCombo_currentIndexChanged( int index )
{
	Q_UNUSED( index );
	if ( !mDeviceCombo )
		return;

	const int current = mDeviceCombo->currentIndex();
	InputPortConfig::SdlDeviceSource source;

	// The stored name is what identifies the device once it is unplugged, so it
	// has to be the plain device name, not the decorated label.
	const bool changed = InputPortConfig::selectDevice(
		QtYabause::settings(), source, mPort, mPad, mPerType,
		mDeviceCombo->itemData( current ).toString(),
		mDeviceCombo->itemData( current, InputDeviceCombo::DeviceNameRole ).toString() );

	if ( changed )
	{
		refreshPadIcons();
		if ( mlInfos )
			mlInfos->clear();
	}

	applyScanDevice();
}
