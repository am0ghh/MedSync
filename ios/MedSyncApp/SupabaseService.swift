import Foundation

@MainActor
class SupabaseService: ObservableObject {
    @Published var doseEvents: [DoseEvent] = []
    @Published var isLoading = false
    @Published var lastCommandStatus: String?

    func fetchDoseEvents() async {
        isLoading = true
        try? await Task.sleep(nanoseconds: 800_000_000)  // simulate network

        doseEvents = [
            DoseEvent(
                id: "1",
                day: "Sunday",
                scheduledTime:  "2026-05-10T09:00:00Z",
                dispensedAt:    nil,
                status:         "pending",
                photoURL:       nil,
                pillsDetected:  nil,
                createdAt:      "2026-05-10T09:00:00Z"
            ),
            DoseEvent(
                id: "2",
                day: "Saturday",
                scheduledTime:  "2026-05-09T09:00:00Z",
                dispensedAt:    "2026-05-09T09:03:21Z",
                status:         "dispensed",
                photoURL:       nil,
                pillsDetected:  nil,   // CV still running
                createdAt:      "2026-05-09T09:00:00Z"
            ),
            DoseEvent(
                id: "3",
                day: "Friday",
                scheduledTime:  "2026-05-08T09:00:00Z",
                dispensedAt:    "2026-05-08T09:01:44Z",
                status:         "dispensed",
                photoURL:       nil,
                pillsDetected:  true,
                createdAt:      "2026-05-08T09:00:00Z"
            ),
            DoseEvent(
                id: "4",
                day: "Thursday",
                scheduledTime:  "2026-05-07T09:00:00Z",
                dispensedAt:    nil,
                status:         "missed",
                photoURL:       nil,
                pillsDetected:  nil,
                createdAt:      "2026-05-07T09:00:00Z"
            ),
            DoseEvent(
                id: "5",
                day: "Wednesday",
                scheduledTime:  "2026-05-06T09:00:00Z",
                dispensedAt:    "2026-05-06T09:02:10Z",
                status:         "dispensed",
                photoURL:       nil,
                pillsDetected:  false,  // dispensed but not taken
                createdAt:      "2026-05-06T09:00:00Z"
            ),
            DoseEvent(
                id: "6",
                day: "Tuesday",
                scheduledTime:  "2026-05-05T09:00:00Z",
                dispensedAt:    "2026-05-05T09:00:58Z",
                status:         "dispensed",
                photoURL:       nil,
                pillsDetected:  true,
                createdAt:      "2026-05-05T09:00:00Z"
            ),
            DoseEvent(
                id: "7",
                day: "Monday",
                scheduledTime:  "2026-05-04T09:00:00Z",
                dispensedAt:    "2026-05-04T09:01:33Z",
                status:         "dispensed",
                photoURL:       nil,
                pillsDetected:  true,
                createdAt:      "2026-05-04T09:00:00Z"
            ),
        ]

        isLoading = false
    }

    func sendCommand(_ command: String) async {
        try? await Task.sleep(nanoseconds: 600_000_000)  // simulate network
        lastCommandStatus = "\(command.capitalized) sent to device"
    }
}
