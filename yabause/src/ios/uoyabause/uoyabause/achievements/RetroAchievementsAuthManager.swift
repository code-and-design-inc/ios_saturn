import Foundation
import FirebaseAuth

protocol RetroAchievementsAuthDelegate: AnyObject {
    func authStateDidChange(isLoggedIn: Bool, username: String?)
}

class RetroAchievementsAuthManager {
    
    static let shared = RetroAchievementsAuthManager()
    
    weak var delegate: RetroAchievementsAuthDelegate?
    
    private let userDefaults = UserDefaults.standard
    private var tempPassword: String?
    
    private struct UserDefaultsKeys {
        static let username = "ra_username"
        static let autoLogin = "ra_auto_login"
        static let isLoggedIn = "ra_is_logged_in"
        static let rememberMe = "ra_remember_me"
    }
    
    private init() {
        Auth.auth().addStateDidChangeListener { [weak self] _, user in
            if user == nil {
                self?.handleFirebaseLogout()
            }
        }
    }
    
    var isRetroAchievementsLoggedIn: Bool {
        return userDefaults.bool(forKey: UserDefaultsKeys.isLoggedIn)
    }
    
    var currentUsername: String? {
        return userDefaults.string(forKey: UserDefaultsKeys.username)
    }
    
    var autoLoginEnabled: Bool {
        get {
            // Always return true - auto login is always enabled
            return true
        }
        set {
            // Do nothing - auto login cannot be disabled
        }
    }
    
    func loginRetroAchievements(username: String, password: String, completion: @escaping (Bool, String?) -> Void) {
        NSLog("RetroAchievementsAuthManager: Starting login process for user: \(username)")
        
        // Store credentials temporarily for saving after successful login
        self.tempPassword = password
        
        // Initialize RetroAchievementsManager if not already initialized
        RetroAchievementsManager.initialize()
        
        guard let manager = RetroAchievementsManager.shared else {
            NSLog("RetroAchievementsAuthManager: Manager initialization failed")
            self.tempPassword = nil
            completion(false, "RetroAchievements manager not initialized")
            return
        }
        
        NSLog("RetroAchievementsAuthManager: Manager initialized successfully, calling loginUser")
        
        manager.loginUser(username: username, password: password) { [weak self] success, errorMessage in
            DispatchQueue.main.async {
                NSLog("RetroAchievementsAuthManager: Login result - success: \(success), error: \(errorMessage ?? "none")")
                
                if success {
                    self?.handleLoginSuccess(username: username, password: password)
                    self?.tempPassword = nil
                    completion(true, nil)
                } else {
                    self?.tempPassword = nil
                    completion(false, errorMessage)
                }
            }
        }
    }
    
    func logoutRetroAchievements() {
        setLoginState(isLoggedIn: false, username: nil)
        
        _ = KeychainHelper.deleteCredentials()
        
        // Initialize manager if needed for logout
        RetroAchievementsManager.initialize()
        if let manager = RetroAchievementsManager.shared {
            manager.logout()
        }
        
        delegate?.authStateDidChange(isLoggedIn: false, username: nil)
    }
    
    func attemptAutoLogin(completion: @escaping (Bool, String?) -> Void) {
        NSLog("RetroAchievementsAuthManager: Attempting auto-login - always enabled")
        
        guard let credentials = KeychainHelper.loadCredentials() else {
            NSLog("RetroAchievementsAuthManager: No stored credentials found in Keychain")
            completion(false, "No stored credentials found")
            return
        }
        
        NSLog("RetroAchievementsAuthManager: Found stored credentials for user: \(credentials.username)")
        
        // loginRetroAchievements will handle manager initialization
        loginRetroAchievements(username: credentials.username, password: credentials.password, completion: completion)
    }
    
    func attemptAutoLogin() {
        attemptAutoLogin { success, error in
            if let error = error {
                print("Auto-login failed: \(error)")
            } else if success {
                print("Auto-login successful")
            }
        }
    }
    
    func onNativeLoginComplete(success: Bool, username: String?) {
        if success, let username = username {
            // Use stored temporary password if available
            handleLoginSuccess(username: username, password: tempPassword)
            tempPassword = nil
        } else {
            setLoginState(isLoggedIn: false, username: nil)
            tempPassword = nil
        }
        
        delegate?.authStateDidChange(isLoggedIn: success, username: username)
    }
    
    private func handleLoginSuccess(username: String, password: String?) {
        setLoginState(isLoggedIn: true, username: username)
        
        // Always save credentials since auto-login is always enabled
        if let password = password {
            let saved = KeychainHelper.saveCredentials(username: username, password: password)
            NSLog("RetroAchievementsAuthManager: Credentials save result: \(saved) for user: \(username)")
            
            // Debug: Verify the save immediately
            if saved {
                if let _ = KeychainHelper.loadCredentials() {
                    NSLog("RetroAchievementsAuthManager: Credentials verified - successfully stored")
                } else {
                    NSLog("RetroAchievementsAuthManager: ERROR - Credentials saved but cannot be loaded!")
                }
            }
        } else {
            NSLog("RetroAchievementsAuthManager: Warning - No password provided for saving credentials")
        }
        
        delegate?.authStateDidChange(isLoggedIn: true, username: username)
    }
    
    private func setLoginState(isLoggedIn: Bool, username: String?) {
        userDefaults.set(isLoggedIn, forKey: UserDefaultsKeys.isLoggedIn)
        
        if let username = username {
            userDefaults.set(username, forKey: UserDefaultsKeys.username)
        } else {
            userDefaults.removeObject(forKey: UserDefaultsKeys.username)
        }
    }
    
    func clearStoredCredentials() {
        let deleted = KeychainHelper.deleteCredentials()
        NSLog("RetroAchievementsAuthManager: Credentials cleared: \(deleted)")
        
        // Debug: Check what was actually in keychain before deletion
        if deleted {
            NSLog("RetroAchievementsAuthManager: Successfully cleared stored credentials")
        }
    }
    
    // Debug function to test credential storage
    func debugTestCredentialStorage() {
        NSLog("RetroAchievementsAuthManager: Testing credential storage...")
        
        // Test save
        let testUsername = "test_user"
        let testPassword = "test_pass"
        let saved = KeychainHelper.saveCredentials(username: testUsername, password: testPassword)
        NSLog("RetroAchievementsAuthManager: Test save result: \(saved)")
        
        // Test load
        if let credentials = KeychainHelper.loadCredentials() {
            NSLog("RetroAchievementsAuthManager: Test load successful - username: \(credentials.username)")
            
            // Clean up test data
            _ = KeychainHelper.deleteCredentials()
            NSLog("RetroAchievementsAuthManager: Test credentials cleaned up")
        } else {
            NSLog("RetroAchievementsAuthManager: Test load failed - no credentials found")
        }
    }
    
    var isFirebaseAuthenticated: Bool {
        return Auth.auth().currentUser != nil
    }
    
    var canUseRetroAchievements: Bool {
        return isFirebaseAuthenticated
    }
    
    private func handleFirebaseLogout() {
        if isRetroAchievementsLoggedIn {
            logoutRetroAchievements()
        }
    }
}
