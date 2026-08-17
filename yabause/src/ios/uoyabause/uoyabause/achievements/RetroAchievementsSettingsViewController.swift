import UIKit
import FirebaseAuth

class RetroAchievementsSettingsViewController: UITableViewController {
    
    // MARK: - Section and Row Enums
    private enum Section: Int, CaseIterable {
        case account = 0
        case settings = 1
        case achievements = 2
        
        var title: String {
            switch self {
            case .account:
                return NSLocalizedString("Account", comment: "Account section title")
            case .settings:
                return NSLocalizedString("Settings", comment: "Settings section title")
            case .achievements:
                return NSLocalizedString("Achievements", comment: "Achievements section title")
            }
        }
    }
    
    private enum AccountRow: Int, CaseIterable {
        case loginStatus = 0
        case loginAction = 1
        case username = 2
    }
    
    private enum SettingsRow: Int, CaseIterable {
        case hardcoreMode = 0
        case notifications = 1
    }
    
    private enum AchievementsRow: Int, CaseIterable {
        case viewAchievements = 0
        case viewLeaderboards = 1
    }
    
    // MARK: - Properties
    private let authManager = RetroAchievementsAuthManager.shared
    private var isLoggedIn: Bool {
        return authManager.isRetroAchievementsLoggedIn
    }
    
