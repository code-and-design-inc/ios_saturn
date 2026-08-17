//
//  UserDefaultsPadConfigurationStorage.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import Foundation

/// Default implementation using UserDefaults
final class UserDefaultsPadConfigurationStorage: PadConfigurationStorage {

    private let userDefaults: UserDefaults
    private let key = PadConfiguration.userDefaultsKey

    init(userDefaults: UserDefaults = .standard) {
        self.userDefaults = userDefaults
    }

    func load() -> PadConfiguration? {
        guard let data = userDefaults.data(forKey: key) else {
            return nil
        }

        do {
            let decoder = JSONDecoder()
            var config = try decoder.decode(PadConfiguration.self, from: data)
            config.validate()
            return config
        } catch {
            print("Failed to decode PadConfiguration: \(error)")
            return nil
        }
    }

    @discardableResult
    func save(_ configuration: PadConfiguration) -> Bool {
        do {
            let encoder = JSONEncoder()
            let data = try encoder.encode(configuration)
            userDefaults.set(data, forKey: key)
            return true
        } catch {
            print("Failed to encode PadConfiguration: \(error)")
            return false
        }
    }

    @discardableResult
    func delete() -> Bool {
        userDefaults.removeObject(forKey: key)
        return true
    }

    var hasCustomConfiguration: Bool {
        return userDefaults.data(forKey: key) != nil
    }
}

// MARK: - PadConfiguration Convenience Extensions

extension PadConfiguration {

    private static var storage: PadConfigurationStorage {
        return UserDefaultsPadConfigurationStorage()
    }

    /// Load configuration from default storage
    static func load() -> PadConfiguration? {
        return storage.load()
    }

    /// Load configuration or return default
    static func loadOrDefault() -> PadConfiguration {
        return load() ?? .default
    }

    /// Save this configuration to default storage
    @discardableResult
    func save() -> Bool {
        return Self.storage.save(self)
    }

    /// Reset to default configuration
    static func resetToDefault() {
        _ = storage.delete()
    }

    /// Check if custom configuration exists
    static var hasCustomConfiguration: Bool {
        return storage.hasCustomConfiguration
    }
}
