import SwiftUI

struct ContentView: View {
    @AppStorage("caregiverModeEnabled") private var caregiverModeEnabled = false

    var body: some View {
        TabView {
            DoseHistoryView()
                .tabItem { Label("History", systemImage: "list.clipboard") }

            CommandPanelView()
                .tabItem { Label("Controls", systemImage: "slider.horizontal.3") }

            if caregiverModeEnabled {
                CaregiverDashboardView()
                    .tabItem { Label("Caregiver", systemImage: "person.2.fill") }
            }

            SettingsView()
                .tabItem { Label("Settings", systemImage: "gearshape") }
        }
        .tint(.blue)
    }
}
