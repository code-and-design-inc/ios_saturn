import UIKit

protocol LocalCheatTableViewCellDelegate: AnyObject {
    func didToggleActivation(_ cell: LocalCheatTableViewCell, isActive: Bool)
    func didTapEdit(_ cell: LocalCheatTableViewCell)
    func didTapDelete(_ cell: LocalCheatTableViewCell)
    func didTapShare(_ cell: LocalCheatTableViewCell)
}

class LocalCheatTableViewCell: UITableViewCell {
    weak var delegate: LocalCheatTableViewCellDelegate?
    private var currentCheat: CheatCode?
    static let identifier = "LocalCheatTableViewCell"
    
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
    
    private let buttonStackView: UIStackView = {
        let stackView = UIStackView()
        stackView.axis = .horizontal
        stackView.spacing = 12
        stackView.distribution = .fillEqually
        return stackView
    }()
    
    private lazy var editButton: UIButton = {
        let button = UIButton(type: .system)
        button.setImage(UIImage(systemName: "pencil"), for: .normal)
        button.tintColor = .systemBlue
        button.addTarget(self, action: #selector(editButtonTapped), for: .touchUpInside)
        return button
    }()
    
    private lazy var deleteButton: UIButton = {
        let button = UIButton(type: .system)
        button.setImage(UIImage(systemName: "trash"), for: .normal)
        button.tintColor = .systemRed
        button.addTarget(self, action: #selector(deleteButtonTapped), for: .touchUpInside)
        return button
    }()
    
    private lazy var shareButton: UIButton = {
        let button = UIButton(type: .system)
        button.setImage(UIImage(systemName: "square.and.arrow.up"), for: .normal)
        button.tintColor = .systemGreen
        button.addTarget(self, action: #selector(shareButtonTapped), for: .touchUpInside)
        return button
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
        contentView.addSubview(activationSwitch)
        contentView.addSubview(buttonStackView)
        
        buttonStackView.addArrangedSubview(editButton)
        buttonStackView.addArrangedSubview(deleteButton)
        buttonStackView.addArrangedSubview(shareButton)
        
        activationSwitch.addTarget(self, action: #selector(switchValueChanged), for: .valueChanged)
        
        descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
        keyLabel.translatesAutoresizingMaskIntoConstraints = false
        activationSwitch.translatesAutoresizingMaskIntoConstraints = false
        buttonStackView.translatesAutoresizingMaskIntoConstraints = false
        
        NSLayoutConstraint.activate([
            descriptionLabel.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 8),
            descriptionLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 16),
            descriptionLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -16),
            
            keyLabel.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: 4),
            keyLabel.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 16),
            keyLabel.trailingAnchor.constraint(equalTo: activationSwitch.leadingAnchor, constant: -8),
            
            activationSwitch.centerYAnchor.constraint(equalTo: keyLabel.centerYAnchor),
            activationSwitch.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -16),
            
            buttonStackView.topAnchor.constraint(equalTo: keyLabel.bottomAnchor, constant: 8),
            buttonStackView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 16),
            buttonStackView.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -8),
            buttonStackView.widthAnchor.constraint(equalToConstant: 120)
        ])
    }
    
    @objc private func switchValueChanged() {
        delegate?.didToggleActivation(self, isActive: activationSwitch.isOn)
    }
    
    @objc private func editButtonTapped() {
        delegate?.didTapEdit(self)
    }
    
    @objc private func deleteButtonTapped() {
        delegate?.didTapDelete(self)
    }
    
    @objc private func shareButtonTapped() {
        delegate?.didTapShare(self)
    }
    
    func configure(with cheat: CheatCode, isActive: Bool) {
        currentCheat = cheat
        descriptionLabel.text = cheat.description
        keyLabel.text = "code: \(cheat.code)"
        activationSwitch.isOn = isActive
    }
}
