/*  Copyright 2024 devMiyax(smiyaxdev@gmail.com)

    This file is part of YabaSanshiro.

    YabaSanshiro is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    YabaSanshiro is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with YabaSanshiro; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

import UIKit
import FirebaseCore
import FirebaseAuth
import FirebaseFirestore

class ReportDialog: UIViewController, UITextViewDelegate {

    // MARK: - Properties
    private var productionNumber: String
    private var emulationRating: Int = 3
    private var gameRating: Int = 3

    private var emulationRatingControl: StarRatingView!
    private var gameRatingControl: StarRatingView!
    private var emulationRateText: UILabel!
    private var gameRateText: UILabel!
    private var messageTextView: UITextView!
    private var sendButton: UIButton!

    // Completion handler
    var completionHandler: ((Int, String?, Bool) -> Void)?

    // MARK: - Initialization
    init(productionNumber: String) {
        self.productionNumber = productionNumber
        super.init(nibName: nil, bundle: nil)
        modalPresentationStyle = .pageSheet
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Lifecycle Methods
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupNavigationBar()
        setupKeyboardNotifications()
    }

    // MARK: - UI Setup
    private func setupNavigationBar() {
        title = NSLocalizedString("Report Game", comment: "Title for the report dialog")
        let closeButton = UIBarButtonItem(title: NSLocalizedString("Close", comment: "Close Dialog"), style: .plain, target: self, action: #selector(closeButtonTapped))
        navigationItem.leftBarButtonItem = closeButton
    }

    private func setupUI() {
        view.backgroundColor = .systemBackground

        // Add tap gesture to dismiss keyboard when tapping outside text fields
        let tapGesture = UITapGestureRecognizer(target: self, action: #selector(dismissKeyboard))
        tapGesture.cancelsTouchesInView = false
        view.addGestureRecognizer(tapGesture)

        // Create a scroll view to contain all elements
        let scrollView = UIScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scrollView)

        // Create a content view for the scroll view
        let contentView = UIView()
        contentView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.addSubview(contentView)

        // Setup constraints for scroll view
        NSLayoutConstraint.activate([
            scrollView.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),

            contentView.topAnchor.constraint(equalTo: scrollView.topAnchor),
            contentView.leadingAnchor.constraint(equalTo: scrollView.leadingAnchor),
            contentView.trailingAnchor.constraint(equalTo: scrollView.trailingAnchor),
            contentView.bottomAnchor.constraint(equalTo: scrollView.bottomAnchor),
            contentView.widthAnchor.constraint(equalTo: scrollView.widthAnchor)
        ])

        // Notice Label
        let noticeLabel = UILabel()
        noticeLabel.translatesAutoresizingMaskIntoConstraints = false
        noticeLabel.text = NSLocalizedString("report_notice", comment: "Notice text for the report dialog")
        noticeLabel.numberOfLines = 0
        noticeLabel.textAlignment = .center
        noticeLabel.font = UIFont.systemFont(ofSize: 14)
        contentView.addSubview(noticeLabel)

        // Game Rating Label
        let gameRatingLabel = UILabel()
        gameRatingLabel.translatesAutoresizingMaskIntoConstraints = false
        gameRatingLabel.text = NSLocalizedString("Game Rating", comment: "Label for game rating")
        gameRatingLabel.font = UIFont.boldSystemFont(ofSize: 16)
        contentView.addSubview(gameRatingLabel)

        // Game Rating Control
        gameRatingControl = StarRatingView()
        gameRatingControl.translatesAutoresizingMaskIntoConstraints = false
        gameRatingControl.setRating(3) // Default to 3
        gameRatingControl.starColor = .systemYellow // Yellow stars for game rating
        gameRatingControl.ratingDidChange = { [weak self] rating in
            print("Game rating changed to: \(rating)")
            self?.gameRating = rating
            self?.updateGameRatingText()
        }
        contentView.addSubview(gameRatingControl)

        // Game Rating Text
        gameRateText = UILabel()
        gameRateText.translatesAutoresizingMaskIntoConstraints = false
        gameRateText.text = NSLocalizedString("game_report_message_3", comment: "Game rating message for 3 stars")
        gameRateText.numberOfLines = 0
        gameRateText.textAlignment = .center
        gameRateText.font = UIFont.systemFont(ofSize: 14)
        contentView.addSubview(gameRateText)

        // Emulation Rating Label
        let emulationRatingLabel = UILabel()
        emulationRatingLabel.translatesAutoresizingMaskIntoConstraints = false
        emulationRatingLabel.text = NSLocalizedString("Emulation Rating", comment: "Label for emulation rating")
        emulationRatingLabel.font = UIFont.boldSystemFont(ofSize: 16)
        contentView.addSubview(emulationRatingLabel)

        // Emulation Rating Control
        emulationRatingControl = StarRatingView()
        emulationRatingControl.translatesAutoresizingMaskIntoConstraints = false
        emulationRatingControl.setRating(3) // Default to 3
        emulationRatingControl.starColor = .systemBlue // Blue stars for emulation rating
        emulationRatingControl.ratingDidChange = { [weak self] rating in
            print("Emulation rating changed to: \(rating)")
            self?.emulationRating = rating
            self?.updateEmulationRatingText()
        }
        contentView.addSubview(emulationRatingControl)

        // Emulation Rating Text
        emulationRateText = UILabel()
        emulationRateText.translatesAutoresizingMaskIntoConstraints = false
        emulationRateText.text = NSLocalizedString("report_message_3", comment: "Emulation rating message for 3 stars")
        emulationRateText.numberOfLines = 0
        emulationRateText.textAlignment = .center
        emulationRateText.font = UIFont.systemFont(ofSize: 14)
        contentView.addSubview(emulationRateText)

        // Message Label
        let messageLabel = UILabel()
        messageLabel.translatesAutoresizingMaskIntoConstraints = false
        messageLabel.text = NSLocalizedString("Comments", comment: "Label for comments")
        messageLabel.font = UIFont.boldSystemFont(ofSize: 16)
        contentView.addSubview(messageLabel)

        // Message Text View
        messageTextView = UITextView()
        messageTextView.translatesAutoresizingMaskIntoConstraints = false
        messageTextView.layer.borderColor = UIColor.systemGray4.cgColor
        messageTextView.layer.borderWidth = 1.0
        messageTextView.layer.cornerRadius = 5.0
        messageTextView.font = UIFont.systemFont(ofSize: 14)
        messageTextView.delegate = self
        contentView.addSubview(messageTextView)

        // Send Button
        sendButton = UIButton(type: .system)
        sendButton.translatesAutoresizingMaskIntoConstraints = false
        sendButton.setTitle(NSLocalizedString("Send", comment: "Send button title"), for: .normal)
        sendButton.backgroundColor = .systemBlue
        sendButton.setTitleColor(.white, for: .normal)
        sendButton.layer.cornerRadius = 8.0
        sendButton.addTarget(self, action: #selector(sendButtonTapped), for: .touchUpInside)
        contentView.addSubview(sendButton)

        // Setup constraints
        NSLayoutConstraint.activate([
            noticeLabel.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 20),
            noticeLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            noticeLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            gameRatingLabel.topAnchor.constraint(equalTo: noticeLabel.bottomAnchor, constant: 20),
            gameRatingLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            gameRatingLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            gameRatingControl.topAnchor.constraint(equalTo: gameRatingLabel.bottomAnchor, constant: 10),
            gameRatingControl.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            gameRatingControl.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            gameRateText.topAnchor.constraint(equalTo: gameRatingControl.bottomAnchor, constant: 10),
            gameRateText.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            gameRateText.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            emulationRatingLabel.topAnchor.constraint(equalTo: gameRateText.bottomAnchor, constant: 20),
            emulationRatingLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            emulationRatingLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            emulationRatingControl.topAnchor.constraint(equalTo: emulationRatingLabel.bottomAnchor, constant: 10),
            emulationRatingControl.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            emulationRatingControl.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            emulationRateText.topAnchor.constraint(equalTo: emulationRatingControl.bottomAnchor, constant: 10),
            emulationRateText.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            emulationRateText.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            messageLabel.topAnchor.constraint(equalTo: emulationRateText.bottomAnchor, constant: 20),
            messageLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            messageLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),

            messageTextView.topAnchor.constraint(equalTo: messageLabel.bottomAnchor, constant: 10),
            messageTextView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            messageTextView.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            messageTextView.heightAnchor.constraint(equalToConstant: 100),

            sendButton.topAnchor.constraint(equalTo: messageTextView.bottomAnchor, constant: 20),
            sendButton.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            sendButton.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            sendButton.heightAnchor.constraint(equalToConstant: 44),
            sendButton.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -20)
        ])
    }

    // MARK: - Actions
    @objc private func closeButtonTapped() {
        dismiss(animated: true)
    }

    @objc private func dismissKeyboard() {
        view.endEditing(true)
    }

    // Rating changes are now handled by the StarRatingView's ratingDidChange closure

    @objc private func sendButtonTapped() {
        handleSendReport()
    }

    // MARK: - Helper Methods
    private func updateGameRatingText() {
        switch gameRating {
        case 1:
            gameRateText.text = NSLocalizedString("game_report_message_1", comment: "Game rating message for 1 star")
        case 2:
            gameRateText.text = NSLocalizedString("game_report_message_2", comment: "Game rating message for 2 stars")
        case 3:
            gameRateText.text = NSLocalizedString("game_report_message_3", comment: "Game rating message for 3 stars")
        case 4:
            gameRateText.text = NSLocalizedString("game_report_message_4", comment: "Game rating message for 4 stars")
        case 5:
            gameRateText.text = NSLocalizedString("game_report_message_5", comment: "Game rating message for 5 stars")
        default:
            gameRateText.text = NSLocalizedString("game_report_message_3", comment: "Game rating message for 3 stars")
        }
    }

    private func updateEmulationRatingText() {
        switch emulationRating {
        case 1:
            emulationRateText.text = NSLocalizedString("report_message_1", comment: "Emulation rating message for 1 star")
        case 2:
            emulationRateText.text = NSLocalizedString("report_message_2", comment: "Emulation rating message for 2 stars")
        case 3:
            emulationRateText.text = NSLocalizedString("report_message_3", comment: "Emulation rating message for 3 stars")
        case 4:
            emulationRateText.text = NSLocalizedString("report_message_4", comment: "Emulation rating message for 4 stars")
        case 5:
            emulationRateText.text = NSLocalizedString("report_message_5", comment: "Emulation rating message for 5 stars")
        default:
            emulationRateText.text = NSLocalizedString("report_message_3", comment: "Emulation rating message for 3 stars")
        }
    }

    private func handleSendReport() {
        // Check if user is signed in
        guard let currentUser = Auth.auth().currentUser else {
            showAlert(title: NSLocalizedString("Sign In Required", comment: "Title for sign in required alert"),
                     message: NSLocalizedString("You need to sign in to submit a report.", comment: "Message for sign in required"))
            return
        }

        let message = messageTextView.text ?? ""

        // Initialize Firestore
        let db = Firestore.firestore()
        let userId = currentUser.uid

        // Create rating document data
        var ratingData: [String: Any] = [
            "rating": gameRating,
            "emulation_rating": emulationRating,
            "comment": message,
            "uid": userId,
            "display_name": currentUser.displayName ?? "",
            "photo_url": currentUser.photoURL?.absoluteString ?? "",
            "platform": "ios",
            "version": Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "",
            "version_code": Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "",
            "timestamp": FieldValue.serverTimestamp(),
            "isVisible": true
        ]

        // Search in games collection by production_number
        db.collection("games")
            .whereField("product_number", isEqualTo: productionNumber)
            .getDocuments { [weak self] querySnapshot, error in
                guard let self = self else { return }

                if let error = error {
                    self.showAlert(title: NSLocalizedString("Error", comment: "Error title"),
                                  message: error.localizedDescription)
                    return
                }

                if let querySnapshot = querySnapshot, querySnapshot.documents.isEmpty {
                    // Game not found, create a new game document first
                    let gameData: [String: Any] = [
                        "product_number": self.productionNumber,
                        "created_at": FieldValue.serverTimestamp()
                    ]

                    db.collection("games")
                        .addDocument(data: gameData) { error in
                            if let error = error {
                                self.showAlert(title: NSLocalizedString("Error", comment: "Error title"),
                                              message: error.localizedDescription)
                                return
                            }

                            // Add rating to the ratings subcollection with userId as document ID
                            db.collection("games").document(self.productionNumber).collection("ratings")
                                .document(userId)
                                .setData(ratingData) { error in
                                    if let error = error {
                                        self.showAlert(title: NSLocalizedString("Error", comment: "Error title"),
                                                      message: error.localizedDescription)
                                    } else {
                                        self.showAlert(title: NSLocalizedString("Success", comment: "Success title"),
                                                      message: NSLocalizedString("report_sent_success", comment: "Report sent successfully message")) {
                                            self.completionHandler?(self.gameRating, message, false)
                                            self.dismiss(animated: true)
                                        }
                                    }
                                }
                        }
                } else if let querySnapshot = querySnapshot, !querySnapshot.documents.isEmpty {
                    // Game exists, add rating to its ratings subcollection with userId as document ID
                    let gameDoc = querySnapshot.documents[0]
                    gameDoc.reference.collection("ratings")
                        .document(userId)
                        .setData(ratingData) { [weak self] error in
                            guard let self = self else { return }

                            if let error = error {
                                self.showAlert(title: NSLocalizedString("Error", comment: "Error title"),
                                              message: error.localizedDescription)
                            } else {
                                self.showAlert(title: NSLocalizedString("Success", comment: "Success title"),
                                              message: NSLocalizedString("report_sent_success", comment: "Report sent successfully message")) {
                                    self.completionHandler?(self.gameRating, message, false)
                                    self.dismiss(animated: true)
                                }
                            }
                        }
                }
            }
    }

    private func showAlert(title: String, message: String, completion: (() -> Void)? = nil) {
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button title"), style: .default) { _ in
            completion?()
        })
        present(alert, animated: true)
    }

    // MARK: - Keyboard Handling
    private func setupKeyboardNotifications() {
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillShow(_:)), name: UIResponder.keyboardWillShowNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(keyboardWillHide(_:)), name: UIResponder.keyboardWillHideNotification, object: nil)
    }

    // MARK: - UITextViewDelegate
    func textViewDidBeginEditing(_ textView: UITextView) {
        // The keyboard will show notification will handle positioning
    }

    func textViewDidEndEditing(_ textView: UITextView) {
        // The keyboard will hide notification will handle restoring position
    }

    func textView(_ textView: UITextView, shouldChangeTextIn range: NSRange, replacementText text: String) -> Bool {
        // Allow the user to use the return key within the text view
        return true
    }

    deinit {
        NotificationCenter.default.removeObserver(self)
    }

    @objc private func keyboardWillShow(_ notification: Notification) {
        guard let keyboardFrame = notification.userInfo?[UIResponder.keyboardFrameEndUserInfoKey] as? CGRect else { return }

        // Get the keyboard animation duration and curve
        let duration = notification.userInfo?[UIResponder.keyboardAnimationDurationUserInfoKey] as? Double ?? 0.3
        let curveValue = notification.userInfo?[UIResponder.keyboardAnimationCurveUserInfoKey] as? Int ?? UIView.AnimationCurve.easeInOut.rawValue
        let curve = UIView.AnimationCurve(rawValue: curveValue) ?? .easeInOut

        // Convert keyboard frame to our view's coordinate system
        let keyboardFrameInView = view.convert(keyboardFrame, from: nil)

        // Find the scroll view
        let scrollView = view.subviews.first(where: { $0 is UIScrollView }) as? UIScrollView

        UIView.animate(withDuration: duration, delay: 0, options: UIView.AnimationOptions(rawValue: UInt(curve.rawValue << 16)), animations: {
            // Adjust the scroll view's content inset to make room for the keyboard
            if let scrollView = scrollView {
                let bottomInset = keyboardFrameInView.height
                scrollView.contentInset.bottom = bottomInset
                scrollView.scrollIndicatorInsets.bottom = bottomInset

                // Scroll to make the text view visible if it's the first responder
                if self.messageTextView.isFirstResponder {
                    let textViewFrame = self.messageTextView.convert(self.messageTextView.bounds, to: scrollView)
                    scrollView.scrollRectToVisible(textViewFrame, animated: false)
                }
            }
        }, completion: nil)
    }

    @objc private func keyboardWillHide(_ notification: Notification) {
        // Get the keyboard animation duration and curve
        let duration = notification.userInfo?[UIResponder.keyboardAnimationDurationUserInfoKey] as? Double ?? 0.3
        let curveValue = notification.userInfo?[UIResponder.keyboardAnimationCurveUserInfoKey] as? Int ?? UIView.AnimationCurve.easeInOut.rawValue
        let curve = UIView.AnimationCurve(rawValue: curveValue) ?? .easeInOut

        // Reset scroll view insets if we adjusted them
        if let scrollView = view.subviews.first(where: { $0 is UIScrollView }) as? UIScrollView {
            UIView.animate(withDuration: duration, delay: 0, options: UIView.AnimationOptions(rawValue: UInt(curve.rawValue << 16)), animations: {
                scrollView.contentInset.bottom = 0
                scrollView.scrollIndicatorInsets.bottom = 0
            }, completion: nil)
        }
    }
}
