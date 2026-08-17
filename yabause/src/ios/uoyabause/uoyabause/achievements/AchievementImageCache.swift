import Foundation
import UIKit

class AchievementImageCache {
    static let shared = AchievementImageCache()
    
    private let cache = NSCache<NSString, UIImage>()
    private let fileManager = FileManager.default
    private let cacheDirectory: URL
    
    private init() {
        // Set up cache directory
        let cachesDirectory = fileManager.urls(for: .cachesDirectory, in: .userDomainMask).first!
        cacheDirectory = cachesDirectory.appendingPathComponent("AchievementBadges")
        
        // Create directory if it doesn't exist
        try? fileManager.createDirectory(at: cacheDirectory, withIntermediateDirectories: true, attributes: nil)
        
        // Configure NSCache
        cache.countLimit = 200 // Maximum 200 images in memory
        cache.totalCostLimit = 50 * 1024 * 1024 // 50MB memory limit
    }
    
    func loadImage(for achievementId: String, state: Int, completion: @escaping (UIImage?) -> Void) {
        let cacheKey = "\(achievementId)_\(state)" as NSString
        
        // Check memory cache first
        if let cachedImage = cache.object(forKey: cacheKey) {
            completion(cachedImage)
            return
        }
        
        // Check disk cache
        let filename = "\(cacheKey).png"
        let fileURL = cacheDirectory.appendingPathComponent(filename)
        
        if fileManager.fileExists(atPath: fileURL.path) {
            DispatchQueue.global(qos: .utility).async { [weak self] in
                if let image = UIImage(contentsOfFile: fileURL.path) {
                    // Cache in memory
                    self?.cache.setObject(image, forKey: cacheKey, cost: self?.imageCost(image) ?? 0)
                    
                    DispatchQueue.main.async {
                        completion(image)
                    }
                } else {
                    DispatchQueue.main.async {
                        completion(nil)
                    }
                }
            }
            return
        }
        
        // Not in cache, need to download
        downloadAndCacheImage(achievementId: achievementId, state: state, completion: completion)
    }
    
    private func downloadAndCacheImage(achievementId: String, state: Int, completion: @escaping (UIImage?) -> Void) {
        guard let manager = RetroAchievementsManager.shared,
              let achievementIdInt = Int(achievementId) else {
            NSLog("AchievementImageCache: Manager or achievementId unavailable")
            completion(nil)
            return
        }
        
        guard let urlString = manager.getAchievementBadgeURL(achievementId: achievementIdInt, state: state) else {
            NSLog("AchievementImageCache: Failed to get badge URL for achievement \(achievementId) state \(state)")
            completion(nil)
            return
        }
        
        NSLog("AchievementImageCache: Got badge URL for achievement \(achievementId) state \(state): \(urlString)")
        
        guard let url = URL(string: urlString) else {
            NSLog("AchievementImageCache: Invalid URL string: \(urlString)")
            completion(nil)
            return
        }
        
        URLSession.shared.dataTask(with: url) { [weak self] data, response, error in
            if let error = error {
                NSLog("AchievementImageCache: Download failed for \(achievementId): \(error.localizedDescription)")
                DispatchQueue.main.async {
                    completion(nil)
                }
                return
            }
            
            guard let self = self,
                  let data = data else {
                NSLog("AchievementImageCache: No data received for \(achievementId)")
                DispatchQueue.main.async {
                    completion(nil)
                }
                return
            }
            
            guard let image = UIImage(data: data) else {
                NSLog("AchievementImageCache: Failed to create image from data for \(achievementId)")
                DispatchQueue.main.async {
                    completion(nil)
                }
                return
            }
            
            NSLog("AchievementImageCache: Successfully downloaded and created image for \(achievementId) state \(state)")
            
            // Cache the image
            let cacheKey = "\(achievementId)_\(state)" as NSString
            
            // Memory cache
            self.cache.setObject(image, forKey: cacheKey, cost: self.imageCost(image))
            
            // Disk cache
            DispatchQueue.global(qos: .utility).async {
                let filename = "\(cacheKey).png"
                let fileURL = self.cacheDirectory.appendingPathComponent(filename)
                
                if let pngData = image.pngData() {
                    try? pngData.write(to: fileURL)
                }
            }
            
            DispatchQueue.main.async {
                completion(image)
            }
        }.resume()
    }
    
    private func imageCost(_ image: UIImage) -> Int {
        return Int(image.size.width * image.size.height * 4) // Approximate bytes for RGBA
    }
    
    func clearCache() {
        cache.removeAllObjects()
        
        // Clear disk cache
        DispatchQueue.global(qos: .utility).async { [weak self] in
            guard let self = self else { return }
            
            do {
                let contents = try self.fileManager.contentsOfDirectory(at: self.cacheDirectory, includingPropertiesForKeys: nil)
                for fileURL in contents {
                    try? self.fileManager.removeItem(at: fileURL)
                }
            } catch {
                print("Error clearing achievement image cache: \(error)")
            }
        }
    }
    
    func getCacheSize() -> Int64 {
        guard let enumerator = fileManager.enumerator(at: cacheDirectory, includingPropertiesForKeys: [.fileSizeKey]) else {
            return 0
        }
        
        var totalSize: Int64 = 0
        for case let fileURL as URL in enumerator {
            do {
                let resourceValues = try fileURL.resourceValues(forKeys: [.fileSizeKey])
                if let fileSize = resourceValues.fileSize {
                    totalSize += Int64(fileSize)
                }
            } catch {
                continue
            }
        }
        
        return totalSize
    }
}