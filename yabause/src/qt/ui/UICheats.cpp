/*	Copyright 2008 Filipe Azevedo <pasnox@gmail.com>

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
#include "UICheats.h"
#include "UICheatAR.h"
#include "UICheatRaw.h"
#include "UIYabause.h"
#include "../CommonDialogs.h"
#include "../QtYabause.h"

#include <QMessageBox>
#include <QMetaObject>
#include <QStringList>

#include <firebase/app.h>
#include <firebase/auth.h>
#include <firebase/database.h>
#include <firebase/variant.h>

#include <algorithm>
#include <cstring>
#include <string>

using firebase::database::DataSnapshot;
using firebase::database::DatabaseReference;
using firebase::Variant;

namespace
{
std::string trimGameCode( const char* raw )
{
	if ( raw == nullptr ) {
		return std::string();
	}
	std::string s( raw );
	// Cs2 itemnum is a fixed 11-char buffer right-padded with spaces.
	while ( !s.empty() && ( s.back() == ' ' || s.back() == '\0' ) ) {
		s.pop_back();
	}
	return s;
}
}

// Static storage: survives dialog open/close cycles.
std::map<std::string, std::vector<int>> UICheats::sEnabledCloudCheats;

UICheats::UICheats( QWidget* p )
	: QDialog( p ),
	  mCheats( nullptr ),
	  mCloudListenerAttached( false )
{
	// set up dialog
	setupUi( this );
	if ( p && !p->isFullScreen() )
		setWindowFlags( Qt::Sheet );

	// Local tab: populate from existing core cheat list.
	int cheatsCount = 0;
	mCheats = CheatGetList( &cheatsCount );

	// Validate the static cloud-index map. If any index is out of range (e.g., the
	// core list was mutated outside this dialog), wipe the map to recover sanely.
	{
		bool validMap = true;
		for ( const auto& kv : sEnabledCloudCheats ) {
			for ( int idx : kv.second ) {
				if ( idx < 0 || idx >= cheatsCount ) {
					validMap = false;
					break;
				}
			}
			if ( !validMap ) break;
		}
		if ( !validMap ) {
			sEnabledCloudCheats.clear();
		}
	}

	// Collect indices owned by cloud-enabled cheats so we skip them in the Local tab.
	std::set<int> cloudOwnedIndices;
	for ( const auto& kv : sEnabledCloudCheats ) {
		cloudOwnedIndices.insert( kv.second.begin(), kv.second.end() );
	}

	for ( int id = 0; id < cheatsCount; id++ ) {
		if ( cloudOwnedIndices.find( id ) == cloudOwnedIndices.end() ) {
			addCode( id );
		}
	}
	pbSaveFile->setEnabled( cheatsCount > 0 );

	// Shared tab: derive game code; if no game is running, disable the tab.
	mGameCode = trimGameCode( QtYabause::getCurrentCdSerial() );
	const int sharedTabIndex = twCheatTabs->indexOf( tabShared );
	if ( mGameCode.empty() ) {
		twCheatTabs->setTabEnabled( sharedTabIndex, false );
		twCheatTabs->setTabToolTip( sharedTabIndex, tr( "No game running" ) );
	} else {
		// Defer to auth state: hook listener and reflect current state.
		firebase::auth::Auth* auth = firebase::auth::Auth::GetAuth( UIYabause::getFirebaseApp() );
		if ( auth != nullptr ) {
			auth->AddAuthStateListener( this );
			firebase::auth::User user = auth->current_user();
			applyAuthState( user.is_valid() );
		} else {
			applyAuthState( false );
		}
	}

	// retranslate widgets
	QtYabause::retranslateWidget( this );
}

void UICheats::addCode( int id )
{
	// generate caption
	QString s;
	switch ( mCheats[id].type )
	{
		case CHEATTYPE_ENABLE:
			s = QtYabause::translate( "Enable Code : %1 %2" ).arg( (int)mCheats[id].addr, 8, 16, QChar( '0' ) ).arg( (int)mCheats[id].val, 8, 16, QChar( '0' ) );
			break;
		case CHEATTYPE_BYTEWRITE:
			s = QtYabause::translate( "Byte Write : %1 %2" ).arg( (int)mCheats[id].addr, 8, 16, QChar( '0' ) ).arg( (int)mCheats[id].val, 2, 16, QChar( '0' ) );
			break;
		case CHEATTYPE_WORDWRITE:
			s = QtYabause::translate( "Word Write : %1 %2" ).arg( (int)mCheats[id].addr, 8, 16, QChar( '0' ) ).arg( (int)mCheats[id].val, 4, 16, QChar( '0' ) );
			break;
		case CHEATTYPE_LONGWRITE:
			s = QtYabause::translate( "Long Write : %1 %2" ).arg( (int)mCheats[id].addr, 8, 16, QChar( '0' ) ).arg( (int)mCheats[id].val, 8, 16, QChar( '0' ) );
			break;
		default:
			break;
	}
	// update item
	QTreeWidgetItem* it = new QTreeWidgetItem( twCheats );
	it->setText( 0, s );
	it->setText( 1, mCheats[id].desc );
	it->setText( 2, mCheats[id].enable ? QtYabause::translate( "Enabled" ) : QtYabause::translate( "Disabled" ) );
	// enable buttons
	pbClear->setEnabled( true );
	pbSaveFile->setEnabled( true );
}

void UICheats::addARCode( const QString& c, const QString& d )
{
	// need check in list if already is code
	// add code
	if ( CheatAddARCode( c.toLatin1().constData() ) != 0 )
	{
		CommonDialogs::information( QtYabause::translate( "Unable to add code" ) );
		return;
	}
	// change the description
	int cheatsCount;
	mCheats = CheatGetList( &cheatsCount );
	if ( CheatChangeDescriptionByIndex( cheatsCount -1, d.toLatin1().data() ) != 0 )
		CommonDialogs::information( QtYabause::translate( "Unable to change description" ) );
	// add code in treewidget
	addCode( cheatsCount -1 );
}

void UICheats::addRawCode( int t, const QString& a, const QString& v, const QString& d )
{
	// need check in list if already is code
	bool b;
	quint32 u;
	// check address
	u = a.toUInt( &b, 16 );
	if ( !b )
	{
		CommonDialogs::information( QtYabause::translate( "Invalid Address" ) );
		return;
	}
	// check value
	u = v.toUInt( &b, 16 );
	if ( !b )
	{
		CommonDialogs::information( QtYabause::translate( "Invalid Value" ) );
		return;
	}
	// add value
	if ( CheatAddCode( t, a.toUInt(NULL, 16), v.toUInt() ) != 0 )
	{
		CommonDialogs::information( QtYabause::translate( "Unable to add code" ) );
		return;
	}
	// get cheats and cheats count
	int cheatsCount;
	mCheats = CheatGetList( &cheatsCount );
	// change description
	if ( CheatChangeDescriptionByIndex( cheatsCount -1, d.toLatin1().data() ) != 0 )
		CommonDialogs::information( QtYabause::translate( "Unable to change description" ) );
	// add code in treewidget
	addCode( cheatsCount -1 );
}

void UICheats::on_twCheats_itemSelectionChanged()
{ pbDelete->setEnabled( twCheats->selectedItems().count() ); }

void UICheats::on_twCheats_itemDoubleClicked( QTreeWidgetItem* it, int )
{
	if ( it )
	{
		// get id of item
		int id = twCheats->indexOfTopLevelItem( it );
		// if ok
		if ( id != -1 )
		{
			// disable cheat
			if ( mCheats[id].enable )
				CheatDisableCode( id );
			// enable cheat
			else
				CheatEnableCode( id );
			// update treewidget item
			it->setText( 2, mCheats[id].enable ? QtYabause::translate( "Enabled" ) : QtYabause::translate( "Disabled" ) );
		}
	}
}

void UICheats::on_pbDelete_clicked()
{
	clearAllCloudActive();
	// get current selected item
	if ( QTreeWidgetItem* it = twCheats->selectedItems().value( 0 ) )
	{
		// get item id
		int id = twCheats->indexOfTopLevelItem( it );
		// remove cheat
		if ( CheatRemoveCodeByIndex( id ) != 0 )
		{
			CommonDialogs::information( QtYabause::translate( "Unable to remove code" ) );
			return;
		}
		// delete item
		delete it;
		// disable buttons
		pbClear->setEnabled( twCheats->topLevelItemCount() );
	}
}

void UICheats::on_pbClear_clicked()
{
	clearAllCloudActive();
	// clear cheats
	CheatClearCodes();
	// clear treewidget items
	twCheats->clear();
	// disable buttons
	pbDelete->setEnabled( false );
	pbClear->setEnabled( false );
}

void UICheats::on_pbAR_clicked()
{
	// add AR code if dialog exec
	UICheatAR d( this );
	if ( d.exec() )
		addARCode( d.leCode->text(), d.teDescription->toPlainText() );
}

void UICheats::on_pbRaw_clicked()
{
	// add RAW code if dialog exec
	UICheatRaw d( this );
	if ( d.exec() && d.type() != -1 )
		addRawCode( d.type(), d.leAddress->text(), d.leValue->text(), d.teDescription->toPlainText() );
}

void UICheats::on_pbSaveFile_clicked()
{
	const QString s = CommonDialogs::getSaveFileName( ".", QtYabause::translate( "Choose a cheat file to save to" ), QtYabause::translate( "Yabause Cheat Files (*.yct);;All Files (*)" ) );
	if ( !s.isEmpty() )
		if ( CheatSave( s.toLatin1().constData() ) != 0 )
			CommonDialogs::information( QtYabause::translate( "Unable to open file for loading" ) );
}

void UICheats::on_pbLoadFile_clicked()
{
	sEnabledCloudCheats.clear();
	const QString s = CommonDialogs::getOpenFileName( ".", QtYabause::translate( "Choose a cheat file to open" ), QtYabause::translate( "Yabause Cheat Files (*.yct);;All Files (*)" ) );
	if ( !s.isEmpty() )
	{
		if ( CheatLoad( s.toLatin1().constData() ) == 0 )
		{
			// clear tree
			twCheats->clear();
			// get cheats and cheats count
			int cheatsCount;
			mCheats = CheatGetList( &cheatsCount );
			// add cheats
			for ( int i = 0; i < cheatsCount; i++ )
				addCode( i );
		}
		else
			CommonDialogs::information( QtYabause::translate( "Unable to open file for saving" ) );
	}
}

void UICheats::OnValueChanged( const firebase::database::DataSnapshot& snapshot )
{
	std::vector<CloudCheatItem> newItems;
	std::string myUid;

	firebase::auth::Auth* auth = firebase::auth::Auth::GetAuth( UIYabause::getFirebaseApp() );
	if ( auth != nullptr ) {
		firebase::auth::User user = auth->current_user();
		if ( user.is_valid() ) {
			myUid = user.uid();
		}
	}

	std::vector<DataSnapshot> children = snapshot.children();
	newItems.reserve( children.size() );

	for ( size_t i = 0; i < children.size(); ++i ) {
		const DataSnapshot& c = children[i];
		CloudCheatItem item;
		item.key = c.key();
		item.star_count = 0;
		item.liked_by_me = false;

		if ( c.Child( "description" ).is_valid() && c.Child( "description" ).value().is_string() ) {
			item.description = c.Child( "description" ).value().string_value();
		}
		if ( c.Child( "cheat_code" ).is_valid() && c.Child( "cheat_code" ).value().is_string() ) {
			item.cheat_code = c.Child( "cheat_code" ).value().string_value();
		}
		if ( c.Child( "star_count" ).is_valid() && c.Child( "star_count" ).value().is_int64() ) {
			item.star_count = c.Child( "star_count" ).value().int64_value();
		}
		if ( !myUid.empty() && c.Child( "like_users" ).is_valid() ) {
			DataSnapshot likeNode = c.Child( "like_users" ).Child( myUid );
			if ( likeNode.is_valid() && likeNode.exists() ) {
				item.liked_by_me = true;
			}
		}

		if ( item.cheat_code.empty() ) {
			continue; // skip malformed entries
		}
		newItems.push_back( item );
	}

	mCloudItems = std::move( newItems );

	// Marshal UI work to the Qt main thread.
	QMetaObject::invokeMethod( this, "rebuildCloudList", Qt::QueuedConnection );
}

void UICheats::OnCancelled( const firebase::database::Error& error_code, const char* error_message )
{
	Q_UNUSED( error_code );
	QString msg = error_message ? QString::fromUtf8( error_message ) : QString( "Database error" );
	QMetaObject::invokeMethod( this, "showCloudError", Qt::QueuedConnection, Q_ARG( QString, msg ) );
}

UICheats::~UICheats()
{
	detachCloudListener();

	firebase::auth::Auth* auth = firebase::auth::Auth::GetAuth( UIYabause::getFirebaseApp() );
	if ( auth != nullptr ) {
		auth->RemoveAuthStateListener( this );
	}
}

void UICheats::OnAuthStateChanged( firebase::auth::Auth* auth )
{
	const bool signedIn = ( auth != nullptr ) && auth->current_user().is_valid();
	QMetaObject::invokeMethod( this, "applyAuthState", Qt::QueuedConnection, Q_ARG( bool, signedIn ) );
}

void UICheats::applyAuthState( bool signedIn )
{
	if ( mGameCode.empty() ) {
		return;
	}
	if ( signedIn ) {
		swCloudState->setCurrentWidget( pageContent );
		attachCloudListener();
	} else {
		detachCloudListener();
		swCloudState->setCurrentWidget( pageSignInRequired );
		twCloudCheats->clear();
		mCloudItems.clear();
		pbCloudEnable->setEnabled( false );
		pbCloudLike->setEnabled( false );
	}
}

void UICheats::attachCloudListener()
{
	if ( mCloudListenerAttached || mGameCode.empty() ) {
		return;
	}
	firebase::database::Database* db =
		firebase::database::Database::GetInstance( UIYabause::getFirebaseApp() );
	if ( db == nullptr ) {
		return;
	}
	DatabaseReference ref = db->GetReference()
								.Child( "shared-cheats" )
								.Child( mGameCode );
	ref.AddValueListener( this );
	mCloudListenerAttached = true;
}

void UICheats::detachCloudListener()
{
	if ( !mCloudListenerAttached || mGameCode.empty() ) {
		return;
	}
	firebase::database::Database* db =
		firebase::database::Database::GetInstance( UIYabause::getFirebaseApp() );
	if ( db == nullptr ) {
		mCloudListenerAttached = false;
		return;
	}
	DatabaseReference ref = db->GetReference()
								.Child( "shared-cheats" )
								.Child( mGameCode );
	ref.RemoveValueListener( this );
	mCloudListenerAttached = false;
}

void UICheats::rebuildCloudList()
{
	// Sort by star_count descending; ties broken by description ascending.
	std::sort( mCloudItems.begin(), mCloudItems.end(),
		[]( const CloudCheatItem& a, const CloudCheatItem& b ) {
			if ( a.star_count != b.star_count ) {
				return a.star_count > b.star_count;
			}
			return a.description < b.description;
		} );

	twCloudCheats->clear();

	if ( mCloudItems.empty() ) {
		QTreeWidgetItem* placeholder = new QTreeWidgetItem( twCloudCheats );
		placeholder->setText( 1, tr( "No shared cheats available for this game" ) );
		placeholder->setDisabled( true );
		pbCloudEnable->setEnabled( false );
		pbCloudLike->setEnabled( false );
		return;
	}

	for ( size_t i = 0; i < mCloudItems.size(); ++i ) {
		const CloudCheatItem& item = mCloudItems[i];

		// First line of cheat_code as preview.
		QString fullCode = QString::fromUtf8( item.cheat_code.c_str() );
		QString preview = fullCode.section( '\n', 0, 0 );
		const bool enabled = ( sEnabledCloudCheats.find( item.key ) != sEnabledCloudCheats.end() );

		QTreeWidgetItem* row = new QTreeWidgetItem( twCloudCheats );
		row->setData( 0, Qt::UserRole, QString::fromStdString( item.key ) );
		row->setText( 0, QString( "%1 %2" ).arg( QChar( 0x2605 ) ).arg( (qlonglong)item.star_count ) );
		row->setText( 1, QString::fromUtf8( item.description.c_str() ) );
		row->setText( 2, preview );
		row->setToolTip( 2, fullCode );
		row->setText( 3, enabled ? tr( "Enabled" ) : tr( "Disabled" ) );
	}

	on_twCloudCheats_itemSelectionChanged();
}

void UICheats::showCloudError( QString message )
{
	QMessageBox::warning( this, tr( "Shared Cheats" ), message );
}

int UICheats::selectedCloudRow() const
{
	QList<QTreeWidgetItem*> sel = twCloudCheats->selectedItems();
	if ( sel.isEmpty() ) {
		return -1;
	}
	return twCloudCheats->indexOfTopLevelItem( sel.first() );
}

void UICheats::on_twCloudCheats_itemSelectionChanged()
{
	const int row = selectedCloudRow();
	const bool valid = ( row >= 0 && row < (int)mCloudItems.size() );
	pbCloudEnable->setEnabled( valid );
	pbCloudLike->setEnabled( valid );
}

void UICheats::on_pbCloudEnable_clicked()
{
	const int row = selectedCloudRow();
	if ( row < 0 || row >= (int)mCloudItems.size() ) {
		return;
	}
	const CloudCheatItem& item = mCloudItems[row];
	if ( sEnabledCloudCheats.find( item.key ) != sEnabledCloudCheats.end() ) {
		disableCloudCheat( item.key );
	} else {
		enableCloudCheat( item );
	}
	rebuildCloudList();
}

void UICheats::on_twCloudCheats_itemDoubleClicked( QTreeWidgetItem* item, int /*column*/ )
{
	if ( item == nullptr ) {
		return;
	}
	twCloudCheats->setCurrentItem( item );
	on_pbCloudEnable_clicked();
}

