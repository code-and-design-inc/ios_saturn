import Foundation
import AVFoundation
import AudioToolbox
import FirebaseAuth

// MARK: - Notifications
extension Notification.Name {
    static let retroAchievementsHardcoreModeChanged = Notification.Name("retroAchievementsHardcoreModeChanged")
}

// MARK: - C Function Declarations
// These functions are declared in YabaInterface.h and implemented in YabaInterface.mm
@_silgen_name("YabauseRA_Initialize")
func YabauseRA_Initialize() -> Int32

@_silgen_name("YabauseRA_Shutdown") 
func YabauseRA_Shutdown()

@_silgen_name("YabauseRA_InitializeUser")
func YabauseRA_InitializeUser(_ username: UnsafePointer<CChar>, _ token: UnsafePointer<CChar>) -> Int32

@_silgen_name("YabauseRA_Logout")
func YabauseRA_Logout()

@_silgen_name("YabauseRA_IsUserLoggedIn")
func YabauseRA_IsUserLoggedIn() -> Int32

@_silgen_name("YabauseRA_SetHardcoreEnabled")
func YabauseRA_SetHardcoreEnabled(_ enabled: Int32)

@_silgen_name("YabauseRA_Reset")
func YabauseRA_Reset()

@_silgen_name("YabauseRA_IsProcessingRequired")
func YabauseRA_IsProcessingRequired() -> Int32

@_silgen_name("YabauseRA_LoadGame")
func YabauseRA_LoadGame(_ hash: UnsafePointer<CChar>) -> Int32

@_silgen_name("YabauseRA_LoadGameFromFile")
func YabauseRA_LoadGameFromFile(_ path: UnsafePointer<CChar>) -> Int32

@_silgen_name("YabauseRA_GetRichPresenceDisplayString")
func YabauseRA_GetRichPresenceDisplayString() -> UnsafePointer<CChar>?

@_silgen_name("YabauseRA_GetGameHash")
func YabauseRA_GetGameHash() -> UnsafePointer<CChar>?

@_silgen_name("YabauseRA_GetAchievementBadgeURL")
func YabauseRA_GetAchievementBadgeURL(_ achievementId: Int32, _ state: Int32) -> UnsafePointer<CChar>?

@_silgen_name("YabauseRA_GetAchievementBadgeName")
func YabauseRA_GetAchievementBadgeName(_ achievementId: Int32) -> UnsafePointer<CChar>?

@_silgen_name("YabauseRA_CreateAchievementListJSON")
func YabauseRA_CreateAchievementListJSON(_ category: Int32, _ grouping: Int32) -> UnsafeMutablePointer<CChar>?

// No more dangerous C structures - using JSON-based approach

@_silgen_name("YabauseRA_SetServerCallback")
func YabauseRA_SetServerCallback(_ callback: @convention(c) (UnsafePointer<CChar>, UnsafePointer<CChar>?, UnsafeMutableRawPointer?) -> Void)

@_silgen_name("YabauseRA_CompleteServerRequest") 
func YabauseRA_CompleteServerRequest(_ userdata: UnsafeMutableRawPointer?, _ httpStatusCode: Int32, _ responseBody: UnsafePointer<CChar>?, _ responseLength: Int)

@_silgen_name("YabauseRA_SetEventCallback")
func YabauseRA_SetEventCallback(_ callback: @convention(c) (Int32, UnsafePointer<CChar>) -> Void)

@_silgen_name("YabauseRA_SetLoginCallback")
func YabauseRA_SetLoginCallback(_ callback: @convention(c) (Int32, UnsafePointer<CChar>?, UnsafePointer<CChar>?, UInt32, UnsafePointer<CChar>?) -> Void)

@_silgen_name("YabauseRA_SetGamePlacardCallback")
func YabauseRA_SetGamePlacardCallback(_ callback: @convention(c) (UnsafePointer<CChar>?, UnsafePointer<CChar>?, UInt32, UInt32, UInt32, UInt32, Int32) -> Void)

@_silgen_name("YabauseRA_DoFrame")
func YabauseRA_DoFrame()

@_silgen_name("YabauseRA_BeginChangeMedia")
func YabauseRA_BeginChangeMedia(_ path: UnsafePointer<CChar>) -> Int32

@_silgen_name("YabauseRA_GetGameImageURL")
func YabauseRA_GetGameImageURL(_ buffer: UnsafeMutablePointer<CChar>, _ bufferSize: Int) -> Int32

// MARK: - Data Models
struct RAUser {
    let username: String
    let displayName: String
    let score: Int
    let softcoreScore: Int
    let token: String
}

struct RAAchievement {
    let id: Int
    let title: String
    let description: String
    let badge: String
    let points: Int
    let type: Int
    let state: Int
    let unlockTime: Date?
    let category: Int
}

struct RALeaderboard {
    let id: Int
    let title: String
    let description: String
    let definition: String
    let format: Int
    let lowerIsBetter: Bool
    let hidden: Bool
}

struct RAGameInfo {
    let id: Int
    let title: String
    let consoleName: String
    let hash: String
    let achievementCount: Int
    let leaderboardCount: Int
    let richPresencePatch: String?
}

// MARK: - RetroAchievements Manager
class RetroAchievementsManager {
    
    // MARK: - Singleton
    static var shared: RetroAchievementsManager?
    
    // MARK: - Properties  
    private var currentUser: RAUser?
    private var currentGame: RAGameInfo?
    private var achievements: [RAAchievement] = []
    private var leaderboards: [RALeaderboard] = []
    private var isHardcoreEnabled = true
    private var userDesiredHardcoreMode = true  // User's preferred hardcore setting
    private var richPresenceDisplayString = ""
    
    // Login completion handler for async callback
    private var loginCompletion: ((Bool, String?) -> Void)?
    
    // Note: Direct HTTP requests removed - using rcheevos integration instead
    // private let urlSession: URLSession - REMOVED
    // private let baseURL - REMOVED (was using direct API calls)
    
    // Callbacks
    var onAchievementUnlocked: ((RAAchievement) -> Void)?
    var onLeaderboardSubmitted: ((RALeaderboard, Int) -> Void)?
    var onRichPresenceUpdated: ((String) -> Void)?
    var onGameLoaded: ((RAGameInfo) -> Void)?
    
    // MARK: - Initialization
    private init() {
        // Load user's preferred hardcore mode setting
        userDesiredHardcoreMode = UserDefaults.standard.bool(forKey: "ra_hardcore_mode_enabled")
        if UserDefaults.standard.object(forKey: "ra_hardcore_mode_enabled") == nil {
            // Default to true for hardcore on first launch
            userDesiredHardcoreMode = true
            UserDefaults.standard.set(true, forKey: "ra_hardcore_mode_enabled")
        }
        isHardcoreEnabled = userDesiredHardcoreMode
        
        // Initialize rcheevos integration
        NSLog("RetroAchievementsManager: Initializing with rcheevos integration")
        
        // Initialize native RetroAchievements system
        if YabauseRA_Initialize() != 0 {
            NSLog("RetroAchievementsManager: Native initialization successful")
            
            // Set up server callback for HTTP requests (Step 2)
            setupServerCallback()
            
            // Set up event handler for achievements and notifications (Step 3)
            setupEventCallback()
            
            // Set up login callback for async login completion
            setupLoginCallback()
            
            // Set up game placard callback for game load events
            setupGamePlacardCallback()
        } else {
            NSLog("RetroAchievementsManager: Native initialization failed")
        }
    }
    
    static func initialize() {
        if shared == nil {
            NSLog("RetroAchievementsManager: Initializing new instance")
            shared = RetroAchievementsManager()
            
            NSLog("RetroAchievementsManager: Instance created and initialized successfully")
        } else {
            NSLog("RetroAchievementsManager: Using existing instance")
        }
    }
    
    // MARK: - Authentication
    func loginUser(username: String, password: String, completion: @escaping (Bool, String?) -> Void) {
        NSLog("RetroAchievements: Starting rcheevos login for user: \(username)")
        NSLog("RetroAchievements: Using official rcheevos integration")
        
        // Store completion handler for async callback
        self.loginCompletion = completion
        
        // Use the native rcheevos login function (this is async)
        let result = YabauseRA_InitializeUser(username, password)
        
        if result != 0 {
            NSLog("RetroAchievements: Native login initiated successfully")
            // The actual completion will be handled by the login callback
            // Don't call completion here - wait for the async callback
        } else {
            NSLog("RetroAchievements: Failed to initiate native login")
            self.loginCompletion = nil
            completion(false, "Failed to initiate login request")
        }
    }
    
    // API Key authentication using rcheevos
    func loginUserWithAPIKey(username: String, apiKey: String, completion: @escaping (Bool, String?) -> Void) {
        NSLog("RetroAchievements: Starting API Key login for user: \(username)")
        
        // Use rcheevos for API Key authentication
        // Note: rcheevos handles API Key authentication internally
        let result = YabauseRA_InitializeUser(username, apiKey)
        
        if result != 0 {
            NSLog("RetroAchievements: API Key login successful")
            
            if let nativeUsername = YabauseRA_GetUsername() {
                let usernameString = String(cString: nativeUsername)
                
                let user = RAUser(
                    username: usernameString,
                    displayName: usernameString,
                    score: 0,
                    softcoreScore: 0,
                    token: apiKey
                )
                
                self.currentUser = user
                self.onLoginComplete(success: true)
                completion(true, nil)
            } else {
                completion(false, "Login succeeded but user information unavailable")
            }
        } else {
            NSLog("RetroAchievements: API Key login failed")
            completion(false, "Invalid API key or server connection failed")
        }
    }
    
    func logout() {
        currentUser = nil
        currentGame = nil
        achievements.removeAll()
        leaderboards.removeAll()
        isHardcoreEnabled = false
        // Note: Don't reset userDesiredHardcoreMode - keep user's preference
        richPresenceDisplayString = ""
        
        // Logout from native layer
        logoutNativeUser()
        
        onLoginStateChanged(isLoggedIn: false)
    }
    
    var isUserLoggedIn: Bool {
        return currentUser != nil
    }
    
    // MARK: - Game Loading
    
