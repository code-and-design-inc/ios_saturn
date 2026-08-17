import UIKit
import FirebaseDatabase
import FirebaseAuth

class LocalCheatViewController: UIViewController, LocalCheatTableViewCellDelegate {
    private var database: DatabaseReference?

    private var gameId: String
    private weak var delegate: CheatViewControllerDelegate?
    private var cheatCodes: [CheatCode] = []
    private var dataSnapshot: DataSnapshot?

    private lazy var tableView: UITableView = {
        let table = UITableView(frame: .zero, style: .plain)
        table.delegate = self
        table.dataSource = self
        table.register(LocalCheatTableViewCell.self, forCellReuseIdentifier: LocalCheatTableViewCell.identifier)
        return table
    }()

    private lazy var addButton: UIButton = {
        let button = UIButton(type: .system)
        button.setTitle(NSLocalizedString("add_action_replay_code", comment: ""), for: .normal)
        button.backgroundColor = .secondary
        button.setTitleColor(.appWhite, for: .normal)
        button.layer.cornerRadius = 8
        button.addTarget(self, action: #selector(addButtonTapped), for: .touchUpInside)
        return button
    }()

    // MARK: - Initialization

    static func newInstance(gameId: String, delegate: CheatViewControllerDelegate) -> LocalCheatViewController {
        return LocalCheatViewController(gameId: gameId, delegate: delegate)
    }

    init(gameId: String, delegate: CheatViewControllerDelegate) {
        self.gameId = gameId
        self.delegate = delegate
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Lifecycle

    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        loadLocalCheats()
    }

    // MARK: - Setup

    private func showCheatInputAlert(title: String, cheat: CheatCode? = nil) {
        let customVC = UIViewController()
        customVC.preferredContentSize = CGSize(width: 300, height: 250)

        let descriptionTextField = UITextField()
        descriptionTextField.placeholder = NSLocalizedString("description_placeholder", comment: "")
        descriptionTextField.borderStyle = .roundedRect
        descriptionTextField.text = cheat?.description

        let textView = UITextView()
        textView.font = .systemFont(ofSize: 14)
        textView.layer.borderWidth = 1
        textView.layer.cornerRadius = 5
        textView.delegate = self

        if let cheat = cheat {
            textView.text = cheat.code
            textView.textColor = .appBlack
        } else {
            textView.text = NSLocalizedString("cheat_code_example", comment: "")
            textView.textColor = .appDisable
        }

        customVC.view.addSubview(descriptionTextField)
        customVC.view.addSubview(textView)

        descriptionTextField.translatesAutoresizingMaskIntoConstraints = false
        textView.translatesAutoresizingMaskIntoConstraints = false

        NSLayoutConstraint.activate([
            descriptionTextField.topAnchor.constraint(equalTo: customVC.view.topAnchor, constant: 8),
            descriptionTextField.leadingAnchor.constraint(equalTo: customVC.view.leadingAnchor, constant: 8),
            descriptionTextField.trailingAnchor.constraint(equalTo: customVC.view.trailingAnchor, constant: -8),
            descriptionTextField.heightAnchor.constraint(equalToConstant: 40),

            textView.topAnchor.constraint(equalTo: descriptionTextField.bottomAnchor, constant: 8),
            textView.leadingAnchor.constraint(equalTo: customVC.view.leadingAnchor, constant: 8),
            textView.trailingAnchor.constraint(equalTo: customVC.view.trailingAnchor, constant: -8),
            textView.bottomAnchor.constraint(equalTo: customVC.view.bottomAnchor, constant: -8)
        ])

        let alertController = UIAlertController(
            title: title,
            message: NSLocalizedString("enter_action_replay_code", comment: ""),
            preferredStyle: .alert
        )

        alertController.setValue(customVC, forKey: "contentViewController")

        let saveAction = UIAlertAction(title: cheat == nil ? NSLocalizedString("add", comment: "") : NSLocalizedString("save", comment: ""), style: .default) { [weak self] _ in
            guard let self = self,
                  let description = descriptionTextField.text,
                  let code = textView.text,
                  !description.isEmpty,
                  !code.isEmpty,
                  let userId = Auth.auth().currentUser?.uid,
                  self.validateCheatCode(code) else { return }

            let lines = code.components(separatedBy: .newlines)
            let validatedCodes = lines.filter { !$0.trimmingCharacters(in: .whitespaces).isEmpty }
                                    .map { $0.trimmingCharacters(in: .whitespaces) }
                                    .joined(separator: "\n")

            let cheatData: [String: Any] = [
                "description": description,
                "cheat_code": validatedCodes,
                "created_at": ServerValue.timestamp()
            ]

            if let cheat = cheat, let key = self.getCheatKey(for: cheat) {
                self.database?.child(key).updateChildValues(cheatData)
            } else {
                self.database?.childByAutoId().setValue(cheatData)
            }
        }

        let cancelAction = UIAlertAction(title: NSLocalizedString("Cancel", comment: ""), style: .cancel)

        alertController.addAction(saveAction)
        alertController.addAction(cancelAction)

        present(alertController, animated: true)
    }

    @objc private func addButtonTapped() {
        showCheatInputAlert(title: NSLocalizedString("add_action_replay_code", comment: ""))
    }

    private func setupUI() {
        view.addSubview(tableView)
        view.addSubview(addButton)

        tableView.translatesAutoresizingMaskIntoConstraints = false
        addButton.translatesAutoresizingMaskIntoConstraints = false

        NSLayoutConstraint.activate([
            tableView.topAnchor.constraint(equalTo: view.topAnchor),
            tableView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            tableView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            tableView.bottomAnchor.constraint(equalTo: addButton.topAnchor, constant: -8),

            addButton.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 16),
            addButton.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -16),
            addButton.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -8),
            addButton.heightAnchor.constraint(equalToConstant: 44)
        ])
    }

    // MARK: - Data Loading

    private func loadLocalCheats() {
        guard let userId = Auth.auth().currentUser?.uid else { return }

        let baseRef = Database.database().reference()
        let baseUrl = "/user-posts/\(userId)/cheat/\(gameId)"
        database = baseRef.child(baseUrl)

        database?.observe(.value) { [weak self] snapshot in
            guard let self = self else { return }

            self.dataSnapshot = snapshot
            var newCheats: [CheatCode] = []

            for child in snapshot.children {
                guard let snapshot = child as? DataSnapshot,
                      let dict = snapshot.value as? [String: Any],
                      let code = dict["cheat_code"] as? String,
                      let description = dict["description"] as? String else { continue }

                newCheats.append(CheatCode(code: code, description: description))
            }

            self.cheatCodes = newCheats
            self.tableView.reloadData()
        }
    }

    private func getCheatKey(for cheat: CheatCode) -> String? {
        guard let snapshot = dataSnapshot else { return nil }

        for child in snapshot.children {
            guard let snapshot = child as? DataSnapshot,
                  let dict = snapshot.value as? [String: Any],
                  let code = dict["cheat_code"] as? String,
                  let description = dict["description"] as? String,
                  code == cheat.code && description == cheat.description else { continue }

            return snapshot.key
        }

        return nil
    }
}

