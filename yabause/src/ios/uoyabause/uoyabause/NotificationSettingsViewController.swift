import UIKit
import UserNotifications
import GameController

class NotificationSettingsViewController: UITableViewController {
    
    // MARK: - GameController Support
    private var gameController: GCController?
    
    // MARK: - Properties
    private var notificationPermissionGranted = false
    
    // Visible notification categories (excluding achievements and leaderboards to avoid conflicts with native notifications)
    private let visibleCategories: [NotificationCategory] = [
        .gameUpdates,
        .systemNotifications, 
        .promotions
    ]
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        title = NSLocalizedString("Notification Settings", comment: "Settings item for notification settings")
        setupAppearance()
        
        // Initialize default notification settings if not set
        initializeDefaultSettings()
        
        // Check notification permission
        refreshNotificationPermission()
        
        // GameController setup
        setupGameController()
        
        // Observe app becoming active to refresh permission status
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(appDidBecomeActive),
            name: UIApplication.didBecomeActiveNotification,
            object: nil
        )
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        refreshNotificationPermission()
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        // Additional check in case the user just returned from Settings app
        refreshNotificationPermission()
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    @objc private func appDidBecomeActive() {
        // Refresh when app becomes active (e.g., returning from Settings app)
        refreshNotificationPermission()
    }
    
    // MARK: - Setup
    
    private func setupAppearance() {
        tableView.backgroundColor = .defaultBackground
        tableView.separatorColor = .separator
        
        // Navigation bar appearance
        if #available(iOS 13.0, *) {
            navigationController?.navigationBar.backgroundColor = .colorPrimary
        }
    }
    
    private func setupGameController() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(gameControllerDidConnect),
            name: .GCControllerDidConnect,
            object: nil
        )
        
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(gameControllerDidDisconnect),
            name: .GCControllerDidDisconnect,
            object: nil
        )
        
        if let controller = GCController.controllers().first {
            gameController = controller
        }
    }
    
    @objc private func gameControllerDidConnect(notification: Notification) {
        gameController = notification.object as? GCController
    }
    
    @objc private func gameControllerDidDisconnect(notification: Notification) {
        gameController = nil
    }
    
    private func initializeDefaultSettings() {
        for category in NotificationCategory.allCases {
            let key = category.userDefaultsKey
            if UserDefaults.standard.object(forKey: key) == nil {
                UserDefaults.standard.set(category.isEnabledByDefault, forKey: key)
            }
        }
    }
    
    private func refreshNotificationPermission() {
        UNUserNotificationCenter.current().getNotificationSettings { [weak self] settings in
            DispatchQueue.main.async {
                guard let self = self else { return }
                
                print("NotificationSettings: Current authorization status: \(settings.authorizationStatus.rawValue)")
                
                let isAuthorized = settings.authorizationStatus == .authorized
                let wasAuthorized = self.notificationPermissionGranted
                self.notificationPermissionGranted = isAuthorized
                
                print("NotificationSettings: Permission granted: \(isAuthorized) (was: \(wasAuthorized))")
                
                // Always reload data to update both permission status and switch states
                self.tableView.reloadData()
                
                // If permission changed, update all category switches
                if wasAuthorized != isAuthorized {
                    // Update switch enabled states
                    for (index, _) in self.visibleCategories.enumerated() {
                        let indexPath = IndexPath(row: index, section: 1)
                        if let cell = self.tableView.cellForRow(at: indexPath),
                           let switchView = cell.accessoryView as? UISwitch {
                            switchView.isEnabled = isAuthorized
                        }
                    }
                    
                    // Show a toast message about the permission change
                    if isAuthorized {
                        self.showToast(message: NSLocalizedString("Notifications allowed", comment: "Toast message when notifications are allowed"))
                    } else {
                        self.showToast(message: NSLocalizedString("Notifications disabled", comment: "Toast message when notifications are disabled"))
                    }
                }
            }
        }
    }
    
    // MARK: - Table View Data Source
    
    override func numberOfSections(in tableView: UITableView) -> Int {
        return 2
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch section {
        case 0:
            return 1 // Permission status
        case 1:
            return visibleCategories.count // Notification categories
        default:
            return 0
        }
    }
    
    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch section {
        case 0:
            return NSLocalizedString("Notification Permission", comment: "Notification permission section title")
        case 1:
            return NSLocalizedString("Notification Categories", comment: "Notification categories section title")
        default:
            return nil
        }
    }
    
    override func tableView(_ tableView: UITableView, titleForFooterInSection section: Int) -> String? {
        switch section {
        case 0:
            return notificationPermissionGranted ? 
                NSLocalizedString("Notifications are allowed.", comment: "Message when notifications are allowed") : 
                NSLocalizedString("To receive notifications, please allow notifications in the Settings app.", comment: "Message to prompt user to enable notifications")
        case 1:
            return NSLocalizedString("Please select the types of notifications you want to receive.", comment: "Message for notification type selection")
        default:
            return nil
        }
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cellIdentifier = "NotificationSettingsCell"
        var cell = tableView.dequeueReusableCell(withIdentifier: cellIdentifier)
        
        if cell == nil {
            cell = UITableViewCell(style: .subtitle, reuseIdentifier: cellIdentifier)
        }
        
        guard let tableViewCell = cell else { return UITableViewCell() }
        
        // Apply color scheme
        tableViewCell.backgroundColor = .defaultBackground
        tableViewCell.textLabel?.textColor = .adaptiveTextColor
        tableViewCell.detailTextLabel?.textColor = .secondaryLabel
        
        if indexPath.section == 0 {
            // Permission status cell
            tableViewCell.textLabel?.text = NSLocalizedString("Notification Permission Status", comment: "Label for notification permission status")
            
            // Clear accessory view to prevent cell reuse issues
            tableViewCell.accessoryView = nil
            
            // Get real-time permission status
            UNUserNotificationCenter.current().getNotificationSettings { settings in
                DispatchQueue.main.async { [weak self] in
                    let statusText: String
                    let statusColor: UIColor
                    switch settings.authorizationStatus {
                    case .authorized:
                        statusText = NSLocalizedString("Allowed ✓", comment: "Status text when notifications are allowed")
                        statusColor = .systemGreen
                    case .denied:
                        statusText = NSLocalizedString("Denied ✗", comment: "Status text when notifications are denied")
                        statusColor = .systemRed
                    case .notDetermined:
                        statusText = NSLocalizedString("Not Set", comment: "Status text when notifications are not set")
                        statusColor = .systemOrange
                    case .provisional:
                        statusText = NSLocalizedString("Provisional", comment: "Status text for provisional notification permission")
                        statusColor = .systemBlue
                    case .ephemeral:
                        statusText = NSLocalizedString("Ephemeral", comment: "Status text for ephemeral notification permission")
                        statusColor = .systemBlue
                    @unknown default:
                        statusText = NSLocalizedString("Unknown", comment: "Status text for unknown notification permission")
                        statusColor = .systemGray
                    }
                    
                    tableViewCell.detailTextLabel?.text = statusText
                    tableViewCell.detailTextLabel?.textColor = statusColor
                    self?.notificationPermissionGranted = (settings.authorizationStatus == .authorized)
                    
                    print("NotificationSettings: Real-time status check - \(settings.authorizationStatus.rawValue): \(statusText)")
                }
            }
            
            tableViewCell.accessoryType = .disclosureIndicator
            tableViewCell.selectionStyle = .default
        } else if indexPath.section == 1 {
            // Notification category cells
            let category = visibleCategories[indexPath.row]
            tableViewCell.textLabel?.text = category.displayName
            tableViewCell.detailTextLabel?.text = category.description
            tableViewCell.detailTextLabel?.textColor = .secondaryLabel  // Reset color from section 0
            
            // Clear accessory type to prevent conflicts
            tableViewCell.accessoryType = .none
            
            let switchView = UISwitch()
            switchView.isOn = MessagingManager.shared.isNotificationCategoryEnabled(category: category)
            switchView.onTintColor = .tint
            switchView.addTarget(self, action: #selector(notificationCategorySwitchChanged(_:)), for: .valueChanged)
            switchView.tag = indexPath.row
            switchView.isEnabled = notificationPermissionGranted
            
            tableViewCell.accessoryView = switchView
            tableViewCell.selectionStyle = .none
        }
        
        return tableViewCell
    }
    
    // MARK: - Table View Delegate
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        if indexPath.section == 0 {
            // Open system settings for notification permissions
            openSystemSettings()
        }
    }
    
    // MARK: - Actions
    
    @objc private func notificationCategorySwitchChanged(_ sender: UISwitch) {
        let category = visibleCategories[sender.tag]
        MessagingManager.shared.setNotificationCategoryEnabled(category: category, enabled: sender.isOn)
        
        // Show confirmation feedback
        if sender.isOn {
            showToast(message: String(format: NSLocalizedString("%@ notifications enabled", comment: "Toast message when specific notification type is enabled"), category.displayName))
        } else {
            showToast(message: String(format: NSLocalizedString("%@ notifications disabled", comment: "Toast message when specific notification type is disabled"), category.displayName))
        }
    }
    
    private func openSystemSettings() {
        guard let settingsUrl = URL(string: UIApplication.openSettingsURLString) else {
            return
        }
        
        if UIApplication.shared.canOpenURL(settingsUrl) {
            UIApplication.shared.open(settingsUrl)
        }
    }
    
    private func showToast(message: String) {
        let alert = UIAlertController(title: nil, message: message, preferredStyle: .alert)
        
        present(alert, animated: true) {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) {
                alert.dismiss(animated: true)
            }
        }
    }
   
}

