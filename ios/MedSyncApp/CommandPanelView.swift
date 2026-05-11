import SwiftUI

struct CommandPanelView: View {
    @EnvironmentObject var service: SupabaseService
    @StateObject private var ble = BluetoothManager()
    @State private var showingConfirm = false
    @State private var pendingCommand = ""
    @State private var isSending = false

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 24) {

                    // BLE status card
                    VStack(alignment: .leading, spacing: 6) {
                        HStack {
                            Image(systemName: ble.state.systemImage)
                                .foregroundStyle(
                                    ble.state == .connected ? Color.green :
                                    ble.state == .scanning  ? Color.orange : Color.secondary
                                )
                            Text(ble.state.label)
                                .font(.subheadline)
                                .fontWeight(.medium)
                            Spacer()
                            if ble.state == .disconnected || ble.state == .scanning {
                                Button("Connect") { ble.scanForDevice() }
                                    .buttonStyle(.borderedProminent)
                                    .tint(.blue)
                                    .controlSize(.small)
                                    .disabled(ble.state == .scanning)
                            } else {
                                Button("Disconnect") { ble.disconnect() }
                                    .buttonStyle(.bordered)
                                    .tint(.red)
                                    .controlSize(.small)
                            }
                        }
                        if let status = ble.deviceStatus, ble.state == .connected {
                            Text(formattedDeviceStatus(status))
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .lineLimit(2)
                        }
                    }
                    .padding()
                    .background(
                        ble.state == .connected ? Color.green.opacity(0.12) :
                        ble.state == .scanning  ? Color.orange.opacity(0.12) : Color(.systemGray6)
                    )
                    .clipShape(RoundedRectangle(cornerRadius: 12))
                    .padding(.horizontal)

                    if let status = service.lastCommandStatus {
                        HStack {
                            Image(systemName: "checkmark.circle.fill").foregroundStyle(.green)
                            Text(status).font(.subheadline)
                            Spacer()
                        }
                        .padding()
                        .background(Color.green.opacity(0.1))
                        .clipShape(RoundedRectangle(cornerRadius: 12))
                        .padding(.horizontal)
                        .transition(.move(edge: .top).combined(with: .opacity))
                    }

                    // Dispense
                    CommandSection(
                        title: "Dispense",
                        icon: "pills.fill",
                        iconColor: .blue,
                        description: "Rotates the carousel to today's slot and drops medication into the cup. Only active during the dose window (9:00–10:00 AM)."
                    ) {
                        CommandButton(
                            label: "Dispense Now",
                            icon: "arrow.clockwise.circle.fill",
                            color: .blue,
                            isLoading: isSending && pendingCommand == "dispense"
                        ) { confirm("dispense") }
                    }

                    // Loading mode
                    CommandSection(
                        title: "Loading Mode",
                        icon: "tray.and.arrow.down.fill",
                        iconColor: .orange,
                        description: "Rotates the blocked slot over the dispense hole so you can safely refill the weekly compartments."
                    ) {
                        HStack(spacing: 12) {
                            CommandButton(
                                label: "Enter Load",
                                icon: "lock.fill",
                                color: .orange,
                                isLoading: isSending && pendingCommand == "load"
                            ) { confirm("load") }

                            CommandButton(
                                label: "Exit Load",
                                icon: "lock.open.fill",
                                color: .green,
                                isLoading: isSending && pendingCommand == "unload"
                            ) { confirm("unload") }
                        }
                    }

                    // Slot Navigation
                    CommandSection(
                        title: "Slot Navigation",
                        icon: "circle.grid.2x2.fill",
                        iconColor: .purple,
                        description: "Go directly to any day slot or the load slot. Use Calibrate first: physically place Monday's compartment over the dispense hole, then tap Calibrate to set that as home."
                    ) {
                        LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible()), GridItem(.flexible())], spacing: 10) {
                            ForEach(Array(zip(["Mon","Tue","Wed","Thu","Fri","Sat","Sun","Load"], 0..<8)), id: \.1) { label, slot in
                                Button {
                                    ble.sendCommand("GOTO:\(slot)")
                                } label: {
                                    Text(label)
                                        .font(.caption)
                                        .fontWeight(.semibold)
                                        .frame(maxWidth: .infinity)
                                        .padding(.vertical, 10)
                                        .background(slot == 7 ? Color.orange.opacity(0.15) : Color.purple.opacity(0.12))
                                        .foregroundStyle(slot == 7 ? Color.orange : Color.purple)
                                        .clipShape(RoundedRectangle(cornerRadius: 8))
                                }
                                .disabled(ble.state != .connected)
                            }
                        }
                        Button {
                            ble.sendCommand("CALIBRATE")
                        } label: {
                            HStack {
                                Image(systemName: "scope")
                                Text("Calibrate Home (Set as Monday)")
                                    .fontWeight(.semibold)
                            }
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 12)
                            .background(Color.gray.opacity(0.15))
                            .foregroundStyle(.primary)
                            .clipShape(RoundedRectangle(cornerRadius: 10))
                        }
                        .disabled(ble.state != .connected)
                    }

                    Text("Commands sent directly over Bluetooth.")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal)
                }
                .padding(.vertical)
                .animation(.easeInOut, value: service.lastCommandStatus)
            }
            .navigationTitle("Device Controls")
            .confirmationDialog(
                "Send \(pendingCommand.capitalized)?",
                isPresented: $showingConfirm,
                titleVisibility: .visible
            ) {
                Button("Send \(pendingCommand.capitalized)") {
                    Task { await sendPending() }
                }
                Button("Cancel", role: .cancel) {}
            } message: {
                Text("The device will act within 5 seconds.")
            }
        }
    }

    private func formattedDeviceStatus(_ raw: String) -> String {
        guard
            let data = raw.data(using: .utf8),
            let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
            let day = json["day"] as? String,
            let time = json["time"] as? String,
            let statusWord = json["status"] as? String
        else {
            return raw
        }
        return "\(day)  •  \(time)\nStatus: \(statusWord.capitalized)"
    }

    private func confirm(_ cmd: String) {
        pendingCommand = cmd
        showingConfirm = true
    }

    private func sendPending() async {
        isSending = true
        await service.sendCommand(pendingCommand)
        if ble.state == .connected {
            ble.sendCommand(pendingCommand.uppercased())
        }
        isSending = false
    }
}

// MARK: - Sub-views

struct CommandSection<Content: View>: View {
    let title: String
    let icon: String
    let iconColor: Color
    let description: String
    @ViewBuilder let content: () -> Content

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(spacing: 10) {
                Image(systemName: icon).foregroundStyle(iconColor).font(.title2)
                Text(title).font(.title2).fontWeight(.semibold)
            }
            Text(description)
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
            content()
        }
        .padding()
        .background(Color(.secondarySystemGroupedBackground))
        .clipShape(RoundedRectangle(cornerRadius: 16))
        .padding(.horizontal)
    }
}

struct CommandButton: View {
    let label: String
    let icon: String
    let color: Color
    let isLoading: Bool
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack {
                if isLoading {
                    ProgressView().tint(.white)
                } else {
                    Image(systemName: icon)
                }
                Text(label).fontWeight(.semibold)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 14)
            .background(color)
            .foregroundStyle(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
        }
        .disabled(isLoading)
    }
}
