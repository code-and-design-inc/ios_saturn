//
//  PadButtons+Extensions.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import Foundation

// MARK: - PadButtons Extensions

extension PadButtons: CaseIterable, Codable {

    public static var allCases: [PadButtons] {
        return [.up, .right, .down, .left, .rightTrigger, .leftTrigger,
                .start, .a, .b, .c, .x, .y, .z, .last]
    }

    /// Human-readable name for UI display
    var displayName: String {
        switch self {
        case .up: return "Up"
        case .right: return "Right"
        case .down: return "Down"
        case .left: return "Left"
        case .rightTrigger: return "R"
        case .leftTrigger: return "L"
        case .start: return "Start"
        case .a: return "A"
        case .b: return "B"
        case .c: return "C"
        case .x: return "X"
        case .y: return "Y"
        case .z: return "Z"
        case .last: return ""
        @unknown default: return ""
        }
    }

    /// Buttons that can be positioned (excludes D-PAD directions and last)
    static var positionableButtons: [PadButtons] {
        return [.rightTrigger, .leftTrigger, .start, .a, .b, .c, .x, .y, .z]
    }

    /// D-PAD direction buttons
    static var dpadButtons: [PadButtons] {
        return [.up, .right, .down, .left]
    }

    /// Action buttons (A/B/C, X/Y/Z)
    static var actionButtons: [PadButtons] {
        return [.a, .b, .c, .x, .y, .z]
    }

    /// Trigger buttons (L/R)
    static var triggerButtons: [PadButtons] {
        return [.leftTrigger, .rightTrigger]
    }
}