    // New method to load game from file path (preferred method)
    func loadGameFromFile(path: String, completion: @escaping (Bool, String?) -> Void) {
        guard isUserLoggedIn else {
            NSLog("RetroAchievements: Cannot load game - user not logged in")
            completion(false, "User not logged in")
            return
        }
        
        NSLog("RetroAchievements: Starting game identification from file: \(path)")
        
        // Use native layer to identify and load game
        // This will use RC_CONSOLE_SATURN (or RC_CONSOLE_UNKNOWN) and let rcheevos identify the game
        let success = path.withCString { pathPtr in
            YabauseRA_LoadGameFromFile(pathPtr)
        }
        
        if success != 0 {
            NSLog("RetroAchievements: Game loading initiated successfully")
            // The native layer will handle callbacks for when game is actually loaded
            completion(true, nil)
        } else {
            NSLog("RetroAchievements: Failed to initiate game loading")
            completion(false, "Failed to identify game")
        }
    }
    
    // Legacy method for hash-based loading (kept for compatibility)
    func loadGame(hash: String, completion: @escaping (Bool, String?) -> Void) {
        guard isUserLoggedIn else {
            completion(false, "User not logged in")
            return
        }
        
        NSLog("RetroAchievements: Loading game with hash: \(hash)")
        
        // Use rcheevos native layer to load game by hash
        let success = hash.withCString { hashPtr in
            YabauseRA_LoadGame(hashPtr)
        }
        
        if success != 0 {
            NSLog("RetroAchievements: Game loading with hash initiated successfully")
            completion(true, nil)
        } else {
            NSLog("RetroAchievements: Failed to load game with hash")
            completion(false, "Game not found or unsupported")
        }
    }
    
    // Show game loaded notification
    private func showGamePlacard(_ gameInfo: RAGameInfo) {
        NSLog("RetroAchievements: Game loaded - \(gameInfo.title) [\(gameInfo.consoleName)]")
        NSLog("RetroAchievements: \(gameInfo.achievementCount) achievements, \(gameInfo.leaderboardCount) leaderboards")
        
        // Restore user's preferred hardcore mode first (before checking if processing is required)
        isHardcoreEnabled = userDesiredHardcoreMode && isUserLoggedIn
        setNativeHardcoreMode(enabled: isHardcoreEnabled)
        
        // Automatically disable hardcore mode for games without RetroAchievements functionality
        if YabauseRA_IsProcessingRequired() == 0 {
            NSLog("RetroAchievements: No RetroAchievements functionality available - temporarily disabling hardcore mode")
            isHardcoreEnabled = false
            setNativeHardcoreMode(enabled: false)
        }
        
        // Calculate unlocked achievements count
        let unlockedCount = achievements.filter { $0.state == 1 }.count
        
        // Show game placard UI (simplified notification for now)
        NSLog("RetroAchievements: Would show game placard for \(gameInfo.title)")
        // TODO: Integrate proper game placard UI when project file is updated
        
        // Show game placard notification
        RetroAchievementsNotificationView.showGamePlacard(
            gameTitle: gameInfo.title,
            imageUrl: nil,
            unlockedAchievements: 0,
            totalAchievements: gameInfo.achievementCount
        )
    }
    
    private func parseGameInfo(json: [String: Any]) {
        guard let gameId = json["ID"] as? Int,
              let title = json["Title"] as? String,
              let consoleName = json["ConsoleName"] as? String else {
            return
        }
        
        let gameInfo = RAGameInfo(
            id: gameId,
            title: title,
            consoleName: consoleName,
            hash: "",
            achievementCount: (json["NumAchievements"] as? Int) ?? 0,
            leaderboardCount: (json["NumLeaderboards"] as? Int) ?? 0,
            richPresencePatch: json["RichPresencePatch"] as? String
        )
        
        currentGame = gameInfo
        
        // Parse achievements
        if let achievementsData = json["Achievements"] as? [String: Any] {
            parseAchievements(achievementsData: achievementsData)
        }
        
        // Parse leaderboards
        if let leaderboardsData = json["Leaderboards"] as? [String: Any] {
            parseLeaderboards(leaderboardsData: leaderboardsData)
        }
        
        onGameLoaded?(gameInfo)
    }
    
    // MARK: - Media Change Support
    
    /// Handle media changes for multi-disc games
    /// - Parameter path: Path to the new disc/media file
    /// - Parameter completion: Callback with success status and optional error message
    func changeMedia(path: String, completion: @escaping (Bool, String?) -> Void) {
        guard isUserLoggedIn else {
            NSLog("RetroAchievements: Cannot change media - user not logged in")
            completion(false, "User not logged in")
            return
        }
        
        NSLog("RetroAchievements: Changing media to: \(path)")
        
        let result = YabauseRA_BeginChangeMedia(path)
        
        if result != 0 {
            NSLog("RetroAchievements: Media change initiated successfully")
            // The actual result will be handled by the event callback
            completion(true, nil)
        } else {
            NSLog("RetroAchievements: Failed to initiate media change")
            completion(false, "Failed to initiate media change")
        }
    }
    
    private func parseAchievements(achievementsData: [String: Any]) {
        achievements.removeAll()
        
        for (_, achievementData) in achievementsData {
            if let achData = achievementData as? [String: Any],
               let id = achData["ID"] as? Int,
               let title = achData["Title"] as? String,
               let description = achData["Description"] as? String,
               let badge = achData["BadgeName"] as? String,
               let points = achData["Points"] as? Int {
                
                let achievement = RAAchievement(
                    id: id,
                    title: title,
                    description: description,
                    badge: badge,
                    points: points,
                    type: (achData["Type"] as? Int) ?? 0,
                    state: (achData["DateEarned"] != nil) ? 1 : 0,
                    unlockTime: parseDate(achData["DateEarned"] as? String),
                    category: (achData["Category"] as? Int) ?? 0
                )
                
                achievements.append(achievement)
            }
        }
    }
    
    private func parseLeaderboards(leaderboardsData: [String: Any]) {
        leaderboards.removeAll()
        
        for (_, leaderboardData) in leaderboardsData {
            if let lbData = leaderboardData as? [String: Any],
               let id = lbData["ID"] as? Int,
               let title = lbData["Title"] as? String,
               let description = lbData["Description"] as? String,
               let definition = lbData["Mem"] as? String,
               let format = lbData["Format"] as? Int {
                
                let leaderboard = RALeaderboard(
                    id: id,
                    title: title,
                    description: description,
                    definition: definition,
                    format: format,
                    lowerIsBetter: (lbData["LowerIsBetter"] as? Bool) ?? false,
                    hidden: (lbData["Hidden"] as? Bool) ?? false
                )
                
                leaderboards.append(leaderboard)
            }
        }
    }
    
    private func parseDate(_ dateString: String?) -> Date? {
        guard let dateString = dateString, !dateString.isEmpty else { return nil }
        
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd HH:mm:ss"
        return formatter.date(from: dateString)
    }
    
    // MARK: - Hardcore Mode
    func setHardcoreEnabled(_ enabled: Bool) {
        guard isUserLoggedIn else {
            isHardcoreEnabled = false
            userDesiredHardcoreMode = enabled  // Still track user preference
            
            // Post notification to update UI even when logged out
            NotificationCenter.default.post(name: .retroAchievementsHardcoreModeChanged, object: nil)
            return
        }
        
        isHardcoreEnabled = enabled
        userDesiredHardcoreMode = enabled  // Track user's desired state
        
        // Update native layer
        setNativeHardcoreMode(enabled: enabled)
        
        // Save user's global hardcore preference
        UserDefaults.standard.set(enabled, forKey: "ra_hardcore_mode_enabled")
        UserDefaults.standard.synchronize()
        
        // Post notification to update UI
        NotificationCenter.default.post(name: .retroAchievementsHardcoreModeChanged, object: nil)
    }
    
    func getHardcoreEnabled() -> Bool {
        return isHardcoreEnabled && isUserLoggedIn
    }
    
    // MARK: - Badge Image Functions
    func getAchievementBadgeURL(achievementId: Int, state: Int) -> String? {
        let cResult = YabauseRA_GetAchievementBadgeURL(Int32(achievementId), Int32(state))
        if let cString = cResult {
            return String(cString: cString)
        }
        return nil
    }
    
    func getAchievementBadgeName(achievementId: Int) -> String? {
        let cResult = YabauseRA_GetAchievementBadgeName(Int32(achievementId))
        if let cString = cResult {
            return String(cString: cString)
        }
        return nil
    }
    
    // MARK: - Achievement List Functions
    func createAchievementList(category: Int32 = 1, grouping: Int32 = 1) -> [RAAchievement] {
        // RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL = 1
        // RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS = 1
        guard let jsonCString = YabauseRA_CreateAchievementListJSON(category, grouping) else {
            NSLog("Failed to create achievement list JSON from rcheevos")
            return []
        }
        
        let jsonString = String(cString: jsonCString)
        free(jsonCString) // Important: free the malloc'd memory from C++
        
        NSLog("Received achievement list JSON (\(jsonString.count) chars)")
        
        // Parse JSON data
        guard let jsonData = jsonString.data(using: .utf8) else {
            NSLog("Failed to convert JSON string to data")
            return []
        }
        
        do {
            let json = try JSONSerialization.jsonObject(with: jsonData, options: [])
            guard let rootDict = json as? [String: Any],
                  let buckets = rootDict["buckets"] as? [[String: Any]] else {
                NSLog("Invalid JSON structure")
                return []
            }
            
            var achievements: [RAAchievement] = []
            
            // Process each bucket
            for bucket in buckets {
                guard let label = bucket["label"] as? String,
                      let bucketAchievements = bucket["achievements"] as? [[String: Any]] else {
                    continue
                }
                
                NSLog("Processing bucket: \(label) with \(bucketAchievements.count) achievements")
                
                // Process achievements in this bucket
                for achievementDict in bucketAchievements {
                    guard let id = achievementDict["id"] as? Int,
                          let title = achievementDict["title"] as? String,
                          let description = achievementDict["description"] as? String,
                          let points = achievementDict["points"] as? Int,
                          let unlocked = achievementDict["unlocked"] as? Int else {
                        continue
                    }
                    
                    let state = achievementDict["state"] as? Int ?? 0
                    let unlockTime = achievementDict["unlock_time"] as? Int ?? 0
                    let measuredPercent = achievementDict["measured_percent"] as? Double ?? 0.0
                    
                    // Determine unlock state based on rcheevos official logic
                    let isUnlocked = (unlocked != 0)
                    
                    // Debug logging to verify the JSON values
                    NSLog("JSON values - ID=\(id): points=\(points), state=\(state), unlocked=\(unlocked), unlock_time=\(unlockTime), measured_percent=\(measuredPercent)")
                    
                    let achievement = RAAchievement(
                        id: id,
                        title: title,
                        description: description,
                        badge: "", // Badge URL will be loaded separately via API calls
                        points: points,
                        type: 0, // Default type
                        state: state,
                        unlockTime: isUnlocked && unlockTime > 0 ? Date(timeIntervalSince1970: TimeInterval(unlockTime)) : nil,
                        category: 0 // Default category
                    )
                    
                    achievements.append(achievement)
                    
                    NSLog("Achievement: ID=\(achievement.id), Title='\(achievement.title)', Points=\(achievement.points), State=\(achievement.state)")
                }
            }
            
            NSLog("Successfully converted \(achievements.count) achievements from JSON")
            return achievements
            
        } catch {
            NSLog("Failed to parse achievement list JSON: \(error)")
            return []
        }
    }
    
