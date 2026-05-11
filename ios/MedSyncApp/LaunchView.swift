import SwiftUI

struct LaunchView: View {
    @State private var scale: CGFloat = 0.7
    @State private var opacity: Double = 0

    var body: some View {
        ZStack {
            LinearGradient(
                colors: [
                    Color(red: 0.04, green: 0.48, blue: 1.0),
                    Color(red: 0.0, green: 0.78, blue: 0.74)
                ],
                startPoint: .topLeading,
                endPoint: .bottomTrailing
            )
            .ignoresSafeArea()

            VStack(spacing: 16) {
                Image(systemName: "pills.fill")
                    .font(.system(size: 72))
                    .foregroundColor(.white)
                    .scaleEffect(scale)

                Text("MedSync")
                    .font(.system(size: 42, weight: .bold, design: .rounded))
                    .foregroundColor(.white)

                Text("Smart Medication Assistant")
                    .font(.subheadline)
                    .foregroundColor(.white.opacity(0.8))

                ProgressView()
                    .progressViewStyle(CircularProgressViewStyle(tint: .white))
            }
            .opacity(opacity)
        }
        .onAppear {
            withAnimation(.spring(response: 0.6, dampingFraction: 0.7)) {
                scale = 1.0
                opacity = 1
            }
        }
    }
}
