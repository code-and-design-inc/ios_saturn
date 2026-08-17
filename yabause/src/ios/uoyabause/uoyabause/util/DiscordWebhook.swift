import Foundation

/**
 * Discord Webhook を使用してメッセージを送信するユーティリティクラス
 */
class DiscordWebhook {
    
    private static let tag = "DiscordWebhook"
    
    /**
     * Discordのウェブフックにメッセージを送信する
     * 
     * @param webhookUrl Discordのウェブフックのエンドポイント
     * @param username 表示するユーザー名
     * @param content メッセージの内容
     * @param embedTitle 埋め込みのタイトル（オプション）
     * @param embedDescription 埋め込みの説明（オプション）
     * @param embedColor 埋め込みの色（オプション、16進数のカラーコード）
     * @param avatarUrl アバターのURL（オプション）
     * @param completion 完了ハンドラ
     */
    static func sendMessage(
        webhookUrl: String,
        username: String,
        content: String,
        embedTitle: String? = nil,
        embedDescription: String? = nil,
        embedColor: Int? = nil,
        avatarUrl: String? = nil,
        completion: @escaping (Bool) -> Void
    ) {
        guard let url = URL(string: webhookUrl) else {
            print("\(tag): Invalid webhook URL")
            completion(false)
            return
        }
        
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        
        // JSONデータの作成
        var json: [String: Any] = [
            "username": username,
            "content": content,
            "tts": false
        ]
        
        // アバターURLが指定されている場合は設定
        if let avatarUrl = avatarUrl {
            json["avatar_url"] = avatarUrl
        }
        
        // 埋め込みが指定されている場合は追加
        if embedTitle != nil || embedDescription != nil {
            var embed: [String: Any] = [:]
            
            if let title = embedTitle {
                embed["title"] = title
            }
            
            if let description = embedDescription {
                embed["description"] = description
            }
            
            if let color = embedColor {
                embed["color"] = color
            }
            
            json["embeds"] = [embed]
        }
        
        do {
            let jsonData = try JSONSerialization.data(withJSONObject: json, options: [])
            request.httpBody = jsonData
            
            let task = URLSession.shared.dataTask(with: request) { data, response, error in
                if let error = error {
                    print("\(tag): Error sending Discord webhook: \(error.localizedDescription)")
                    completion(false)
                    return
                }
                
                guard let httpResponse = response as? HTTPURLResponse else {
                    print("\(tag): Invalid response")
                    completion(false)
                    return
                }
                
                print("\(tag): Discord webhook response code: \(httpResponse.statusCode)")
                completion(httpResponse.statusCode == 204) // Discordのウェブフックは成功時に204を返す
            }
            
            task.resume()
        } catch {
            print("\(tag): Error creating JSON: \(error.localizedDescription)")
            completion(false)
        }
    }
    
    /**
     * 新記録（1位）をDiscordに投稿する
     * 
     * @param webhookUrl Discordのウェブフックのエンドポイント
     * @param gameId ゲームID
     * @param leaderboardName リーダーボード名
     * @param userName ユーザー名
     * @param score スコア（タイム）
     * @param avatarUrl アバターのURL（オプション）
     * @param completion 完了ハンドラ
     */
    static func sendNewRecordMessage(
        webhookUrl: String,
        gameId: String,
        leaderboardName: String,
        userName: String,
        score: Int64,
        avatarUrl: String? = nil,
        completion: @escaping (Bool) -> Void
    ) {
        // タイムをフォーマット（ミリ秒をMM:SS.MMMの形式に変換）
        let formattedTime = formatTime(msec: score)
        
        // Build message content
        let content = "🏆 **NEW RECORD!** 🏆"
        let embedTitle = "\(leaderboardName) - New Record"
        let embedDescription = "**\(userName)** achieved 1st place with a time of **\(formattedTime)**!"
        
        // Embed color (gold)
        let embedColor = 0xFFD700 // Gold color hex code
        
        sendMessage(
            webhookUrl: webhookUrl,
            username: "Leaderboard Bot",
            content: content,
            embedTitle: embedTitle,
            embedDescription: embedDescription,
            embedColor: embedColor,
            avatarUrl: avatarUrl,
            completion: completion
        )
    }
    
    /**
     * タイムをフォーマットする（ミリ秒をMM:SS.MMMの形式に変換）
     */
    private static func formatTime(msec: Int64) -> String {
        let min = msec / 60000
        let sec = (msec % 60000) / 1000
        let ms = msec % 1000
        return String(format: "%d:%02d.%03d", min, sec, ms)
    }
}
