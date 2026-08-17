//
//  YabausePadDelegate.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

// Note: YabausePadView is defined in YabausePadView.swift

/// Delegate protocol for receiving pad input events
/// Implementers should forward these events to the emulator core via PerKeyDown/PerKeyUp
protocol YabausePadDelegate: AnyObject {

    /// Called when a button is pressed
    /// - Parameters:
    ///   - padView: The pad view that sent the event
    ///   - button: The button that was pressed
    /// - Note: For D-PAD, separate events are sent for each direction (up, down, left, right)
    func padView(_ padView: YabausePadView, didPress button: PadButtons)

    /// Called when a button is released
    /// - Parameters:
    ///   - padView: The pad view that sent the event
    ///   - button: The button that was released
    func padView(_ padView: YabausePadView, didRelease button: PadButtons)

    /// Called when analog stick position changes (only in analog mode)
    /// - Parameters:
    ///   - padView: The pad view that sent the event
    ///   - x: X axis value (0-255, 128 = center)
    ///   - y: Y axis value (0-255, 128 = center)
    func padView(_ padView: YabausePadView, didChangeAnalog x: UInt8, y: UInt8)

    /// Called when trigger value changes in analog mode
    /// - Parameters:
    ///   - padView: The pad view that sent the event
    ///   - trigger: The trigger button (leftTrigger or rightTrigger)
    ///   - value: Analog value (0-255)
    func padView(_ padView: YabausePadView, didChangeTrigger trigger: PadButtons, value: UInt8)
}

/// Default implementation for optional analog methods
extension YabausePadDelegate {
    func padView(_ padView: YabausePadView, didChangeAnalog x: UInt8, y: UInt8) {
        // Default empty implementation
    }

    func padView(_ padView: YabausePadView, didChangeTrigger trigger: PadButtons, value: UInt8) {
        // Default empty implementation
    }
}
