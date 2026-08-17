//
//  PadTheme.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import Foundation
import UIKit

/// Available pad visual themes
enum PadTheme: String, Codable, CaseIterable {
    case classic = "classic"
    case modern = "modern"

    /// Display name for UI
    var displayName: String {
        switch self {
        case .classic:
            return NSLocalizedString("Classic", comment: "Classic pad theme")
        case .modern:
            return NSLocalizedString("Modern", comment: "Modern pad theme")
        }
    }

    /// Asset name suffix for this theme
    var assetSuffix: String {
        switch self {
        case .classic:
            return ""
        case .modern:
            return "_modern"
        }
    }

    /// Get the image for a pad component
    func image(for component: PadComponent) -> UIImage? {
        let baseName = component.assetBaseName
        let fullName = baseName + assetSuffix
        return UIImage(named: fullName)
    }
}

/// Pad visual components that have theme-specific images
enum PadComponent: String, CaseIterable {
    case left = "pad_l"
    case middle = "pad_m"
    case right = "pad_r"
    case topLeft = "pad_top_l"
    case topRight = "pad_top_r"

    var assetBaseName: String {
        return rawValue
    }
}
