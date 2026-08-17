import UIKit

class RetroAchievementsLoginViewController: UIViewController {
    
    // MARK: - UI Components
    private let scrollView: UIScrollView = {
        let scrollView = UIScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        return scrollView
    }()
    
    private let contentView: UIView = {
        let view = UIView()
        view.translatesAutoresizingMaskIntoConstraints = false
        return view
    }()
    
    private let logoImageView: UIImageView = {
        let imageView = UIImageView()
        imageView.contentMode = .scaleAspectFit
        imageView.translatesAutoresizingMaskIntoConstraints = false
        return imageView
    }()
    
    private let titleLabel: UILabel = {
        let label = UILabel()
        label.text = NSLocalizedString("RetroAchievements Login", comment: "Title for RetroAchievements login screen")
        label.font = .systemFont(ofSize: 24, weight: .bold)
        label.textAlignment = .center
        label.textColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private let descriptionLabel: UILabel = {
        let label = UILabel()
        label.text = NSLocalizedString("Sign in to your RetroAchievements account to unlock achievements and compete on leaderboards.", comment: "Description for RetroAchievements login")
        label.font = .systemFont(ofSize: 16)
        label.textAlignment = .center
        label.textColor = .secondaryLabel
        label.numberOfLines = 0
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private let usernameTextField: UITextField = {
        let textField = UITextField()
        textField.placeholder = NSLocalizedString("Username", comment: "Username placeholder")
        textField.borderStyle = .roundedRect
        textField.autocapitalizationType = .none
        textField.autocorrectionType = .no
        textField.returnKeyType = .next
        textField.translatesAutoresizingMaskIntoConstraints = false
        return textField
    }()
    
    private let passwordTextField: UITextField = {
        let textField = UITextField()
        textField.placeholder = NSLocalizedString("Password", comment: "Password placeholder")
        textField.borderStyle = .roundedRect
        textField.isSecureTextEntry = true
        textField.returnKeyType = .done
        textField.translatesAutoresizingMaskIntoConstraints = false
        return textField
    }()
    
    private let rememberMeSwitch: UISwitch = {
        let switchControl = UISwitch()
        switchControl.isOn = true
        switchControl.onTintColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
        switchControl.translatesAutoresizingMaskIntoConstraints = false
        return switchControl
    }()
    
    private let rememberMeLabel: UILabel = {
        let label = UILabel()
        label.text = NSLocalizedString("Remember Me", comment: "Remember me option")
        label.font = .systemFont(ofSize: 16)
        label.translatesAutoresizingMaskIntoConstraints = false
        return label
    }()
    
    private let loginButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Sign In", comment: "Sign in button"), for: .normal)
        button.backgroundColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
        button.setTitleColor(.white, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 18, weight: .semibold)
        button.layer.cornerRadius = 8
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }()
    
    private let cancelButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Cancel", comment: "Cancel button"), for: .normal)
        button.backgroundColor = .systemGray
        button.setTitleColor(.white, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 18, weight: .medium)
        button.layer.cornerRadius = 8
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }()
    
    private let loadingIndicator: UIActivityIndicatorView = {
        let indicator = UIActivityIndicatorView(style: .medium)
        indicator.translatesAutoresizingMaskIntoConstraints = false
        indicator.hidesWhenStopped = true
        return indicator
    }()
    
