import SwiftUI

struct ContentView: View {
    var body: some View {
        TabView {
            DoseHistoryView()
                .tabItem { Label("History", systemImage: "list.clipboard") }

            CommandPanelView()
                .tabItem { Label("Controls", systemImage: "slider.horizontal.3") }
        }
        .tint(.blue)
    }
}
