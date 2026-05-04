Based on the provided spreadsheet data, here is a detailed specification for the game, designed for development on an **Arduino Mega** with a **Grove Shield**.-----# Game Specification: "PPI Master" Bomb Defusal System## 1\. Project Overview**Aim:** A collaborative bomb-defusal game where a "Defuser" interacts with hardware modules while a "Manual Operator" provides instructions based on a serial number and visual cues.
**Style:** Interactive puzzle-based gameplay with real-time sensor feedback, audio cues, and a countdown timer.
**Hardware Platform:** Arduino Mega (Master Controller) with Grove Shield.-----## 2\. Global Game Logic & Serial Number### Serial Number Configuration  * **Format:** `[Letter][Letter][Letter][Letter]-[Digit][Digit][Digit][Digit]` (e.g., `ABCD-1234`).
  * **Display:** Shown on the **LCD Screen** throughout the game.
  * **Logic Dependencies:** The serial number determines the solutions for the **Compass Alignment**, **Encoder Tuning**, and **Button Combo** modules.### Game State Management  * **Start:** Triggered by turning the **Keyed Switch** to "ON". This starts the timer and activates all modules.
  * **Defusal:** All modules must report a "Complete" status. Once all are done, the player must turn the **Keyed Switch** to "OFF".
  * **Failure/Explosion:**
      * Timer reaches `00:00`.
      * Keyed Switch is turned to "OFF" before all modules are complete.
      * Certain modules (like Button Combo) may trigger a "Strike" on failure; the code should track strikes (3 strikes = explosion).-----## 3\. Module Specifications & Code Details### A. Master Timer Module  * **Hardware:** 4-Digit Display (Pin **D4**).
  * **Functionality:**
      * Countdown from a predefined limit (e.g., 5:00).
      * **Logic:** Constantly check if a specific digit (e.g., '1') is present in any position of the timer. If true, trigger a secondary output (e.g., a chainable LED).
      * **Code Requirement:** Use a non-blocking timer (millis()) to ensure other modules remain responsive.### B. Core Overheating (Random Event)  * **Hardware:** DHT11 Temp/Humidity Sensor (Pin **D4**), LCD Screen (I2C: **SDA/SCL**).
  * **Logic:**
      * Randomly trigger a "CORE OVERHEAT\!" message on the LCD.
      * **Defusal:** The player must blow into the sensor vent. The code must monitor humidity levels; once humidity exceeds a specific threshold (indicating breath), the message clears.
  * **Code Comment:** Ensure the threshold is calibrated for ambient conditions.### C. Encoder Tuning Game  * **Hardware:** 2x Angle Encoders (Pins **D5/D6** and **D7/D8**), LED Bar (Pins **D3/D2**).
  * **Logic:**
      * The LED Bar starts fully lit.
      * The player must perform a sequence of rotations (e.g., "Left dial 3 clicks CW, Right dial 2 clicks CCW").
      * **Manual Dependency:** The sequence is determined by the Serial Number.
      * **Feedback:** Each correct step in the sequence turns off one segment of the LED Bar. Completion is reached when only the final red LED is lit.### D. Maze Module  * **Hardware:** RGB LED Matrix (I2C), 2x Angle Encoders (reused from Tuning module or separate).
  * **Logic:**
      * The Matrix displays a "Player" dot and a "Goal" dot.
      * The "Maze" is invisible to the defuser. The Manual Operator has the map.
      * **Code Requirement:** Implement a coordinate system for the 8x8 or 16x16 matrix. Prevent the player dot from moving through "wall" coordinates.### E. Distance Control Module  * **Hardware:** Ultrasonic Sensor (Pins **D9/D10**), LED Bar (reused).
  * **Logic:**
      * The LED Bar acts as a proximity meter.
      * The player must move an object (or their hand) to a specific distance range (e.g., 15cm - 20cm).
      * **Completion:** The correct distance must be maintained for 5 consecutive seconds. If the distance fluctuates outside the range, the timer resets.### F. Compass Alignment Game  * **Hardware:** Compass Sensor (I2C), Buzzer 1 (Output).
  * **Logic:**
      * **Audio Feedback:** Pitch rises as the device rotates toward North (350°-10°) and falls toward South (170°-190°).
      * **Stage 1:** If the sum of the 4 serial digits is **even**, point North. If **odd**, point South. Hold for 5 seconds.
      * **Stage 2:** Convert the 1st Serial Letter to a number (A=1, B=2...). If it matches the last Serial Digit, point North; otherwise, point South.
  * **Code Requirement:** Implement a "Strike" sound and reset the stage if the wrong direction is held.### G. Button Combo Module  * **Hardware:** Red LED Button (Pin **D11**), Green LED Button (Pin **D12**), Buzzer 2 (Pin **D13**).
  * **Logic:**
      * A sequence of button presses is required (e.g., R-G-R-R).
      * **Manual Dependency:** The sequence order is derived from the Serial Number.
      * **Timing:** The entire sequence must be entered within a short time window.
      * **Feedback:** Correct press = short beep; Wrong press = Strike sound + module reset.-----## 4\. Hardware Allocation Summary (Pin Map)

| Component             | Pin / Port    | Module                           |
| :-------------------- | :------------ | :------------------------------- |
| **DHT11 Sensor**      | D4            | Core Overheating                 |
| **Angle Encoder 1**   | D5, D6        | Encoder Tuning / Maze            |
| **Angle Encoder 2**   | D7, D8        | Encoder Tuning / Maze            |
| **Ultrasonic Sensor** | D9, D10       | Distance Control                 |
| **Red LED Button**    | D11           | Button Combo                     |
| **Green LED Button**  | D12           | Button Combo                     |
| **Buzzer 2**          | D13           | Button Combo                     |
| **LCD Screen**        | I2C (SDA/SCL) | Global / Serial Display          |
| **RGB LED Matrix**    | I2C           | Maze Module                      |
| **Compass**           | I2C           | Compass Alignment                |
| **LED Bar**           | D3, D2        | Distance / Tuning Feedback       |
| **4-Digit Display**   | D4            | Timer                            |
| **Buzzer 1**          | Shared        | Global Sounds (Strikes/Complete) |

## 5\. Developer Notes1.  **Serial Communication:** Use the Serial Monitor for debugging module states (e.g., `Serial.println("Module 3: Stage 2 Complete");`).
2.  **Modular Code:** Write each game as a separate function or class to allow for easy testing and integration.
3.  **Interrupts:** Consider using interrupts for the Angle Encoders to ensure no "clicks" are missed during rotation.