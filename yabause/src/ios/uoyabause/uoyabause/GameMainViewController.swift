//
//  GameMainViewController.swift
//  YabaSnashiro
//
//  Created by Shinya Miyamoto on 2024/07/21.
//  Copyright © 2024 devMiyax. All rights reserved.
//

import Foundation
import UIKit
import FirebaseAuth
import FirebaseFirestore
import GameController



class GameMainViewController: UIViewController
{
    enum MenuState {
        case opened
        case closed
    }

    private var menuState : MenuState = .closed
    
    // GameController support
    private var gameController: GCController?
    private var keyMapper: KeyMapper!

    var selectedFile = ""
    var productNumber: String?
    var gameVC: GameViewController?
    let menuVC = MenuViewController()

     override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
         if let gameVC = segue.destination as? GameViewController {
             self.gameVC = gameVC
             gameVC.selectedFile = self.selectedFile
             gameVC.productNumber = self.productNumber // 追加
             self.gameVC?.gdelegate = self
         }
     }


    override func viewDidLoad() {
        super.viewDidLoad()
        menuVC.delegate = self
        view.backgroundColor = .black
        addChild(menuVC)
        view.addSubview(menuVC.view)
        menuVC.didMove(toParent: self)
        view.sendSubviewToBack(menuVC.view)
        
        // Initialize GameController support
        setupGameController()
    }

    override var supportedInterfaceOrientations: UIInterfaceOrientationMask {
        // landscapeフラグに応じて画面の向きを設定
        let ud = UserDefaults.standard
        let landscape = ud.bool(forKey: "landscape")

        if landscape {
            return .landscape
        } else {
            return .all
        }
    }

    override var prefersHomeIndicatorAutoHidden: Bool {
        return true
    }

    //@available(iOS 11, *)
    override var childForHomeIndicatorAutoHidden: UIViewController? {
        return nil
    }
    
    // MARK: - GameController Support
    
    private func setupGameController() {
        // Initialize KeyMapper
        keyMapper = KeyMapper()
        
        // Check for already connected controllers
        discoverWirelessControllers()
        
        // Set up notifications for controller connection/disconnection
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(controllerDidConnect),
            name: .GCControllerDidConnect,
            object: nil
        )
        
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(controllerDidDisconnect),
            name: .GCControllerDidDisconnect,
            object: nil
        )
    }
    
    private func discoverWirelessControllers() {
        guard GCController.controllers().isEmpty else {
            // Controller already connected
            setupControllerInput(GCController.controllers().first!)
            return
        }
        
        GCController.startWirelessControllerDiscovery { 
            // Discovery completed
        }
    }
    
    @objc private func controllerDidConnect(_ notification: Notification) {
        guard let controller = notification.object as? GCController else { return }
        setupControllerInput(controller)
    }
    
    @objc private func controllerDidDisconnect(_ notification: Notification) {
        gameController = nil
        
        // Notify MenuViewController of controller disconnection
        menuVC.setGameController(nil)
    }
    
    private func setupControllerInput(_ controller: GCController) {
        gameController = controller
        
        // Pass controller to MenuViewController
        if let menuVC = menuVC as? MenuViewController {
            menuVC.setGameController(controller)
        }
        
        // Set up extended gamepad if available
        if let extendedGamepad = controller.extendedGamepad {
            setupExtendedGamepadHandlers(extendedGamepad)
        }
        // Fallback to micro gamepad
        else if let microGamepad = controller.microGamepad {
            setupMicroGamepadHandlers(microGamepad)
        }
    }
    
    private func setupExtendedGamepadHandlers(_ gamepad: GCExtendedGamepad) {
        // Clear all existing handlers first to avoid conflicts
        clearAllGamepadHandlers(gamepad)
        
        // Option button handler for menu toggle (MFI_BUTTON_OPTION)
        gamepad.buttonOptions?.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                DispatchQueue.main.async {
                    self?.handleMenuToggle()
                }
            }
        }
        
        // Set up handlers based on current menu state
        if menuState == .opened {
            setupMenuNavigationHandlers(gamepad)
        } else {
            setupGameHandlers(gamepad)
        }
    }
    
    private func clearAllGamepadHandlers(_ gamepad: GCExtendedGamepad) {
        // Clear all button handlers to prevent conflicts
        gamepad.dpad.up.pressedChangedHandler = nil
        gamepad.dpad.down.pressedChangedHandler = nil
        gamepad.dpad.left.pressedChangedHandler = nil
        gamepad.dpad.right.pressedChangedHandler = nil
        gamepad.buttonA.pressedChangedHandler = nil
        gamepad.buttonB.pressedChangedHandler = nil
        gamepad.buttonX.pressedChangedHandler = nil
        gamepad.buttonY.pressedChangedHandler = nil
    }
    
    private func setupMenuNavigationHandlers(_ gamepad: GCExtendedGamepad) {
        // D-Pad navigation for menu
        gamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed && self?.menuState == .opened {
                DispatchQueue.main.async {
                    self?.menuVC.moveFocusUp()
                }
            }
        }
        
        gamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed && self?.menuState == .opened {
                DispatchQueue.main.async {
                    self?.menuVC.moveFocusDown()
                }
            }
        }
        
        // A button for menu selection
        gamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed && self?.menuState == .opened {
                DispatchQueue.main.async {
                    self?.menuVC.handleAButtonPress()
                }
            }
        }
        
        // B button for menu close
        gamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed && self?.menuState == .opened {
                DispatchQueue.main.async {
                    // Resume game when closing menu with B button
                    self?.gameVC?.isPaused = false
                    self?.toggleMenu(completion: nil)
                }
            }
        }
    }
    
    private func setupGameHandlers(_ gamepad: GCExtendedGamepad) {
        // When menu is closed, only Option button should work
        // Game-specific handlers would go here if needed
        // For now, we don't need any handlers when menu is closed
    }
    
    private func setupMicroGamepadHandlers(_ gamepad: GCMicroGamepad) {
        // Micro gamepad doesn't have option button, so we skip this setup
        // Could potentially use a different button mapping if needed
    }
    
    private func handleMenuToggle() {
        // Handle Option button press to toggle menu
        if menuState == .opened {
            // Resume game when closing menu with Option button
            gameVC?.isPaused = false
        }
        toggleMenu(completion: nil)
    }
    
    // MARK: - UIAlertController GameController Support
    
    private func presentAlertWithGamepadSupport(_ alert: UIAlertController) {
        present(alert, animated: true) { [weak self] in
            self?.setupAlertGamepadSupport(for: alert)
        }
    }
    
    private func setupAlertGamepadSupport(for alert: UIAlertController) {
        guard let controller = gameController,
              let extendedGamepad = controller.extendedGamepad,
              alert.actions.count > 0 else { return }
        
        // Temporarily disable MenuViewController gamepad handlers to avoid conflicts
        if menuState == .opened {
            menuVC.setGameController(nil)
        }
        
        var currentSelectedIndex = 0
        let actions = alert.actions
        
        // Set initial preferred action (first action)
        if let firstAction = actions.first {
            alert.preferredAction = firstAction
        }
        
        // Store original handlers for restoration
        var actionHandlers: [(UIAlertAction) -> Void] = []
        
        // Create new actions with gamepad-aware handlers
        for action in actions {
            let originalTitle = action.title
            let originalStyle = action.style
            
            // Store reference to execute action manually
            actionHandlers.append { _ in
                // Execute based on action title and context
                self.executeAlertAction(title: originalTitle, style: originalStyle, alert: alert)
            }
        }
        
        // D-Pad navigation for alert actions
        extendedGamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                DispatchQueue.main.async {
                    currentSelectedIndex = max(0, currentSelectedIndex - 1)
                    alert.preferredAction = actions[currentSelectedIndex]
                }
            }
        }
        
        extendedGamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                DispatchQueue.main.async {
                    currentSelectedIndex = min(actions.count - 1, currentSelectedIndex + 1)
                    alert.preferredAction = actions[currentSelectedIndex]
                }
            }
        }
        
        // A button for selection
        extendedGamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                DispatchQueue.main.async {
                    if currentSelectedIndex < actionHandlers.count {
                        alert.dismiss(animated: true) {
                            actionHandlers[currentSelectedIndex](actions[currentSelectedIndex])
                            // Restore menu gamepad handlers after action
                            self?.clearAlertGamepadHandlers()
                        }
                    }
                }
            }
        }
        
        // B button for cancel (if cancel action exists)
        extendedGamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
            if pressed {
                DispatchQueue.main.async {
                    // Find cancel action index or use last action
                    if let cancelIndex = actions.firstIndex(where: { $0.style == .cancel }) {
                        alert.dismiss(animated: true) {
                            actionHandlers[cancelIndex](actions[cancelIndex])
                            // Restore menu gamepad handlers after action
                            self?.clearAlertGamepadHandlers()
                        }
                    } else {
                        // No cancel action, just dismiss and restore game if needed
                        alert.dismiss(animated: true) {
                            self?.gameVC?.isPaused = false
                            // Restore menu gamepad handlers
                            self?.clearAlertGamepadHandlers()
                        }
                    }
                }
            }
        }
    }
    
    private func executeAlertAction(title: String?, style: UIAlertAction.Style, alert: UIAlertController) {
        // Handle specific actions based on title and context
        guard let title = title else { return }
        
        switch title {
        case NSLocalizedString("Yes", comment: "Confirm action to exit the application"):
            // Exit confirmation
            enterBackGround()
            exit(0)
        case NSLocalizedString("No", comment: "Cancel the exit action"):
            // Cancel exit - resume game
            gameVC?.isPaused = false
        case "OK":
            // Generic OK action - resume game if paused
            gameVC?.isPaused = false
        default:
            // Default behavior - resume game if it was paused
            if style == .cancel {
                gameVC?.isPaused = false
            }
        }
    }
    
    private func clearAlertGamepadHandlers() {
        guard let controller = gameController,
              let extendedGamepad = controller.extendedGamepad else { return }
        
        // Restore appropriate handlers based on current menu state
        setupExtendedGamepadHandlers(extendedGamepad)
        
        // Ensure MenuViewController has controller reference for visual focus
        if menuState == .opened {
            menuVC.setGameController(controller)
        }
    }
    
    // MARK: - Modal ViewController GameController Support
    
    private func enableModalGamepadSupport(for viewController: UIViewController) {
        guard let controller = gameController,
              let extendedGamepad = controller.extendedGamepad else { return }
        
        // Store original B button handler
        let originalBHandler = extendedGamepad.buttonB.pressedChangedHandler
        
        // Set B button handler for modal dismissal
        extendedGamepad.buttonB.pressedChangedHandler = { [weak self, weak viewController] (button, value, pressed) in
            if pressed {
                DispatchQueue.main.async {
                    if let presentedVC = viewController {
                        presentedVC.dismiss(animated: true) {
                            // Restore original game state and handlers
                            self?.gameVC?.isPaused = false
                            // Restore original B button handler
                            extendedGamepad.buttonB.pressedChangedHandler = originalBHandler
                            // Restore menu handlers since we're returning to game
                            if let controller = self?.gameController,
                               let extendedGamepad = controller.extendedGamepad {
                                self?.setupExtendedGamepadHandlers(extendedGamepad)
                            }
                        }
                    }
                }
            }
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self, name: .GCControllerDidConnect, object: nil)
        NotificationCenter.default.removeObserver(self, name: .GCControllerDidDisconnect, object: nil)
    }
}

