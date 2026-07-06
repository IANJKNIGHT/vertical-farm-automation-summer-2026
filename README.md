# Aeration Control System (ACS) — Architecture & Technical Specifications

Welcome to the core repository for the Vertical Farm Aeration Control System (ACS). This system provides automated, closed-loop micro-climate management across vertical grow walls using a centralized master/satellite network architecture. It is designed to withstand the electrically noisy and high-humidity environments typical of commercial indoor agriculture.

## System Architecture Overview
The system utilizes a **Master-Slave (Client-Server) Topology** operating over an industrial **RS-485 serial bus**. A single Master Controller acts as the orchestrator, while multiple Satellite Leaf Nodes act as the local hardware executors distributed across the plant canopy.

[ Host Laptop / PC ]
        │ (USB-C)
        ▼
┌──────────────────┐
│  MASTER NODE     │ ─── [ Permanent 120Ω Termination ]
└──────────────────┘
        │
        └─── (120Ω Differential Twisted Pair + 12V DC Bus)
                 │
                 ├──► ┌───────────────────┐
                 │    │ SATELLITE NODE 1  │ ─── [ JP1 Jumper: OPEN ]
                 │    └───────────────────┘
                 │              │
                 │              ├──► Canopy Fans (PWM / Tachometer)
                 │              ├──► Shutter/Louver Servos
                 │              └──► DHT22 Climate Sensors
                 │
                 └──► ┌───────────────────┐
                      │ SATELLITE NODE N  │ ─── [ JP1 Jumper: SHORTED ]
                      └───────────────────┘ (Physical End of Line)

## 1. The Master Controller
The **Master Controller** handles user interaction, high-level environment recipes, data logging, and network orchestration.

**Key Functional Systems**
* **Core Processing:** Driven by a Raspberry Pi RP2040 microcontroller clocked via an external 12MHz crystal oscillator for precise, time-deterministic scheduling.

* **Storage & Non-Volatile Memory:** A 2MB SPI Flash memory chip holds static grow recipes. A dedicated Micro-SD card slot acts as an automated "digital grow journal," logging continuous telemetry profiles.
* **Host Interface**: An onboard FTDI FT232RL USB-to-UART converter allows seamless plug-and-play configuration and firmware deployment directly from a laptop over a structural USB Type-C connection.
* **Power Regulation:** Uses an AP2112K-3.3TR Low-Dropout (LDO) linear regulator to deliver stable, ultra-low-noise 3.3V power to the computing logic, mitigating digital glitches during communication spikes.

## 2. The Satellite Leaf Nodes
Distributed directly within the grow environment, the **Satellite Leaf Nodes** operate as the muscle of the farm, executing physical actions and reporting raw environmental data back to the master.

### Local Control Loops & Actuators
* Micro-Climate Monitoring: A dedicated sensor port hooks directly into canopy-level telemetry sensors (e.g., DHT22 modules) to sample localized temperature and relative humidity.
* Dynamic Airflow Generation: Features a 4-pin PC PWM Fan Header. The node uses Pulse-Width Modulation (PWM) to accurately scale fan speeds while reading the high-frequency Tachometer line to instantly detect physical motor stalls or failures.
* Mechanical Actuation: Dedicated 3-pin male headers output high-current 5V PWM commands to position-control servo motors, opening or closing physical ventilation louvers depending on the PID loop response.

### Power Infrastructure & Electrical Safety
* **High-Efficiency Regulation:** Because distribution lines run an industrial 12V DC bus to prevent long-distance power drops, each node hosts a step-down **DC-DC Buck Regulator**. This achieves over 90% power efficiency, preventing localized heat dissipation that could skew sensor readings or stress components inside sealed enclosures.
* **Inductive Inrush Protection:** Large bulk electrolytic capacitors ($100\mu\text{F}$ to $470\mu\text{F}$) act as local energy reservoirs to absorb severe current dips when heavy fan motors spin up.
* **Flyback Clamping:** Fast-acting **SS35 Schottky Barrier Diodes** are placed across inductive actuator connections to capture and safely clamp negative inductive voltage spikes (ε = -L \frac{di}{dt}$) generated when fans or servos alter speeds or power off.
* **Technician Fault Tolerance:** A heavy-duty 1N5400 Rectifier Diode provides reverse-polarity protection. If the 12V bus is wired backward during rapid field maintenance, the diode short-circuits the fault and blows an inline 4A overcurrent fuse, completely isolating the node and eliminating fire risks.

## 3. Physical Network Layer & Data Integrity  
The system is hardwired using the robust **RS-485 Differential Signaling Standard** via Texas Instruments THVD1450D transceivers.

### Noise Cancellation
Commercial vertical farms suffer from extreme Electromagnetic Interference (EMI) caused by high-power LED grow lights and AC water pumps. By transmitting data as a differential voltage over a twisted wire pair (Lines A and B), environmental noise is coupled equally into both paths. The receiver evaluates only the voltage delta (ΔV = V_A$ - V_B$), mathematically canceling out ambient electrical noise:

  ΔV = (V_A$ + V_noise$) - (V_B$ + V_noise$) = V_A$ - V_B$

### Impedance Matching & Echo Prevention
To prevent data frames from bouncing off the ends of the cables and causing data-corrupting echoes (signal reflections), the network must be physically terminated to match the cable's 120Ω characteristic impedance.
* **The Master Node** includes a permanently soldered $120\Omega$ resistor across its output lines.
* **Intermediate Satellite Nodes** must keep their onboarding jumper (JP1) **open** to remain high-impedance, letting data pass through smoothly.
* **The Final Satellite Node** at the physical end of the daisy chain **must have its** JP1 **jumper installed**. This engages its internal $120\Omega$ resistor, acting as an electrical shock absorber to terminate the signals cleanly.

## Hardware Deployment Summary
| Parameter | Master Node Specification | Satellite Node Specification |
| :-------: | :-----------------------: | :--------------------------: |
| **Logic Voltage** | 3.3V DC (Via AP2112K-3.3) | 3.3V DC (Via AP2112K-3.3) | 
| **Main Power Input** | 5V DC via USB-C | 12V DC Unregulated Bus |
| **Actuator Support** | N/A (Control Interfaces Only) | 12V PWM Fans, 5V Servos |
|**Local Telemetry** | OLED Menu Display, Rotary Encoder | DHT22 Sensor Header, Fan Tachometer |
| **Safety Features** | Overvoltage protection via USB | 4A Fuse, 1N5400 Reverse Protection, SS35 Flyback Clamps |

