# Goals

Source: https://tomasvicar.github.io/AUI-public/labs/real-time-ai.html
(MPA-PRM, Application IV: real-time AI on a microcontroller. A 4-hour graded group project, demonstrated live.)

## What we build
A small real-time monitoring device: an ESP32-S3 with an MPU-6050 (HW-123 / GY-521) accelerometer and gyroscope that recognizes what is happening to it and shows the state on the board's RGB LED.

| LED     | State                                        |
|---------|----------------------------------------------|
| green   | calm breathing (sensor on the chest)         |
| red     | fast breathing (sensor on the chest)         |
| blue    | not attached (held in the hand or lying still) |
| magenta | shaking                                      |

## Hard constraints
- Sensor reading, features and classification all run **on the ESP32-S3**. The computer is used only for training and flashing.
- During the demo, USB provides **power only**: no data and no computation over it.
- One device supports all four states.
- **Compulsory (otherwise the whole project gets 0 points):**
  - a GitHub repository, shared with the user `tomasvicar` (Settings → Collaborators) before the lab ends
  - at least 3 intermediate commits, each pushed during the lab

## Graded parts (10 points, each counts only if shown live)
| # | Part | Points | Demo |
|---|------|--------|------|
| 1 | Movement vs. shaking | 2.5 | slow movement in the hand → blue, shaking → magenta, and back |
| 2 | Breathing (all 4 states) | 2.5 | on the chest, calm breathing → green, fast breathing → red; taken off, held or put down → blue |
| 3 | BLE monitor | 1.5 | GitHub Pages site using Web Bluetooth: shows the current state and a timeline; alarms when fast breathing or "not attached" lasts too long, and when the connection is lost |
| 4 | Web page served by the board | 1 | board runs a Wi-Fi hotspot; its page shows the live signal and the prediction with all class probabilities |
| 5 | Own extension | 1.5 | one idea done properly; explain what it adds and how it was done |
| 6 | Documentation | 1 | README with group members, a link to the monitoring page and a wiring photo; also covers build/flash, data recording, training, results and AI-tool usage |

## Principles (from the task and CLAUDE.md)
- Start with part 1 to get the whole chain working end to end, then add the breathing classes.
- Evaluate honestly: split **whole recordings** between train and test before cutting them into windows.
- Start with a simple baseline (hand-made window features + a simple model) before trying anything more complex.
- Report only metrics we actually measured.
- Document which AI tools were used and for what. Every member must be able to explain the code.
