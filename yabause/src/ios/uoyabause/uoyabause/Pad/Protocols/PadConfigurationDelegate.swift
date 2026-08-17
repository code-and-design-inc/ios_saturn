//
//  PadConfigurationDelegate.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

/// Delegate protocol for the pad configuration editor
/// Note: Pure Swift protocol (not @objc) because ButtonPosition is a Swift struct
protocol PadConfigurationDelegate: AnyObject {

    /// Called when button position is updated during editing
    /// - Parameters:
    ///   - button: The button being moved
    ///   - position: New position (normalized coordinates)
    func didUpdateButtonPosition(_ button: PadButtons, to position: ButtonPosition)

    /// Called when D-PAD position is updated during editing
    /// - Parameter position: New position (normalized coordinates)
    func didUpdateDPadPosition(to position: ButtonPosition)

    /// Called when opacity is changed
    /// - Parameter opacity: New opacity value (0.0 - 1.0)
    func didUpdateOpacity(_ opacity: Float)

    /// Called when skin is changed
    /// - Parameter skinId: New skin identifier
    func didSelectSkin(_ skinId: String)

    /// Called when user requests reset to default
    func didRequestReset()

    /// Called when user saves changes
    func didSaveConfiguration()

    /// Called when user cancels editing
    func didCancelEditing()
}
