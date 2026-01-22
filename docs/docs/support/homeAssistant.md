# Using OpenSprinkler with Amazon Echo or Google Home Assistant

You can use voice input such as Amazon Echo or Google Home Assistant with OpenSprinkler. This is done through an integration with IFTTT. The basic steps are:

* Go to the IFTTT website or use the IFTTT mobile app to log in.
* Create an applet where the **trigger (this)** can be **Amazon Alexa**, **Google Assistant**, **SMS** message, etc.
* The **action (that)** should be a **Webhook** that sends a web request to your OpenSprinkler to trigger a certain action, such as turning on a zone or starting a program.
    * To find out the specific HTTP command you wish to use, check the [OpenSprinkler API documentation](../2.2.1/221_4_api.md).
    * The web request method is GET, and the content type is text/plain.
* If your controller runs firmware 2.1.9 or prior, the web request requires you to set up port forwarding on your router in order for an external web request to reach your OpenSprinkler.
    * The port forwarded request is in the form of `http://external_ip:external_port/command_and_parameters`. For example, the request to turn zone 1 on for 10 minutes is: `http://external_ip:external_port/cm?pw=your_password_in_md5&sid=0&en=1&t=600`
* If you have firmware 2.2.0 or later, you can enable the OTC token. This allows you to send a web request using the OTC token to avoid port forwarding on your router.
    * The OTC web request is in the form of `https://cloud.openthings.io/forward/v1/your_otc_token/command_and_parameters`. For example, the request to turn on zone 1 for 10 minutes is: `https://cloud.openthings.io/forward/v1/your_otc_token/cm?pw=your_password_in_md5&sid=0&en=1&t=600`

Archived information: The content of the previous version of this article is in [this blog post](https://opensprinkler.com/ifttt-with-opensprinkler-and-opengarage/).
