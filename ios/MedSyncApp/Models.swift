import Foundation
import SwiftUI

struct DoseEvent: Codable, Identifiable {
    let id: String
    let day: String
    let scheduledTime: String
    let dispensedAt: String?
    let status: String        // "pending" | "dispensed" | "missed"
    let photoURL: String?
    let pillsDetected: Bool?
    let createdAt: String

    enum CodingKeys: String, CodingKey {
        case id, day, status
        case scheduledTime = "scheduled_time"
        case dispensedAt   = "dispensed_at"
        case photoURL      = "photo_url"
        case pillsDetected = "pills_detected"
        case createdAt     = "created_at"
    }

    var statusColor: Color {
        switch status {
        case "dispensed": return .green
        case "missed":    return .red
        default:          return .orange
        }
    }
}
