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
#ifndef UICHEATS_H
#define UICHEATS_H

#include "ui_UICheats.h"
#include "../QtYabause.h"

#include <firebase/auth.h>
#include <firebase/database.h>

#include <map>
#include <set>
#include <string>
#include <vector>

struct CloudCheatItem
{
	std::string key;
	std::string description;
	std::string cheat_code;
	int64_t star_count;
	bool liked_by_me;
};

class UICheats :
	public QDialog,
	public Ui::UICheats,
	public firebase::database::ValueListener,
	public firebase::auth::AuthStateListener
{
	Q_OBJECT

public:
	UICheats( QWidget* parent = 0 );
	virtual ~UICheats();

	// firebase::database::ValueListener
	void OnValueChanged( const firebase::database::DataSnapshot& snapshot ) override;
	void OnCancelled( const firebase::database::Error& error_code, const char* error_message ) override;

	// firebase::auth::AuthStateListener
	void OnAuthStateChanged( firebase::auth::Auth* auth ) override;

protected:
	cheatlist_struct* mCheats;

	// Local tab helpers (unchanged)
	void addCode( int id );
	void addARCode( const QString& code, const QString& description );
	void addRawCode( int type, const QString& address, const QString& value, const QString& description );

	// Shared tab state
	std::string mGameCode;                                  // empty when no game is running
	std::vector<CloudCheatItem> mCloudItems;                // last known snapshot
	// Static so the cloud-key -> core-index mapping survives dialog open/close cycles.
	// The Saturn core cheat list is a process-global, so indices stay valid as long as
	// no external code mutates the list between dialog opens. Local-tab actions that
	// mutate the list (pbDelete, pbClear, pbLoadFile) call clearAllCloudActive() first.
	static std::map<std::string, std::vector<int>> sEnabledCloudCheats;
	bool mCloudListenerAttached;

	// Shared tab helpers
	void attachCloudListener();
	void detachCloudListener();
	void clearAllCloudActive();
	int  selectedCloudRow() const;
	void enableCloudCheat( const CloudCheatItem& item );
	void disableCloudCheat( const std::string& cloudKey );

protected slots:
	// Local tab slots (unchanged)
	void on_twCheats_itemSelectionChanged();
	void on_twCheats_itemDoubleClicked( QTreeWidgetItem* item, int column );
	void on_pbDelete_clicked();
	void on_pbClear_clicked();
	void on_pbAR_clicked();
	void on_pbRaw_clicked();
	void on_pbSaveFile_clicked();
	void on_pbLoadFile_clicked();

	// Shared tab slots
	void on_twCloudCheats_itemSelectionChanged();
	void on_twCloudCheats_itemDoubleClicked( QTreeWidgetItem* item, int column );
	void on_pbCloudEnable_clicked();
	void on_pbCloudLike_clicked();
	void on_pbCloudRefresh_clicked();

	// Marshaled from Firebase worker thread to UI thread
	void rebuildCloudList();
	void showCloudError( QString message );
	void applyAuthState( bool signedIn );
};

#endif // UICHEATS_H
