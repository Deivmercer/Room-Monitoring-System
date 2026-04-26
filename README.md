# Room Monitoring System

This project implements an IoT monitoring system designed to collect environmental data across multiple rooms in a building. The system measures temperature, humidity, and Wi-Fi signal strength, and provides immediate visual feedback through LEDs based on configurable threshold values. Collected data is stored in InfluxDB and visualized through dashboards, enabling real-time monitoring and historical analysis.

## Hardware materials

- NodeMCU ESP8266
- Temperature and humidity sensor (DHT11)
- Button (KY-004)
- Linear potentiometer (KY-040)
- Display (LCD1602A)
- Yellow led
- 220Ω resistor
- RGB led (KY-009)

## Sketch

![Sketch](sketch_bb.png)