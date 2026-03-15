import serial

SERIAL_PORT = "COM5"
BAUD_RATE = 115200
OUTPUT_FILE = "flight_log.csv"

ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

with open(OUTPUT_FILE, "w", newline="") as f:
    print("Logging serial data to", OUTPUT_FILE)

    # Wait for header and write it
    while True:
        line = ser.readline().decode(errors="ignore").strip()
        if line.startswith("timestamp_ms"):
            f.write(line + "\n")
            print("Header written")
            break

    try:
        while True:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                print(line)
                f.write(line + "\n")
    except KeyboardInterrupt:
        print("Logging stopped by user")

ser.close()