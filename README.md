# Closed-Loop Motor Speed Control System (STM32)

## Overview
This project implements a closed-loop motor speed control system using an STM32 microcontroller. The system regulates motor speed using a proportional feedback controller and PWM-based power control. User input is provided through a 4×4 matrix keypad, and system performance is verified through experimental testing.

The design focuses on embedded control implementation, PWM motor drive, and real-time feedback using a tachometer-based sensing system.

---

## System Objectives
- Regulate motor speed with **≤5% steady-state error**
- Accept user input via a **4×4 matrix keypad**
- Generate **9 kHz PWM signal** for motor power control
- Implement an **up/down decade counter with ≤0.01% error**

---

## System Description

### User Input
- A 4×4 matrix keypad is used for user input
- Keypad is read using a **row-column scanning method**
- Input selects predefined motor speed levels mapped to PWM duty cycles

---

### PWM Motor Control
- PWM signal generated using **STM32 timer peripheral**
- PWM frequency set to **9 kHz**
- PWM output drives a **MOSFET low-side switching stage**
- Motor powered from a **15V supply modulated via PWM duty cycle**

---

### Speed Sensing and Feedback
- Motor speed measured using a **tachometer generating an AC signal proportional to speed**
- Tachometer output is passed through a **rectifier stage**
- Rectified signal produces a DC voltage proportional to motor speed
- Feedback signal is used for closed-loop control

---

### Control System
- A **proportional feedback controller** adjusts PWM duty cycle based on speed error
- System continuously compares measured speed vs. target speed
- Controller tuning performed experimentally to achieve stability and low steady-state error

---

## Performance Requirements
- Steady-state speed error: **≤5% (met)**
- Decade counter accuracy: **≤0.01% (implemented as specified)**
- Stable PWM generation at **9 kHz**
- Reliable closed-loop response under load variation

---

## Hardware Implementation
- STM32 microcontroller
- DC motor
- MOSFET driver (low-side switching configuration)
- Tachometer feedback system
- Rectifier circuit for speed measurement
- 4×4 matrix keypad
- 15V motor power supply

---

## Software / Tools
- Embedded C (STM32 firmware)
- STM32 timer peripherals (PWM generation)
- Matrix keypad scanning routine
- Basic proportional control algorithm

---

## Repository Structure
