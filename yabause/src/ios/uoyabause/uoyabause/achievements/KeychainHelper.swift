import Foundation
import Security

class KeychainHelper {
    
    private init() {}
    
    static func save(key: String, value: String) -> Bool {
        guard let data = value.data(using: .utf8) else {
            return false
        }
        
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: "YabaSnashiro.RetroAchievements",
            kSecAttrAccount as String: key,
            kSecValueData as String: data,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        ]
        
        SecItemDelete(query as CFDictionary)
        
        let status = SecItemAdd(query as CFDictionary, nil)
        return status == errSecSuccess
    }
    
    static func load(key: String) -> String? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: "YabaSnashiro.RetroAchievements",
            kSecAttrAccount as String: key,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne
        ]
        
        var result: AnyObject?
        let status = SecItemCopyMatching(query as CFDictionary, &result)
        
        NSLog("KeychainHelper.load: key=\(key), status=\(status), errSecSuccess=\(errSecSuccess)")
        
        guard status == errSecSuccess,
              let data = result as? Data,
              let value = String(data: data, encoding: .utf8) else {
            NSLog("KeychainHelper.load: Failed to load key=\(key), status=\(status)")
            return nil
        }
        
        NSLog("KeychainHelper.load: Successfully loaded key=\(key), value length=\(value.count)")
        return value
    }
    
    static func delete(key: String) -> Bool {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: "YabaSnashiro.RetroAchievements",
            kSecAttrAccount as String: key
        ]
        
        let status = SecItemDelete(query as CFDictionary)
        return status == errSecSuccess || status == errSecItemNotFound
    }
    
    static func saveCredentials(username: String, password: String) -> Bool {
        let credentials = "\(username):\(password)"
        NSLog("KeychainHelper.saveCredentials: Saving credentials for user=\(username), credentials length=\(credentials.count)")
        let result = save(key: "ra_credentials", value: credentials)
        NSLog("KeychainHelper.saveCredentials: Save result=\(result)")
        return result
    }
    
    static func loadCredentials() -> (username: String, password: String)? {
        NSLog("KeychainHelper.loadCredentials: Attempting to load credentials")
        
        guard let credentials = load(key: "ra_credentials") else {
            NSLog("KeychainHelper.loadCredentials: No credentials found in keychain")
            return nil
        }
        
        NSLog("KeychainHelper.loadCredentials: Found credentials: \(credentials)")
        
        guard let separatorIndex = credentials.firstIndex(of: ":") else {
            NSLog("KeychainHelper.loadCredentials: No separator found in credentials")
            return nil
        }
        
        let username = String(credentials[..<separatorIndex])
        let password = String(credentials[credentials.index(after: separatorIndex)...])
        
        NSLog("KeychainHelper.loadCredentials: Successfully parsed username=\(username)")
        return (username: username, password: password)
    }
    
    static func deleteCredentials() -> Bool {
        return delete(key: "ra_credentials")
    }
}