    // MARK: - Rich Presence
    func updateRichPresence() {
        guard let game = currentGame,
              let richPresencePatch = game.richPresencePatch,
              !richPresencePatch.isEmpty else {
            return
        }
        
        // Get rich presence string from native layer
        if let cDisplayString = YabauseRA_GetRichPresenceDisplayString() {
            let displayString = String(cString: cDisplayString)
            if !displayString.isEmpty {
                richPresenceDisplayString = displayString
                onRichPresenceUpdated?(displayString)
            }
        }
    }
    
    // MARK: - Native Bridge Methods
    private func onLoginComplete(success: Bool) {
        // Native layer is already initialized through the rcheevos login callback
        // No need to call YabauseRA_InitializeUser again
        onLoginStateChanged(isLoggedIn: success)
    }
    
    func onLoginStateChanged(isLoggedIn: Bool) {
        // If logging out, disable hardcore mode
        if !isLoggedIn && isHardcoreEnabled {
            isHardcoreEnabled = false
            setNativeHardcoreMode(enabled: false)
        }
        
        // Notify auth manager
        RetroAchievementsAuthManager.shared.onNativeLoginComplete(success: isLoggedIn, username: currentUser?.username)
        
        // Post notification to update UI
        NotificationCenter.default.post(name: .retroAchievementsHardcoreModeChanged, object: nil)
    }
    
    // Called from native layer when achievement is unlocked
    func onAchievementTriggered(achievementId: Int) {
        if let achievementIndex = achievements.firstIndex(where: { $0.id == achievementId }) {
            // Update achievement state
            achievements[achievementIndex] = RAAchievement(
                id: achievements[achievementIndex].id,
                title: achievements[achievementIndex].title,
                description: achievements[achievementIndex].description,
                badge: achievements[achievementIndex].badge,
                points: achievements[achievementIndex].points,
                type: achievements[achievementIndex].type,
                state: 1, // Mark as unlocked
                unlockTime: Date(),
                category: achievements[achievementIndex].category
            )
            
            let achievement = achievements[achievementIndex]
            onAchievementUnlocked?(achievement)
            
            // Show achievement notification
            RetroAchievementsNotificationView.showAchievementUnlocked(
                achievementId: achievement.id,
                title: achievement.title,
                description: achievement.description,
                points: achievement.points,
                imageUrl: "https://media.retroachievements.org/Badge/\(achievement.badge).png",
                isUnofficial: achievement.type != 0 // Unofficial achievements typically have non-zero type
            )
            
            // Check for mastery (all achievements unlocked)
            checkForMastery()
        }
    }
    
    // Check if player has achieved mastery (100% completion)
    private func checkForMastery() {
        guard let game = currentGame, achievements.count > 0 else { return }
        
        let unlockedAchievements = achievements.filter { $0.state == 1 }
        let totalAchievements = achievements.count
        
        // Check if all achievements are unlocked
        if unlockedAchievements.count == totalAchievements {
            NSLog("RetroAchievements: MASTERY ACHIEVED! \(game.title) - \(totalAchievements)/\(totalAchievements)")
            
            // Calculate total points earned
            let totalPoints = achievements.reduce(0) { $0 + $1.points }
            
            // Show mastery notification
            showMasteryNotification(gameTitle: game.title, totalPoints: totalPoints, achievementCount: totalAchievements)
        }
    }
    
    // Show mastery celebration notification
    private func showMasteryNotification(gameTitle: String, totalPoints: Int, achievementCount: Int) {
        
        // Show game mastery notification
        RetroAchievementsNotificationView.showGameMastery(
            gameTitle: gameTitle,
            imageUrl: nil,
            achievementCount: achievementCount,
            points: totalPoints,
            isHardcore: getHardcoreEnabled(),
            username: currentUser?.username,
            playtime: nil
        )
    }
    
    // Called from native layer when leaderboard is submitted
    func onLeaderboardSubmitted(leaderboardId: Int, score: Int) {
        if let leaderboard = leaderboards.first(where: { $0.id == leaderboardId }) {
            onLeaderboardSubmitted?(leaderboard, score)
            
            // Show leaderboard notification
            RetroAchievementsNotificationView.showLeaderboardSubmit(
                leaderboardId: leaderboard.id,
                title: leaderboard.title,
                description: leaderboard.description,
                scoreString: String(score)
            )
        }
    }
    
    // MARK: - HTTP Requests for Native Layer
    func performHTTPRequest(url: String, postData: String?, completion: @escaping (String?, String?) -> Void) {
        guard let requestURL = URL(string: url) else {
            let errorMessage = "Invalid URL: \(url)"
            NSLog("RetroAchievements: \(errorMessage)")
            showServerError(errorMessage)
            completion(nil, errorMessage)
            return
        }
        
        var request = URLRequest(url: requestURL)
        
        // Add proper User-Agent for all requests
        let userAgent = "YabaSanshiro/1.18.1 (iOS \(UIDevice.current.systemVersion)) rcheevos/11.5"
        request.setValue(userAgent, forHTTPHeaderField: "User-Agent")
        
        if let postData = postData, !postData.isEmpty {
            request.httpMethod = "POST"
            request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
            request.httpBody = postData.data(using: .utf8)
        } else {
            request.httpMethod = "GET"
        }
        
        NSLog("RetroAchievements: HTTP Request - \(request.httpMethod ?? "GET") \(url)")
        
        // Use a URLSession with reasonable timeout
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30.0
        config.timeoutIntervalForResource = 60.0
        let session = URLSession(configuration: config)
        
        session.dataTask(with: request) { data, response, error in
            DispatchQueue.main.async {
                if let error = error {
                    let errorMessage = "Network error: \(error.localizedDescription)"
                    NSLog("RetroAchievements: \(errorMessage)")
                    self.handleNetworkError(error)
                    completion(nil, errorMessage)
                    return
                }
                
                if let httpResponse = response as? HTTPURLResponse {
                    NSLog("RetroAchievements: HTTP Response - Status: \(httpResponse.statusCode)")
                    
                    // Handle HTTP error codes
                    if httpResponse.statusCode >= 400 {
                        let errorMessage = self.getHTTPErrorMessage(httpResponse.statusCode)
                        NSLog("RetroAchievements: HTTP Error - \(errorMessage)")
                        self.showServerError(errorMessage)
                        completion(nil, errorMessage)
                        return
                    }
                }
                
                if let data = data, let responseString = String(data: data, encoding: .utf8) {
                    completion(responseString, nil)
                } else {
                    let errorMessage = "No data received from server"
                    NSLog("RetroAchievements: \(errorMessage)")
                    self.showServerError(errorMessage)
                    completion(nil, errorMessage)
                }
            }
        }.resume()
    }
    
    // MARK: - Error Handling
    private func handleNetworkError(_ error: Error) {
        let nsError = error as NSError
        var userMessage = "Network connection failed"
        
        switch nsError.code {
        case NSURLErrorNotConnectedToInternet:
            userMessage = "No internet connection available"
        case NSURLErrorTimedOut:
            userMessage = "Request timed out - server may be busy"
        case NSURLErrorCannotFindHost:
            userMessage = "Cannot reach RetroAchievements server"
        case NSURLErrorNetworkConnectionLost:
            userMessage = "Network connection lost"
        default:
            userMessage = "Network error: \(error.localizedDescription)"
        }
        
        showServerError(userMessage)
    }
    
    private func getHTTPErrorMessage(_ statusCode: Int) -> String {
        switch statusCode {
        case 400:
            return "Bad request - Invalid parameters"
        case 401:
            return "Authentication failed - Check your credentials"
        case 403:
            return "Access forbidden - Invalid permissions"
        case 404:
            return "Service not found - API endpoint may have changed"
        case 429:
            return "Too many requests - Please wait and try again"
        case 500...599:
            return "RetroAchievements server error - Please try again later"
        default:
            return "HTTP error \(statusCode)"
        }
    }
    
    private func showServerError(_ message: String) {
        RetroAchievementsNotificationView.showServerError(message)
    }
    
    // MARK: - Game Progress
    func loadGameProgress(gameId: Int) -> Data? {
        let key = "ra_game_progress_\(gameId)"
        return UserDefaults.standard.data(forKey: key)
    }
    
    func saveGameProgress(gameId: Int, progressData: Data) {
        let key = "ra_game_progress_\(gameId)"
        UserDefaults.standard.set(progressData, forKey: key)
        UserDefaults.standard.synchronize()
    }
    
    // MARK: - Accessors
    func getCurrentUser() -> RAUser? {
        return currentUser
    }
    
    func getCurrentGame() -> RAGameInfo? {
        return currentGame
    }
    
    func getAchievements() -> [RAAchievement] {
        return achievements
    }
    
    func getLeaderboards() -> [RALeaderboard] {
        return leaderboards
    }
    
    func getCurrentGameHash() -> String? {
        let gameHash = YabauseRA_GetGameHash()
        if let hash = gameHash {
            return String(cString: hash)
        }
        return nil
    }
    
    func getCurrentGameImageURL() -> String? {
        let bufferSize = 256
        let buffer = UnsafeMutablePointer<CChar>.allocate(capacity: bufferSize)
        defer { buffer.deallocate() }
        
        let result = YabauseRA_GetGameImageURL(buffer, bufferSize)
        if result != 0 {
            return String(cString: buffer)
        }
        return nil
    }
    
    // MARK: - Native Layer Integration
    func initializeNativeLayer() -> Bool {
        // Call native initialization function
        let result = YabauseRA_Initialize()
        return result != 0
    }
    
