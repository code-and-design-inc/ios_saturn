//
//  TouchState.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

/// Thread-safe state for tracking active touches per button
final class TouchState {
    private var activeTouches: Set<UITouch> = []
    private let lock = NSLock()

    /// Whether any touch is currently active
    var isPressed: Bool {
        lock.lock()
        defer { lock.unlock() }
        return !activeTouches.isEmpty
    }

    /// Number of active touches
    var touchCount: Int {
        lock.lock()
        defer { lock.unlock() }
        return activeTouches.count
    }

    /// Add a touch to tracking
    /// - Parameter touch: The touch to add
    /// - Returns: True if this is the first touch (button just pressed)
    @discardableResult
    func addTouch(_ touch: UITouch) -> Bool {
        lock.lock()
        let wasEmpty = activeTouches.isEmpty
        activeTouches.insert(touch)
        lock.unlock()
        return wasEmpty
    }

    /// Remove a touch from tracking
    /// - Parameter touch: The touch to remove
    /// - Returns: True if this was the last touch (button just released)
    @discardableResult
    func removeTouch(_ touch: UITouch) -> Bool {
        lock.lock()
        activeTouches.remove(touch)
        let isEmpty = activeTouches.isEmpty
        lock.unlock()
        return isEmpty
    }

    /// Clear all active touches
    func clearAllTouches() {
        lock.lock()
        activeTouches.removeAll()
        lock.unlock()
    }

    /// Check if a specific touch is being tracked
    /// - Parameter touch: The touch to check
    /// - Returns: True if the touch is currently tracked
    func containsTouch(_ touch: UITouch) -> Bool {
        lock.lock()
        defer { lock.unlock() }
        return activeTouches.contains(touch)
    }

    /// Get all currently tracked touches
    var allTouches: Set<UITouch> {
        lock.lock()
        defer { lock.unlock() }
        return activeTouches
    }
}
