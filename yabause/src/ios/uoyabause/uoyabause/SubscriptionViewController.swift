//
//  SubscriptionViewController.swift
//  YabaSnashiro
//
//  Created by Claude on 2025/07/31.
//

import UIKit
import StoreKit
import Combine

class SubscriptionViewController: UIViewController {
    
    private var scrollView: UIScrollView!
    private var contentView: UIView!
    private var subscriptionStackView: UIStackView!
    private var loadingIndicator: UIActivityIndicatorView!
    private var cancellables = Set<AnyCancellable>()
    
    private var monthlyProductButton: UIButton!
    private var yearlyProductButton: UIButton!
    private var restoreButton: UIButton!
    private var closeButton: UIButton!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupSubscriptions()
        loadProducts()
    }
    
    private func setupUI() {
        view.backgroundColor = .systemBackground
        
        // Close button
        closeButton = UIButton(type: .system)
        closeButton.setTitle("✕", for: .normal)
        closeButton.titleLabel?.font = UIFont.systemFont(ofSize: 24, weight: .medium)
        closeButton.addTarget(self, action: #selector(closeButtonTapped), for: .touchUpInside)
        closeButton.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(closeButton)
        
        // Scroll view
        scrollView = UIScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(scrollView)
        
        // Content view
        contentView = UIView()
        contentView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.addSubview(contentView)
        
        // Main stack view
        subscriptionStackView = UIStackView()
        subscriptionStackView.axis = .vertical
        subscriptionStackView.spacing = 24
        subscriptionStackView.alignment = .fill
        subscriptionStackView.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(subscriptionStackView)
        
        // Header
        let headerView = createHeaderView()
        subscriptionStackView.addArrangedSubview(headerView)
        
        // Features
        let featuresView = createFeaturesView()
        subscriptionStackView.addArrangedSubview(featuresView)
        
        // Product buttons
        monthlyProductButton = createProductButton(title: NSLocalizedString("Monthly Premium", comment: "Monthly subscription title"), 
                                                   subtitle: NSLocalizedString("Loading...", comment: "Loading placeholder"))
        monthlyProductButton.addTarget(self, action: #selector(monthlyButtonTapped), for: .touchUpInside)
        subscriptionStackView.addArrangedSubview(monthlyProductButton)
        
        yearlyProductButton = createProductButton(title: NSLocalizedString("Yearly Premium", comment: "Yearly subscription title"), 
                                                  subtitle: NSLocalizedString("Loading...", comment: "Loading placeholder"))
        yearlyProductButton.addTarget(self, action: #selector(yearlyButtonTapped), for: .touchUpInside)
        subscriptionStackView.addArrangedSubview(yearlyProductButton)
        
        // Restore button
        restoreButton = UIButton(type: .system)
        restoreButton.setTitle(NSLocalizedString("Restore Purchases", comment: "Restore purchases button"), for: .normal)
        restoreButton.titleLabel?.font = UIFont.systemFont(ofSize: 16)
        restoreButton.addTarget(self, action: #selector(restoreButtonTapped), for: .touchUpInside)
        subscriptionStackView.addArrangedSubview(restoreButton)
        
        // Loading indicator
        loadingIndicator = UIActivityIndicatorView(style: .large)
        loadingIndicator.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(loadingIndicator)
        
        // Constraints
        NSLayoutConstraint.activate([
            closeButton.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 16),
            closeButton.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -20),
            closeButton.widthAnchor.constraint(equalToConstant: 44),
            closeButton.heightAnchor.constraint(equalToConstant: 44),
            
            scrollView.topAnchor.constraint(equalTo: closeButton.bottomAnchor, constant: 8),
            scrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            
            contentView.topAnchor.constraint(equalTo: scrollView.topAnchor),
            contentView.leadingAnchor.constraint(equalTo: scrollView.leadingAnchor),
            contentView.trailingAnchor.constraint(equalTo: scrollView.trailingAnchor),
            contentView.bottomAnchor.constraint(equalTo: scrollView.bottomAnchor),
            contentView.widthAnchor.constraint(equalTo: scrollView.widthAnchor),
            
            subscriptionStackView.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 20),
            subscriptionStackView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 20),
            subscriptionStackView.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -20),
            subscriptionStackView.bottomAnchor.constraint(equalTo: contentView.bottomAnchor, constant: -20),
            
            loadingIndicator.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            loadingIndicator.centerYAnchor.constraint(equalTo: view.centerYAnchor)
        ])
    }
    
    private func createHeaderView() -> UIView {
        let headerView = UIView()
        
        let titleLabel = UILabel()
        titleLabel.text = NSLocalizedString("Yaba Sanshiro 2 Premium", comment: "Premium subscription title")
        titleLabel.font = UIFont.systemFont(ofSize: 28, weight: .bold)
        titleLabel.textAlignment = .center
        titleLabel.numberOfLines = 0
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        
        let subtitleLabel = UILabel()
        subtitleLabel.text = NSLocalizedString("Unlock unlimited games and remove ads", comment: "Premium subscription subtitle")
        subtitleLabel.font = UIFont.systemFont(ofSize: 18, weight: .medium)
        subtitleLabel.textAlignment = .center
        subtitleLabel.textColor = .secondaryLabel
        subtitleLabel.numberOfLines = 0
        subtitleLabel.translatesAutoresizingMaskIntoConstraints = false
        
        headerView.addSubview(titleLabel)
        headerView.addSubview(subtitleLabel)
        
        NSLayoutConstraint.activate([
            titleLabel.topAnchor.constraint(equalTo: headerView.topAnchor),
            titleLabel.leadingAnchor.constraint(equalTo: headerView.leadingAnchor),
            titleLabel.trailingAnchor.constraint(equalTo: headerView.trailingAnchor),
            
            subtitleLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 8),
            subtitleLabel.leadingAnchor.constraint(equalTo: headerView.leadingAnchor),
            subtitleLabel.trailingAnchor.constraint(equalTo: headerView.trailingAnchor),
            subtitleLabel.bottomAnchor.constraint(equalTo: headerView.bottomAnchor)
        ])
        
        return headerView
    }
    
    private func createFeaturesView() -> UIView {
        let featuresView = UIView()
        let stackView = UIStackView()
        stackView.axis = .vertical
        stackView.spacing = 16
        stackView.translatesAutoresizingMaskIntoConstraints = false
        
        let features = [
            NSLocalizedString("Install unlimited games", comment: "Premium feature: unlimited games"),
            NSLocalizedString("No advertisements", comment: "Premium feature: no ads"),
            NSLocalizedString("Cloud save synchronization", comment: "Premium feature: cloud saves"),
            NSLocalizedString("Priority customer support", comment: "Premium feature: support")
        ]
        
        for feature in features {
            let featureView = createFeatureRow(text: feature)
            stackView.addArrangedSubview(featureView)
        }
        
        featuresView.addSubview(stackView)
        
        NSLayoutConstraint.activate([
            stackView.topAnchor.constraint(equalTo: featuresView.topAnchor),
            stackView.leadingAnchor.constraint(equalTo: featuresView.leadingAnchor),
            stackView.trailingAnchor.constraint(equalTo: featuresView.trailingAnchor),
            stackView.bottomAnchor.constraint(equalTo: featuresView.bottomAnchor)
        ])
        
        return featuresView
    }
    
    private func createFeatureRow(text: String) -> UIView {
        let rowView = UIView()
        
        let checkmarkLabel = UILabel()
        checkmarkLabel.text = "✓"
        checkmarkLabel.font = UIFont.systemFont(ofSize: 20, weight: .bold)
        checkmarkLabel.textColor = .systemGreen
        checkmarkLabel.translatesAutoresizingMaskIntoConstraints = false
        
        let textLabel = UILabel()
        textLabel.text = text
        textLabel.font = UIFont.systemFont(ofSize: 16)
        textLabel.numberOfLines = 0
        textLabel.translatesAutoresizingMaskIntoConstraints = false
        
        rowView.addSubview(checkmarkLabel)
        rowView.addSubview(textLabel)
        
        NSLayoutConstraint.activate([
            checkmarkLabel.leadingAnchor.constraint(equalTo: rowView.leadingAnchor),
            checkmarkLabel.centerYAnchor.constraint(equalTo: rowView.centerYAnchor),
            checkmarkLabel.widthAnchor.constraint(equalToConstant: 30),
            
            textLabel.leadingAnchor.constraint(equalTo: checkmarkLabel.trailingAnchor, constant: 8),
            textLabel.trailingAnchor.constraint(equalTo: rowView.trailingAnchor),
            textLabel.topAnchor.constraint(equalTo: rowView.topAnchor),
            textLabel.bottomAnchor.constraint(equalTo: rowView.bottomAnchor)
        ])
        
        return rowView
    }
    
    private func createProductButton(title: String, subtitle: String) -> UIButton {
        let button = UIButton(type: .system)
        button.backgroundColor = .systemBlue
        button.layer.cornerRadius = 12
        button.contentEdgeInsets = UIEdgeInsets(top: 16, left: 20, bottom: 16, right: 20)
        
        let titleLabel = UILabel()
        titleLabel.text = title
        titleLabel.font = UIFont.systemFont(ofSize: 18, weight: .semibold)
        titleLabel.textColor = .white
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        
        let subtitleLabel = UILabel()
        subtitleLabel.text = subtitle
        subtitleLabel.font = UIFont.systemFont(ofSize: 14)
        subtitleLabel.textColor = .white.withAlphaComponent(0.8)
        subtitleLabel.translatesAutoresizingMaskIntoConstraints = false
        
        button.addSubview(titleLabel)
        button.addSubview(subtitleLabel)
        
        NSLayoutConstraint.activate([
            titleLabel.centerXAnchor.constraint(equalTo: button.centerXAnchor),
            titleLabel.topAnchor.constraint(equalTo: button.topAnchor, constant: 16),
            
            subtitleLabel.centerXAnchor.constraint(equalTo: button.centerXAnchor),
            subtitleLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: 4),
            subtitleLabel.bottomAnchor.constraint(equalTo: button.bottomAnchor, constant: -16)
        ])
        
        return button
    }
    
    private func setupSubscriptions() {
        if #available(iOS 15.0, *) {
            SubscriptionManager.shared.$availableProducts
                .receive(on: DispatchQueue.main)
                .sink { [weak self] products in
                    self?.updateProductButtons(products: products)
                }
                .store(in: &cancellables)
            
            SubscriptionManager.shared.$subscriptionStatus
                .receive(on: DispatchQueue.main)
                .sink { [weak self] status in
                    switch status {
                    case .loading:
                        self?.loadingIndicator.startAnimating()
                    case .subscribed:
                        self?.loadingIndicator.stopAnimating()
                        self?.dismiss(animated: true)
                    default:
                        self?.loadingIndicator.stopAnimating()
                    }
                }
                .store(in: &cancellables)
        } else {
            LegacySubscriptionManager.shared.$availableProducts
                .receive(on: DispatchQueue.main)
                .sink { [weak self] products in
                    self?.updateLegacyProductButtons(products: products)
                }
                .store(in: &cancellables)
        }
    }
    
    @available(iOS 15.0, *)
    private func updateProductButtons(products: [Product]) {
        for product in products {
            if product.id == SubscriptionManager.monthlyProductId {
                updateProductButton(monthlyProductButton, with: product.displayName, price: product.displayPrice)
            } else if product.id == SubscriptionManager.yearlyProductId {
                updateProductButton(yearlyProductButton, with: product.displayName, price: product.displayPrice)
            }
        }
    }
    
    private func updateLegacyProductButtons(products: [SKProduct]) {
        for product in products {
            let formatter = NumberFormatter()
            formatter.numberStyle = .currency
            formatter.locale = product.priceLocale
            let priceString = formatter.string(from: product.price) ?? ""
            
            if product.productIdentifier == SubscriptionManager.monthlyProductId {
                updateProductButton(monthlyProductButton, with: product.localizedTitle, price: priceString)
            } else if product.productIdentifier == SubscriptionManager.yearlyProductId {
                updateProductButton(yearlyProductButton, with: product.localizedTitle, price: priceString)
            }
        }
    }
    
    private func updateProductButton(_ button: UIButton, with title: String, price: String) {
        if let titleLabel = button.subviews.first(where: { $0 is UILabel && ($0 as! UILabel).font.pointSize == 18 }) as? UILabel {
            titleLabel.text = title
        }
        if let subtitleLabel = button.subviews.first(where: { $0 is UILabel && ($0 as! UILabel).font.pointSize == 14 }) as? UILabel {
            subtitleLabel.text = price
        }
    }
    
    private func loadProducts() {
        if #available(iOS 15.0, *) {
            Task {
                await SubscriptionManager.shared.requestProducts()
            }
        }
    }
    
    @objc private func monthlyButtonTapped() {
        purchaseProduct(identifier: SubscriptionManager.monthlyProductId)
    }
    
    @objc private func yearlyButtonTapped() {
        purchaseProduct(identifier: SubscriptionManager.yearlyProductId)
    }
    
    private func purchaseProduct(identifier: String) {
        loadingIndicator.startAnimating()
        
        if #available(iOS 15.0, *) {
            guard let product = SubscriptionManager.shared.availableProducts.first(where: { $0.id == identifier }) else {
                loadingIndicator.stopAnimating()
                return
            }
            
            Task {
                do {
                    _ = try await SubscriptionManager.shared.purchase(product)
                } catch {
                    DispatchQueue.main.async {
                        self.loadingIndicator.stopAnimating()
                        self.showError(error)
                    }
                }
            }
        } else {
            guard let product = LegacySubscriptionManager.shared.availableProducts.first(where: { $0.productIdentifier == identifier }) else {
                loadingIndicator.stopAnimating()
                return
            }
            LegacySubscriptionManager.shared.purchase(product)
            loadingIndicator.stopAnimating()
        }
    }
    
    @objc private func restoreButtonTapped() {
        loadingIndicator.startAnimating()
        
        Task {
            await UnifiedSubscriptionManager.shared.restorePurchases()
            DispatchQueue.main.async {
                self.loadingIndicator.stopAnimating()
            }
        }
    }
    
    @objc private func closeButtonTapped() {
        dismiss(animated: true)
    }
    
    private func showError(_ error: Error) {
        let alert = UIAlertController(
            title: NSLocalizedString("Purchase Failed", comment: "Purchase error title"),
            message: error.localizedDescription,
            preferredStyle: .alert
        )
        alert.addAction(UIAlertAction(title: NSLocalizedString("OK", comment: "OK button"), style: .default))
        present(alert, animated: true)
    }
}