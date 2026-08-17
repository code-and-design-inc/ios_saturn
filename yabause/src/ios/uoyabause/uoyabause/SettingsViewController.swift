import UIKit
import GameController

class SettingsViewController: UITableViewController {
    
    // MARK: - GameController Support
    private var gameController: GCController?
    private var selectedIndexPath: IndexPath = IndexPath(row: 0, section: 0)
    private var isGameControllerMode: Bool = false
    
    // MARK: - Settings Structure
    private enum SettingsSection: Int, CaseIterable {
        case cartridge = 0
        case resolution = 1
        case sound = 2
        case notification = 3
        case display = 4
        case pad = 5

        var title: String {
            switch self {
            case .cartridge:
                return NSLocalizedString("Cartridge", comment: "Settings section title for cartridge")
            case .resolution:
                return NSLocalizedString("Resolution", comment: "Settings section title for resolution")
            case .sound:
                return NSLocalizedString("Sound", comment: "Settings section title for sound")
            case .notification:
                return NSLocalizedString("Notification", comment: "Settings section title for notification")
            case .display:
                return NSLocalizedString("Display", comment: "Settings section title for display")
            case .pad:
                return NSLocalizedString("Pad", comment: "Settings section title for pad")
            }
        }
    }

    private enum PadRow: Int, CaseIterable {
        case padSettings = 0

        var title: String {
            switch self {
            case .padSettings:
                return NSLocalizedString("Pad Settings", comment: "Settings row title for pad settings")
            }
        }
    }
    
    private enum CartridgeRow: Int, CaseIterable {
        case cartridgeType = 0

        var title: String {
            switch self {
            case .cartridgeType:
                return NSLocalizedString("Cartridge Type", comment: "Settings row title for cartridge type")
            }
        }
    }
    
    private enum ResolutionRow: Int, CaseIterable {
        case renderingResolution = 0

        var title: String {
            switch self {
            case .renderingResolution:
                return NSLocalizedString("Rendering Resolution", comment: "Settings row title for rendering resolution")
            }
        }
    }
    
    private enum SoundRow: Int, CaseIterable {
        case soundQuality = 0

        var title: String {
            switch self {
            case .soundQuality:
                return NSLocalizedString("Sound Quality", comment: "Settings row title for sound quality")
            }
        }
    }
    
    private enum NotificationRow: Int, CaseIterable {
        case notificationSettings = 0

        var title: String {
            switch self {
            case .notificationSettings:
                return NSLocalizedString("Notification Settings", comment: "Settings row title for notification settings")
            }
        }
    }
    
    private enum DisplayRow: Int, CaseIterable {
        case showFPS = 0
        case frameSkip = 1
        case keepAspectRatio = 2
        case rotateScreen = 3
        case landscapeMode = 4
        case analogAsDPad = 5

        var title: String {
            switch self {
            case .showFPS:
                return NSLocalizedString("Show FPS", comment: "Settings row title for showing FPS")
            case .frameSkip:
                return NSLocalizedString("Frame Skip", comment: "Settings row title for frame skip")
            case .keepAspectRatio:
                return NSLocalizedString("Keep Aspect Ratio", comment: "Settings row title for keeping aspect ratio")
            case .rotateScreen:
                return NSLocalizedString("Rotate Screen", comment: "Settings row title for rotating screen")
            case .landscapeMode:
                return NSLocalizedString("Landscape Mode", comment: "Settings row title for landscape mode")
            case .analogAsDPad:
                return NSLocalizedString("Analog as D-Pad", comment: "Settings row title for using analog stick as D-Pad")
            }
        }
    }
    
    // MARK: - Settings Data
    private let cartridgeOptions = [
        NSLocalizedString("None", comment: "No backup RAM"),
        NSLocalizedString("4Mbit BackupRam", comment: "4 megabit backup RAM"),
        NSLocalizedString("8Mbit BackupRam", comment: "8 megabit backup RAM"),
        NSLocalizedString("16Mbit BackupRam", comment: "16 megabit backup RAM"),
        NSLocalizedString("32Mbit BackupRam", comment: "32 megabit backup RAM"),
        NSLocalizedString("8Mbit DRAM", comment: "8 megabit DRAM"),
        NSLocalizedString("32Mbit DRAM", comment: "32 megabit DRAM")
    ]
    private let cartridgeValues: [Int] = [0, 2, 3, 4, 5, 6, 7]
    
