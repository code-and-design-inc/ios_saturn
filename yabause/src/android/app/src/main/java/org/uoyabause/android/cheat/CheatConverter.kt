/*  Copyright 2019 devMiyax(smiyaxdev@gmail.com)

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
package org.uoyabause.android.cheat

import android.util.Log
import com.google.firebase.database.DatabaseReference
import com.google.firebase.database.FirebaseDatabase
import org.uoyabause.android.YabauseStorage
import org.uoyabause.android.auth.AuthState
import java.lang.Exception

class CheatConverter {
    fun hasOldVersion(): Boolean {
        val cheats: List<Cheat> =
            try {
                YabauseStorage.cheatDao.getAll()
            } catch (e: Exception) {
                return false
            }
        return cheats.isNotEmpty()
    }

    fun execute(): Int {
        val database: DatabaseReference
        if (AuthState.realUser() == null) {
            return -1
        }
        val cheats = YabauseStorage.cheatDao.getAll()
        if (cheats.isEmpty()) {
            return -1
        }
        val baseref = FirebaseDatabase.getInstance().reference
        val baseurl = "/user-posts/" + AuthState.realUser()!!.uid + "/cheat/"
        database = baseref.child(baseurl)
        for (cheat in cheats) {
            val key = database.child(cheat.gameid!!).push().key
            database
                .child(cheat.gameid!!)
                .child(key!!)
                .child("description")
                .setValue(cheat.description)
            database
                .child(cheat.gameid!!)
                .child(key)
                .child("cheat_code")
                .setValue(cheat.cheat_code)
            Log.d(
                "CheatConverter",
                "game:" + cheat.gameid + " desc:" + cheat.description + " code:" + cheat.cheat_code,
            )
        }
        YabauseStorage.cheatDao.deleteAll()
        return 0
    }
}
