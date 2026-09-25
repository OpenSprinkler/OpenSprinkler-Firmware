# Using Weather Underground (WU)

[Weather Underground](https://www.wunderground.com/) provides observations from Personal Weather Stations (PWS). OpenSprinkler firmware 2.2.0 and later can use a WU station for current and historical observations, together with forecast data for the controller's location.

Apple WeatherKit is the default provider and is recommended for general use. Choose WU when you have access to a valid WU API key and want observations from a specific PWS.

!!! note
    Weather Underground controls its account, PWS-registration, and API-key policies. Its website and setup steps may change independently of OpenSprinkler.

---

## Step 1: Create a WU Account

Visit the [Weather Underground sign-up page](https://www.wunderground.com/signup) and create an account, or sign in to an existing account.

---

## Step 2: Register a Personal Weather Station

WU provides API access through accounts associated with a Personal Weather Station:

1. Log in to your WU account and click **My Profile** in the upper-right corner.
2. Open the **My Devices** tab.
3. Add a new PWS device. You do not need to own a physical weather station to complete this registration.
4. Enter your address and select one of the available device types, such as **AcuRite 3-in-1 Weather Station**.
5. Complete the registration. WU generates a PWS station ID for the new device.

If you do not own a physical PWS, the generated station will not submit valid weather observations. **Do not use that inactive station for weather adjustments.** Its registration allows you to create an API key in Step 3; select a nearby active PWS as the OpenSprinkler weather location in Step 4.

---

## Step 3: Create an API Key

After the PWS has been registered, open the **API Keys** tab in your WU account and create an API key. WU normally assigns one key per account, regardless of the number of registered devices. The key is a 32-character lowercase hexadecimal string.

Treat the API key as a private credential and do not publish it.

---

## Step 4: Configure WU in OpenSprinkler

1. On the OpenSprinkler homepage, go to **Edit Options** → **Weather Adjustment**.
2. Select **WeatherUnderground** as the **Weather Data Provider**.
3. Enter the WU API key and select **Verify**.
4. Submit the changes.
5. Return to **Edit Options** → **System** and select **Location**. Nearby WU stations appear as blue dots on the map.
6. Select an active nearby station and submit the location. OpenSprinkler records that station's PWS ID and sets the controller location to the station's coordinates.

The selected PWS must report valid observations. A nearby station that is offline or missing required measurements can cause weather updates to fail.

For an overview of provider capabilities and adjustment methods, see [Using Weather Adjustments](weather-adjustments.md).

**Troubleshooting: If the Blue Dots Do Not Appear**

Move or zoom the location map and allow it time to refresh. If the blue dots still do not appear, you can set the station ID through a configuration export:

1. Export the controller configuration immediately before editing it, and keep an unchanged backup copy. Importing restores the complete configuration, so do not make other controller changes between the export and import.
2. Inside the exported file's `settings.wto` object, set `"pws"` to the station ID. If the field is missing, add `"pws":"<station ID>"` to that object. Confirm that the same object also contains `"provider":"WU"` and the lowercase `"key":"<API key>"`.
3. Save the edited JSON file and import it into OpenSprinkler.

Use only letters and numbers in the PWS station ID. An invalid station ID or API key causes the weather request to be rejected.