// MARK: - UITableViewDelegate, UITableViewDataSource

extension LocalCheatViewController: UITableViewDelegate, UITableViewDataSource {
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return cheatCodes.count
    }

    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        guard let cell = tableView.dequeueReusableCell(withIdentifier: LocalCheatTableViewCell.identifier, for: indexPath) as? LocalCheatTableViewCell else {
            return UITableViewCell()
        }
        let cheat = cheatCodes[indexPath.row]
        cell.delegate = self
        cell.configure(with: cheat, isActive: delegate?.isActive(cheat.code) ?? false)
        return cell
    }
}

// MARK: - LocalCheatTableViewCellDelegate

extension LocalCheatViewController {
    func didToggleActivation(_ cell: LocalCheatTableViewCell, isActive: Bool) {
        guard let indexPath = tableView.indexPath(for: cell) else { return }
        let cheat = cheatCodes[indexPath.row]

        if isActive {
            delegate?.addActiveCheat(cheat.code)
        } else {
            delegate?.removeActiveCheat(cheat.code)
        }
    }

    func didTapEdit(_ cell: LocalCheatTableViewCell) {
        guard let indexPath = tableView.indexPath(for: cell) else { return }
        let cheat = cheatCodes[indexPath.row]
        showCheatInputAlert(title: NSLocalizedString("edit_action_replay_code", comment: ""), cheat: cheat)
    }

    func didTapDelete(_ cell: LocalCheatTableViewCell) {
        guard let indexPath = tableView.indexPath(for: cell) else { return }
        let cheat = cheatCodes[indexPath.row]

        let alert = UIAlertController(
            title: NSLocalizedString("delete_action_replay_code", comment: ""),
            message: NSLocalizedString("confirm_delete_code", comment: ""),
            preferredStyle: .alert
        )

        alert.addAction(UIAlertAction(title: NSLocalizedString("Delete", comment: ""), style: .destructive) { [weak self] _ in
            guard let self = self,
                  let key = self.getCheatKey(for: cheat) else { return }

            self.database?.child(key).removeValue()
        })

        alert.addAction(UIAlertAction(title: NSLocalizedString("Cancel", comment: ""), style: .cancel))
        present(alert, animated: true)
    }

