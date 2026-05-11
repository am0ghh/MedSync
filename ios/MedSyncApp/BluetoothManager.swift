import Foundation
import CoreBluetooth

// MARK: - BLE State

enum BLEState: Equatable {
    case disconnected
    case scanning
    case connected

    var label: String {
        switch self {
        case .disconnected: return "Not Connected"
        case .scanning:     return "Scanning…"
        case .connected:    return "Connected"
        }
    }

    var systemImage: String {
        switch self {
        case .disconnected: return "bluetooth"
        case .scanning:     return "antenna.radiowaves.left.and.right"
        case .connected:    return "checkmark.circle.fill"
        }
    }
}

// MARK: - BluetoothManager

class BluetoothManager: NSObject, ObservableObject {

    // Published state — always mutated on main queue
    @Published var state: BLEState = .disconnected
    @Published var deviceStatus: String?

    // CoreBluetooth
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var commandChar: CBCharacteristic?
    private var statusChar: CBCharacteristic?

    // UUIDs defined in the shared BLE protocol
    private let serviceUUID    = CBUUID(string: "4FAFC201-1FB5-459E-8FCC-C5C9C331914B")
    private let commandCharUUID = CBUUID(string: "BEB5483E-36E1-4688-B7F5-EA07361B26A8")
    private let statusCharUUID  = CBUUID(string: "BEB5483E-36E1-4688-B7F5-EA07361B26A9")

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    // MARK: - Public API

    func scanForDevice() {
        guard central.state == .poweredOn else { return }
        DispatchQueue.main.async { self.state = .scanning }
        central.scanForPeripherals(withServices: [serviceUUID], options: nil)
    }

    func disconnect() {
        guard let peripheral = peripheral else { return }
        central.cancelPeripheralConnection(peripheral)
    }

    func sendCommand(_ cmd: String) {
        guard let peripheral = peripheral,
              let commandChar = commandChar,
              let data = cmd.data(using: .utf8) else { return }
        peripheral.writeValue(data, for: commandChar, type: .withoutResponse)
    }
}

// MARK: - CBCentralManagerDelegate

extension BluetoothManager: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state != .poweredOn {
            DispatchQueue.main.async {
                self.state = .disconnected
                self.deviceStatus = nil
            }
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        central.stopScan()
        self.peripheral = peripheral
        self.peripheral?.delegate = self
        central.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        self.peripheral = nil
        commandChar = nil
        statusChar = nil
        DispatchQueue.main.async {
            self.state = .disconnected
            self.deviceStatus = nil
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BluetoothManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services where service.uuid == serviceUUID {
            peripheral.discoverCharacteristics([commandCharUUID, statusCharUUID],
                                               for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let chars = service.characteristics else { return }
        for char in chars {
            if char.uuid == commandCharUUID {
                commandChar = char
            } else if char.uuid == statusCharUUID {
                statusChar = char
                peripheral.setNotifyValue(true, for: char)
            }
        }
        // Mark connected once we have located at least the command characteristic
        if commandChar != nil {
            DispatchQueue.main.async { self.state = .connected }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard characteristic.uuid == statusCharUUID,
              let data = characteristic.value,
              let json = String(data: data, encoding: .utf8) else { return }
        DispatchQueue.main.async {
            let previous = self.deviceStatus
            self.deviceStatus = json
            // Fire missed-dose notification once when status transitions to "missed".
            if previous?.contains("\"missed\"") == false && json.contains("\"missed\"") {
                Task { await NotificationManager.shared.sendMissedDoseNotification() }
            }
        }
    }
}
