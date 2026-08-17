//
//  PadConfigurationStorage.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import Foundation

/// Protocol for pad configuration persistence
protocol PadConfigurationStorage {

    /// Load saved configuration
    /// - Returns: Saved configuration or nil if none exists
    func load() -> PadConfiguration?

    /// Save configuration
    /// - Parameter configuration: Configuration to save
    /// - Returns: True if save was successful
    @discardableResult
    func save(_ configuration: PadConfiguration) -> Bool

    /// Delete saved configuration (reset to default)
    /// - Returns: True if deletion was successful
    @discardableResult
    func delete() -> Bool

    /// Check if custom configuration exists
    var hasCustomConfiguration: Bool { get }
}
