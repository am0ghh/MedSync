import SwiftUI

@main
struct MedSyncApp: App {
    @StateObject private var service = SupabaseService()
    @State private var showLaunch = true

    var body: some Scene {
        WindowGroup {
            ZStack {
                ContentView()
                    .environmentObject(service)

                if showLaunch {
                    LaunchView()
                        .transition(.opacity)
                }
            }
            .animation(.easeOut(duration: 0.5), value: showLaunch)
            .task {
                try? await Task.sleep(nanoseconds: 2_200_000_000)
                showLaunch = false
            }
        }
    }
}
