package org.uoyabause.android.game

import android.util.Log
import com.google.firebase.firestore.FirebaseFirestore
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.uoyabause.android.auth.AuthState

/*
Desert	Personal record
	    0909	2
		090A	53
		090B	21
Forest	Personal record
	    0BA9	4
		0BAA	25
		0BAB	68
Mountain Personal record
	    0E49	3
		0EAA	56
		0EAB	58
*/

class SegaRallyRecord {
    var minutes: Int = 0
    var seconds: Int = 0
    var milliseconds: Int = 0

    // Convert to total milliseconds for comparison and storage
    fun toTotalMilliseconds(): Long = (minutes * 60000L) + (seconds * 1000L) + milliseconds
}

class SegaRallyBackup {
    var records: MutableList<SegaRallyRecord> = mutableListOf()

    constructor(bin: ByteArray) {
        // Extract timing data for each stage
        extractStageRecord(bin, 0x0909, 0x090A, 0x090B) // Desert
        extractStageRecord(bin, 0x0BA9, 0x0BAA, 0x0BAB) // Forest
        extractStageRecord(bin, 0x0E49, 0x0E4A, 0x0E4B) // Mountain
    }

    private fun extractStageRecord(
        bin: ByteArray,
        minAddr: Int,
        secAddr: Int,
        msecAddr: Int,
    ) {
        val record = SegaRallyRecord()

        if (minAddr < bin.size && secAddr < bin.size && msecAddr < bin.size) {
            val minByte = bin[minAddr].toInt() and 0xFF
            val secByte = bin[secAddr].toInt() and 0xFF
            val msecByte = bin[msecAddr].toInt() and 0xFF

            // Extract timing data using the provided formulas
            record.minutes = minByte and 0x0F
            record.seconds = ((secByte shr 4) and 0x0F) * 10 + (secByte and 0x0F) // Fixed formula
            record.milliseconds = (((msecByte shr 4) and 0x0F) * 10 + (msecByte and 0x0F)) * 10 // Fixed formula
        }

        records.add(record)
    }
}

class SegaRally : BaseGame {
    constructor(gameCode: String) {
        // リーダーボードの初期化
        leaderBoards = mutableListOf<LeaderBoard>()
        leaderBoards?.add(LeaderBoard("Desert", "01"))
        leaderBoards?.add(LeaderBoard("Forest", "02"))
        leaderBoards?.add(LeaderBoard("Mountain", "03"))

        // BaseGameのinitGameIdを使用してgameIdを初期化
        CoroutineScope(Dispatchers.IO).launch {
            // gameIdの初期化を待機
            initGameId(gameCode)

            // gameIdが設定された後にleaderboardsコレクションの初期化を行う
            if (gameId.isNotEmpty()) {
                initLeaderboards()
            }
        }
    }

    // leaderboardsコレクションの初期化
    private fun initLeaderboards() {
        if (gameId.isEmpty()) return
        val db = FirebaseFirestore.getInstance()
        val leaderboardsRef = db.collection("games").document(gameId).collection("leaderboards")
        leaderboardsRef.get().addOnSuccessListener { result ->
            if (result.isEmpty) {
                val leaderboardsData =
                    listOf(
                        Pair("01", "Desert"),
                        Pair("02", "Forest"),
                        Pair("03", "Mountain"),
                    )
                leaderboardsData.forEach { (id, name) ->
                    val data = hashMapOf("name" to name, "scoreType" to "TIME", "rankingType" to "ASCENDING")
                    leaderboardsRef.document(id).set(data)
                }
            }
        }
    }

    override fun onBackUpUpdated(
        fname: String,
        before: ByteArray,
        after: ByteArray,
    ) {
        Log.d("SegaRally", "onBackUpUpdated called fname=$fname")

        if (gameId == "") return

        // SEGARALLY_0 のときのみスコア評価処理を実行
        if (fname != "SEGARALLY_0" && fname != "RALLYPLUS_0") return

        val currentUser = AuthState.realUser() ?: return
/*
        // ここから追加: afterの内容を16バイトごとにHEXで出力
        var i = 0
        while (i < after.size) {
            val lineAddr = String.format("%04X:", i)
            val lineBytes = StringBuilder()
            for (j in 0 until 16) {
                if (i + j < after.size) {
                    lineBytes.append(String.format(" %02X", after[i + j]))
                } else {
                    lineBytes.append("   ") // データが足りない場合はスペース
                }
                if (j == 7) lineBytes.append(" ") // 8バイトごとにスペース
            }
            Log.d(tag, "$lineAddr${lineBytes.toString()}")
            i += 16
        }
        // ここまで追加
*/
        val minLength = minOf(before.size, after.size)
        for (i in 0 until minLength) {
            if (before[i] != after[i]) {
                val beforeHex = String.format("%02X", before[i])
                val afterHex = String.format("%02X", after[i])
                Log.d("SegaRally", String.format("%04X: %s, %s", i, beforeHex, afterHex))
            }
        }
        if (before.size > after.size) {
            for (i in minLength until before.size) {
                val beforeHex = String.format("%02X", before[i])
                Log.d("SegaRally", String.format("%04X: %s, --", i, beforeHex))
            }
        } else if (after.size > before.size) {
            for (i in minLength until after.size) {
                val afterHex = String.format("%02X", after[i])
                Log.d("SegaRally", String.format("%04X: --, %s", i, afterHex))
            }
        }

        val beforeRecord = SegaRallyBackup(before)
        val afterRecord = SegaRallyBackup(after)

        Log.d(
            "SegaRally",
            String.format(
                "Desert %02d:%02d.%03d",
                afterRecord.records[0].minutes,
                afterRecord.records[0].seconds,
                afterRecord.records[0].milliseconds,
            ),
        )
        Log.d(
            "SegaRally",
            String.format(
                "Forest %02d:%02d.%03d",
                afterRecord.records[1].minutes,
                afterRecord.records[1].seconds,
                afterRecord.records[1].milliseconds,
            ),
        )
        Log.d(
            "SegaRally",
            String.format(
                "Mountain %02d:%02d.%03d",
                afterRecord.records[2].minutes,
                afterRecord.records[2].seconds,
                afterRecord.records[2].milliseconds,
            ),
        )

        for (i in 0..2) { // 3 stages: Desert, Forest, Mountain
            val beforeTime = beforeRecord.records[i].toTotalMilliseconds()
            val afterTime = afterRecord.records[i].toTotalMilliseconds()

            // Check if there's a new record (shorter time and not zero)
            if (afterTime > 0 && (beforeTime == 0L || afterTime < beforeTime)) {
                val score = afterTime
                val gid = leaderBoards?.get(i)?.id
                if (gid != null) {
                    // Use the display name if available, but ensure we're using the Firebase UID for the document ID
                    val userName = currentUser.displayName ?: "Anonymous"

                    // Log the user information for debugging
                    Log.d(
                        "SegaRally",
                        "Submitting score for user: ${currentUser.uid}, display name: $userName, stage: ${leaderBoards?.get(
                            i,
                        )?.title}, time: ${formatTime(score)}",
                    )

                    submitScoreToFirestore(gameId, gid, score, userName, "discord_webhook_url_segarally")
                    logScoreEvent(score, gid)
                    leaderBoards?.get(i)?.id?.let { this.uievent.onNewRecord(it) }
                }
            }
        }
    }

    private fun formatTime(msec: Long): String {
        val min = msec / 60000
        val sec = (msec % 60000) / 1000
        val ms = msec % 1000
        return String.format("%d:%02d.%03d", min, sec, ms)
    }
}
