# Interactions Between Rain Delay, Rain Sensor, and Weather Algorithm

* **Rain Delay**: A **manual control** that prevents watering for a specified duration set by the user.
* **Rain Sensor**: Requires an **external rain sensor**, which automatically stops watering when rain is detected.
* **Weather-Based Adjustment**: Uses real-time weather data to dynamicaly adjust the watering percentage based on temperature, humidity, and precipitation.

Although these features serve similar purposes, they **operate independently**. The chart below outlines their differences:

| **Trigger** | **Action** | **Delay Time** | **Notes** |
| ----------- | ---------- | -------------- | --------- |
| **Rain Sensor** | Triggered by an external rain sensor. Stops watering of stations (except those set to **ignore rain**). | Built-in mechanical delay, adjustable by evaporation vents on the sensor. | Stations that are stopped or scheduled to run during the delay time will **NOT** resume after the delay time is over. |
| **Rain Delay** | Manually set by the user. Stops watering of stations for a specified duration (except those set to **ignore rain delay**). | Defined by the user. | Same as Rain Sensor. |
| **Weather-Based Adjustment** | Watering percentage automatically calculated by the selected adjustment method using real-time weather data. | No explicit delay time. Watering percentage is calculated based on weather conditions. | Accuracy depends on the proximity of your closest weather station. |
