//
//  ButtonPosition.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import Foundation
import UIKit

/// Represents a button position using normalized coordinates (0.0 - 1.0)
struct ButtonPosition: Codable, Equatable {
    /// X position normalized to screen width (0.0 = left, 1.0 = right)
    var x: CGFloat

    /// Y position normalized to screen height (0.0 = top, 1.0 = bottom)
    var y: CGFloat

    /// Convert to absolute position within given bounds
    /// - Parameter bounds: The container bounds
    /// - Returns: Absolute CGPoint
    func absolutePosition(in bounds: CGRect) -> CGPoint {
        return CGPoint(
            x: bounds.width * x,
            y: bounds.height * y
        )
    }

    /// Create from absolute position
    /// - Parameters:
    ///   - point: The absolute point
    ///   - bounds: The container bounds
    /// - Returns: Normalized ButtonPosition
    static func normalized(from point: CGPoint, in bounds: CGRect) -> ButtonPosition {
        return ButtonPosition(
            x: bounds.width > 0 ? point.x / bounds.width : 0,
            y: bounds.height > 0 ? point.y / bounds.height : 0
        )
    }

    /// Clamp position to valid range [0.0, 1.0]
    /// - Returns: Clamped position
    func clamped() -> ButtonPosition {
        return ButtonPosition(
            x: max(0, min(1, x)),
            y: max(0, min(1, y))
        )
    }

    /// Calculate distance to another position (normalized)
    /// - Parameter other: Other position
    /// - Returns: Distance in normalized coordinates
    func distance(to other: ButtonPosition) -> CGFloat {
        let dx = x - other.x
        let dy = y - other.y
        return sqrt(dx * dx + dy * dy)
    }
}
