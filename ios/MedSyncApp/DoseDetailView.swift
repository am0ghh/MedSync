import SwiftUI

struct DoseDetailView: View {
    let event: DoseEvent

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {

                // Pill photo
                Group {
                    if let rawURL = event.photoURL, let url = URL(string: rawURL) {
                        AsyncImage(url: url) { phase in
                            switch phase {
                            case .empty:
                                photoPlaceholder(icon: "arrow.down.circle", text: "Loading photo…")
                                    .overlay(ProgressView())
                            case .success(let image):
                                image
                                    .resizable()
                                    .scaledToFill()
                                    .frame(height: 260)
                                    .clipped()
                                    .clipShape(RoundedRectangle(cornerRadius: 16))
                            case .failure:
                                photoPlaceholder(icon: "photo.slash", text: "Photo unavailable")
                            @unknown default:
                                EmptyView()
                            }
                        }
                    } else {
                        photoPlaceholder(icon: "camera.slash", text: "No photo captured")
                    }
                }

                // CV analysis result
                if event.status == "dispensed" {
                    CVResultCard(pillsDetected: event.pillsDetected)
                }

                // Details
                VStack(alignment: .leading, spacing: 0) {
                    DetailRow(label: "Day",       value: event.day.capitalized)
                    Divider().padding(.leading)
                    DetailRow(label: "Status",    value: event.status.capitalized)
                    Divider().padding(.leading)
                    DetailRow(label: "Scheduled", value: formatTime(event.scheduledTime))
                    if let dispensedAt = event.dispensedAt {
                        Divider().padding(.leading)
                        DetailRow(label: "Dispensed", value: formatTime(dispensedAt))
                    }
                }
                .background(Color(.secondarySystemGroupedBackground))
                .clipShape(RoundedRectangle(cornerRadius: 12))
            }
            .padding()
        }
        .background(Color(.systemGroupedBackground))
        .navigationTitle(event.day.capitalized)
        .navigationBarTitleDisplayMode(.inline)
    }

    @ViewBuilder
    private func photoPlaceholder(icon: String, text: String) -> some View {
        RoundedRectangle(cornerRadius: 16)
            .fill(Color(.systemGray5))
            .frame(height: 260)
            .overlay(
                VStack(spacing: 8) {
                    Image(systemName: icon).font(.largeTitle)
                    Text(text).font(.caption)
                }
                .foregroundStyle(.secondary)
            )
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

// MARK: - Sub-views

struct CVResultCard: View {
    let pillsDetected: Bool?

    var body: some View {
        HStack(spacing: 14) {
            Image(systemName: icon)
                .font(.title)
                .foregroundStyle(color)

            VStack(alignment: .leading, spacing: 2) {
                Text("CV Analysis")
                    .font(.caption)
                    .foregroundStyle(.secondary)
                Text(label)
                    .font(.headline)
                    .foregroundStyle(color)
            }
            Spacer()
        }
        .padding()
        .background(color.opacity(0.1))
        .clipShape(RoundedRectangle(cornerRadius: 12))
    }

    private var icon: String {
        switch pillsDetected {
        case true:  return "checkmark.seal.fill"
        case false: return "exclamationmark.triangle.fill"
        default:    return "clock.fill"
        }
    }

    private var label: String {
        switch pillsDetected {
        case true:  return "Pills detected in cup"
        case false: return "Cup appears empty"
        default:    return "Analysis pending…"
        }
    }

    private var color: Color {
        switch pillsDetected {
        case true:  return .green
        case false: return .red
        default:    return .orange
        }
    }
}

struct DetailRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label).foregroundStyle(.secondary)
            Spacer()
            Text(value).fontWeight(.medium)
        }
        .padding(.horizontal)
        .padding(.vertical, 12)
    }
}