    func shutdownNativeLayer() {
        // Call native shutdown function
        YabauseRA_Shutdown()
    }
    
    func initializeNativeUser(username: String, token: String) -> Bool {
        // Call native user initialization
        let result = username.withCString { usernamePtr in
            token.withCString { tokenPtr in
                YabauseRA_InitializeUser(usernamePtr, tokenPtr)
            }
        }
        return result != 0
    }
    
    func logoutNativeUser() {
        YabauseRA_Logout()
    }
    
    func setNativeHardcoreMode(enabled: Bool) {
        YabauseRA_SetHardcoreEnabled(enabled ? 1 : 0)
    }
    
    func loadGameInNativeLayer(hash: String) -> Bool {
        let result = hash.withCString { hashPtr in
            YabauseRA_LoadGame(hashPtr)
        }
        return result != 0
    }
    
    // MARK: - HTTP Server Communication (Step 2 of rcheevos integration)
    private func setupServerCallback() {
        NSLog("RetroAchievementsManager: Setting up HTTP server callback for rcheevos")
        
        // Set the server callback function
        YabauseRA_SetServerCallback { (urlPtr: UnsafePointer<CChar>, postDataPtr: UnsafePointer<CChar>?, userdata: UnsafeMutableRawPointer?) in
            // Convert C strings to Swift strings
            let url = String(cString: urlPtr)
            let postData: String? = postDataPtr != nil ? String(cString: postDataPtr!) : nil
            
            NSLog("RetroAchievements: HTTP request from rcheevos - \(url)")
            
            // Get the shared manager instance and handle the HTTP request
            if let manager = RetroAchievementsManager.shared {
                manager.handleServerRequest(url: url, postData: postData, userdata: userdata)
            } else {
                NSLog("RetroAchievements: ERROR - No shared manager instance available")
                // Complete request with error
                YabauseRA_CompleteServerRequest(userdata, 0, nil, 0)
            }
        }
    }
    
    private func handleServerRequest(url: String, postData: String?, userdata: UnsafeMutableRawPointer?) {
        NSLog("RetroAchievements: Handling HTTP request to \(url)")
        
        guard let requestURL = URL(string: url) else {
            NSLog("RetroAchievements: Invalid URL: \(url)")
            YabauseRA_CompleteServerRequest(userdata, 0, nil, 0)
            return
        }
        
        var request = URLRequest(url: requestURL)
        request.timeoutInterval = 30.0
        
        // Add proper User-Agent for RetroAchievements API
        let userAgent = "YabaSanshiro/1.18.1 (iOS \(UIDevice.current.systemVersion)) rcheevos/11.5"
        request.setValue(userAgent, forHTTPHeaderField: "User-Agent")
        
        if let postData = postData, !postData.isEmpty {
            request.httpMethod = "POST"
            request.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
            request.httpBody = postData.data(using: .utf8)
            NSLog("RetroAchievements: POST request with \(postData.count) characters of data")
        } else {
            request.httpMethod = "GET"
        }
        
        // Use a URLSession with a reasonable timeout
        let config = URLSessionConfiguration.default
        config.timeoutIntervalForRequest = 30.0
        config.timeoutIntervalForResource = 60.0
        let session = URLSession(configuration: config)
        
        session.dataTask(with: request) { data, response, error in
            // Handle response on main thread
            DispatchQueue.main.async {
                var httpStatusCode: Int32 = 0
                var responseBody: String? = nil
                
                if let error = error {
                    let nsError = error as NSError
                    NSLog("RetroAchievements: HTTP request failed: \(error.localizedDescription)")
                    
                    // Map specific error types to appropriate status codes
                    switch nsError.code {
                    case NSURLErrorTimedOut:
                        NSLog("RetroAchievements: Request timed out - rcheevos will handle retry for achievement unlocks")
                        httpStatusCode = 0 // Timeout - rcheevos will queue for retry if needed
                    case NSURLErrorNotConnectedToInternet, NSURLErrorCannotFindHost, NSURLErrorNetworkConnectionLost:
                        NSLog("RetroAchievements: Network connectivity issue - rcheevos will handle retry for achievement unlocks")
                        httpStatusCode = 0 // Network error - rcheevos will queue for retry if needed
                    case NSURLErrorCancelled:
                        NSLog("RetroAchievements: Request was cancelled")
                        httpStatusCode = 0
                    default:
                        NSLog("RetroAchievements: Other network error: \(nsError.localizedDescription)")
                        httpStatusCode = 0
                    }
                } else {
                    if let httpResponse = response as? HTTPURLResponse {
                        httpStatusCode = Int32(httpResponse.statusCode)
                        NSLog("RetroAchievements: HTTP response status: \(httpStatusCode)")
                        
                        // Log server errors for debugging
                        if httpStatusCode >= 500 {
                            NSLog("RetroAchievements: Server error response (\(httpStatusCode)) - rcheevos will handle error events")
                        } else if httpStatusCode >= 400 {
                            NSLog("RetroAchievements: Client error response (\(httpStatusCode))")
                        }
                    }
                    
                    if let data = data {
                        responseBody = String(data: data, encoding: .utf8)
                        NSLog("RetroAchievements: Received response: \(responseBody?.prefix(200) ?? "nil")")
                        
                        // Check for empty response which might indicate server issues
                        if responseBody?.isEmpty == true {
                            NSLog("RetroAchievements: Empty response body received")
                        }
                    } else {
                        NSLog("RetroAchievements: No response data received")
                    }
                }
                
                // Complete the rcheevos request
                // For timeouts and network errors, pass NULL response_body as specified
                if let responseBody = responseBody, !responseBody.isEmpty {
                    responseBody.withCString { responsePtr in
                        YabauseRA_CompleteServerRequest(userdata, httpStatusCode, responsePtr, responseBody.utf8.count)
                    }
                } else {
                    // NULL response_body for timeouts and errors - rcheevos will handle retries appropriately
                    YabauseRA_CompleteServerRequest(userdata, httpStatusCode, nil, 0)
                }
            }
        }.resume()
    }
    
    // MARK: - Login Callback Setup
    private func setupLoginCallback() {
        NSLog("RetroAchievementsManager: Setting up login callback for rcheevos login notifications")
        
        // Set the login callback function
        YabauseRA_SetLoginCallback { (success: Int32, usernamePtr: UnsafePointer<CChar>?, displayNamePtr: UnsafePointer<CChar>?, score: UInt32, errorPtr: UnsafePointer<CChar>?) in
            
            NSLog("RetroAchievements: Login callback from rcheevos - Success: \(success != 0)")
            
            // Get the shared manager instance and handle the login result
            if let manager = RetroAchievementsManager.shared {
                let username = usernamePtr != nil ? String(cString: usernamePtr!) : nil
                let displayName = displayNamePtr != nil ? String(cString: displayNamePtr!) : nil
                let errorMessage = errorPtr != nil ? String(cString: errorPtr!) : nil
                
                manager.handleLoginComplete(success: success != 0, username: username, displayName: displayName, score: score, error: errorMessage)
            } else {
                NSLog("RetroAchievements: ERROR - No shared manager instance available for login callback")
            }
        }
    }
    
    private func handleLoginComplete(success: Bool, username: String?, displayName: String?, score: UInt32, error: String?) {
        NSLog("RetroAchievements: Handling login completion - Success: \(success)")
        
        if success, let username = username {
            NSLog("RetroAchievements: Login successful for user: \(username), Score: \(score)")
            
            // Create user object
            let user = RAUser(
                username: username,
                displayName: displayName ?? username,
                score: Int(score),
                softcoreScore: 0,
                token: "" // Token not available through rcheevos callback
            )
            
            self.currentUser = user
            self.onLoginComplete(success: true)
            
            // Call stored completion handler
            if let completion = self.loginCompletion {
                completion(true, nil)
                self.loginCompletion = nil
            }
        } else {
            NSLog("RetroAchievements: Login failed - \(error ?? "Unknown error")")
            
            // Call stored completion handler with error
            if let completion = self.loginCompletion {
                completion(false, error ?? "Login failed")
                self.loginCompletion = nil
            }
        }
    }
    
    // MARK: - Game Placard Callback Setup
    private func setupGamePlacardCallback() {
        NSLog("RetroAchievementsManager: Setting up game placard callback for rcheevos game events")
        
        // Set the game placard callback function
        YabauseRA_SetGamePlacardCallback { (gameTitlePtr: UnsafePointer<CChar>?, imageUrlPtr: UnsafePointer<CChar>?, unlockedAchievements: UInt32, totalAchievements: UInt32, unlockedPoints: UInt32, totalPoints: UInt32, hasUnsupported: Int32) in
            
            NSLog("RetroAchievements: Game placard callback from rcheevos")
            
            // Convert C strings to Swift strings
            let gameTitle = gameTitlePtr != nil ? String(cString: gameTitlePtr!) : "Unknown Game"
            let imageUrl = imageUrlPtr != nil ? String(cString: imageUrlPtr!) : nil
            
            NSLog("RetroAchievements: Game placard - \(gameTitle), Achievements: \(unlockedAchievements)/\(totalAchievements)")
            
            // Show placard on main thread
            DispatchQueue.main.async {
                RetroAchievementsGamePlacardManager.shared.showGamePlacardFromCallback(
                    gameTitle: gameTitle,
                    imageUrl: imageUrl,
                    unlockedAchievements: Int(unlockedAchievements),
                    totalAchievements: Int(totalAchievements),
                    unlockedPoints: Int(unlockedPoints),
                    totalPoints: Int(totalPoints),
                    hasUnsupported: hasUnsupported != 0
                )
            }
        }
    }
    
    // MARK: - Event Handling (Step 3 of rcheevos integration)
    private func setupEventCallback() {
        NSLog("RetroAchievementsManager: Setting up event callback for rcheevos notifications")
        
        // Set the event callback function
        YabauseRA_SetEventCallback { (eventType: Int32, messagePtr: UnsafePointer<CChar>) in
            let message = String(cString: messagePtr)
            
            NSLog("RetroAchievements: Event from rcheevos - Type: \(eventType), Message: \(message)")
            
            // Get the shared manager instance and handle the event
            if let manager = RetroAchievementsManager.shared {
                manager.handleRcheevosEvent(eventType: eventType, message: message)
            } else {
                NSLog("RetroAchievements: ERROR - No shared manager instance available for event")
            }
        }
    }
    
