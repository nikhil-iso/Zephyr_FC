# Zephyr_FC

Zephyr_FC is a simple flight computer and simulation package for logging, visualizing, and analyzing flight data. It is designed for small-scale rocketry or similar projects, providing real-time telemetry, post-flight analysis, and orientation visualization.

## Features

- **Serial Logging:** Capture flight data from a microcontroller via serial port and save to CSV.
- **Live Telemetry:** Real-time altitude plotting and apogee detection during flight.
- **Post-Flight Analysis:** Analyze flight metrics and plot altitude profiles.
- **Orientation Visualization:** 3D animation of flight orientation using logged pitch, roll, and altitude.

## File Overview

- `serial_logger.py`: Logs serial data to `flight_log.csv`.
- `live_telemetry.py`: Displays live altitude telemetry and apogee.
- `post_flight_analysis.py`: Analyzes and plots flight data after the flight.
- `orientation_visualization.py`: Animates the flight orientation in 3D.
- `flight_log.csv`: Data file generated during flight.

## Usage

1. **Logging Data**
    - Connect your flight computer to the specified serial port.
    - Run `serial_logger.py` to record flight data.

2. **Live Telemetry**
    - Run `live_telemetry.py` for real-time altitude visualization during flight.

3. **Post-Flight Analysis**
    - After the flight, run `post_flight_analysis.py` to view metrics and altitude profile.

4. **Orientation Visualization**
    - Run `orientation_visualization.py` to animate the flight's orientation.

## Requirements

- Python 3.x
- `numpy`, `pandas`, `matplotlib`, `pyserial`

Install dependencies:
```bash
pip install numpy pandas matplotlib pyserial
```

## Notes

- Ensure the serial port settings match your hardware.
- The CSV header must be present for analysis scripts to work.

## License

This project is for educational and hobby use.
