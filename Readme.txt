Vending-machine v1.0 (working) Developed by Nxt-999

The serial-and LCD-messages are in German, so you might need to translate it for your purpose, as well as you might need to change special character placement. I translated the comments for you, so it's easier to undertsand what the code's doing.

Features

🛒 2 products available – selectable via 3x4 matrix keypad – confirmed with #, reset with *

⚙️ 2 gear motors – one for each product slot (e.g. product 22 and 58) – configurable duration and direction

💰 Coin detection – uses an infrared sensor to detect payment – product dispensing only allowed after valid payment

💡 (Optional) Product detection via light sensor – recognizes if a product was successfully dispensed – currently disabled in code, can be enabled when needed

🔐 Secured output flap – servo opens flap only after payment and motor rotation – anti-theft protection included

📟 User-friendly LCD interface – displays instructions, product numbers, prices, and confirmation messages – supports special characters like €, ä, ü for native messages

🧠 Highlights Input blocking during active process (no double entry or skipping payment)

All user actions reflected on the LCD in real time

Easy to expand: add more motors, sensors or new keypad logic

Modular code with named display functions and full Serial debugging

🔧 Hardware Used

Arduino Mega
3x4 Matrix Keypad
LCD I2C 16x2 Display
2x Gear Motors (controlled via digital pins)
1x IR Sensor (coin detection)
1x Servo Motor (flap control)
[Optional] Analog light sensor (product verification)