    private func handleRcheevosEvent(eventType: Int32, message: String) {
        // Handle different rcheevos event types
        // These constants should match rc_client_event_type_t values from rcheevos
        switch eventType {
        case 1: // RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED
            NSLog("RetroAchievements: Achievement unlocked via rcheevos: \(message)")
            handleAchievementTriggeredEvent(message: message)
            
        case 2: // RC_CLIENT_EVENT_LEADERBOARD_STARTED
            NSLog("RetroAchievements: Leaderboard started: \(message)")
            handleLeaderboardStartedEvent(message: message)
            
        case 3: // RC_CLIENT_EVENT_LEADERBOARD_FAILED
            NSLog("RetroAchievements: Leaderboard failed: \(message)")
            handleLeaderboardFailedEvent(message: message)
            
        case 4: // RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED
            NSLog("RetroAchievements: Leaderboard score submitted via rcheevos: \(message)")
            //handleLeaderboardSubmittedEvent(message: message)
            
        case 5: // RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW
            handleChallengeIndicatorEvent(message: message, show: true)
            
        case 6: // RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE
            handleChallengeIndicatorEvent(message: message, show: false)
            
        case 7: // RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW
            NSLog("RetroAchievements: Achievement progress indicator show: \(message)")
            handleProgressIndicatorEvent(message: message, show: true)
            
        case 8: // RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE
            NSLog("RetroAchievements: Achievement progress indicator hide: \(message)")
            handleProgressIndicatorEvent(message: message, show: false)
            
        case 9: // RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE
            NSLog("RetroAchievements: Achievement progress indicator update: \(message)")
            handleProgressIndicatorUpdateEvent(message: message)
            
        case 10: // RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW
            NSLog("RetroAchievements: Leaderboard tracker show: \(message)")
            handleLeaderboardTrackerEvent(message: message, show: true)
            
        case 11: // RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE  
            NSLog("RetroAchievements: Leaderboard tracker hide: \(message)")
            handleLeaderboardTrackerEvent(message: message, show: false)
            
        case 12: // RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE
            NSLog("RetroAchievements: Leaderboard tracker update: \(message)")
            handleLeaderboardTrackerUpdateEvent(message: message)
            
        case 13: // RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD
            NSLog("RetroAchievements: Leaderboard scoreboard received: \(message)")
            handleLeaderboardScoreboardEvent(message: message)
            
        case 14: // RC_CLIENT_EVENT_RESET
            NSLog("RetroAchievements: System reset requested: \(message)")
            
            // Call the reset function to reset emulator and notify runtime
            YabauseRA_Reset()
            
            // Handle reset event - typically occurs when hardcore mode changes
            if message.contains("unrecognized") || message.contains("media") {
                // Show notification that hardcore mode was disabled due to unrecognized media
                DispatchQueue.main.async {
                    let alert = UIAlertController(
                        title: "Hardcore Mode Disabled",
                        message: "Unrecognized media inserted. Achievements are still available in softcore mode.",
                        preferredStyle: .alert
                    )
                    alert.addAction(UIAlertAction(title: "OK", style: .default))
                    
                    // Present on the top view controller
                    if let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene,
                       let topVC = windowScene.windows.first?.rootViewController {
                        var presentedVC = topVC
                        while let presented = presentedVC.presentedViewController {
                            presentedVC = presented
                        }
                        presentedVC.present(alert, animated: true)
                    }
                }
                isHardcoreEnabled = false
            }
            
        case 15: // RC_CLIENT_EVENT_GAME_COMPLETED
            NSLog("RetroAchievements: Game completed (mastery): \(message)")
            handleGameCompletedEvent(message: message)
            
        case 16: // RC_CLIENT_EVENT_SERVER_ERROR
            NSLog("RetroAchievements: Server error: \(message)")
            handleServerErrorEvent(message: message)
            
        case 17: // RC_CLIENT_EVENT_DISCONNECTED
            NSLog("RetroAchievements: Disconnected: \(message)")
            // TODO: Handle disconnection
            
        case 18: // RC_CLIENT_EVENT_RECONNECTED
            NSLog("RetroAchievements: Reconnected: \(message)")
            // TODO: Handle reconnection
            
        case 19: // RC_CLIENT_EVENT_SUBSET_COMPLETED
            NSLog("RetroAchievements: Subset completed: \(message)")
            // TODO: Handle subset completion
            
        default:
            NSLog("RetroAchievements: Unknown event type \(eventType): \(message)")
        }
    }
    
    private func handleAchievementTriggeredEvent(message: String) {
        NSLog("RetroAchievements: Processing achievement triggered event: \(message)")
        
        // Try to parse JSON data first
        if let data = message.data(using: .utf8),
           let triggeredInfo = try? JSONDecoder().decode(AchievementTriggeredInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed achievement triggered JSON - ID: \(triggeredInfo.id), Title: \(triggeredInfo.title)")
            
            // All UI updates must be on main thread
            DispatchQueue.main.async {
                // Show achievement unlock notification with parsed data
                RetroAchievementsNotificationView.showAchievementUnlocked(
                    achievementId: triggeredInfo.id,
                    title: triggeredInfo.title,
                    description: triggeredInfo.description,
                    points: triggeredInfo.points,
                    imageUrl: triggeredInfo.badgeUrl.isEmpty ? nil : triggeredInfo.badgeUrl,
                    isUnofficial: triggeredInfo.isUnofficial
                )
                
                // Schedule Firebase Messaging notification if enabled
                if MessagingManager.shared.isNotificationCategoryEnabled(category: .achievements) {
                    MessagingManager.shared.scheduleAchievementNotification(
                        title: "アチーブメント解除！",
                        body: "\(triggeredInfo.title) - \(triggeredInfo.points)点",
                        achievementId: String(triggeredInfo.id)
                    )
                }
                
                // Play achievement unlock sound
                self.playAchievementUnlockSound(isUnofficial: triggeredInfo.isUnofficial)
                
                NSLog("RetroAchievements: Achievement unlock notification displayed for: \(triggeredInfo.title)")
            }
            
        } else {
            // Fallback to generic message processing
            NSLog("RetroAchievements: Failed to parse JSON, using fallback processing")
            
            // All UI updates must be on main thread
            DispatchQueue.main.async {
                // Show achievement unlock notification
                let achievementTitle = message.isEmpty ? "Achievement Unlocked!" : message
                
                RetroAchievementsNotificationView.showAchievementUnlocked(
                    achievementId: 0,
                    title: achievementTitle,
                    description: "Congratulations on unlocking this achievement!",
                    points: 10,
                    imageUrl: nil,
                    isUnofficial: false
                )
                
                // Play achievement unlock sound (assume official since we don't have data)
                self.playAchievementUnlockSound(isUnofficial: false)
                
                NSLog("RetroAchievements: Fallback achievement unlock notification displayed")
            }
        }
    }
    
    private func handleLeaderboardStartedEvent(message: String) {
        NSLog("RetroAchievements: Processing leaderboard started event: \(message)")
        
        // Try to parse JSON data first
        if let data = message.data(using: .utf8),
           let leaderboardInfo = try? JSONDecoder().decode(LeaderboardEventInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed leaderboard started JSON - ID: \(leaderboardInfo.id), Title: \(leaderboardInfo.title)")
            
            DispatchQueue.main.async {
                // Show leaderboard started notification
                RetroAchievementsNotificationView.showRichPresenceUpdate("🏁 Leaderboard Started: \(leaderboardInfo.title)")
                
                // Could also show a more detailed notification if desired
                // RetroAchievementsNotificationView.showLeaderboardStarted(
                //     leaderboardId: leaderboardInfo.id,
                //     title: leaderboardInfo.title,
                //     description: leaderboardInfo.description ?? ""
                // )
            }
        } else {
            // Fallback to simple message
            DispatchQueue.main.async {
                RetroAchievementsNotificationView.showRichPresenceUpdate("🏁 Leaderboard Started: \(message)")
            }
        }
    }
    
    private func handleLeaderboardFailedEvent(message: String) {
        NSLog("RetroAchievements: Processing leaderboard failed event: \(message)")
        
        // Try to parse JSON data first
        if let data = message.data(using: .utf8),
           let leaderboardInfo = try? JSONDecoder().decode(LeaderboardEventInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed leaderboard failed JSON - ID: \(leaderboardInfo.id), Title: \(leaderboardInfo.title)")
            
            DispatchQueue.main.async {
                // Show leaderboard failed notification with more detail
                let failureMessage = leaderboardInfo.submittedScore != nil ? 
                    "❌ Leaderboard Failed: \(leaderboardInfo.title) - Score not submitted" :
                    "❌ Leaderboard Failed: \(leaderboardInfo.title)"
                RetroAchievementsNotificationView.showRichPresenceUpdate(failureMessage)
            }
        } else {
            // Fallback to simple message
            DispatchQueue.main.async {
                RetroAchievementsNotificationView.showRichPresenceUpdate("❌ Leaderboard Failed: \(message)")
            }
        }
    }
    
    private func handleLeaderboardSubmittedEvent(message: String) {
        NSLog("RetroAchievements: Processing leaderboard submitted event: \(message)")
        
        // Try to parse JSON data first
        if let data = message.data(using: .utf8),
           let leaderboardInfo = try? JSONDecoder().decode(LeaderboardEventInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed leaderboard submitted JSON - ID: \(leaderboardInfo.id), Title: \(leaderboardInfo.title), Score: \(leaderboardInfo.submittedScore ?? "N/A")")
            
            DispatchQueue.main.async {
                // Show proper leaderboard submission notification
                RetroAchievementsNotificationView.showLeaderboardSubmit(
                    leaderboardId: leaderboardInfo.id,
                    title: leaderboardInfo.title,
                    description: leaderboardInfo.description ?? "New score submitted!",
                    scoreString: leaderboardInfo.submittedScore ?? ""
                )
                
                // Schedule Firebase Messaging notification if enabled
                if MessagingManager.shared.isNotificationCategoryEnabled(category: .leaderboards) {
                    let gameTitle = self.currentGame?.title ?? "ゲーム"
                    MessagingManager.shared.scheduleLeaderboardNotification(
                        title: "リーダーボード更新！",
                        body: "\(gameTitle) - \(leaderboardInfo.title): \(leaderboardInfo.submittedScore ?? "")",
                        gameId: String(self.currentGame?.id ?? 0)
                    )
                }
                
                // Also update the leaderboards array if we have it
                if let leaderboard = self.leaderboards.first(where: { $0.id == leaderboardInfo.id }) {
                    // Trigger any additional callbacks
                    if let score = leaderboardInfo.submittedScore, let numericScore = Int(score) {
                        self.onLeaderboardSubmitted?(leaderboard, numericScore)
                    }
                }
            }
        } else {
            // Fallback to simple message
            DispatchQueue.main.async {
                // Try to extract basic info from the message string
                RetroAchievementsNotificationView.showLeaderboardSubmit(
                    leaderboardId: 0,
                    title: "Leaderboard",
                    description: message,
                    scoreString: ""
                )
            }
        }
    }
    
