//
//  DPadState.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import Foundation

/// Represents the current state of the D-PAD
struct DPadState: Equatable {
    var left: Bool = false
    var right: Bool = false
    var up: Bool = false
    var down: Bool = false

    /// Returns true if any direction is active
    var isActive: Bool {
        return left || right || up || down
    }

    /// Returns the combined direction as PadButtons array
    var activeButtons: [PadButtons] {
        var buttons: [PadButtons] = []
        if up { buttons.append(.up) }
        if down { buttons.append(.down) }
        if left { buttons.append(.left) }
        if right { buttons.append(.right) }
        return buttons
    }

    /// Neutral state (no direction pressed)
    static let neutral = DPadState()

    /// Create state from angle (in radians) and distance from center
    /// - Parameters:
    ///   - angle: Angle in radians (0 = right, counter-clockwise)
    ///   - normalized: Whether angle is in normalized form
    /// - Returns: DPadState with appropriate directions set
    static func from(angle: Double) -> DPadState {
        // Normalize angle to 0-2*pi range
        var normalizedAngle = angle
        if normalizedAngle < 0 {
            normalizedAngle += 2 * .pi
        }

        // 22.5 degree sectors for 8 directions
        // Each sector is 45 degrees (pi/4), offset by 22.5 degrees (pi/8)
        let sector = Int((normalizedAngle + .pi / 8) / (.pi / 4)) % 8

        var state = DPadState()
        switch sector {
        case 0: // Right
            state.right = true
        case 1: // Right-Down
            state.right = true
            state.down = true
        case 2: // Down
            state.down = true
        case 3: // Left-Down
            state.down = true
            state.left = true
        case 4: // Left
            state.left = true
        case 5: // Left-Up
            state.left = true
            state.up = true
        case 6: // Up
            state.up = true
        case 7: // Right-Up
            state.up = true
            state.right = true
        default:
            break
        }
        return state
    }
}
