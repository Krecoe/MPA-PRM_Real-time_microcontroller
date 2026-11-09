# PROJECT GOAL
Exact instructions are here:
https://tomasvicar.github.io/AUI-public/labs/real-time-ai.html


Build a small real-time monitoring device (accelerometer and gyroscope) on a ESP32 microcontroller that can classify what is happening to it - whether the microcontroller lies calmly or lies on a steadily/rapidly breathing chest, whether it's being shaken or held in hand without breathing motion (exact classes will be explained further down).

- Collect our own data with the sensor and label it
- train a classification model and validate it
- run it on the board in real time, show the class result on RGB LED
- build a website hosted by the board itself for viewing and monitoring the results over the Bluetooth Low Energy and raisesalarm as a bedside monitor would
- document everything so someone can rebuild this device
- computation must run only on the chip, computer is only for training the model and adding power supply to the chip

# GITHUB COLLABORATION
This project is a group project and my collaborator is the main person, I am just a supporting role.

# Pipeline
- create a PIPELINE.md where we will make plans and discuss the pipeline for this project.
If you have any questions, ask me instead of assuming. Ask more than necessary, even.