extension GameMainViewController: GameViewControllerDelegate {

    func didTapMenuButton() {
        // Animate the menu
        // print("tap the menu")

        if( menuState == .opened ){
            self.gameVC?.isPaused = false
        }
        toggleMenu( completion: nil )
    }
    
    func didPresentModal(_ viewController: UIViewController) {
        // Enable B button support for the presented modal
        enableModalGamepadSupport(for: viewController)
    }
    
    func didDismissModal() {
        // Modal was dismissed, restore normal game handlers
        if let controller = gameController,
           let extendedGamepad = controller.extendedGamepad {
            setupExtendedGamepadHandlers(extendedGamepad)
        }
    }

    func toggleMenu( completion: (() -> Void)? ){
        switch menuState {
        case .closed:

            self.gameVC?.isPaused = true

            // open it
            UIView.animate(withDuration: 0.5, delay: 0, usingSpringWithDamping: 0.8, initialSpringVelocity: 0, options: .curveEaseInOut) {

                self.gameVC?.view.frame.origin.x = self.menuVC.optimalWidth

            }completion:{ [weak self] done in
                if done {
                    self?.menuState = .opened
                    // Reconfigure gamepad handlers for menu navigation
                    if let controller = self?.gameController,
                       let extendedGamepad = controller.extendedGamepad {
                        self?.setupExtendedGamepadHandlers(extendedGamepad)
                    }
                    completion?()
                }
            }

        case .opened:


            // close it
            UIView.animate(withDuration: 0.5, delay: 0, usingSpringWithDamping: 0.8, initialSpringVelocity: 0, options: .curveEaseInOut) {

                self.gameVC?.view.frame.origin.x = 0

            }completion:{ [weak self] done in
                if done {
                    self?.menuState = .closed
                    // Reconfigure gamepad handlers for game mode
                    if let controller = self?.gameController,
                       let extendedGamepad = controller.extendedGamepad {
                        self?.setupExtendedGamepadHandlers(extendedGamepad)
                    }
                    completion?()
                }
            }
        }
    }
}


