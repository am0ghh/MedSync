import Foundation
import UserNotifications

@MainActor
final class NotificationManager: ObservableObject {

    static let shared = NotificationManager()

    private init() {}

    // MARK: - Permission

    func requestPermission() {
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound, .badge]) { granted, error in
            if let error {
                print("[NotificationManager] Permission request error: \(error.localizedDescription)")
            } else {
                print("[NotificationManager] Notification permission granted: \(granted)")
            }
        }
    }

    // MARK: - Daily Dose Reminder

    func scheduleDoseReminder() {
        let center = UNUserNotificationCenter.current()
        let identifier = "medsync.dose.reminder"

        // Remove any existing reminder with the same identifier before rescheduling.
        center.removePendingNotificationRequests(withIdentifiers: [identifier])

        let content = UNMutableNotificationContent()
        content.title = "Time for your medication \u{1F48A}"
        content.body = "MedSync is ready to dispense your dose."
        content.sound = .default

        var dateComponents = DateComponents()
        dateComponents.hour = 9
        dateComponents.minute = 0

        let trigger = UNCalendarNotificationTrigger(dateMatching: dateComponents, repeats: true)
        let request = UNNotificationRequest(identifier: identifier, content: content, trigger: trigger)

        center.add(request) { error in
            if let error {
                print("[NotificationManager] Failed to schedule dose reminder: \(error.localizedDescription)")
            } else {
                print("[NotificationManager] Daily dose reminder scheduled at 09:00.")
            }
        }
    }

    func cancelDoseReminder() {
        UNUserNotificationCenter.current().removePendingNotificationRequests(
            withIdentifiers: ["medsync.dose.reminder"]
        )
        print("[NotificationManager] Daily dose reminder cancelled.")
    }

    // MARK: - Missed Dose Notification

    func sendMissedDoseNotification() {
        let content = UNMutableNotificationContent()
        content.title = "Missed Dose Alert"
        content.body = "A dose was not taken. Please check on the patient."
        content.sound = .default

        let identifier = "medsync.missed.\(Date().timeIntervalSince1970)"
        let request = UNNotificationRequest(identifier: identifier, content: content, trigger: nil)

        UNUserNotificationCenter.current().add(request) { error in
            if let error {
                print("[NotificationManager] Failed to send missed dose notification: \(error.localizedDescription)")
            }
        }
    }

    // MARK: - Refill Notification

    func sendRefillNotification() {
        let content = UNMutableNotificationContent()
        content.title = "Refill Needed"
        content.body = "All 7 compartments have been dispensed. Please refill MedSync."
        content.sound = .default

        let request = UNNotificationRequest(identifier: "medsync.refill", content: content, trigger: nil)

        UNUserNotificationCenter.current().add(request) { error in
            if let error {
                print("[NotificationManager] Failed to send refill notification: \(error.localizedDescription)")
            }
        }
    }

    // MARK: - Refill Check

    /// Counts how many distinct days in the current ISO week have status "dispensed" or "missed".
    /// Fires a refill notification if all 7 days are accounted for.
    func checkAndSendRefillIfNeeded(events: [DoseEvent]) {
        let allDays: Set<String> = ["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"]
        let resolvedDays = Set(
            events
                .filter { $0.status == "dispensed" || $0.status == "missed" }
                .map { $0.day.capitalized }
        ).intersection(allDays)

        print("[NotificationManager] Resolved days this week: \(resolvedDays.count)/7")

        if resolvedDays.count >= 7 {
            sendRefillNotification()
        }
    }
}
