import SwiftUI

struct DoseHistoryView: View {
    @EnvironmentObject var service: SupabaseService

    var body: some View {
        NavigationStack {
            Group {
                if service.isLoading && service.doseEvents.isEmpty {
                    ProgressView("Loading…")
                        .frame(maxWidth: .infinity, maxHeight: .infinity)
                } else if service.doseEvents.isEmpty {
                    ContentUnavailableView(
                        "No Dose Records",
                        systemImage: "pill",
                        description: Text("Add your Supabase credentials in Config.swift")
                    )
                } else {
                    List(service.doseEvents) { event in
                        NavigationLink(destination: DoseDetailView(event: event)) {
                            DoseEventRow(event: event)
                        }
                    }
                    .listStyle(.insetGrouped)
                }
            }
            .navigationTitle("MedSync")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button {
                        Task { await service.fetchDoseEvents() }
                    } label: {
                        if service.isLoading {
                            ProgressView()
                        } else {
                            Image(systemName: "arrow.clockwise")
                        }
                    }
                }
            }
            .task { await service.fetchDoseEvents() }
            .refreshable { await service.fetchDoseEvents() }
        }
    }
}

// MARK: - Row

struct DoseEventRow: View {
    let event: DoseEvent

    var body: some View {
        HStack(spacing: 12) {
            Circle()
                .fill(event.statusColor)
                .frame(width: 11, height: 11)

            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text(event.day.capitalized)
                        .font(.headline)
                    Spacer()
                    StatusBadge(status: event.status)
                }

                if let dispensedAt = event.dispensedAt {
                    Text("Dispensed: \(formatTime(dispensedAt))")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                } else {
                    Text("Scheduled: \(formatTime(event.scheduledTime))")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                if event.status == "dispensed" {
                    HStack(spacing: 4) {
                        Image(systemName: pillIcon)
                            .font(.caption2)
                        Text(pillText)
                            .font(.caption2)
                    }
                    .foregroundStyle(pillColor)
                }
            }
        }
        .padding(.vertical, 4)
    }

    private var pillIcon: String {
        switch event.pillsDetected {
        case true:  return "checkmark.circle.fill"
        case false: return "xmark.circle.fill"
        default:    return "clock.fill"
        }
    }

    private var pillText: String {
        switch event.pillsDetected {
        case true:  return "Pills taken"
        case false: return "Pills not taken"
        default:    return "CV analysis pending"
        }
    }

    private var pillColor: Color {
        switch event.pillsDetected {
        case true:  return .green
        case false: return .red
        default:    return .orange
        }
    }

    private func formatTime(_ iso: String) -> String {
        let f = ISO8601DateFormatter()
        for opts: ISO8601DateFormatter.Options in [
            [.withInternetDateTime, .withFractionalSeconds],
            [.withInternetDateTime],
        ] {
            f.formatOptions = opts
            if let date = f.date(from: iso) {
                let out = DateFormatter()
                out.dateStyle = .medium
                out.timeStyle = .short
                return out.string(from: date)
            }
        }
        return iso
    }
}

// MARK: - Status badge

struct StatusBadge: View {
    let status: String

    var body: some View {
        Text(status.uppercased())
            .font(.caption2)
            .fontWeight(.bold)
            .padding(.horizontal, 8)
            .padding(.vertical, 3)
            .background(color.opacity(0.15))
            .foregroundStyle(color)
            .clipShape(Capsule())
    }

    private var color: Color {
        switch status {
        case "dispensed": return .green
        case "missed":    return .red
        default:          return .orange
        }
    }
}
