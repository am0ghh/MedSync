import SwiftUI

struct CaregiverDashboardView: View {
    @EnvironmentObject var service: SupabaseService

    private let weekDays = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
    private let dayAbbreviations = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]

    private var taken: Int {
        service.doseEvents.filter { $0.status == "dispensed" && $0.pillsDetected == true }.count
    }

    private var dispensedUnconfirmed: Int {
        service.doseEvents.filter { $0.status == "dispensed" && $0.pillsDetected == nil }.count
    }

    private var notTaken: Int {
        service.doseEvents.filter { $0.status == "dispensed" && $0.pillsDetected == false }.count
    }

    private var missed: Int {
        service.doseEvents.filter { $0.status == "missed" }.count
    }

    private var missedEvents: [DoseEvent] {
        service.doseEvents.filter { $0.status == "missed" }
    }

    private var daysWithData: Int {
        Set(service.doseEvents.map { $0.day }).count
    }

    private func circleColor(for day: String) -> Color {
        guard let event = service.doseEvents.first(where: { $0.day == day }) else {
            return Color(.systemGray4)
        }
        if event.status == "missed" { return .red }
        if event.status == "dispensed" {
            if event.pillsDetected == true { return .green }
            if event.pillsDetected == false { return .red }
            return .orange
        }
        return .orange
    }

    private func formattedTime(_ isoString: String) -> String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: isoString) else { return isoString }
        let display = DateFormatter()
        display.dateFormat = "h:mm a"
        return display.string(from: date)
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Weekly Summary") {
                    HStack(spacing: 0) {
                        SummaryCell(count: taken, label: "Taken", color: .green)
                        SummaryCell(count: dispensedUnconfirmed, label: "Unconfirmed", color: .orange)
                        SummaryCell(count: notTaken, label: "Not taken", color: .red)
                        SummaryCell(count: missed, label: "Missed", color: .red.opacity(0.7))
                    }
                    .padding(.vertical, 4)
                }

                Section("7-Day Overview") {
                    HStack(spacing: 8) {
                        ForEach(Array(zip(weekDays, dayAbbreviations)), id: \.0) { day, abbr in
                            VStack(spacing: 4) {
                                Circle()
                                    .fill(circleColor(for: day))
                                    .frame(width: 28, height: 28)
                                Text(abbr)
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                            }
                            .frame(maxWidth: .infinity)
                        }
                    }
                    .padding(.vertical, 4)
                }

                Section("Missed Doses") {
                    if missedEvents.isEmpty {
                        Text("No missed doses this week.")
                            .foregroundStyle(.secondary)
                    } else {
                        ForEach(missedEvents) { event in
                            HStack {
                                Text(event.day)
                                Spacer()
                                Text(formattedTime(event.scheduledTime))
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }

                Section("Refill Status") {
                    if daysWithData >= 7 {
                        Label("Refill needed this week", systemImage: "exclamationmark.triangle.fill")
                            .foregroundStyle(.red)
                    } else {
                        Label("\(daysWithData) of 7 days dispensed", systemImage: "checkmark.circle.fill")
                            .foregroundStyle(daysWithData > 0 ? .green : .secondary)
                    }
                }

                Section("Photo Feed") {
                    RoundedRectangle(cornerRadius: 12)
                        .fill(Color(.systemGray5))
                        .frame(height: 120)
                        .overlay(
                            Text("Photo feed available when Supabase is connected.")
                                .font(.caption)
                                .foregroundStyle(.secondary)
                                .multilineTextAlignment(.center)
                                .padding()
                        )
                        .listRowInsets(EdgeInsets())
                        .padding(.vertical, 4)
                }
            }
            .navigationTitle("Caregiver Dashboard")
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button {
                        Task { await service.fetchDoseEvents() }
                    } label: {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
            .task {
                if service.doseEvents.isEmpty {
                    await service.fetchDoseEvents()
                }
            }
        }
    }
}

private struct SummaryCell: View {
    let count: Int
    let label: String
    let color: Color

    var body: some View {
        VStack(spacing: 2) {
            Text("\(count)")
                .font(.title2.bold())
                .foregroundStyle(color)
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
        }
        .frame(maxWidth: .infinity)
    }
}