    private let resolutionOptions = [
        NSLocalizedString("Native", comment: "Resolution option for native resolution"),
        NSLocalizedString("4x", comment: "Resolution option for 4 times the native resolution"),
        NSLocalizedString("2x", comment: "Resolution option for 2 times the native resolution"),
        NSLocalizedString("Original", comment: "Resolution option for original resolution")
    ]
    private let resolutionValues: [Int] = [0, 1, 2, 3]
    
    private let soundOptions = [
        NSLocalizedString("High Quality but slow", comment: "Sound option for high quality but slower performance"),
        NSLocalizedString("Low Quality but fast", comment: "Sound option for lower quality but faster performance")
    ]
    private let soundValues: [Int] = [1, 0]
    
    // MARK: - Lifecycle
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupGameController()
        loadSettings()
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        tableView.reloadData()
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        print("SettingsViewController: viewWillDisappear - clearing GameController handlers")
        
        // GameControllerハンドラーを完全にクリア
        clearAllGameControllerHandlers()
    }
    
    // MARK: - Setup
    private func setupUI() {
        title = NSLocalizedString("Settings", comment: "Settings screen title")
        
        // Add close button for modal presentation
        if let navigationController = navigationController,
           navigationController.viewControllers.first == self {
            navigationItem.leftBarButtonItem = UIBarButtonItem(
                barButtonSystemItem: .close,
                target: self,
                action: #selector(closeButtonTapped)
            )
        }
        
        // Background color
        view.backgroundColor = .defaultBackground
        tableView.backgroundColor = .defaultBackground
        tableView.separatorColor = .colorAccent
        
        // Register cell
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "SettingsCell")
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "SwitchCell")
    }
    
    @objc private func closeButtonTapped() {
        // UserProfileViewControllerと同じように直接dismissを呼ぶ
        if let navigationController = navigationController {
            navigationController.dismiss(animated: true, completion: nil)
        } else {
            dismiss(animated: true, completion: nil)
        }
    }
    
    private func setupGameController() {
        // Existing game controller setup code
        if let controller = GCController.controllers().first {
            gameController = controller
            setupControllerInputHandlers(controller)
        }
        
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
    
    @objc private func controllerDidConnect(_ notification: Notification) {
        guard let controller = notification.object as? GCController else { return }
        gameController = controller
        setupControllerInputHandlers(controller)
    }
    
    @objc private func controllerDidDisconnect(_ notification: Notification) {
        guard let controller = notification.object as? GCController,
              controller == gameController else { return }
        gameController = nil
        isGameControllerMode = false
        
        // Remove gamepad selection visual
        for section in 0..<tableView.numberOfSections {
            for row in 0..<tableView.numberOfRows(inSection: section) {
                let indexPath = IndexPath(row: row, section: section)
                if let cell = tableView.cellForRow(at: indexPath) {
                    cell.backgroundColor = .defaultBackground
                }
            }
        }
    }
    
    private func setupControllerInputHandlers(_ controller: GCController) {
        if let extendedGamepad = controller.extendedGamepad {
            // A button (決定)
            extendedGamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.handleGamepadSelection()
                    }
                }
            }
            
            // B button (戻る)
            extendedGamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        // UserProfileViewControllerと同じように直接dismissを呼ぶ
                        if let navigationController = self?.navigationController {
                            navigationController.dismiss(animated: true, completion: nil)
                        } else {
                            self?.dismiss(animated: true, completion: nil)
                        }
                    }
                }
            }
            
            // Menu button (戻る)
            extendedGamepad.buttonMenu.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        // UserProfileViewControllerと同じように直接dismissを呼ぶ
                        if let navigationController = self?.navigationController {
                            navigationController.dismiss(animated: true, completion: nil)
                        } else {
                            self?.dismiss(animated: true, completion: nil)
                        }
                    }
                }
            }
            
            // D-pad navigation (上下でリスト移動)
            extendedGamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateUp()
                    }
                }
            }
            
            extendedGamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateDown()
                    }
                }
            }
            
            // L/R triggers (セクション間移動)
            extendedGamepad.leftShoulder.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateToPreviousSection()
                    }
                }
            }
            
            extendedGamepad.rightShoulder.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateToNextSection()
                    }
                }
            }
            
            // Left stick as alternative D-pad
            extendedGamepad.leftThumbstick.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateUp()
                    }
                }
            }
            
            extendedGamepad.leftThumbstick.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateDown()
                    }
                }
            }
            
        } else if let microGamepad = controller.microGamepad {
            // A button (決定)
            microGamepad.buttonA.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.handleGamepadSelection()
                    }
                }
            }
            
            // X button (戻る)
            microGamepad.buttonX.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        // UserProfileViewControllerと同じように直接dismissを呼ぶ
                        if let navigationController = self?.navigationController {
                            navigationController.dismiss(animated: true, completion: nil)
                        } else {
                            self?.dismiss(animated: true, completion: nil)
                        }
                    }
                }
            }
            
            // D-pad navigation
            microGamepad.dpad.up.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateUp()
                    }
                }
            }
            
            microGamepad.dpad.down.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.navigateDown()
                    }
                }
            }
        }
        
        // Enable gamepad mode when controller is connected
        isGameControllerMode = true
        updateSelectionVisual()
    }
    
    private func clearAllGameControllerHandlers() {
        print("SettingsViewController: clearAllGameControllerHandlers called")
        
        if let controller = gameController {
            if let extendedGamepad = controller.extendedGamepad {
                // すべてのExtended Gamepadハンドラーをクリア
                extendedGamepad.buttonA.pressedChangedHandler = nil
                extendedGamepad.buttonB.pressedChangedHandler = nil
                extendedGamepad.buttonX.pressedChangedHandler = nil
                extendedGamepad.buttonY.pressedChangedHandler = nil
                extendedGamepad.buttonMenu.pressedChangedHandler = nil
                extendedGamepad.buttonOptions?.pressedChangedHandler = nil
                
                // D-padハンドラーをクリア
                extendedGamepad.dpad.up.pressedChangedHandler = nil
                extendedGamepad.dpad.down.pressedChangedHandler = nil
                extendedGamepad.dpad.left.pressedChangedHandler = nil
                extendedGamepad.dpad.right.pressedChangedHandler = nil
                
                // ショルダーボタンとトリガーもクリア
                extendedGamepad.leftShoulder.pressedChangedHandler = nil
                extendedGamepad.rightShoulder.pressedChangedHandler = nil
                extendedGamepad.leftTrigger.pressedChangedHandler = nil
                extendedGamepad.rightTrigger.pressedChangedHandler = nil
                
                // サムスティックハンドラーもクリア
                extendedGamepad.leftThumbstick.up.pressedChangedHandler = nil
                extendedGamepad.leftThumbstick.down.pressedChangedHandler = nil
                extendedGamepad.leftThumbstick.left.pressedChangedHandler = nil
                extendedGamepad.leftThumbstick.right.pressedChangedHandler = nil
                extendedGamepad.rightThumbstick.up.pressedChangedHandler = nil
                extendedGamepad.rightThumbstick.down.pressedChangedHandler = nil
                extendedGamepad.rightThumbstick.left.pressedChangedHandler = nil
                extendedGamepad.rightThumbstick.right.pressedChangedHandler = nil
                
                print("SettingsViewController: Extended gamepad handlers cleared")
            }
            
            if let microGamepad = controller.microGamepad {
                // すべてのMicro Gamepadハンドラーをクリア
                microGamepad.buttonA.pressedChangedHandler = nil
                microGamepad.buttonX.pressedChangedHandler = nil
                microGamepad.dpad.up.pressedChangedHandler = nil
                microGamepad.dpad.down.pressedChangedHandler = nil
                microGamepad.dpad.left.pressedChangedHandler = nil
                microGamepad.dpad.right.pressedChangedHandler = nil
                
                print("SettingsViewController: Micro gamepad handlers cleared")
            }
        } else {
            print("SettingsViewController: No game controller to clear")
        }
        
        // ゲームコントローラーモードを無効にする
        isGameControllerMode = false
        print("SettingsViewController: GameController handlers completely cleared")
    }
    
    
    private func loadSettings() {
        // Initialize UserDefaults for landscape mode
        let userDefaults = UserDefaults.standard
        userDefaults.register(defaults: ["landscape": true])
    }
    
    // MARK: - Settings Helper Methods
    static func getSettingFilename() -> String {
        let libraryPath = NSSearchPathForDirectoriesInDomains(.libraryDirectory, .userDomainMask, true)[0]
        let filename = "settings.plist"
        let filePath = libraryPath + "/" + filename
        return filePath
    }
    
    static func getSettingPlist() -> NSMutableDictionary {
        let filePath = getSettingFilename()
        let manager = FileManager()
        if !manager.fileExists(atPath: filePath) {
            let bundleFilePath = Bundle.main.path(forResource: "settings", ofType: "plist")
            do {
                try FileManager.default.copyItem(atPath: bundleFilePath!, toPath: filePath)
            } catch let error as NSError {
                print("error occurred, here are the details:\n \(error)")
            }
        }
        let plist = NSMutableDictionary(contentsOfFile: filePath)
        return plist!
    }
    
    // MARK: - Table View Data Source
    override func numberOfSections(in tableView: UITableView) -> Int {
        return SettingsSection.allCases.count
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        guard let settingsSection = SettingsSection(rawValue: section) else { return 0 }

        switch settingsSection {
        case .cartridge:
            return CartridgeRow.allCases.count
        case .resolution:
            return ResolutionRow.allCases.count
        case .sound:
            return SoundRow.allCases.count
        case .notification:
            return NotificationRow.allCases.count
        case .display:
            return DisplayRow.allCases.count
        case .pad:
            return PadRow.allCases.count
        }
    }
    
    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        guard let settingsSection = SettingsSection(rawValue: section) else { return nil }
        return settingsSection.title
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        guard let settingsSection = SettingsSection(rawValue: indexPath.section) else {
            return UITableViewCell()
        }
        
        switch settingsSection {
        case .cartridge:
            return createCartridgeCell(for: indexPath)
        case .resolution:
            return createResolutionCell(for: indexPath)
        case .sound:
            return createSoundCell(for: indexPath)
        case .notification:
            return createNotificationCell(for: indexPath)
        case .display:
            return createDisplayCell(for: indexPath)
        case .pad:
            return createPadCell(for: indexPath)
        }
    }

    // MARK: - Pad Cell Creation
    private func createPadCell(for indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: "SettingsCell")
        cell.textLabel?.text = PadRow.allCases[indexPath.row].title
        cell.accessoryType = .disclosureIndicator
        applyCellStyling(to: cell)
        return cell
    }
    
    // MARK: - Cell Creation Methods
    private func createCartridgeCell(for indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: "SettingsCell")
        cell.textLabel?.text = CartridgeRow.allCases[indexPath.row].title
        cell.accessoryType = .disclosureIndicator
        
        // Get current cartridge setting
        let plist = SettingsViewController.getSettingPlist()
        let cartIndex = plist.value(forKey: "cartridge") as! Int
        
        if let valueIndex = cartridgeValues.firstIndex(of: cartIndex) {
            cell.detailTextLabel?.text = cartridgeOptions[valueIndex]
        }
        
        applyCellStyling(to: cell)
        return cell
    }
    
    private func createResolutionCell(for indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: "SettingsCell")
        cell.textLabel?.text = ResolutionRow.allCases[indexPath.row].title
        cell.accessoryType = .disclosureIndicator
        
        // Get current resolution setting
        let plist = SettingsViewController.getSettingPlist()
        let resolutionIndex = plist.value(forKey: "rendering resolution") as? Int ?? 0
        
        if let valueIndex = resolutionValues.firstIndex(of: resolutionIndex) {
            cell.detailTextLabel?.text = resolutionOptions[valueIndex]
        }
        
        applyCellStyling(to: cell)
        return cell
    }
    
    private func createSoundCell(for indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: "SettingsCell")
        cell.textLabel?.text = SoundRow.allCases[indexPath.row].title
        cell.accessoryType = .disclosureIndicator
        
        // Get current sound setting
        let plist = SettingsViewController.getSettingPlist()
        let soundIndex = plist.value(forKey: "sound quality") as? Int ?? 1
        
        if let valueIndex = soundValues.firstIndex(of: soundIndex) {
            cell.detailTextLabel?.text = soundOptions[valueIndex]
        }
        
        applyCellStyling(to: cell)
        return cell
    }
    
    private func createNotificationCell(for indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: "SettingsCell")
        cell.textLabel?.text = NotificationRow.allCases[indexPath.row].title
        cell.accessoryType = .disclosureIndicator
        
        applyCellStyling(to: cell)
        return cell
    }
    
    private func createDisplayCell(for indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "SwitchCell", for: indexPath)
        let displayRow = DisplayRow.allCases[indexPath.row]
        cell.textLabel?.text = displayRow.title
        
        let switchView = UISwitch()
        switchView.onTintColor = .colorPrimary
        switchView.tag = indexPath.row
        
        // Get current settings
        let plist = SettingsViewController.getSettingPlist()
        let userDefaults = UserDefaults.standard
        
        switch displayRow {
        case .showFPS:
            switchView.isOn = plist.value(forKey: "show fps") as! Bool
            switchView.addTarget(self, action: #selector(showFPSChanged(_:)), for: .valueChanged)
        case .frameSkip:
            switchView.isOn = plist.value(forKey: "frame skip") as! Bool
            switchView.addTarget(self, action: #selector(frameSkipChanged(_:)), for: .valueChanged)
        case .keepAspectRatio:
            switchView.isOn = plist.value(forKey: "keep aspect rate") as! Bool
            switchView.addTarget(self, action: #selector(aspectRatioChanged(_:)), for: .valueChanged)
        case .rotateScreen:
            switchView.isOn = plist.value(forKey: "rotate screen") as! Bool
            switchView.addTarget(self, action: #selector(rotateScreenChanged(_:)), for: .valueChanged)
        case .landscapeMode:
            switchView.isOn = userDefaults.bool(forKey: "landscape")
            switchView.addTarget(self, action: #selector(landscapeModeChanged(_:)), for: .valueChanged)
        case .analogAsDPad:
            switchView.isOn = plist.value(forKey: "analog as dpad") as? Bool ?? false
            switchView.addTarget(self, action: #selector(analogAsDPadChanged(_:)), for: .valueChanged)
        }
        
        cell.accessoryView = switchView
        cell.selectionStyle = .none
        
        applyCellStyling(to: cell)
        return cell
    }
    
    private func applyCellStyling(to cell: UITableViewCell) {
        cell.backgroundColor = .defaultBackground
        cell.textLabel?.textColor = .adaptiveTextColor
        cell.detailTextLabel?.textColor = .adaptiveTextColor
        
        let selectedBackgroundView = UIView()
        selectedBackgroundView.backgroundColor = .selectedBackground
        cell.selectedBackgroundView = selectedBackgroundView
    }
    
    // MARK: - Table View Delegate
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        guard let settingsSection = SettingsSection(rawValue: indexPath.section) else { return }
        
        switch settingsSection {
        case .cartridge:
            showCartridgePicker()
        case .resolution:
            showResolutionPicker()
        case .sound:
            showSoundPicker()
        case .notification:
            showNotificationSettings()
        case .display:
            // Switches handle themselves
            break
        case .pad:
            showPadSettings()
        }
    }

    private func showPadSettings() {
        let config = PadConfiguration.load() ?? .default
        let padSettingsVC = PadSettingsViewController(configuration: config) { [weak self] newConfig in
            // Notify any active YabausePadView of the configuration change
            NotificationCenter.default.post(
                name: NSNotification.Name("PadConfigurationChanged"),
                object: nil,
                userInfo: ["configuration": newConfig]
            )
        }
        navigationController?.pushViewController(padSettingsVC, animated: true)
    }
    
    // MARK: - Settings Actions
    @objc private func showFPSChanged(_ sender: UISwitch) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(sender.isOn, forKey: "show fps" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
    }
    
    @objc private func frameSkipChanged(_ sender: UISwitch) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(sender.isOn, forKey: "frame skip" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
    }
    
    @objc private func aspectRatioChanged(_ sender: UISwitch) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(sender.isOn, forKey: "keep aspect rate" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
    }
    
    @objc private func rotateScreenChanged(_ sender: UISwitch) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(sender.isOn, forKey: "rotate screen" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
    }
    
    @objc private func landscapeModeChanged(_ sender: UISwitch) {
        let userDefaults = UserDefaults.standard
        userDefaults.set(sender.isOn, forKey: "landscape")
        userDefaults.synchronize()
    }
    
    @objc private func analogAsDPadChanged(_ sender: UISwitch) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(sender.isOn, forKey: "analog as dpad" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
    }
    
    private func showCartridgePicker() {
        let alertController = UIAlertController(
            title: NSLocalizedString("Cartridge Type", comment: "Alert title for cartridge type picker"),
            message: NSLocalizedString("Select cartridge type", comment: "Alert message for cartridge type picker"),
            preferredStyle: .actionSheet)

        for (index, option) in cartridgeOptions.enumerated() {
            let action = UIAlertAction(title: option, style: .default) { [weak self] _ in
                self?.selectCartridge(index: index)
            }
            alertController.addAction(action)
        }

        let cancelAction = UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel, handler: nil)
        alertController.addAction(cancelAction)
        
        // iPad support
        if let popover = alertController.popoverPresentationController {
            if let cell = tableView.cellForRow(at: IndexPath(row: 0, section: SettingsSection.cartridge.rawValue)) {
                popover.sourceView = cell
                popover.sourceRect = cell.bounds
            }
        }
        
        present(alertController, animated: true, completion: nil)
    }
    
    private func selectCartridge(index: Int) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(cartridgeValues[index], forKey: "cartridge" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
        
        // Update the cell display
        if let cell = tableView.cellForRow(at: IndexPath(row: 0, section: SettingsSection.cartridge.rawValue)) {
            cell.detailTextLabel?.text = cartridgeOptions[index]
        }
    }
    
    private func showResolutionPicker() {
        let alertController = UIAlertController(
            title: NSLocalizedString("Rendering Resolution", comment: "Alert title for rendering resolution picker"),
            message: NSLocalizedString("Select rendering resolution", comment: "Alert message for rendering resolution picker"),
            preferredStyle: .actionSheet)

        for (index, option) in resolutionOptions.enumerated() {
            let action = UIAlertAction(title: option, style: .default) { [weak self] _ in
                self?.selectResolution(index: index)
            }
            alertController.addAction(action)
        }

        let cancelAction = UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel, handler: nil)
        alertController.addAction(cancelAction)
        
        // iPad support
        if let popover = alertController.popoverPresentationController {
            if let cell = tableView.cellForRow(at: IndexPath(row: 0, section: SettingsSection.resolution.rawValue)) {
                popover.sourceView = cell
                popover.sourceRect = cell.bounds
            }
        }
        
        present(alertController, animated: true, completion: nil)
    }
    
    private func selectResolution(index: Int) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(resolutionValues[index], forKey: "rendering resolution" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
        
        // Update the cell display
        if let cell = tableView.cellForRow(at: IndexPath(row: 0, section: SettingsSection.resolution.rawValue)) {
            cell.detailTextLabel?.text = resolutionOptions[index]
        }
    }
    
    private func showSoundPicker() {
        let alertController = UIAlertController(
            title: NSLocalizedString("Sound Quality", comment: "Alert title for sound quality picker"),
            message: NSLocalizedString("Select sound quality", comment: "Alert message for sound quality picker"),
            preferredStyle: .actionSheet)

        for (index, option) in soundOptions.enumerated() {
            let action = UIAlertAction(title: option, style: .default) { [weak self] _ in
                self?.selectSound(index: index)
            }
            alertController.addAction(action)
        }

        let cancelAction = UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel, handler: nil)
        alertController.addAction(cancelAction)
        
        // iPad support
        if let popover = alertController.popoverPresentationController {
            if let cell = tableView.cellForRow(at: IndexPath(row: 0, section: SettingsSection.sound.rawValue)) {
                popover.sourceView = cell
                popover.sourceRect = cell.bounds
            }
        }
        
        present(alertController, animated: true, completion: nil)
    }
    
    private func selectSound(index: Int) {
        let plist = SettingsViewController.getSettingPlist()
        plist.setObject(soundValues[index], forKey: "sound quality" as NSCopying)
        plist.write(toFile: SettingsViewController.getSettingFilename(), atomically: true)
        
        // Update the cell display
        if let cell = tableView.cellForRow(at: IndexPath(row: 0, section: SettingsSection.sound.rawValue)) {
            cell.detailTextLabel?.text = soundOptions[index]
        }
    }
    
    private func showNotificationSettings() {
        let notificationSettingsVC = NotificationSettingsViewController()
        notificationSettingsVC.title = NSLocalizedString("Notification Settings", comment: "Notification settings screen title")
        navigationController?.pushViewController(notificationSettingsVC, animated: true)
    }
    
    // MARK: - Header Styling
    override func tableView(_ tableView: UITableView, willDisplayHeaderView view: UIView, forSection section: Int) {
        if let headerView = view as? UITableViewHeaderFooterView {
            headerView.textLabel?.textColor = .adaptiveTextColor
            headerView.contentView.backgroundColor = .colorPrimary
        }
    }
    
    // MARK: - GameController Navigation
    
    private func navigateUp() {
        guard isGameControllerMode else { return }
        
        var newSection = selectedIndexPath.section
        var newRow = selectedIndexPath.row
        
        if newRow > 0 {
            newRow -= 1
        } else if newSection > 0 {
            newSection -= 1
            newRow = tableView.numberOfRows(inSection: newSection) - 1
        } else {
            // Wrap to last item
            newSection = SettingsSection.allCases.count - 1
            newRow = tableView.numberOfRows(inSection: newSection) - 1
        }
        
        selectedIndexPath = IndexPath(row: newRow, section: newSection)
        updateSelectionVisual()
    }
    
    private func navigateDown() {
        guard isGameControllerMode else { return }
        
        var newSection = selectedIndexPath.section
        var newRow = selectedIndexPath.row
        let maxRow = tableView.numberOfRows(inSection: newSection) - 1
        
        if newRow < maxRow {
            newRow += 1
        } else if newSection < SettingsSection.allCases.count - 1 {
            newSection += 1
            newRow = 0
        } else {
            // Wrap to first item
            newSection = 0
            newRow = 0
        }
        
        selectedIndexPath = IndexPath(row: newRow, section: newSection)
        updateSelectionVisual()
    }
    
    private func navigateToPreviousSection() {
        guard isGameControllerMode else { return }
        
        var newSection = selectedIndexPath.section
        
        if newSection > 0 {
            newSection -= 1
        } else {
            newSection = SettingsSection.allCases.count - 1
        }
        
        selectedIndexPath = IndexPath(row: 0, section: newSection)
        updateSelectionVisual()
    }
    
    private func navigateToNextSection() {
        guard isGameControllerMode else { return }
        
        var newSection = selectedIndexPath.section
        
        if newSection < SettingsSection.allCases.count - 1 {
            newSection += 1
        } else {
            newSection = 0
        }
        
        selectedIndexPath = IndexPath(row: 0, section: newSection)
        updateSelectionVisual()
    }
    
    private func handleGamepadSelection() {
        guard isGameControllerMode else { return }
        
        guard let settingsSection = SettingsSection(rawValue: selectedIndexPath.section) else { return }
        
        switch settingsSection {
        case .cartridge:
            showCartridgePicker()
        case .resolution:
            showResolutionPicker()
        case .sound:
            showSoundPicker()
        case .notification:
            showNotificationSettings()
        case .display:
            // Handle switch toggle for display settings
            handleDisplaySwitch()
        case .pad:
            showPadSettings()
        }
    }

    private func handleDisplaySwitch() {
        let cell = tableView.cellForRow(at: selectedIndexPath)
        if let switchView = cell?.accessoryView as? UISwitch {
            switchView.setOn(!switchView.isOn, animated: true)
            
            // Trigger the appropriate action based on the display row
            let displayRow = DisplayRow.allCases[selectedIndexPath.row]
            switch displayRow {
            case .showFPS:
                showFPSChanged(switchView)
            case .frameSkip:
                frameSkipChanged(switchView)
            case .keepAspectRatio:
                aspectRatioChanged(switchView)
            case .rotateScreen:
                rotateScreenChanged(switchView)
            case .landscapeMode:
                landscapeModeChanged(switchView)
            case .analogAsDPad:
                analogAsDPadChanged(switchView)
            }
        }
    }
    
    private func updateSelectionVisual() {
        guard isGameControllerMode else { return }
        
        // Remove previous selection visual
        for section in 0..<tableView.numberOfSections {
            for row in 0..<tableView.numberOfRows(inSection: section) {
                let indexPath = IndexPath(row: row, section: section)
                if let cell = tableView.cellForRow(at: indexPath) {
                    cell.backgroundColor = .defaultBackground
                }
            }
        }
        
        // Add selection visual to current item
        if let cell = tableView.cellForRow(at: selectedIndexPath) {
            cell.backgroundColor = .selectedBackground
        }
        
        // Scroll to make sure selected item is visible
        tableView.scrollToRow(at: selectedIndexPath, at: .middle, animated: true)
    }
    
    // MARK: - Navigation
    
    deinit {
        print("SettingsViewController: deinit called")
        
        // GameControllerハンドラーを確実にクリア
        clearAllGameControllerHandlers()
        
        // NotificationCenter observersを削除
        NotificationCenter.default.removeObserver(self)
        
        print("SettingsViewController: deinit completed")
    }
}