    private let createAccountButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("Don't have an account? Create one", comment: "Create account link"), for: .normal)
        button.setTitleColor(UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0), for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 14)
        button.translatesAutoresizingMaskIntoConstraints = false
        return button
    }()
    
    // MARK: - Properties
    var onLoginSuccess: ((String) -> Void)?
    var onLoginCancel: (() -> Void)?
    
    // MARK: - Lifecycle
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupActions()
        loadRetroAchievementsLogo()
        loadStoredCredentials()
        
        // Setup keyboard handling
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillShow), name: UIResponder.keyboardWillShowNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillHide), name: UIResponder.keyboardWillHideNotification, object: nil)
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        usernameTextField.becomeFirstResponder()
    }
    
    // MARK: - UI Setup
    private func setupUI() {
        view.backgroundColor = .systemBackground
        
        view.addSubview(scrollView)
        scrollView.addSubview(contentView)
        
        contentView.addSubview(logoImageView)
        contentView.addSubview(titleLabel)
        contentView.addSubview(descriptionLabel)
        contentView.addSubview(usernameTextField)
        contentView.addSubview(passwordTextField)
        contentView.addSubview(rememberMeSwitch)
        contentView.addSubview(rememberMeLabel)
        contentView.addSubview(loginButton)
        contentView.addSubview(cancelButton)
        contentView.addSubview(loadingIndicator)
        contentView.addSubview(createAccountButton)
        
        NSLayoutConstraint.activate([
            // ScrollView constraints
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            
            // ContentView constraints
            contentView.topAnchor.constraint(equalTo: scrollView.topAnchor),
            contentView.leadingAnchor.constraint(equalTo: scrollView.leadingAnchor),
            contentView.trailingAnchor.constraint(equalTo: scrollView.trailingAnchor),
            contentView.bottomAnchor.constraint(equalTo: scrollView.bottomAnchor),
            contentView.widthAnchor.constraint(equalTo: scrollView.widthAnchor),
            
            // Logo constraints
            logoImageView.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 40),
            logoImageView.centerXAnchor.constraint(equalTo: contentView.centerXAnchor),
            logoImageView.widthAnchor.constraint(equalToConstant: 80),
            logoImageView.heightAnchor.constraint(equalToConstant: 80),
            
            // Title constraints
            titleLabel.topAnchor.constraint(equalTo: logoImageView.bottomAnchor, constant: 20),
            titleLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            titleLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            
            // Description constraints
            descriptionLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 10),
            descriptionLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            descriptionLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            
            // Username field constraints
            usernameTextField.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: 40),
            usernameTextField.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            usernameTextField.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            usernameTextField.heightAnchor.constraint(equalToConstant: 44),
            
            // Password field constraints
            passwordTextField.topAnchor.constraint(equalTo: usernameTextField.bottomAnchor, constant: 15),
            passwordTextField.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            passwordTextField.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            passwordTextField.heightAnchor.constraint(equalToConstant: 44),
            
            // Remember me switch constraints
            rememberMeSwitch.topAnchor.constraint(equalTo: passwordTextField.bottomAnchor, constant: 20),
            rememberMeSwitch.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            
            // Remember me label constraints
            rememberMeLabel.centerYAnchor.constraint(equalTo: rememberMeSwitch.centerYAnchor),
            rememberMeLabel.leadingAnchor.constraint(equalTo: rememberMeSwitch.trailingAnchor, constant: 10),
            
            // Login button constraints
            loginButton.topAnchor.constraint(equalTo: rememberMeSwitch.bottomAnchor, constant: 30),
            loginButton.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            loginButton.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            loginButton.heightAnchor.constraint(equalToConstant: 50),
            
            // Cancel button constraints
            cancelButton.topAnchor.constraint(equalTo: loginButton.bottomAnchor, constant: 10),
            cancelButton.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            cancelButton.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            cancelButton.heightAnchor.constraint(equalToConstant: 50),
            
            // Loading indicator constraints
            loadingIndicator.centerXAnchor.constraint(equalTo: loginButton.centerXAnchor),
            loadingIndicator.centerYAnchor.constraint(equalTo: loginButton.centerYAnchor),
            
            // Create account button constraints
            createAccountButton.topAnchor.constraint(equalTo: cancelButton.bottomAnchor, constant: 20),
            createAccountButton.centerXAnchor.constraint(equalTo: contentView.centerXAnchor),
            createAccountButton.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -20)
        ])
    }
    
    private func setupActions() {
        loginButton.addTarget(self, action: #selector(loginButtonTapped), for: .touchUpInside)
        cancelButton.addTarget(self, action: #selector(cancelButtonTapped), for: .touchUpInside)
        createAccountButton.addTarget(self, action: #selector(createAccountButtonTapped), for: .touchUpInside)
        
        usernameTextField.delegate = self
        passwordTextField.delegate = self
    }
    
    private func loadRetroAchievementsLogo() {
        guard let logoURL = URL(string: "https://docs.retroachievements.org/ra-logo-big-shadow.png") else {
            // Fallback to system icon
            logoImageView.image = UIImage(systemName: "gamecontroller.fill")
            logoImageView.tintColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
            return
        }
        
        DispatchQueue.global().async {
            if let data = try? Data(contentsOf: logoURL), let image = UIImage(data: data) {
                DispatchQueue.main.async {
                    self.logoImageView.image = image
                }
            } else {
                DispatchQueue.main.async {
                    self.logoImageView.image = UIImage(systemName: "gamecontroller.fill")
                    self.logoImageView.tintColor = UIColor(red: 255/255.0, green: 102/255.0, blue: 0/255.0, alpha: 1.0)
                }
            }
        }
    }
    
    private func loadStoredCredentials() {
        // Load stored username if remember me was enabled
        let authManager = RetroAchievementsAuthManager.shared
        rememberMeSwitch.isOn = authManager.autoLoginEnabled
        
        if let storedUsername = authManager.currentUsername {
            usernameTextField.text = storedUsername
        }
    }
    
    // MARK: - Actions
    @objc private func loginButtonTapped() {
        guard let username = usernameTextField.text?.trimmingCharacters(in: .whitespacesAndNewlines),
              let password = passwordTextField.text?.trimmingCharacters(in: .whitespacesAndNewlines),
              !username.isEmpty, !password.isEmpty else {
            showError(NSLocalizedString("Please enter both username and password.", comment: "Login validation error"))
            return
        }
        
        NSLog("RetroAchievementsLoginViewController: Starting login for user: \(username)")
        setLoginInProgress(true)
        
        let authManager = RetroAchievementsAuthManager.shared
        // Set the Remember Me preference before login
        authManager.autoLoginEnabled = rememberMeSwitch.isOn
        authManager.loginRetroAchievements(username: username, password: password) { [weak self] success, error in
            DispatchQueue.main.async {
                NSLog("RetroAchievementsLoginViewController: Login completed - success: \(success), error: \(error ?? "none")")
                self?.setLoginInProgress(false)
                
                if success {
                    // Don't manipulate credentials here - they're already handled by AuthManager
                    // based on the autoLoginEnabled setting we set before login
                    if self?.rememberMeSwitch.isOn == false {
                        NSLog("RetroAchievementsLoginViewController: Remember Me disabled - credentials will not persist")
                    } else {
                        NSLog("RetroAchievementsLoginViewController: Remember Me enabled - credentials should be saved")
                    }
                    
                    NSLog("RetroAchievementsLoginViewController: Login successful, calling onLoginSuccess")
                    self?.onLoginSuccess?(username)
                    self?.dismiss(animated: true)
                } else {
                    let errorMessage = error ?? NSLocalizedString("Login failed. Please check your credentials.", comment: "Generic login error")
                    NSLog("RetroAchievementsLoginViewController: Login failed with message: \(errorMessage)")
                    self?.showError(errorMessage)
                }
            }
        }
    }
    
    @objc private func cancelButtonTapped() {
        onLoginCancel?()
        dismiss(animated: true)
    }
    
    @objc private func createAccountButtonTapped() {
        // Open RetroAchievements registration page
        guard let url = URL(string: "https://retroachievements.org/createaccount.php") else { return }
        
        if UIApplication.shared.canOpenURL(url) {
            UIApplication.shared.open(url)
        }
    }
    
    // MARK: - Helper Methods
    private func setLoginInProgress(_ inProgress: Bool) {
        loginButton.isEnabled = !inProgress
        cancelButton.isEnabled = !inProgress
        usernameTextField.isEnabled = !inProgress
        passwordTextField.isEnabled = !inProgress
        rememberMeSwitch.isEnabled = !inProgress
        
        if inProgress {
            loginButton.setTitle("", for: .normal)
            loadingIndicator.startAnimating()
        } else {
            loginButton.setTitle(NSLocalizedString("Sign In", comment: "Sign in button"), for: .normal)
            loadingIndicator.stopAnimating()
        }
    }
    
    private func showError(_ message: String) {
        let alert = UIAlertController(
            title: NSLocalizedString("Login Error", comment: "Login error title"),
            message: message,
            preferredStyle: .alert
        )
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
    
    // MARK: - Keyboard Handling
    @objc private func keyboardWillShow(notification: NSNotification) {
        guard let keyboardSize = (notification.userInfo?[UIResponder.keyboardFrameEndUserInfoKey] as? NSValue)?.cgRectValue else { return }
        
        let contentInsets = UIEdgeInsets(top: 0, left: 0, bottom: keyboardSize.height, right: 0)
        scrollView.contentInset = contentInsets
        scrollView.scrollIndicatorInsets = contentInsets
    }
    
    @objc private func keyboardWillHide(notification: NSNotification) {
        scrollView.contentInset = .zero
        scrollView.scrollIndicatorInsets = .zero
    }
}

// MARK: - UITextFieldDelegate
extension RetroAchievementsLoginViewController: UITextFieldDelegate {
    func textFieldShouldReturn(_ textField: UITextField) -> Bool {
        if textField == usernameTextField {
            passwordTextField.becomeFirstResponder()
        } else if textField == passwordTextField {
            loginButtonTapped()
        }
        return true
    }
}