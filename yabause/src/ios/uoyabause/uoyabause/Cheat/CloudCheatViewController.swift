import UIKit
import FirebaseDatabase
import FirebaseAuth

// クラウド用のチートコードモデル
struct CloudCheatCode {
    let baseCheat: CheatCode
    let key: String
    let star_count: Int
    let like_users: [String]
    var isActive: Bool
    
    init(baseCheat: CheatCode, key: String, star_count: Int, like_users: [String], isActive: Bool) {
        self.baseCheat = baseCheat
        self.key = key
        self.star_count = star_count
        self.like_users = like_users
        self.isActive = isActive
    }
    
    static func from(snapshot: DataSnapshot) -> CloudCheatCode? {
        guard let dict = snapshot.value as? [String: Any],
              let code = dict["cheat_code"] as? String,
              let description = dict["description"] as? String else {
            return nil
        }
        
        let starCount = dict["star_count"] as? Int ?? 0
        let likeUsers = (dict["like_users"] as? [String: Bool])?.compactMap { $0.key } ?? []
        
        return CloudCheatCode(
            baseCheat: CheatCode(code: code, description: description),
            key: snapshot.key,
            star_count: starCount,
            like_users: likeUsers,
            isActive: false
        )
    }
    
    var code: String { baseCheat.code }
    var description: String { baseCheat.description }
}

class CloudCheatViewController: UIViewController {
    private var database: DatabaseReference?
    
    private var gameId: String
    private weak var delegate: CheatViewControllerDelegate?
    private var cheatCodes: [CloudCheatCode] = []
    private var isLoading = false
    
    private lazy var tableView: UITableView = {
        let table = UITableView(frame: .zero, style: .plain)
        table.delegate = self
        table.dataSource = self
        table.register(CloudCheatTableViewCell.self, forCellReuseIdentifier: CloudCheatTableViewCell.identifier)
        table.rowHeight = UITableView.automaticDimension
        table.estimatedRowHeight = 80
        return table
    }()
    
    private lazy var activityIndicator: UIActivityIndicatorView = {
        let indicator = UIActivityIndicatorView(style: .medium)
        indicator.hidesWhenStopped = true
        return indicator
    }()
    
    // MARK: - Initialization
    
    static func newInstance(gameId: String, delegate: CheatViewControllerDelegate) -> CloudCheatViewController {
        return CloudCheatViewController(gameId: gameId, delegate: delegate)
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
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        loadCloudCheats()
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        view.addSubview(tableView)
        view.addSubview(activityIndicator)
        
        tableView.translatesAutoresizingMaskIntoConstraints = false
        activityIndicator.translatesAutoresizingMaskIntoConstraints = false
        
        NSLayoutConstraint.activate([
            tableView.topAnchor.constraint(equalTo: view.topAnchor),
            tableView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            tableView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            tableView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            
            activityIndicator.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            activityIndicator.centerYAnchor.constraint(equalTo: view.centerYAnchor)
        ])
    }
    
    // MARK: - Data Loading
    
    private func loadCloudCheats() {
        guard !isLoading else { return }
        isLoading = true
        activityIndicator.startAnimating()
        
        let baseRef = Database.database().reference()
        let baseUrl = "/shared-cheats/\(gameId)"
        database = baseRef.child(baseUrl)
        
        guard let database = database else {
            isLoading = false
            activityIndicator.stopAnimating()
            return
        }
        
        database.observe(.value) { [weak self] (snapshot: DataSnapshot) in
            guard let self = self else { return }
            self.isLoading = false
            self.activityIndicator.stopAnimating()
            
            if snapshot.hasChildren() {
                var newCheats: [CloudCheatCode] = []
                
                for child in snapshot.children {
                    guard let snapshot = child as? DataSnapshot,
                          let cheat = CloudCheatCode.from(snapshot: snapshot) else { continue }
                    let isEnabled = self.delegate?.isActive(cheat.baseCheat.code) ?? false
                    let updatedCheat = CloudCheatCode(
                        baseCheat: cheat.baseCheat,
                        key: cheat.key,
                        star_count: cheat.star_count,
                        like_users: cheat.like_users,
                        isActive: isEnabled)
                    newCheats.append(updatedCheat)
                }
                
                self.cheatCodes = newCheats.sorted { $0.star_count > $1.star_count }
                self.tableView.reloadData()
            }
        } withCancel: { [weak self] (error: Error) in
            guard let self = self else { return }
            self.isLoading = false
            self.activityIndicator.stopAnimating()
            self.showError(error)
        }
    }
    
    private func showError(_ error: Error) {
        let alert = UIAlertController(
            title: "エラー",
            message: "チートコードの取得に失敗しました: \(error.localizedDescription)",
            preferredStyle: .alert
        )
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
}

// MARK: - CloudCheatTableViewCellDelegate

extension CloudCheatViewController: CloudCheatTableViewCellDelegate {
    func didToggleActivation(_ cell: CloudCheatTableViewCell, isActive: Bool) {
        guard let indexPath = tableView.indexPath(for: cell) else { return }
        let cheat = cheatCodes[indexPath.row]
        
        if isActive {
            delegate?.addActiveCheat(cheat.code)
        } else {
            delegate?.removeActiveCheat(cheat.code)
        }
    }
    
    func didToggleLike(_ cell: CloudCheatTableViewCell) {
        guard let indexPath = tableView.indexPath(for: cell),
              let database = database,
              let userId = Auth.auth().currentUser?.uid else { return }
        
        let cheat = cheatCodes[indexPath.row]
        let likeRef = database.child(cheat.key).child("like_users").child(userId)
        
        let cheatRef = database.child(cheat.key)
        if cheat.like_users.contains(userId) {
            // いいねを解除
            likeRef.removeValue()
            cheatRef.child("star_count").runTransactionBlock { (currentData: MutableData) -> TransactionResult in
                var value = currentData.value as? Int ?? 0
                value = max(0, value - 1)
                currentData.value = value
                return TransactionResult.success(withValue: currentData)
            }
        } else {
            // いいねを追加
            likeRef.setValue(true)
            cheatRef.child("star_count").runTransactionBlock { (currentData: MutableData) -> TransactionResult in
                var value = currentData.value as? Int ?? 0
                value += 1
                currentData.value = value
                return TransactionResult.success(withValue: currentData)
            }
        }
    }
}

// MARK: - UITableViewDelegate, UITableViewDataSource

extension CloudCheatViewController: UITableViewDelegate, UITableViewDataSource {
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return cheatCodes.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        guard let cell = tableView.dequeueReusableCell(withIdentifier: CloudCheatTableViewCell.identifier, for: indexPath) as? CloudCheatTableViewCell else {
            return UITableViewCell()
        }
        let cheat = cheatCodes[indexPath.row]
        cell.delegate = self
        cell.configure(with: cheat)
        return cell
    }
}
