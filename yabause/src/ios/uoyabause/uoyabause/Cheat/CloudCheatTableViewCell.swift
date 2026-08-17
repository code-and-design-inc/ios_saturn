import UIKit

protocol CloudCheatTableViewCellDelegate: AnyObject {
    func didToggleActivation(_ cell: CloudCheatTableViewCell, isActive: Bool)
    func didToggleLike(_ cell: CloudCheatTableViewCell)
}

class CloudCheatTableViewCell: UITableViewCell {
    weak var delegate: CloudCheatTableViewCellDelegate?
    private var currentCheat: CloudCheatCode?
    static let identifier = "CloudCheatTableViewCell"
    
    private let descriptionLabel: UILabel = {
        let label = UILabel()
        label.numberOfLines = 0
        label.font = .systemFont(ofSize: 14)
        return label
    }()
    
    private let keyLabel: UILabel = {
        let label = UILabel()
        label.font = .systemFont(ofSize: 12)
        label.textColor = .gray
        label.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
        label.setContentHuggingPriority(.defaultLow, for: .horizontal)
        return label
    }()
    
    private let activationSwitch: UISwitch = {
        let toggle = UISwitch()
        return toggle
    }()
    
    private let likeButton: UIButton = {
        let button = UIButton(type: .system)
        button.setImage(UIImage(systemName: "heart"), for: .normal)
        button.tintColor = .systemRed
        return button
    }()
    
    private let likeCountLabel: UILabel = {
        let label = UILabel()
        label.font = .systemFont(ofSize: 12)
        label.textColor = .gray
        return label
    }()
    
    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: style, reuseIdentifier: reuseIdentifier)
        setupUI()
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    private func setupUI() {
        contentView.addSubview(descriptionLabel)
        contentView.addSubview(keyLabel)
        contentView.addSubview(likeButton)
        contentView.addSubview(likeCountLabel)
        contentView.addSubview(activationSwitch)
        
        activationSwitch.addTarget(self, action: #selector(switchValueChanged), for: .valueChanged)
        likeButton.addTarget(self, action: #selector(likeButtonTapped), for: .touchUpInside)
        
        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        keyLabel.translatesAutoresizingMaskIntoConstraints = false
        likeButton.translatesAutoresizingMaskIntoConstraints = false
        likeCountLabel.translatesAutoresizingMaskIntoConstraints = false
        activationSwitch.translatesAutoresizingMaskIntoConstraints = false
        
        NSLayoutConstraint.activate([
            descriptionLabel.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 8),
            descriptionLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 16),
            descriptionLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -16),
            
            keyLabel.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: 4),
            keyLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 16),
            
            likeButton.centerYAnchor.constraint(equalTo: keyLabel.centerYAnchor),
            likeButton.leadingAnchor.constraint(equalTo: keyLabel.trailingAnchor, constant: 16),
            likeButton.widthAnchor.constraint(equalToConstant: 24),
            likeButton.heightAnchor.constraint(equalToConstant: 24),
            
            likeCountLabel.centerYAnchor.constraint(equalTo: likeButton.centerYAnchor),
            likeCountLabel.leadingAnchor.constraint(equalTo: likeButton.trailingAnchor, constant: 4),
            likeCountLabel.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -8),
            
            activationSwitch.centerYAnchor.constraint(equalTo: keyLabel.centerYAnchor),
            activationSwitch.leadingAnchor.constraint(equalTo: likeCountLabel.trailingAnchor, constant: 16),
            activationSwitch.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -16)
        ])
    }
    
    @objc private func switchValueChanged() {
        delegate?.didToggleActivation(self, isActive: activationSwitch.isOn)
    }
    
    @objc private func likeButtonTapped() {
        delegate?.didToggleLike(self)
    }
    
    private func updateLikeStatus(isLiked: Bool, count: Int) {
        likeButton.setImage(UIImage(systemName: isLiked ? "heart.fill" : "heart"), for: .normal)
        likeCountLabel.text = "\(count)"
    }
    
    func configure(with cheat: CloudCheatCode) {
        currentCheat = cheat
        descriptionLabel.text = cheat.description
        keyLabel.text = "code: \(cheat.code)"
        activationSwitch.isOn = cheat.isActive
        updateLikeStatus(isLiked: cheat.like_users.contains(Auth.auth().currentUser?.uid ?? ""), count: cheat.star_count)
    }
}