void UICheats::enableCloudCheat( const CloudCheatItem& item )
{
	QString full = QString::fromUtf8( item.cheat_code.c_str() );
	QStringList lines = full.split( '\n', Qt::SkipEmptyParts );

	std::vector<int> indices;
	for ( int i = 0; i < lines.size(); ++i ) {
		QByteArray bytes = lines[i].trimmed().toLatin1();
		if ( bytes.isEmpty() ) {
			continue;
		}
		int beforeCount = 0;
		(void)CheatGetList( &beforeCount );
		if ( CheatAddARCode( bytes.constData() ) != 0 ) {
			// Roll back any indices we already added.
			std::sort( indices.begin(), indices.end(), std::greater<int>() );
			for ( int idx : indices ) {
				CheatRemoveCodeByIndex( idx );
			}
			// Refresh cached pointer: prior CheatAddCode calls may have reallocated the core list.
			int dummyCount = 0;
			mCheats = CheatGetList( &dummyCount );
			CommonDialogs::information( tr( "Unable to add code" ) );
			return;
		}
		int newIndex = beforeCount; // newly added entry is appended at the end
		CheatEnableCode( newIndex );
		indices.push_back( newIndex );
	}
	// Refresh cached pointer: CheatAddCode may realloc the core list.
	int dummyCount = 0;
	mCheats = CheatGetList( &dummyCount );
	if ( indices.empty() ) {
		return;
	}
	sEnabledCloudCheats[item.key] = std::move( indices );
}

