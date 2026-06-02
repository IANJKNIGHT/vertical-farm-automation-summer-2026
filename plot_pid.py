import serial
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import time
import csv

# Adjust 'COM3' to match your RP2350's actual serial port assignment
ser = serial.Serial('COM4', 115200, timeout=1)

csv_filename = "pid_telemetry_log.csv"

times, targets, rpms, outputs, kp_vals, ki_vals, kd_vals, prev_errors, integral_errors = [], [], [], [], [], [], [], [], []
start_time = time.time()

# 1. Initialize the CSV file and write headers
with open(csv_filename, mode='w', newline='') as f:
    writer = csv.writer (f)
    writer.writerow([
        "Timestamp (s)", "Target RPM", "Current RPM", 
        "Kp", "Ki", "Kd", "Prev Error", "Integral Error", "PID Output (%)"
    ])

fig, (ax1, ax2, ax3) = plt.subplots(3, 1, sharex=True)

def update(frame):
    if ser.in_waiting > 0:
        # Read whatever is in the buffer right now, regardless of newlines
        raw_data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        print(f"BUFFER CONTENTS: {raw_data}")
        
        # Split it manually to see if there are hidden DATA: lines
        lines = raw_data.split('\n')
        for line in lines:
            line = line.strip()
            try:
                data_payload = line.replace("DATA:", "")
                values = data_payload.split(',')
                
                # Parse values
                target_val = int(values[0])
                current_val = int(values[1])
                kp_val = float(values[2])
                ki_val = float(values[3])
                kd_val = float(values[4])
                prev_error_val = float(values[5])
                integral_error_val = float(values[6])
                output_val = float(values[7])
                current_t = time.time() - start_time

                # 2. Append to lists
                times.append(current_t)
                targets.append(target_val)
                kp_vals.append(kp_val)
                ki_vals.append(ki_val)
                kd_vals.append(kd_val)
                prev_errors.append(prev_error_val)
                integral_errors.append(integral_error_val)
                rpms.append(current_val)
                outputs.append(output_val)
            
                if len(times) > 100:
                    times.pop(0)
                    targets.pop(0)
                    rpms.pop(0)
                    outputs.pop(0)
                    kp_vals.pop(0)
                    ki_vals.pop(0)
                    kd_vals.pop(0)
                    prev_errors.pop(0)
                    integral_errors.pop(0)

                ax1.clear()
                ax2.clear()
                ax3.clear()
                
                # ROW 1: System Speeds (The Primary View)
                ax1.plot(times, rpms, label="Current RPM", color='blue', linewidth=2)
                ax1.plot(times, targets, label="Target RPM", color='red', linestyle='--', linewidth=1.5)
                ax1.set_ylabel("Speed (RPM)")
                ax1.legend(loc="upper left")
                
                # Dynamic title changes to show your live coefficients safely inside the frame!
                ax1.set_title(f"PID Real-Time Tuning Dashboard  |  Kp: {kp_val:.3f}   Ki: {ki_val:.4f}   Kd: {kd_val:.4f}", 
                            fontsize=12, fontweight='bold', pad=10)

                # ROW 2: Control Effort (Duty Cycle)
                ax2.set_title(f"Target_RPM = {target_val}", fontsize=10, pad=8)
                ax2.plot(times, outputs, label="PID Output (%)", color='green')
                ax2.set_ylabel("Duty Cycle %")
                ax2.set_ylim(-5, 105)
                ax2.grid(True, linestyle=':', alpha=0.6)

                # ROW 3: Integration Diagnostics (Catch Windup Before it Pins the Fan)
                ax3.plot(times, integral_errors, label="Integral Error Accum.", color='magenta')
                ax3.set_ylabel("Integral Error")
                ax3.set_xlabel("Time (seconds)")
                ax3.grid(True, linestyle=':', alpha=0.6)
            except (ValueError, IndexError):
                pass # Ignore malformed serial lines`

ani = FuncAnimation(fig, update, interval=100, cache_frame_data=False)
plt.tight_layout()
plt.show()