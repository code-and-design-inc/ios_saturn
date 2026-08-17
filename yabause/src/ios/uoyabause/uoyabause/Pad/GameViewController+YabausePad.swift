//
//  GameViewController+YabausePad.swift
//  YabaSnashiro
//
//  Created by Claude Code on 2024/12/21.
//  Copyright 2024 devMiyax. All rights reserved.
//

import UIKit

// MARK: - YabausePadDelegate Implementation

extension GameViewController: YabausePadDelegate {

    /// Called when a button is pressed on the YabausePad
    func padView(_ padView: YabausePadView, didPress button: PadButtons) {
        // Don't process touch input if a controller is connected
        guard !hasControllerConnected() else { return }

        PerKeyDown(UInt32(button.rawValue))

        // In analog mode, also send trigger analog values
        if padView.isAnalogMode {
            switch button {
            case .leftTrigger:
                PerAxisValue(PERANALOG_AXIS_LTRIGGER, 255)
            case .rightTrigger:
                PerAxisValue(PERANALOG_AXIS_RTRIGGER, 255)
            default:
                break
            }
        }
    }

    /// Called when a button is released on the YabausePad
    func padView(_ padView: YabausePadView, didRelease button: PadButtons) {
        // Don't process touch input if a controller is connected
        guard !hasControllerConnected() else { return }

        PerKeyUp(UInt32(button.rawValue))

        // In analog mode, also send trigger analog values
        if padView.isAnalogMode {
            switch button {
            case .leftTrigger:
                PerAxisValue(PERANALOG_AXIS_LTRIGGER, 0)
            case .rightTrigger:
                PerAxisValue(PERANALOG_AXIS_RTRIGGER, 0)
            default:
                break
            }
        }
    }

    /// Called when analog stick position changes (only in analog mode)
    func padView(_ padView: YabausePadView, didChangeAnalog x: UInt8, y: UInt8) {
        // Don't process touch input if a controller is connected
        guard !hasControllerConnected() else { return }

        PerAxisValue(PERANALOG_AXIS_X, x)
        PerAxisValue(PERANALOG_AXIS_Y, y)
    }

    /// Called when trigger value changes in analog mode
    func padView(_ padView: YabausePadView, didChangeTrigger trigger: PadButtons, value: UInt8) {
        // Don't process touch input if a controller is connected
        guard !hasControllerConnected() else { return }

        switch trigger {
        case .leftTrigger:
            PerAxisValue(PERANALOG_AXIS_LTRIGGER, value)
        case .rightTrigger:
            PerAxisValue(PERANALOG_AXIS_RTRIGGER, value)
        default:
            break
        }
    }
}

// MARK: - YabausePad Setup

extension GameViewController {

    /// Property to hold the YabausePadView instance
    /// Uses associated object pattern since we're extending an existing class
    private struct AssociatedKeys {
        static var yabausePadView = "yabausePadViewKey"
        static var useNewPad = "useNewPadKey"
    }

    /// The YabausePadView instance (if using new pad system)
    var yabausePadView: YabausePadView? {
        get {
            return objc_getAssociatedObject(self, &AssociatedKeys.yabausePadView) as? YabausePadView
        }
        set {
            objc_setAssociatedObject(self, &AssociatedKeys.yabausePadView, newValue, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
        }
    }

    /// Whether to use the new YabausePad system
    var useNewPadSystem: Bool {
        get {
            return (objc_getAssociatedObject(self, &AssociatedKeys.useNewPad) as? Bool) ?? false
        }
        set {
            objc_setAssociatedObject(self, &AssociatedKeys.useNewPad, newValue, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
        }
    }

    /// Setup the new YabausePadView
    /// Call this from viewDidLoad after setting up other UI elements
    func setupYabausePadView() {
        // Load configuration
        let config = PadConfiguration.loadOrDefault()

        // Create the pad view
        let padView = YabausePadView(frame: view.bounds, configuration: config)
        padView.delegate = self
        padView.autoresizingMask = [.flexibleWidth, .flexibleHeight]

        // Add to view hierarchy (behind menu button but above game view)
        if let menuButton = menuButton {
            view.insertSubview(padView, belowSubview: menuButton)
        } else {
            view.addSubview(padView)
        }

        yabausePadView = padView
        useNewPadSystem = true

        // Hide old UI elements when using new pad
        hideOldPadViews()
    }

    /// Hide the old storyboard-based pad views
    private func hideOldPadViews() {
        leftButton?.isHidden = true
        rightButton?.isHidden = true
        upButton?.isHidden = true
        downButton?.isHidden = true
        aButton?.isHidden = true
        bButton?.isHidden = true
        cButton?.isHidden = true
        xButton?.isHidden = true
        yButton?.isHidden = true
        zButton?.isHidden = true
        leftTrigger?.isHidden = true
        rightTrigger?.isHidden = true
        startButton?.isHidden = true
        leftPanel?.isHidden = true
        rightPanel?.isHidden = true
        leftView?.isHidden = true
        rightView?.isHidden = true
        startView?.isHidden = true
    }

    /// Show/hide the YabausePadView based on controller connection status
    /// - Parameter forceHidden: If true, always hide the pad regardless of controller status
    func updatePadVisibility(forceHidden: Bool = false) {
        if useNewPadSystem {
            let hideOnScreenControls = hasControllerConnected() || forceHidden
            yabausePadView?.isHidden = hideOnScreenControls
        }
    }

    /// Enter pad layout edit mode
    func enterPadEditMode() {
        guard useNewPadSystem, let padView = yabausePadView else { return }
        padView.isEditing = true
    }

    /// Exit pad layout edit mode and save
    func exitPadEditMode(save: Bool) {
        guard useNewPadSystem, let padView = yabausePadView else { return }
        padView.isEditing = false

        if save {
            padView.saveConfiguration()
        } else {
            // Reload original configuration
            padView.updateConfiguration(PadConfiguration.loadOrDefault())
        }
    }

    /// Reset pad layout to defaults
    func resetPadLayout() {
        guard useNewPadSystem, let padView = yabausePadView else { return }
        padView.resetToDefault()
        PadConfiguration.resetToDefault()
    }

    /// Sync analog mode state from GameViewController to YabausePadView
    /// Call this when setAnalogMode is called
    func syncAnalogModeToYabausePad(_ isAnalog: Bool) {
        guard useNewPadSystem, let padView = yabausePadView else { return }
        padView.isAnalogMode = isAnalog
    }
}

// MARK: - Legacy Touch Handling Bridge

extension GameViewController {

    /// Determine whether to use new pad system for touch handling
    /// Returns true if new system should handle this touch
    func shouldUseNewPadForTouch(_ touch: UITouch, in view: UIView) -> Bool {
        guard useNewPadSystem, let padView = yabausePadView else {
            return false
        }

        // Check if touch is within the pad view
        let location = touch.location(in: padView)
        return padView.bounds.contains(location)
    }
}