void UICheats::disableCloudCheat( const std::string& cloudKey )
{
	auto it = sEnabledCloudCheats.find( cloudKey );
	if ( it == sEnabledCloudCheats.end() ) {
		return;
	}
	std::vector<int> indices = it->second;
	sEnabledCloudCheats.erase( it );

	// Remove descending so earlier indices stay stable.
	std::sort( indices.begin(), indices.end(), std::greater<int>() );
	for ( int idx : indices ) {
		CheatRemoveCodeByIndex( idx );
	}
	int dummyCount = 0;
	mCheats = CheatGetList( &dummyCount );
}

void UICheats::clearAllCloudActive()
{
	if ( sEnabledCloudCheats.empty() ) {
		return;
	}
	// Collect all indices and remove descending.
	std::vector<int> all;
	for ( const auto& kv : sEnabledCloudCheats ) {
		all.insert( all.end(), kv.second.begin(), kv.second.end() );
	}
	sEnabledCloudCheats.clear();
	std::sort( all.begin(), all.end(), std::greater<int>() );
	for ( int idx : all ) {
		CheatRemoveCodeByIndex( idx );
	}
	int dummyCount = 0;
	mCheats = CheatGetList( &dummyCount );
}

void UICheats::on_pbCloudLike_clicked()
{
	const int row = selectedCloudRow();
	if ( row < 0 || row >= (int)mCloudItems.size() ) {
		return;
	}
	const CloudCheatItem item = mCloudItems[row];

	firebase::auth::Auth* auth = firebase::auth::Auth::GetAuth( UIYabause::getFirebaseApp() );
	if ( auth == nullptr ) {
		return;
	}
	firebase::auth::User user = auth->current_user();
	if ( !user.is_valid() ) {
		return;
	}
	const std::string uid = user.uid();
	const bool unliking = item.liked_by_me;

	firebase::database::Database* db =
		firebase::database::Database::GetInstance( UIYabause::getFirebaseApp() );
	if ( db == nullptr ) {
		return;
	}

	DatabaseReference itemRef = db->GetReference()
									.Child( "shared-cheats" )
									.Child( mGameCode )
									.Child( item.key );
	DatabaseReference likeRef = itemRef.Child( "like_users" ).Child( uid );
	DatabaseReference starRef = itemRef.Child( "star_count" );

	if ( unliking ) {
		likeRef.RemoveValue();
	} else {
		likeRef.SetValue( Variant( true ) );
	}

	const int delta = unliking ? -1 : 1;
	starRef.RunTransaction(
		[delta]( firebase::database::MutableData* data ) -> firebase::database::TransactionResult {
			int64_t current = 0;
			if ( data->value().is_int64() ) {
				current = data->value().int64_value();
			}
			int64_t next = current + delta;
			if ( next < 0 ) {
				next = 0;
			}
			data->set_value( Variant( next ) );
			return firebase::database::kTransactionResultSuccess;
		} );

	// Optimistic local update; the OnValueChanged callback will reconcile.
	for ( CloudCheatItem& it : mCloudItems ) {
		if ( it.key == item.key ) {
			it.liked_by_me = !unliking;
			it.star_count += delta;
			if ( it.star_count < 0 ) {
				it.star_count = 0;
			}
			break;
		}
	}
	rebuildCloudList();
}

void UICheats::on_pbCloudRefresh_clicked()
{
	detachCloudListener();
	attachCloudListener();
}