    private func handleLeaderboardScoreboardEvent(message: String) {
        NSLog("RetroAchievements: Processing leaderboard scoreboard event: \(message)")
        
        // Try to parse JSON data
        if let data = message.data(using: .utf8),
           let scoreboardInfo = try? JSONDecoder().decode(LeaderboardScoreboardInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed leaderboard scoreboard JSON - ID: \(scoreboardInfo.id), Title: \(scoreboardInfo.title), Score: \(scoreboardInfo.submittedScore)")
            
            DispatchQueue.main.async {
                // Show scoreboard notification with confirmed score
                RetroAchievementsNotificationView.showLeaderboardSubmit(
                    leaderboardId: scoreboardInfo.id,
                    title: scoreboardInfo.title,
                    description: scoreboardInfo.description ?? "Score confirmed!",
                    scoreString: scoreboardInfo.submittedScore
                )
                
                // Submit score to Firebase (Android equivalent)
                self.submitScoreToFirebase(scoreboardInfo: scoreboardInfo)
            }
        } else {
            // Fallback to simple message
            NSLog("RetroAchievements: Failed to parse leaderboard scoreboard JSON: \(message)")
            DispatchQueue.main.async {
                RetroAchievementsNotificationView.showLeaderboardSubmit(
                    leaderboardId: 0,
                    title: "Leaderboard",
                    description: "Score confirmed",
                    scoreString: ""
                )
            }
        }
    }
    
    private func handleLeaderboardTrackerEvent(message: String, show: Bool) {
        NSLog("RetroAchievements: handleLeaderboardTrackerEvent called with show=\(show), message='\(message)'")
        
        if show {
            // Try to parse JSON data first
            if let data = message.data(using: .utf8),
               let trackerInfo = try? JSONDecoder().decode(LeaderboardTrackerInfo.self, from: data) {
                
                NSLog("RetroAchievements: Successfully parsed leaderboard tracker JSON - ID: \(trackerInfo.id), Display: \(trackerInfo.display)")
                
                // All UI updates must be on main thread
                DispatchQueue.main.async {
                    RetroAchievementsNotificationView.showLeaderboardTracker(
                        trackerId: trackerInfo.id,
                        display: trackerInfo.display
                    )
                    
                    NSLog("RetroAchievements: Leaderboard tracker displayed: \(trackerInfo.display)")
                }
                
            } else {
                // Fallback to generic message processing
                NSLog("RetroAchievements: Failed to parse tracker JSON, using fallback processing")
                
                // All UI updates must be on main thread
                DispatchQueue.main.async {
                    // Show generic tracker
                    let fallbackId = message.hashValue
                    RetroAchievementsNotificationView.showLeaderboardTracker(
                        trackerId: fallbackId,
                        display: message.isEmpty ? "Tracking..." : message
                    )
                    
                    NSLog("RetroAchievements: Fallback leaderboard tracker displayed")
                }
            }
        } else {
            // Hide tracker
            // Try to parse JSON to get tracker ID
            if let data = message.data(using: .utf8),
               let trackerInfo = try? JSONDecoder().decode(LeaderboardTrackerInfo.self, from: data) {
                
                DispatchQueue.main.async {
                    RetroAchievementsNotificationView.hideLeaderboardTracker(trackerId: trackerInfo.id)
                }
            } else {
                // Fallback: hide all trackers or use hash ID
                let fallbackId = message.hashValue
                DispatchQueue.main.async {
                    RetroAchievementsNotificationView.hideLeaderboardTracker(trackerId: fallbackId)
                }
            }
        }
    }
    
    private func handleLeaderboardTrackerUpdateEvent(message: String) {
        NSLog("RetroAchievements: handleLeaderboardTrackerUpdateEvent called with message: '\(message)'")
        
        // Try to parse JSON data first
        if let data = message.data(using: .utf8),
           let trackerInfo = try? JSONDecoder().decode(LeaderboardTrackerInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed leaderboard tracker update JSON - ID: \(trackerInfo.id), Display: \(trackerInfo.display)")
            
            // Update existing leaderboard tracker UI
            DispatchQueue.main.async {
                RetroAchievementsNotificationView.updateLeaderboardTracker(
                    trackerId: trackerInfo.id,
                    display: trackerInfo.display
                )
                
                NSLog("RetroAchievements: Leaderboard tracker updated: \(trackerInfo.display)")
            }
            
        } else {
            // Fallback to generic message processing
            NSLog("RetroAchievements: Failed to parse tracker update JSON, using fallback processing")
            
            DispatchQueue.main.async {
                // Try to update with fallback approach
                let fallbackId = message.hashValue
                RetroAchievementsNotificationView.updateLeaderboardTracker(
                    trackerId: fallbackId,
                    display: message.isEmpty ? "Tracking..." : message
                )
                
                NSLog("RetroAchievements: Fallback leaderboard tracker update")
            }
        }
    }
    
    private func handleChallengeIndicatorEvent(message: String, show: Bool) {
        NSLog("DEBUG: handleChallengeIndicatorEvent called with show=\(show), message='\(message)'")
        NSLog("DEBUG: Current login state: isUserLoggedIn=\(isUserLoggedIn)")
        NSLog("DEBUG: Current user: \(currentUser?.username ?? "nil")")
        
        if show {
            NSLog("RetroAchievements: Showing challenge indicator for: \(message)")
            
            // Try to parse message as JSON first (new approach)
            if let challengeInfo = parseChallengeIndicatorJSON(from: message) {
                NSLog("DEBUG: Using JSON-based challenge indicator info")
                
                DispatchQueue.main.async {
                    let imageUrl = challengeInfo.badgeUrl.isEmpty ? nil : challengeInfo.badgeUrl
                    
                    RetroAchievementsNotificationView.showChallengeIndicator(
                        achievementId: challengeInfo.id,
                        title: challengeInfo.title,
                        imageUrl: imageUrl
                    )
                    
                    NSLog("RetroAchievements: Challenge indicator displayed with JSON: \(challengeInfo.title)")
                }
                return
            }
            
            // Fallback to old method if JSON parsing fails
            NSLog("DEBUG: JSON parsing failed, using fallback method")
            
            // Get achievement info from rcheevos for proper badge image
            guard let achievementInfo = getCurrentChallengeAchievementInfo(from: message) else {
                NSLog("RetroAchievements: Could not get challenge achievement info, using generic fallback")
                // Fallback: Show challenge indicator with generic info
                DispatchQueue.main.async {
                    let fallbackId = message.hashValue
                    
                    RetroAchievementsNotificationView.showChallengeIndicator(
                        achievementId: fallbackId,
                        title: "Challenge Active",
                        imageUrl: nil // Will use default system icon
                    )
                    NSLog("DEBUG: Showed generic fallback challenge indicator")
                }
                return
            }
            
            // All UI updates must be on main thread
            DispatchQueue.main.async {
                NSLog("DEBUG: Inside main thread dispatch for challenge indicator")
                NSLog("DEBUG: Achievement ID=\(achievementInfo.id), Badge='\(achievementInfo.badge)'")
                
                // Construct badge image URL only if badge is not empty
                let imageUrl = achievementInfo.badge.isEmpty ? nil : "https://media.retroachievements.org/Badge/\(achievementInfo.badge).png"
                
                NSLog("DEBUG: About to call showChallengeIndicator with ID=\(achievementInfo.id), imageUrl=\(imageUrl ?? "nil")")
                
                // Show challenge indicator with proper badge image
                RetroAchievementsNotificationView.showChallengeIndicator(
                    achievementId: achievementInfo.id,
                    title: achievementInfo.title,
                    imageUrl: imageUrl
                )
                
                NSLog("RetroAchievements: Challenge indicator displayed for achievement: \(achievementInfo.title)")
            }
        } else {
            NSLog("RetroAchievements: Hiding challenge indicator for: \(message)")
            
            DispatchQueue.main.async {
                // Try to parse message as JSON first to get correct achievement ID
                if let challengeInfo = self.parseChallengeIndicatorJSON(from: message) {
                    RetroAchievementsNotificationView.hideChallengeIndicator(achievementId: challengeInfo.id)
                } else if let achievementInfo = self.getCurrentChallengeAchievementInfo(from: message) {
                    RetroAchievementsNotificationView.hideChallengeIndicator(achievementId: achievementInfo.id)
                } else {
                    // Fallback to hashValue if parsing fails
                    let achievementId = message.hashValue
                    RetroAchievementsNotificationView.hideChallengeIndicator(achievementId: achievementId)
                }
            }
        }
    }
    