    func didTapShare(_ cell: LocalCheatTableViewCell) {
        guard let indexPath = tableView.indexPath(for: cell) else { return }
        let cheat = cheatCodes[indexPath.row]

        // shared-cheats/{gameId}にデータをコピー
        let baseRef = Database.database().reference()
        let sharedRef = baseRef.child("shared-cheats").child(gameId)

        let sharedData: [String: Any] = [
            "cheat_code": cheat.code,
            "description": cheat.description,
            "created_at": ServerValue.timestamp(),
            "like_users": [:] // 空の辞書で初期化
        ]

        sharedRef.childByAutoId().setValue(sharedData) { error, _ in
            if let error = error {
                // エラー処理
                let alert = UIAlertController(
                    title: NSLocalizedString("Error", comment: ""),
                    message: String(format: NSLocalizedString("failed_to_share", comment: ""), error.localizedDescription),
                    preferredStyle: .alert
                )
                alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: ""), style: .default))
                self.present(alert, animated: true)
            } else {
                // 成功通知
                let alert = UIAlertController(
                    title: NSLocalizedString("success", comment: ""),
                    message: NSLocalizedString("code_shared", comment: ""),
                    preferredStyle: .alert
                )
                alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: ""), style: .default))
                self.present(alert, animated: true)
            }
        }
    }
}

// MARK: - UITextViewDelegate

extension LocalCheatViewController: UITextViewDelegate {
    func textViewDidBeginEditing(_ textView: UITextView) {
        if textView.textColor == .appDisable {
            textView.text = ""
            textView.textColor = .appBlack
        }
    }

    func textViewDidEndEditing(_ textView: UITextView) {
        if textView.text.isEmpty {
            textView.text = NSLocalizedString("cheat_code_example", comment: "")
            textView.textColor = .appDisable
        }
    }

    func textViewDidChange(_ textView: UITextView) {
        guard !textView.text.isEmpty && textView.textColor != .appDisable else {
            textView.layer.borderColor = UIColor.appDisable.cgColor
            return
        }

        let lines = textView.text.components(separatedBy: .newlines)
        var isValid = true

        // 空の行を除外して検証
        let validLines = lines.filter { !$0.trimmingCharacters(in: .whitespaces).isEmpty }

        for line in validLines {
            let trimmedLine = line.trimmingCharacters(in: .whitespaces)
            // 正規表現パターン: [0-9A-Fa-f]{1}[0-9A-Fa-f]{7} [0-9A-Fa-f]{4}
            // [0-9A-Fa-f]{1}: 拡張コード1桁（16進数）
            // [0-9A-Fa-f]{7}: アドレス7桁（16進数）
            // スペース
            // [0-9A-Fa-f]{4}: データ4桁（16進数）
            let pattern = "^[0-9A-Fa-f]{1}[0-9A-Fa-f]{7} [0-9A-Fa-f]{4}$"

            if let regex = try? NSRegularExpression(pattern: pattern),
               let _ = regex.firstMatch(in: trimmedLine,
                                      range: NSRange(trimmedLine.startIndex..<trimmedLine.endIndex,
                                                   in: trimmedLine)) {
                continue
            }
            isValid = false
            break
        }

        // 視覚的なフィードバック
        textView.layer.borderColor = (isValid ? UIColor.colorPrimary : UIColor.appError).cgColor
    }

    // MARK: - Validation

    private func validateCheatCode(_ code: String) -> Bool {
        let lines = code.components(separatedBy: .newlines)

        // 空の行を除外して検証
        let validLines = lines.filter { !$0.trimmingCharacters(in: .whitespaces).isEmpty }

        for line in validLines {
            let trimmedLine = line.trimmingCharacters(in: .whitespaces)

            // 正規表現パターン: [0-9A-Fa-f]{1}[0-9A-Fa-f]{7} [0-9A-Fa-f]{4}
            // [0-9A-Fa-f]{1}: 拡張コード1桁（16進数）
            // [0-9A-Fa-f]{7}: アドレス7桁（16進数）
            // スペース
            // [0-9A-Fa-f]{4}: データ4桁（16進数）
            let pattern = "^[0-9A-Fa-f]{1}[0-9A-Fa-f]{7} [0-9A-Fa-f]{4}$"

            guard let regex = try? NSRegularExpression(pattern: pattern) else {
                return false
            }

            let range = NSRange(trimmedLine.startIndex..<trimmedLine.endIndex, in: trimmedLine)
            if regex.firstMatch(in: trimmedLine, range: range) == nil {
                // フォーマットが一致しない場合はエラーダイアログを表示
                DispatchQueue.main.async {
                    let alert = UIAlertController(
                        title: NSLocalizedString("Error", comment: ""),
                        message: NSLocalizedString("invalid_format", comment: ""),
                        preferredStyle: .alert
                    )
                    alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: ""), style: .default))
                    self.present(alert, animated: true)
                }
                return false
            }
        }

        return true
    }
}

// MARK: - CheatCode Model

struct CheatCode {
    let code: String
    let description: String
}
