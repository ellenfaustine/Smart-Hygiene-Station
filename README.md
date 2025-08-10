# Smart Hygiene Station

An IoT-enabled contactless hygiene station that automates the handwashing process by sequentially dispensing water, soap, and tissue, using ESP8266 microcontrollers, sensors, and simple HTTP communication for synchronized operation, designed for improved accessibility and usability.

## Features
- **Contactless operation** – sequential dispensing of water → soap → tissue.
- **Accessibility-focused** – visual (LED) and auditory (buzzer) feedback for ease of use.
- **IoT dashboard** – view water, soap, and tissue levels in real-time.
- **Portable design** – lightweight, easy to set up in different locations.

## Hardware
- 3× ESP8266 modules
- Ultrasonic sensors (hand & liquid level detection)
- Water pumps
- Relay modules
- Servo motor (tissue dispensing)
- Limit switch (tissue availability)
- LEDs & buzzers for feedback

## How It Works
1. **Water Dispenser** – Detects a hand, pumps water, and signals the soap dispenser.
2. **Soap Dispenser** – Dispenses soap and signals the next unit.
3. **Tissue Dispenser** – Rotates to dispense tissue, completing the cycle.
4. Units communicate via **HTTP GET** requests to coordinate each stage.

## Demo & Screenshots
*(Replace these with your images)*  
![Water Dispenser Dashboard](./pictures/dashboard/water_dashboard.png)  
![Soap Dispenser Dashboard](./pictures/dashboard/soap_dashboard.png)  
![Tissue Dispenser Dashboard](./pictures/dashboard/tissue_dashboard.png)  

## Setup
1. Clone the repo:
   ```bash
   git clone https://github.com/ellenfaustine/Smart-Hygiene-Station.git
   ```
2. Open each .ino file in Arduino IDE.
3. Update Wi-Fi credentials and node IP addresses in each file.
4. Upload to each ESP8266.
5. Power the units and access their dashboards in a web browser:
   - **Water Dispenser:** http://192.168.18.84
   - **Soap Dispenser:** http://192.168.18.102
   - **Tissue Dispenser:** http://192.168.18.83