    private func handleProgressIndicatorEvent(message: String, show: Bool) {
        NSLog("RetroAchievements: handleProgressIndicatorEvent called with show=\(show), message='\(message)'")
        
        if show {
            // Try to parse JSON data first
            if let data = message.data(using: .utf8),
               let progressInfo = try? JSONDecoder().decode(ProgressIndicatorInfo.self, from: data) {
                
                NSLog("RetroAchievements: Successfully parsed progress indicator JSON - ID: \(progressInfo.id), Progress: \(progressInfo.measuredProgress)")
                
                // All UI updates must be on main thread
                DispatchQueue.main.async {
                    RetroAchievementsNotificationView.showProgressIndicator(
                        achievementId: progressInfo.id,
                        title: progressInfo.title,
                        progress: progressInfo.measuredProgress,
                        percent: progressInfo.measuredPercent,
                        imageUrl: progressInfo.badgeUrl.isEmpty ? nil : progressInfo.badgeUrl
                    )
                    
                    NSLog("RetroAchievements: Progress indicator displayed: \(progressInfo.title) - \(progressInfo.measuredProgress)")
                }
                
            } else {
                // Fallback to generic message processing
                NSLog("RetroAchievements: Failed to parse progress indicator JSON, using fallback processing")
                
                DispatchQueue.main.async {
                    // Show generic progress indicator
                    RetroAchievementsNotificationView.showProgressIndicator(
                        achievementId: message.hashValue,
                        title: "Progress",
                        progress: message.isEmpty ? "Working..." : message,
                        percent: 0.0,
                        imageUrl: nil
                    )
                    
                    NSLog("RetroAchievements: Fallback progress indicator displayed")
                }
            }
        } else {
            // Hide progress indicator
            NSLog("RetroAchievements: Hiding progress indicator")
            
            // Add a small delay to let players see the final progress before hiding
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                RetroAchievementsNotificationView.hideProgressIndicator()
            }
        }
    }
    
    private func handleProgressIndicatorUpdateEvent(message: String) {
        NSLog("RetroAchievements: handleProgressIndicatorUpdateEvent called with message: '\(message)'")
        
        // Try to parse JSON data first
        if let data = message.data(using: .utf8),
           let progressInfo = try? JSONDecoder().decode(ProgressIndicatorInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed progress indicator update JSON - ID: \(progressInfo.id), Progress: \(progressInfo.measuredProgress)")
            
            // Update existing progress indicator UI
            DispatchQueue.main.async {
                RetroAchievementsNotificationView.updateProgressIndicator(
                    achievementId: progressInfo.id,
                    title: progressInfo.title,
                    progress: progressInfo.measuredProgress,
                    percent: progressInfo.measuredPercent,
                    imageUrl: progressInfo.badgeUrl.isEmpty ? nil : progressInfo.badgeUrl
                )
                
                NSLog("RetroAchievements: Progress indicator updated: \(progressInfo.title) - \(progressInfo.measuredProgress)")
            }
            
        } else {
            // Fallback to generic message processing
            NSLog("RetroAchievements: Failed to parse progress indicator update JSON, using fallback processing")
            
            DispatchQueue.main.async {
                // Try to update with fallback approach
                RetroAchievementsNotificationView.updateProgressIndicator(
                    achievementId: message.hashValue,
                    title: "Progress",
                    progress: message.isEmpty ? "Working..." : message,
                    percent: 0.0,
                    imageUrl: nil
                )
                
                NSLog("RetroAchievements: Fallback progress indicator update")
            }
        }
    }
    
    private func handleGameCompletedEvent(message: String) {
        NSLog("RetroAchievements: Processing game mastery event")
        
        // Get current game and user info for mastery display
        guard let currentGame = self.currentGame,
              let currentUser = self.currentUser else {
            NSLog("RetroAchievements: Cannot show mastery - missing game or user info")
            return
        }
        
        // Determine if this is hardcore mastery or softcore completion
        let isHardcore = getHardcoreEnabled()
        let masteryTitle = isHardcore ? "Mastered" : "Completed"
        let fullTitle = "\(masteryTitle) \(currentGame.title)"
        
        // Get total achievements and points for this game
        let totalAchievements = currentGame.achievementCount
        let unlockedAchievements = achievements.filter { $0.state != 0 }.count
        let totalPoints = achievements.reduce(0) { $0 + $1.points }
        let unlockedPoints = achievements.filter { $0.state != 0 }.reduce(0) { $0 + $1.points }
        
        NSLog("RetroAchievements: \(masteryTitle) - \(unlockedAchievements)/\(totalAchievements) achievements, \(unlockedPoints)/\(totalPoints) points")
        
        // Get game image URL for mastery display
        let gameImageURL = getCurrentGameImageURL()
        
        // Show mastery notification on main thread
        DispatchQueue.main.async {
            // Use the existing showGameMastery method
            RetroAchievementsNotificationView.showGameMastery(
                gameTitle: currentGame.title,
                imageUrl: gameImageURL,
                achievementCount: totalAchievements,
                points: totalPoints,
                isHardcore: isHardcore,
                username: currentUser.displayName,
                playtime: nil // Could add playtime tracking in the future
            )
            
            // Play mastery sound (use official achievement sound for now)
            self.playAchievementUnlockSound(isUnofficial: false)
        }
        
        // Update achievements list to reflect any final state changes
        achievements = createAchievementList()
    }
    
    private func handleServerErrorEvent(message: String) {
        NSLog("RetroAchievements: Processing server error event: \(message)")
        
        // Try to parse JSON data first to extract detailed error information
        if let data = message.data(using: .utf8),
           let serverErrorInfo = try? JSONDecoder().decode(ServerErrorInfo.self, from: data) {
            
            NSLog("RetroAchievements: Successfully parsed server error JSON - Error: \(serverErrorInfo.error)")
            
            DispatchQueue.main.async {
                // Show detailed server error notification
                let errorMessage = serverErrorInfo.error.isEmpty ? "RetroAchievements server error occurred" : serverErrorInfo.error
                
                RetroAchievementsNotificationView.showServerError("Server Error: \(errorMessage)")
                
                NSLog("RetroAchievements: Displayed server error notification: \(errorMessage)")
            }
        } else {
            // Fallback to simple message processing
            NSLog("RetroAchievements: Failed to parse server error JSON, using fallback processing")
            
            DispatchQueue.main.async {
                // Show generic server error notification
                let errorMessage = message.isEmpty ? "RetroAchievements server error occurred" : message
                
                RetroAchievementsNotificationView.showServerError("Server Error: \(errorMessage)")
                
                NSLog("RetroAchievements: Displayed fallback server error notification")
            }
        }
    }
    
    // MARK: - Achievement Sound Effects
    
    private func playAchievementUnlockSound(isUnofficial: Bool) {
        do {
            // Use different sounds for official vs unofficial achievements
            let soundName = isUnofficial ? "achievement_unofficial" : "achievement_unlock"
            
            // Try to find the sound file in the app bundle
            if let soundURL = Bundle.main.url(forResource: soundName, withExtension: "wav") {
                let audioPlayer = try AVAudioPlayer(contentsOf: soundURL)
                audioPlayer.play()
                NSLog("RetroAchievements: Playing \(isUnofficial ? "unofficial" : "official") achievement unlock sound")
            } else {
                // Fallback to system sound if custom sound not found
                playSystemAchievementSound(isUnofficial: isUnofficial)
            }
        } catch {
            NSLog("RetroAchievements: Failed to play achievement sound: \(error)")
            // Fallback to system sound
            playSystemAchievementSound(isUnofficial: isUnofficial)
        }
    }
    
    private func playSystemAchievementSound(isUnofficial: Bool) {
        // Use iOS system sounds as fallback
        // Use different system sounds for official vs unofficial
        if isUnofficial {
            // Use a softer sound for unofficial achievements
            AudioServicesPlaySystemSound(1013) // Text received sound
        } else {
            // Use a more prominent sound for official achievements
            AudioServicesPlaySystemSound(1016) // Alert sound
        }
        
        NSLog("RetroAchievements: Playing system achievement sound (\(isUnofficial ? "unofficial" : "official"))")
    }
    
    // Achievement Triggered data structure for JSON parsing
    private struct AchievementTriggeredInfo: Codable {
        let id: Int
        let title: String
        let description: String
        let badge: String
        let badgeUrl: String
        let points: Int
        let state: Int
        let isUnofficial: Bool
        let unlockedTime: Int
        let category: Int
        
        private enum CodingKeys: String, CodingKey {
            case id, title, description, badge, points, state, category
            case badgeUrl = "badge_url"
            case isUnofficial = "is_unofficial"
            case unlockedTime = "unlocked_time"
        }
    }
    
    // Progress Indicator data structure for JSON parsing
    private struct ProgressIndicatorInfo: Codable {
        let id: Int
        let title: String
        let description: String
        let badge: String
        let badgeUrl: String
        let points: Int
        let state: Int
        let measuredProgress: String
        let measuredPercent: Double
        let category: Int
        
        private enum CodingKeys: String, CodingKey {
            case id, title, description, badge, points, state, category
            case badgeUrl = "badge_url"
            case measuredProgress = "measured_progress"
            case measuredPercent = "measured_percent"
        }
    }
    
    // Progress Indicator Hide action structure
    private struct ProgressIndicatorHideAction: Codable {
        let action: String
    }
    
    // Leaderboard Tracker data structure for JSON parsing
    private struct LeaderboardTrackerInfo: Codable {
        let id: Int
        let display: String
        
        private enum CodingKeys: String, CodingKey {
            case id = "id"
            case display = "display"
        }
    }
    
    // Leaderboard Event data structure for JSON parsing (submitted/started/failed events)
    private struct LeaderboardEventInfo: Codable {
        let id: Int
        let title: String
        let description: String?
        let submittedScore: String?
        let bestScore: String?
        let newRank: Int?
        let format: String?
        
        private enum CodingKeys: String, CodingKey {
            case id = "id"
            case title = "title"
            case description = "description"
            case submittedScore = "submitted_score"
            case bestScore = "best_score" 
            case newRank = "new_rank"
            case format = "format"
        }
    }
    
    // Leaderboard Scoreboard data structure for JSON parsing (RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD)
    private struct LeaderboardScoreboardInfo: Codable {
        let id: Int
        let title: String
        let description: String?
        let submittedScore: String
        let bestScore: String?
        let newRank: Int?
        let numEntries: Int?
        let format: String?
        
        private enum CodingKeys: String, CodingKey {
            case id = "id"
            case title = "title"
            case description = "description"
            case submittedScore = "submitted_score"
            case bestScore = "best_score"
            case newRank = "new_rank"
            case numEntries = "num_entries"
            case format = "format"
        }
    }
    
    // Challenge Indicator data structure for JSON parsing
    private struct ChallengeIndicatorInfo: Codable {
        let id: Int
        let title: String
        let description: String
        let badge: String
        let badgeUrl: String
        let points: Int
        let state: Int
        
        enum CodingKeys: String, CodingKey {
            case id = "id"
            case title = "title"
            case description = "description"  
            case badge = "badge"
            case badgeUrl = "badge_url"
            case points = "points"
            case state = "state"
        }
    }
    
    // Server Error data structure for JSON parsing
    private struct ServerErrorInfo: Codable {
        let error: String
        let apiMethod: String?
        let httpStatusCode: Int?
        let requestUrl: String?
        
        private enum CodingKeys: String, CodingKey {
            case error = "error"
            case apiMethod = "api_method"
            case httpStatusCode = "http_status_code"
            case requestUrl = "request_url"
        }
    }
    
    // Parse Challenge Indicator JSON from native layer
    private func parseChallengeIndicatorJSON(from jsonString: String) -> ChallengeIndicatorInfo? {
        guard let data = jsonString.data(using: .utf8) else {
            NSLog("DEBUG: Failed to convert JSON string to data")
            return nil
        }
        
        do {
            let challengeInfo = try JSONDecoder().decode(ChallengeIndicatorInfo.self, from: data)
            NSLog("DEBUG: Successfully parsed challenge indicator JSON: \(challengeInfo.title)")
            return challengeInfo
        } catch {
            NSLog("DEBUG: Failed to parse challenge indicator JSON: \(error)")
            return nil
        }
    }

    // Get current challenge achievement info from rcheevos
    private func getCurrentChallengeAchievementInfo(from message: String) -> RAAchievement? {
        NSLog("DEBUG: getCurrentChallengeAchievementInfo called")
        NSLog("DEBUG: Total achievements available: \(achievements.count)")
        
        // If achievements haven't loaded yet, try to get them directly from rcheevos
        if achievements.isEmpty {
            NSLog("DEBUG: Achievements array empty, trying to load from rcheevos")
            // Force refresh achievement list from native layer with different categories
            // Try category 3 (core achievements) and grouping 1 (locked achievements)
            let refreshedAchievements = createAchievementList(category: 3, grouping: 1)
            NSLog("DEBUG: Refreshed achievements count: \(refreshedAchievements.count)")
            
            // Update the achievements array with refreshed data
            if !refreshedAchievements.isEmpty {
                achievements = refreshedAchievements
                NSLog("DEBUG: Updated achievements array with \(achievements.count) achievements")
            } else {
                // Fallback: try all achievements
                let allAchievements = createAchievementList(category: 0, grouping: 0)
                NSLog("DEBUG: Fallback - all achievements count: \(allAchievements.count)")
                achievements = allAchievements
            }
        }
        
        // Look for achievements that are not yet unlocked (state != 1) - these are potential challenges
        let lockedAchievements = achievements.filter { $0.state != 1 }
        
        NSLog("DEBUG: Found \(lockedAchievements.count) locked achievements")
        NSLog("DEBUG: Total achievements: \(achievements.count)")
        
        // Debug: Print first few achievements for investigation
        for (index, achievement) in achievements.prefix(3).enumerated() {
            NSLog("DEBUG: Achievement[\(index)]: '\(achievement.title)' state=\(achievement.state) badge='\(achievement.badge)'")
        }
        
        // If we have locked achievements, use the first one
        if let challengeAchievement = lockedAchievements.first {
            NSLog("DEBUG: Using locked achievement as challenge: \(challengeAchievement.title)")
            return challengeAchievement
        }
        
        // If no locked achievements, but we have achievements, use the first one anyway for testing
        // This allows Challenge Indicators to show even when all achievements are unlocked
        if let firstAchievement = achievements.first {
            NSLog("DEBUG: No locked achievements, using first available for testing: \(firstAchievement.title)")
            return firstAchievement
        }
        
        NSLog("DEBUG: No achievements available at all")
        return nil
    }
    
    // MARK: - Game Loop Integration (Step 7 of rcheevos integration)
    func processFrame() {
        // Call rcheevos frame processing - this is where achievements are evaluated
        YabauseRA_DoFrame()
    }
    
    // MARK: - Test Notifications
    func testNotificationsInternal() {
        print("Testing RetroAchievements notifications...")
        
        // Test achievement unlock
        DispatchQueue.main.async {
            RetroAchievementsNotificationView.showAchievementUnlocked(
                achievementId: 12345,
                title: "Test Achievement",
                description: "This is a test achievement notification",
                points: 25,
                imageUrl: "https://retroachievements.org/Badge/12345.png",
                isUnofficial: false
            )
        }
        
        // Test leaderboard submit after 2 seconds
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            RetroAchievementsNotificationView.showLeaderboardSubmit(
                leaderboardId: 67890,
                title: "Test Leaderboard",
                description: "Your new score",
                scoreString: "1500"
            )
        }
        
        // Test mastery notification after 4 seconds
        DispatchQueue.main.asyncAfter(deadline: .now() + 4.0) {
            RetroAchievementsNotificationView.showGameMastery(
                gameTitle: "Test Game",
                imageUrl: nil,
                achievementCount: 25,
                points: 1000,
                isHardcore: true,
                username: "TestUser",
                playtime: "2h 30m"
            )
        }
        
        // Test error notification after 6 seconds
        DispatchQueue.main.asyncAfter(deadline: .now() + 6.0) {
            RetroAchievementsNotificationView.showServerError("This is a test error notification")
        }
        
        print("All test notifications queued successfully!")
    }
    
    // MARK: - Firebase Integration
    
    /**
     * Submit RetroAchievements leaderboard score to Firebase
     * This is called from handleLeaderboardScoreboardEvent when a confirmed score is received
     */
    private func submitScoreToFirebase(scoreboardInfo: LeaderboardScoreboardInfo) {
        NSLog("RetroAchievements: Submitting score to Firebase - Leaderboard: \(scoreboardInfo.id), Score: \(scoreboardInfo.submittedScore)")
        
        // Parse score string to numeric value
        let numericScore = parseRetroAchievementsScore(scoreboardInfo.submittedScore)
        
        // Get current username from Firebase Auth or use fallback
        let userName = getCurrentUserName()
        
        // Get current BaseGame instance
        guard let currentGame = getCurrentBaseGame() else {
            NSLog("RetroAchievements: No current BaseGame available for Firebase submission")
            return
        }
        
        // Submit to Firebase using BaseGame
        currentGame.submitRetroAchievementsScore(
            retroAchievementsLeaderboardId: String(scoreboardInfo.id),
            score: numericScore,
            userName: userName,
            title: scoreboardInfo.title,
            onSuccess: {
                NSLog("RetroAchievements: Successfully submitted score to Firebase")
                // Optional: Show success notification
                DispatchQueue.main.async {
                    // Could show a subtle success indicator
                }
            },
            onFailure: { error in
                NSLog("RetroAchievements: Failed to submit score to Firebase: \(error.localizedDescription)")
                // Optional: Show error notification
                DispatchQueue.main.async {
                    // Could show an error toast
                }
            }
        )
    }
    
    /**
     * Get current user name from Firebase Auth
     */
    private func getCurrentUserName() -> String {
        if let currentUser = FirebaseAuth.Auth.auth().currentUser {
            return currentUser.displayName ?? currentUser.email ?? "RetroAchievements User"
        }
        return "Anonymous User"
    }
    
    // MARK: - Game Context Management
    
    /// Current BaseGame instance for Firebase integration
    private weak var currentBaseGame: BaseGame?
    
    /**
     * Set current BaseGame instance from GameViewController
     * This is called when a game is detected/loaded
     */
    func setCurrentGame(_ game: BaseGame?) {
        currentBaseGame = game
        NSLog("RetroAchievements: Current BaseGame set to: \(game != nil ? String(describing: type(of: game!)) : "nil")")
    }
    
    /**
     * Get current BaseGame instance for Firebase submissions
     */
    private func getCurrentBaseGame() -> BaseGame? {
        return currentBaseGame
    }
    
    /**
     * Parse RetroAchievements score string to numeric value for Firebase storage
     * Handles various score formats: time (MM:SS.ss), numeric with commas, plain numbers
     */
    private func parseRetroAchievementsScore(_ scoreString: String) -> Int64 {
        NSLog("RetroAchievements: Parsing score string: '\(scoreString)'")
        
        let trimmedScore = scoreString.trimmingCharacters(in: .whitespacesAndNewlines)
        
        // Handle time format: "MM:SS.ss" -> convert to milliseconds
        if trimmedScore.contains(":") && trimmedScore.contains(".") {
            let components = trimmedScore.components(separatedBy: ":")
            if components.count == 2,
               let minutes = Int(components[0]),
               let secondsAndMs = Double(components[1]) {
                let totalMs = Int64(minutes * 60 * 1000 + Int(secondsAndMs * 1000))
                NSLog("RetroAchievements: Parsed time format '\(scoreString)' -> \(totalMs)ms")
                return totalMs
            }
        }
        
        // Handle numeric format with commas: "1,234,567" -> 1234567
        if trimmedScore.contains(",") {
            let numericString = trimmedScore.replacingOccurrences(of: ",", with: "")
            if let numericValue = Int64(numericString) {
                NSLog("RetroAchievements: Parsed numeric format with commas '\(scoreString)' -> \(numericValue)")
                return numericValue
            }
        }
        
        // Handle plain number: "12345" -> 12345
        if let numericValue = Int64(trimmedScore) {
            NSLog("RetroAchievements: Parsed plain numeric '\(scoreString)' -> \(numericValue)")
            return numericValue
        }
        
        // Fallback to 0 for unparseable scores
        NSLog("RetroAchievements: Failed to parse score string '\(scoreString)', using 0")
        return 0
    }
}



