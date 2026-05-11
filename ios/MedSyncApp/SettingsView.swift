import SwiftUI

struct SettingsView: View {
    @AppStorage("caregiverModeEnabled") private var caregiverModeEnabled = false

    var body: some View {
        NavigationStack {
            Form {
                Section("Caregiver") {
                    Toggle("Caregiver Mode", isOn: $caregiverModeEnabled)
                    Text("Enables a caregiver dashboard tab with adherence summaries and alerts.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Section("Dose Schedule") {
                    LabeledContent("Scheduled Time", value: "9:00 AM")
                    LabeledContent("Dose Window", value: "60 minutes")
                    Text("To change these, edit config.h and reflash the device.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Settings")
        }
    }
}