extension GameMainViewController: MenuViewControllerDelegate {
    func didSelect(menuItem: MenuViewController.MenuOptions) {
        toggleMenu { [weak self] in

            var doNotPause = false
            switch menuItem {
            case .exit:

                // アラートコントローラの作成
                let alertController = UIAlertController(
                    title: NSLocalizedString("Exit Confirmation", comment: "Title for exit confirmation alert"),
                    message: NSLocalizedString("Are you sure you want to exit?", comment: "Message asking if the user is sure about exiting"),
                    preferredStyle: .alert
                )

                // "Yes"ボタンの追加
                let yesAction = UIAlertAction(
                    title: NSLocalizedString("Yes", comment: "Confirm action to exit the application"),
                    style: .default
                ) { action in

                    enterBackGround()
                    exit(0)
                }
                alertController.addAction(yesAction)

                // "No"ボタンの追加
                let noAction = UIAlertAction(
                    title: NSLocalizedString("No", comment: "Cancel the exit action"),
                    style: .default
                ) { action in
                    self?.gameVC?.isPaused = false
                }
                alertController.addAction(noAction)


                self?.presentAlertWithGamepadSupport(alertController)
                doNotPause = true
                break
            case .reset:
                self?.gameVC?.reset()
                break
            case .changeDisk:
                self?.gameVC?.presentFileSelectViewController()
                doNotPause = true
                break
            case .saveState:
                self?.gameVC?.saveState()
                break
            case .loadState:
                // Check if hardcore mode is enabled
                if let raManager = RetroAchievementsManager.shared,
                   raManager.getHardcoreEnabled() {
                    // Show alert that load state is disabled in hardcore mode
                    let alert = UIAlertController(
                        title: NSLocalizedString("Hardcore Mode Active", comment: ""),
                        message: NSLocalizedString("Loading save states is disabled in hardcore mode. Disable hardcore mode in RetroAchievements settings to use this feature.", comment: ""),
                        preferredStyle: .alert
                    )
                    alert.addAction(UIAlertAction(title: "OK", style: .default) { _ in
                        self?.gameVC?.isPaused = false
                    })
                    self?.presentAlertWithGamepadSupport(alert)
                    doNotPause = true
                } else {
                    self?.gameVC?.loadState()
                }
                break
            case .analogMode:
                break
            case .controllerSetting:
                self?.gameVC?.toggleControllSetting()
                break
            case .backupManager:
                self?.gameVC?.presentBackupFileListViewController()
                doNotPause = true
                break
            case .cheat:
                // Check if hardcore mode is enabled first
                if let raManager = RetroAchievementsManager.shared,
                   raManager.getHardcoreEnabled() {
                    // Show alert that cheats are disabled in hardcore mode
                    let alert = UIAlertController(
                        title: NSLocalizedString("Hardcore Mode Active", comment: ""),
                        message: NSLocalizedString("Cheats are disabled in hardcore mode. Disable hardcore mode in RetroAchievements settings to use this feature.", comment: ""),
                        preferredStyle: .alert
                    )
                    alert.addAction(UIAlertAction(title: "OK", style: .default) { _ in
                        self?.gameVC?.isPaused = false
                    })
                    self?.presentAlertWithGamepadSupport(alert)
                    doNotPause = true
                } else {
                    // ログイン状態を確認
                    if let user = Auth.auth().currentUser {
                        // ログイン済みの場合、チート画面を表示
                        self?.gameVC?.presentCheatViewController()
                        doNotPause = true
                    } else {
                        // 未ログインの場合、ログイン画面を表示
                        let loginVC = LoginViewController()
                        let navController = UINavigationController(rootViewController: loginVC)
                        navController.modalPresentationStyle = .pageSheet
                        loginVC.modalPresentationStyle = .fullScreen
                        loginVC.completionHandler = { [weak self] success in
                            if success {
                                // ログイン成功後にチート画面を表示
                                self?.gameVC?.presentCheatViewController()

                            }else{
                                self?.gameVC?.isPaused = false
                            }
                        }
                        self?.present(navController, animated: true, completion: nil)
                        doNotPause = true

                    }
                }
                break
            case .report:
                // ゲームコードを取得
                let productionNumber = YSGetCurrentGameCode() ?? ""

                // ReportDialogを表示
                let reportDialog = ReportDialog(productionNumber: productionNumber)
                reportDialog.completionHandler = { [weak self] (rating, message, screenshot) in
                    self?.gameVC?.isPaused = false
                }

                let navController = UINavigationController(rootViewController: reportDialog)
                navController.modalPresentationStyle = .pageSheet
                self?.present(navController, animated: true, completion: nil)
                
                // Enable B button support for modal dismissal
                self?.enableModalGamepadSupport(for: navController)
                
                doNotPause = true
                break
            case .leaderBoard:
                // ログイン状態を確認
                if let user = Auth.auth().currentUser {
                    // ログイン済みの場合、リーダーボード画面を表示
                    self?.gameVC?.presentLeaderBoardViewController()
                    doNotPause = true
                } else {
                    // 未ログインの場合、ログイン画面を表示
                    let loginVC = LoginViewController()
                    let navController = UINavigationController(rootViewController: loginVC)
                    navController.modalPresentationStyle = .pageSheet
                    loginVC.modalPresentationStyle = .fullScreen
                    loginVC.completionHandler = { [weak self] success in
                        if success {
                            // ログイン成功後にリーダーボード画面を表示
                            self?.gameVC?.presentLeaderBoardViewController()
                        } else {
                            self?.gameVC?.isPaused = false
                        }
                    }
                    self?.present(navController, animated: true, completion: nil)
                    doNotPause = true
                }
                break
            case .achievements:
                // Achievement List View Controller を表示
                let achievementListVC = AchievementListViewController()
                let navController = UINavigationController(rootViewController: achievementListVC)
                navController.modalPresentationStyle = .pageSheet
                self?.present(navController, animated: true, completion: nil)
                
                // Enable B button support for modal dismissal
                self?.enableModalGamepadSupport(for: navController)
                
                doNotPause = true
                break
            }

            if doNotPause == false{
                self?.gameVC?.isPaused = false
            }
        }
    }

    func didChangeAnalogMode(to: Bool){

        toggleMenu { [weak self] in
            let plist = SettingsViewController.getSettingPlist();
            plist.setObject(to, forKey: "analog mode" as NSCopying)
            plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
            self?.gameVC?.setAnalogMode(to: to)
            self?.gameVC?.isPaused = false
        }
    }

}