    // MARK: - UI Elements
    private lazy var hardcoreModeSwitch: UISwitch = {
        let toggle = UISwitch()
        toggle.addTarget(self, action: #selector(hardcoreModeSwitchChanged(_:)), for: .valueChanged)
        return toggle
    }()
    
    
    private lazy var notificationsSwitch: UISwitch = {
        let toggle = UISwitch()
        toggle.addTarget(self, action: #selector(notificationsSwitchChanged(_:)), for: .valueChanged)
        return toggle
    }()
    
    // MARK: - Lifecycle
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupNavigationBar()
        authManager.delegate = self
        updateUI()
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        updateUI()
        tableView.reloadData()
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        title = NSLocalizedString("RetroAchievements", comment: "RetroAchievements settings title")
        
        // Background colors consistent with existing settings
        view.backgroundColor = .defaultBackground
        tableView.backgroundColor = .defaultBackground
        tableView.separatorColor = .colorAccent
        
        // Setup switches
        setupSwitchStyles()
        
        // Register cells
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "BasicCell")
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "SwitchCell")
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "ActionCell")
    }
    
    private func setupNavigationBar() {
        navigationItem.largeTitleDisplayMode = .never
    }
    
    private func setupSwitchStyles() {
        [hardcoreModeSwitch, notificationsSwitch].forEach { switchControl in
            switchControl.onTintColor = .colorAccent
            switchControl.backgroundColor = .systemGray5
            switchControl.layer.cornerRadius = 16
        }
    }
    
    private func updateUI() {
        // Load saved settings
        let userDefaults = UserDefaults.standard
        hardcoreModeSwitch.isOn = userDefaults.bool(forKey: "ra_hardcore_mode_enabled")
        notificationsSwitch.isOn = userDefaults.bool(forKey: "ra_notifications_enabled")
        
        // Update hardcore mode availability
        hardcoreModeSwitch.isEnabled = isLoggedIn
        
        tableView.reloadData()
    }
    
    // MARK: - TableView DataSource
    override func numberOfSections(in tableView: UITableView) -> Int {
        return Section.allCases.count
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        guard let sectionType = Section(rawValue: section) else { return 0 }
        
        switch sectionType {
        case .account:
            return AccountRow.allCases.count
        case .settings:
            return isLoggedIn ? SettingsRow.allCases.count : 0
        case .achievements:
            return isLoggedIn ? AchievementsRow.allCases.count : 0
        }
    }
    
    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        guard let sectionType = Section(rawValue: section) else { return nil }
        return sectionType.title
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        guard let sectionType = Section(rawValue: indexPath.section) else {
            return UITableViewCell()
        }
        
        switch sectionType {
        case .account:
            return configureAccountCell(for: indexPath)
        case .settings:
            return configureSettingsCell(for: indexPath)
        case .achievements:
            return configureAchievementsCell(for: indexPath)
        }
    }
    
    // MARK: - Cell Configuration
    private func configureAccountCell(for indexPath: IndexPath) -> UITableViewCell {
        guard let rowType = AccountRow(rawValue: indexPath.row) else {
            return UITableViewCell()
        }
        
        switch rowType {
        case .loginStatus:
            let cell = tableView.dequeueReusableCell(withIdentifier: "BasicCell", for: indexPath)
            cell.textLabel?.text = NSLocalizedString("Status", comment: "Login status label")
            cell.detailTextLabel?.text = isLoggedIn ? 
                NSLocalizedString("Logged In", comment: "Logged in status") :
                NSLocalizedString("Not Logged In", comment: "Not logged in status")
            cell.selectionStyle = .none
            cell.backgroundColor = .defaultBackground
            cell.textLabel?.textColor = .adaptiveTextColor
            cell.detailTextLabel?.textColor = isLoggedIn ? .systemGreen : .systemRed
            return cell
            
        case .loginAction:
            let cell = tableView.dequeueReusableCell(withIdentifier: "ActionCell", for: indexPath)
            cell.textLabel?.text = isLoggedIn ?
                NSLocalizedString("Logout", comment: "Logout button") :
                NSLocalizedString("Login", comment: "Login button")
            cell.textLabel?.textColor = isLoggedIn ? .systemRed : .colorAccent
            cell.backgroundColor = .defaultBackground
            cell.accessoryType = .disclosureIndicator
            return cell
            
        case .username:
            let cell = tableView.dequeueReusableCell(withIdentifier: "BasicCell", for: indexPath)
            cell.textLabel?.text = NSLocalizedString("Username", comment: "Username label")
            cell.detailTextLabel?.text = authManager.currentUsername ?? NSLocalizedString("None", comment: "No username")
            cell.selectionStyle = .none
            cell.backgroundColor = .defaultBackground
            cell.textLabel?.textColor = .adaptiveTextColor
            cell.detailTextLabel?.textColor = .adaptiveTextColor
            return cell
        }
    }
    
    private func configureSettingsCell(for indexPath: IndexPath) -> UITableViewCell {
        guard let rowType = SettingsRow(rawValue: indexPath.row) else {
            return UITableViewCell()
        }
        
        switch rowType {
        case .hardcoreMode:
            let cell = tableView.dequeueReusableCell(withIdentifier: "SwitchCell", for: indexPath)
            cell.textLabel?.text = NSLocalizedString("Hardcore Mode", comment: "Hardcore mode setting")
            cell.accessoryView = hardcoreModeSwitch
            cell.selectionStyle = .none
            cell.backgroundColor = .defaultBackground
            cell.textLabel?.textColor = .adaptiveTextColor
            return cell
            
        case .notifications:
            let cell = tableView.dequeueReusableCell(withIdentifier: "SwitchCell", for: indexPath)
            cell.textLabel?.text = NSLocalizedString("Achievement Notifications", comment: "Achievement notifications setting")
            cell.accessoryView = notificationsSwitch  
            cell.selectionStyle = .none
            cell.backgroundColor = .defaultBackground
            cell.textLabel?.textColor = .adaptiveTextColor
            return cell
        }
    }
    
    private func configureAchievementsCell(for indexPath: IndexPath) -> UITableViewCell {
        guard let rowType = AchievementsRow(rawValue: indexPath.row) else {
            return UITableViewCell()
        }
        
        switch rowType {
        case .viewAchievements:
            let cell = tableView.dequeueReusableCell(withIdentifier: "ActionCell", for: indexPath)
            cell.textLabel?.text = NSLocalizedString("View Achievements", comment: "View achievements button")
            cell.textLabel?.textColor = .adaptiveTextColor
            cell.backgroundColor = .defaultBackground
            cell.accessoryType = .disclosureIndicator
            return cell
            
        case .viewLeaderboards:
            let cell = tableView.dequeueReusableCell(withIdentifier: "ActionCell", for: indexPath)
            cell.textLabel?.text = NSLocalizedString("View Leaderboards", comment: "View leaderboards button")
            cell.textLabel?.textColor = .adaptiveTextColor
            cell.backgroundColor = .defaultBackground
            cell.accessoryType = .disclosureIndicator
            return cell
        }
    }
    
    // MARK: - TableView Delegate
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        
        guard let sectionType = Section(rawValue: indexPath.section) else { return }
        
        switch sectionType {
        case .account:
            handleAccountRowSelection(indexPath.row)
        case .settings:
            break // Switches handle their own actions
        case .achievements:
            handleAchievementsRowSelection(indexPath.row)
        }
    }
    
    private func handleAccountRowSelection(_ row: Int) {
        guard let rowType = AccountRow(rawValue: row) else { return }
        
        switch rowType {
        case .loginAction:
            if isLoggedIn {
                showLogoutConfirmation()
            } else {
                showLoginDialog()
            }
        default:
            break
        }
    }
    
    private func handleAchievementsRowSelection(_ row: Int) {
        guard let rowType = AchievementsRow(rawValue: row) else { return }
        
        switch rowType {
        case .viewAchievements:
            // TODO: Navigate to achievements list
            showComingSoonAlert(feature: NSLocalizedString("Achievement List", comment: "Achievement list feature"))
        case .viewLeaderboards:
            // TODO: Navigate to leaderboards
            showComingSoonAlert(feature: NSLocalizedString("Leaderboards", comment: "Leaderboards feature"))
        }
    }
    
    // MARK: - Actions
    @objc private func hardcoreModeSwitchChanged(_ sender: UISwitch) {
        let userDefaults = UserDefaults.standard
        userDefaults.set(sender.isOn, forKey: "ra_hardcore_mode_enabled")
        userDefaults.synchronize()
        
        // Update manager if available
        RetroAchievementsManager.shared?.setHardcoreEnabled(sender.isOn)
    }
    
    
    @objc private func notificationsSwitchChanged(_ sender: UISwitch) {
        let userDefaults = UserDefaults.standard
        userDefaults.set(sender.isOn, forKey: "ra_notifications_enabled")
        userDefaults.synchronize()
    }
    
    // MARK: - Login/Logout Methods
    private func showLoginDialog() {
        let alert = UIAlertController(
            title: NSLocalizedString("RetroAchievements Login", comment: "Login dialog title"),
            message: NSLocalizedString("Enter your RetroAchievements credentials", comment: "Login dialog message"),
            preferredStyle: .alert
        )
        
        alert.addTextField { textField in
            textField.placeholder = NSLocalizedString("Username", comment: "Username placeholder")
            textField.autocapitalizationType = .none
            textField.autocorrectionType = .no
        }
        
        alert.addTextField { textField in
            textField.placeholder = NSLocalizedString("Password", comment: "Password placeholder")
            textField.isSecureTextEntry = true
        }
        
        let loginAction = UIAlertAction(title: NSLocalizedString("Login", comment: "Login button"), style: .default) { [weak self] _ in
            guard let username = alert.textFields?[0].text, !username.isEmpty,
                  let password = alert.textFields?[1].text, !password.isEmpty else {
                self?.showErrorAlert(message: NSLocalizedString("Please enter both username and password", comment: "Empty credentials error"))
                return
            }
            
            self?.performLogin(username: username, password: password)
        }
        
        let cancelAction = UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel)
        
        alert.addAction(loginAction)
        alert.addAction(cancelAction)
        
        present(alert, animated: true)
    }
    
    private func performLogin(username: String, password: String) {
        // Show loading indicator
        let loadingAlert = UIAlertController(title: NSLocalizedString("Logging in...", comment: "Login loading message"), message: nil, preferredStyle: .alert)
        present(loadingAlert, animated: true)
        
        authManager.loginRetroAchievements(username: username, password: password) { [weak self] success, errorMessage in
            DispatchQueue.main.async {
                loadingAlert.dismiss(animated: true) {
                    if success {
                        self?.showSuccessAlert(message: NSLocalizedString("Successfully logged in to RetroAchievements", comment: "Login success message"))
                    } else {
                        self?.showErrorAlert(message: errorMessage ?? NSLocalizedString("Login failed", comment: "Generic login error"))
                    }
                    self?.updateUI()
                }
            }
        }
    }
    
    private func showLogoutConfirmation() {
        let alert = UIAlertController(
            title: NSLocalizedString("Logout", comment: "Logout confirmation title"),
            message: NSLocalizedString("Are you sure you want to logout from RetroAchievements?", comment: "Logout confirmation message"),
            preferredStyle: .alert
        )
        
        let logoutAction = UIAlertAction(title: NSLocalizedString("Logout", comment: "Logout button"), style: .destructive) { [weak self] _ in
            self?.authManager.logoutRetroAchievements()
            self?.updateUI()
        }
        
        let cancelAction = UIAlertAction(title: NSLocalizedString("Cancel", comment: "Cancel button"), style: .cancel)
        
        alert.addAction(logoutAction)
        alert.addAction(cancelAction)
        
        present(alert, animated: true)
    }
    
    // MARK: - Utility Methods
    private func showErrorAlert(message: String) {
        let alert = UIAlertController(title: NSLocalizedString("Error", comment: "Error alert title"), message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
    
    private func showSuccessAlert(message: String) {
        let alert = UIAlertController(title: NSLocalizedString("Success", comment: "Success alert title"), message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
    
    private func showComingSoonAlert(feature: String) {
        let message = String(format: NSLocalizedString("%@ will be available in a future update", comment: "Coming soon message"), feature)
        let alert = UIAlertController(title: NSLocalizedString("Coming Soon", comment: "Coming soon title"), message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
}

// MARK: - RetroAchievementsAuthDelegate
extension RetroAchievementsSettingsViewController: RetroAchievementsAuthDelegate {
    func authStateDidChange(isLoggedIn: Bool, username: String?) {
        DispatchQueue.main.async { [weak self] in
            self?.updateUI()
        }
    }
}
