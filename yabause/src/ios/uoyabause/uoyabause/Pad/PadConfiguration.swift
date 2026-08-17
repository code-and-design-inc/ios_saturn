//
//  PadConfiguration.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import Foundation
import UIKit

/// Configuration for the on-screen pad layout and appearance
struct PadConfiguration: Codable, Equatable {
    /// Button positions (normalized coordinates)
    /// Key: PadButtons.rawValue as String
    var buttonPositions: [String: ButtonPosition]

    /// D-PAD center position (normalized)
    var dpadPosition: ButtonPosition

    /// Pad opacity (0.0 - 1.0)
    var opacity: Float

    /// Selected theme
    var theme: PadTheme

    /// D-PAD dead zone ratio (0.1 - 0.3)
    var dpadDeadZoneRatio: Float

    /// Thumb touch radius for hit detection (5 - 50 points)
    var thumbTouchRadius: Float

    /// Pad scale (0.2 - 0.5)
    var padScale: Float

    /// Analog stick max distance (20 - 100 points)
    var analogStickMaxDistance: Float

    // MARK: - Initialization

    init(buttonPositions: [String: ButtonPosition],
         dpadPosition: ButtonPosition,
         opacity: Float,
         theme: PadTheme,
         dpadDeadZoneRatio: Float,
         thumbTouchRadius: Float,
         padScale: Float,
         analogStickMaxDistance: Float) {
        self.buttonPositions = buttonPositions
        self.dpadPosition = dpadPosition
        self.opacity = opacity
        self.theme = theme
        self.dpadDeadZoneRatio = dpadDeadZoneRatio
        self.thumbTouchRadius = thumbTouchRadius
        self.padScale = padScale
        self.analogStickMaxDistance = analogStickMaxDistance
    }

    // MARK: - Convenience Accessors

    /// Get position for a specific button
    func position(for button: PadButtons) -> ButtonPosition? {
        return buttonPositions[String(button.rawValue)]
    }

    /// Set position for a specific button
    mutating func setPosition(_ position: ButtonPosition, for button: PadButtons) {
        buttonPositions[String(button.rawValue)] = position
    }

    // MARK: - Defaults

    /// Default configuration for landscape orientation
    static var `default`: PadConfiguration {
        var positions: [String: ButtonPosition] = [:]

        // Default positions (landscape orientation)
        // Left side: L trigger
        positions[String(PadButtons.leftTrigger.rawValue)] = ButtonPosition(x: 0.15, y: 0.15)

        // Right side: Action buttons (diamond layout)
        positions[String(PadButtons.a.rawValue)] = ButtonPosition(x: 0.85, y: 0.70)
        positions[String(PadButtons.b.rawValue)] = ButtonPosition(x: 0.78, y: 0.80)
        positions[String(PadButtons.c.rawValue)] = ButtonPosition(x: 0.92, y: 0.80)
        positions[String(PadButtons.x.rawValue)] = ButtonPosition(x: 0.85, y: 0.55)
        positions[String(PadButtons.y.rawValue)] = ButtonPosition(x: 0.78, y: 0.65)
        positions[String(PadButtons.z.rawValue)] = ButtonPosition(x: 0.92, y: 0.65)

        // Right side: R trigger
        positions[String(PadButtons.rightTrigger.rawValue)] = ButtonPosition(x: 0.85, y: 0.15)

        // Center bottom: Start
        positions[String(PadButtons.start.rawValue)] = ButtonPosition(x: 0.50, y: 0.90)

        return PadConfiguration(
            buttonPositions: positions,
            dpadPosition: ButtonPosition(x: 0.15, y: 0.70),
            opacity: 0.2,
            theme: .classic,
            dpadDeadZoneRatio: 0.15,
            thumbTouchRadius: 60,
            padScale: 0.25,
            analogStickMaxDistance: 55
        )
    }

    // MARK: - Validation

    /// Validate and fix configuration
    mutating func validate() {
        // Clamp opacity
        opacity = max(0, min(1, opacity))

        // Clamp dead zone ratio (0.1 - 0.3)
        dpadDeadZoneRatio = max(0.1, min(0.3, dpadDeadZoneRatio))

        // Clamp thumb touch radius (40 - 800 points)
        thumbTouchRadius = max(40, min(80, thumbTouchRadius))

        // Clamp pad scale (0.2 - 0.5)
        padScale = max(0.2, min(0.35, padScale))

        // Clamp analog stick max distance (20 - 100 points)
        analogStickMaxDistance = max(20, min(100, analogStickMaxDistance))

        // Clamp all positions
        dpadPosition = dpadPosition.clamped()
        for (key, position) in buttonPositions {
            buttonPositions[key] = position.clamped()
        }

        // Ensure all positionable buttons have positions
        for button in PadButtons.positionableButtons {
            let key = String(button.rawValue)
            if buttonPositions[key] == nil {
                buttonPositions[key] = PadConfiguration.default.buttonPositions[key]
            }
        }
    }

    // MARK: - Persistence Keys

    static let userDefaultsKey = "YabausePadConfiguration"

    // MARK: - Codable

    enum CodingKeys: String, CodingKey {
        case buttonPositions, dpadPosition, opacity, theme
        case dpadDeadZoneRatio, thumbTouchRadius, padScale, analogStickMaxDistance
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        buttonPositions = try container.decode([String: ButtonPosition].self, forKey: .buttonPositions)
        dpadPosition = try container.decode(ButtonPosition.self, forKey: .dpadPosition)
        opacity = try container.decode(Float.self, forKey: .opacity)
        // Provide default value for backward compatibility
        theme = try container.decodeIfPresent(PadTheme.self, forKey: .theme) ?? .classic
        dpadDeadZoneRatio = try container.decode(Float.self, forKey: .dpadDeadZoneRatio)
        thumbTouchRadius = try container.decode(Float.self, forKey: .thumbTouchRadius)
        // Provide default value for backward compatibility
        padScale = try container.decodeIfPresent(Float.self, forKey: .padScale) ?? 0.3
        analogStickMaxDistance = try container.decodeIfPresent(Float.self, forKey: .analogStickMaxDistance) ?? 55
    }
}