// MARK: - Objective-C Bridge Class
@objc public class RetroAchievementsBridge: NSObject {
    
    @objc public static func performHTTPRequest(_ url: String, postData: String?, completion: @escaping (String?, String?) -> Void) {
        RetroAchievementsManager.shared?.performHTTPRequest(url: url, postData: postData, completion: completion)
    }
    
    @objc public static func onAchievementTriggered(_ achievementId: Int32) {
        RetroAchievementsManager.shared?.onAchievementTriggered(achievementId: Int(achievementId))
    }
    
    @objc public static func onLeaderboardSubmitted(_ leaderboardId: Int32, score: Int32) {
        RetroAchievementsManager.shared?.onLeaderboardSubmitted(leaderboardId: Int(leaderboardId), score: Int(score))
    }
    
    @objc public static func loadGame(_ hash: String, completion: @escaping (Bool, String?) -> Void) {
        RetroAchievementsManager.shared?.loadGame(hash: hash, completion: completion)
    }
    
    @objc public static func updateRichPresence() {
        RetroAchievementsManager.shared?.updateRichPresence()
    }
    
    @objc public static func initializeNativeLayer() -> Bool {
        return RetroAchievementsManager.shared?.initializeNativeLayer() ?? false
    }
    
    @objc public static func shutdownNativeLayer() {
        RetroAchievementsManager.shared?.shutdownNativeLayer()
    }
    
    @objc public static func processFrame() {
        RetroAchievementsManager.shared?.processFrame()
    }
    
    // MARK: - Test Notifications
    @objc public static func testNotifications() {
        RetroAchievementsManager.shared?.testNotificationsInternal()
    }
}
