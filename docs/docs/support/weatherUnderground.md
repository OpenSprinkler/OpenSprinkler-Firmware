# Using Weather Underground (WU)

## Background

Weather Underground (WU) was acquired by IBM in 2019, resulting in changes to their API service and the process to obtaining an API key. As of now, OpenSprinkler's default weather service is Apple WeatherKit, and WU is no longer recommended. However, firmware 2.2.0 and later still support WU Personal Weather Stations (PWS). The following guide explains how to acquire a **WU API key** and **PWS station name** as of **July 16, 2019**. These steps may change over time, so proceed at your own discretion.

## Step 1: Create a WU Account

Visit <https://www.wunderground.com/signup> and register for a WU account.

## Step 2: Set Up a Personal Weather Station (PWS)

* Log in to your WU account and click "**My Profile**" (top-right corner of the screen).
* Navigate to the "**My Devices**" tab.
* Add a PWS device (you don't need to own a physical weather station).
* Enter your address, then select any available device (e.g. Acurite 3-in-1 Weather Station).
* A PWS station name will be generated for you.

!!! note
    If you don't own a physical PWS, the generated PWS will not provide valid data. In that case, do NOT use this PWS for weather adjustments.

## Step 3: Create an API Key

Once your PWS device is created, go to the "**API Keys**" tab and create a new API key. WU assigns one key per account regardless of how many devices you have, which will be a 32 character string.

## Step 4: Find and Choose a Nearby PWS Station

When using Weather Underground as a data provider, the location **MUST BE** a valid PWS location with a valid ID. To choose one:

* On your OpenSprinkler's homepage go to **Edit Options --> Weather and Sensors** and select **Weather Underground** as the Weather Data Provider.
* Enter the **WU API key** you created. Click the "**Verify**" button to confirm it is correct.
* **Submit** the changes and go to **Edit Options --> Location**. Nearby PWS stations will appear on the map as **blue dots**.
  * If they do not show up, try moving the map around until they appear.
* Select a blue dot and **Submit** the changes. OpenSprinkler will automatically record the PWS station name.

\
If the blue dots do not show up, use this work around to set the PWS location name manually:

* Export a copy of your configuration (.json file)
* Open the .json file, look for the **"pws":""** field, and enter your PWS location name directly inside the empty quotes.
* Save the file and import it to your controller.
