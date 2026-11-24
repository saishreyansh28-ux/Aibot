# Aibot
A bot that has AI in it.

<img width="435" height="517" alt="Screenshot 2026-11-24 at 4 53 32 PM" src="https://github.com/user-attachments/assets/ac54b260-4d75-4e95-8286-f9fb05305e7c" />

 It connects to your api key and runs the AI
 You replace the code placeholder labeled API, WiFi information with your details and upload to the device.

 It runs using an 
 *ESP32 S3 Mini
 
 *OLED
 
 *Microphone
 
 *Speaker
 
 *Audio Amplifier

 #CASE

 
 <img width="563" height="363" alt="Screenshot 2026-11-24 at 5 06 06 PM" src="https://github.com/user-attachments/assets/09923ec1-6c52-46d3-a4bb-8f579276a833" />

The Oled shows the answer in text on the screen and we can speak to the AI model using the Microphone.

It answers using the speaker
If you cannot hear the words properly, you can alternatively use the OLED Display for the response.

#SCHEMATIC


<img width="714" height="462" alt="Screenshot 2026-11-24 at 5 01 09 PM" src="https://github.com/user-attachments/assets/6144eb15-c643-45de-b2f2-a30e29b960c9" />


#Parts Footprints:
*INMP441(Microphone)

*ESP32-S3-MINI-1(Microcontroller)

*ER_OLEDM0.91_1x-I2C(Display)

*BQ76952PFB(Battery Management System)

*MAX98357A(Audio Amplifier)


#USAGE
While uploading the provided code to the microcontroller, edit the placeholders. Enter your WiFi Information and the Place Holder for Api Key and AI model.
If running a local llm, take the api key provided by the llm and paste it there and edit the AI model with the one you are running.