// MARK: - Game Controller Support Extension

extension NotificationSettingsViewController {
    
    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        guard let gameController = gameController else {
            super.pressesBegan(presses, with: event)
            return
        }
        
        var handled = false
        
        for press in presses {
            guard let key = press.key else { continue }
            
            switch key.keyCode {
            case .keyboardUpArrow:
                // Navigate up
                navigateUp()
                handled = true
            case .keyboardDownArrow:
                // Navigate down
                navigateDown()
                handled = true
            case .keyboardReturnOrEnter, .keyboardSpacebar:
                // Select current cell
                selectCurrentCell()
                handled = true
            case .keyboardEscape:
                // Go back
                navigationController?.popViewController(animated: true)
                handled = true
            default:
                break
            }
        }
        
        if !handled {
            super.pressesBegan(presses, with: event)
        }
    }
    
    private func navigateUp() {
        guard let currentIndexPath = tableView.indexPathForSelectedRow else {
            // Select first cell
            let firstIndexPath = IndexPath(row: 0, section: 0)
            tableView.selectRow(at: firstIndexPath, animated: true, scrollPosition: .none)
            return
        }
        
        var newIndexPath: IndexPath?
        
        if currentIndexPath.row > 0 {
            newIndexPath = IndexPath(row: currentIndexPath.row - 1, section: currentIndexPath.section)
        } else if currentIndexPath.section > 0 {
            let previousSection = currentIndexPath.section - 1
            let lastRowInPreviousSection = tableView.numberOfRows(inSection: previousSection) - 1
            newIndexPath = IndexPath(row: lastRowInPreviousSection, section: previousSection)
        }
        
        if let newIndexPath = newIndexPath {
            tableView.selectRow(at: newIndexPath, animated: true, scrollPosition: .middle)
        }
    }
    
    private func navigateDown() {
        guard let currentIndexPath = tableView.indexPathForSelectedRow else {
            // Select first cell
            let firstIndexPath = IndexPath(row: 0, section: 0)
            tableView.selectRow(at: firstIndexPath, animated: true, scrollPosition: .none)
            return
        }
        
        var newIndexPath: IndexPath?
        let currentSectionRowCount = tableView.numberOfRows(inSection: currentIndexPath.section)
        
        if currentIndexPath.row < currentSectionRowCount - 1 {
            newIndexPath = IndexPath(row: currentIndexPath.row + 1, section: currentIndexPath.section)
        } else if currentIndexPath.section < numberOfSections(in: tableView) - 1 {
            newIndexPath = IndexPath(row: 0, section: currentIndexPath.section + 1)
        }
        
        if let newIndexPath = newIndexPath {
            tableView.selectRow(at: newIndexPath, animated: true, scrollPosition: .middle)
        }
    }
    
    private func selectCurrentCell() {
        guard let currentIndexPath = tableView.indexPathForSelectedRow else { return }
        
        if currentIndexPath.section == 0 {
            openSystemSettings()
        } else if currentIndexPath.section == 1 {
            // Toggle the switch for the notification category
            if let cell = tableView.cellForRow(at: currentIndexPath),
               let switchView = cell.accessoryView as? UISwitch {
                switchView.setOn(!switchView.isOn, animated: true)
                notificationCategorySwitchChanged(switchView)
            }
        }
    }
}
