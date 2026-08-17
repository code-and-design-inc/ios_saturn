import UIKit
@_exported import FirebaseAuth
import FirebaseCore
import AuthenticationServices
import GoogleSignIn
import GameController

@objc public class LoginViewController: UIViewController {
    
    // MARK: - GameController Support
    private var gameController: GCController?
    
    var completionHandler: ((Bool) -> Void)?
    private var loginSucceeded: Bool = false
    
    private let stackView: UIStackView = {
        let stack = UIStackView()
        stack.axis = .vertical
        stack.spacing = 20
        stack.translatesAutoresizingMaskIntoConstraints = false
        return stack
    }()
    
    private let googleSignInButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("sign_in_with_google", comment: "Button title for Google sign in"), for: .normal)
        button.backgroundColor = .white
        button.setTitleColor(.black, for: .normal)
        button.layer.cornerRadius = 8
        button.layer.borderWidth = 1
        button.layer.borderColor = UIColor.black.cgColor
        return button
    }()
    
    private let appleSignInButton: ASAuthorizationAppleIDButton = {
        let button = ASAuthorizationAppleIDButton()
        return button
    }()
    
    override public func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupActions()
        setupNavigationBar()
        setupGameController()
    }
    
    private func setupNavigationBar() {
        let closeButton = UIBarButtonItem(title: NSLocalizedString("Close", comment: "Close Dialoig"), style: .plain, target: self, action: #selector(closeButtonTapped))
        navigationItem.leftBarButtonItem = closeButton
    }
    
    @objc private func closeButtonTapped() {
        dismiss(animated: true)
    }
    
    private let signInInfoLabel: UILabel = {
        let label = UILabel()
        label.text = NSLocalizedString("sign_in_features", comment: "Description of features available after signing in")
        label.numberOfLines = 0
        label.textAlignment = .center
        label.font = UIFont.systemFont(ofSize: 14)
        return label
    }()
    
    private func setupUI() {
        view.backgroundColor = .systemBackground
        title = NSLocalizedString("sign_in", comment: "Title for sign in screen")
        
        view.addSubview(stackView)
        stackView.addArrangedSubview(signInInfoLabel) // 追加
        stackView.addArrangedSubview(googleSignInButton)
        stackView.addArrangedSubview(appleSignInButton)
        
        NSLayoutConstraint.activate([
            stackView.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            stackView.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            stackView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 40),
            stackView.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -40),
            
            googleSignInButton.heightAnchor.constraint(equalToConstant: 44),
            appleSignInButton.heightAnchor.constraint(equalToConstant: 44)
        ])
    }
    
    private func setupActions() {
        googleSignInButton.addTarget(self, action: #selector(handleGoogleSignIn), for: .touchUpInside)
        appleSignInButton.addTarget(self, action: #selector(handleAppleSignIn), for: .touchUpInside)
    }
    
    @objc private func handleGoogleSignIn() {
        guard let clientID = FirebaseApp.app()?.options.clientID else { return }
        let config = GIDConfiguration(clientID: clientID)
        GIDSignIn.sharedInstance.configuration = config
        GIDSignIn.sharedInstance.signIn(withPresenting: self) { [weak self] result, error in
            if let error = error {
                print("Google Sign In Error: \(error.localizedDescription)")
                self?.dismiss(animated: true)
                return
            }
            
            guard let user = result?.user,
                  let idToken = user.idToken?.tokenString else { return }
            
            let credential = GoogleAuthProvider.credential(withIDToken: idToken,
                                                         accessToken: user.accessToken.tokenString)
            
            self?.signInWithFirebase(credential: credential)
        }
    }
    
    @objc private func handleAppleSignIn() {
        let provider = ASAuthorizationAppleIDProvider()
        let request = provider.createRequest()
        request.requestedScopes = [.fullName, .email]
        
        let authorizationController = ASAuthorizationController(authorizationRequests: [request])
        authorizationController.delegate = self
        authorizationController.presentationContextProvider = self
        authorizationController.performRequests()
    }
    
    private func signInWithFirebase(credential: AuthCredential) {
        Auth.auth().signIn(with: credential) { [weak self] result, error in
            if let error = error {
                print("Firebase Sign In Error: \(error.localizedDescription)")
                self?.loginSucceeded = false
                
                // サインイン失敗時のアラート表示
                let alert = UIAlertController(
                    title: NSLocalizedString("sign_in_failed", comment: "Title for sign in error alert"),
                    message: String(format: NSLocalizedString("sign_in_error", comment: "Format for sign in error message"), error.localizedDescription),
                    preferredStyle: .alert
                )

                alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak self] _ in
                    self?.dismiss(animated: true)
                })
                self?.present(alert, animated: true)
                return
            }
            
            // サインイン成功時のアラート表示
            self?.loginSucceeded = true
            let alert = UIAlertController(
                title: NSLocalizedString("sign_in_success", comment: "Title for successful sign in alert"),
                message: NSLocalizedString("sign_in_success_message", comment: "Message for successful sign in"),
                preferredStyle: .alert
            )
            
            alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak self] _ in
                self?.dismiss(animated: true)
            })
            
            self?.present(alert, animated: true)
        }
    }
    
    // MARK: - GameController Support
    
    private func setupGameController() {
        // 既に接続されているコントローラーがあるかチェック
        if let controller = GCController.controllers().first {
            gameController = controller
            setupControllerInputHandlers(controller)
        }
        
        // コントローラー接続の監視
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
    }
    
    private func setupControllerInputHandlers(_ controller: GCController) {
        // Extended gamepadの場合
        if let extendedGamepad = controller.extendedGamepad {
            // B button (キャンセル/戻る)
            extendedGamepad.buttonB.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.closeButtonTapped()
                    }
                }
            }
            
            // Menu button (閉じる)
            extendedGamepad.buttonMenu.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.closeButtonTapped()
                    }
                }
            }
        }
        // Micro gamepadの場合
        else if let microGamepad = controller.microGamepad {
            // X button (キャンセル/戻る)
            microGamepad.buttonX.pressedChangedHandler = { [weak self] (button, value, pressed) in
                if pressed {
                    DispatchQueue.main.async {
                        self?.closeButtonTapped()
                    }
                }
            }
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
}

extension LoginViewController: ASAuthorizationControllerDelegate {
    public func authorizationController(controller: ASAuthorizationController, didCompleteWithAuthorization authorization: ASAuthorization) {
        if let appleIDCredential = authorization.credential as? ASAuthorizationAppleIDCredential {
            guard let appleIDToken = appleIDCredential.identityToken else { return }
            guard let idTokenString = String(data: appleIDToken, encoding: .utf8) else { return }
            
            let credential = OAuthProvider.credential(
                withProviderID: "apple.com",
                idToken: idTokenString,
                rawNonce: nil
            )
            
            signInWithFirebase(credential: credential)
        }
    }
    
    public func authorizationController(controller: ASAuthorizationController, didCompleteWithError error: Error) {
        print("Apple Sign In Error: \(error.localizedDescription)")
        dismiss(animated: true)
    }
}

extension LoginViewController: ASAuthorizationControllerPresentationContextProviding {
    public func presentationAnchor(for controller: ASAuthorizationController) -> ASPresentationAnchor {
        return view.window!
    }
}

extension LoginViewController {
    override public func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
//        if isBeingDismissed {
            completionHandler?(loginSucceeded)
//        }
    }
}
