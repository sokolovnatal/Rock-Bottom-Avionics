<img src="https://yt3.googleusercontent.com/ytc/AL5GRJX-fTvp_QLr1lVtEUI5mkb8UpQ7PthtJGi0XEBB=s900-c-k-c0x00ffffff-no-rj" width="160" height="160">

# Rock Bottom Avionics

Controls RIT Launch Initiative's Space Race's Rock Bottom's L2 Rocket. Whilst grounded, the dev board used (Teensy 4.1), connects to a PC via ethernet to display battery voltages and can get inputs to turn off and on a dedicated flight computer (proof of concept to show we can control critical components reliable.) 

On launch the magnetic ethernet connector disconnects, and the Teensy stores raw data from an Adafruit AHT20 sensor (humidity and temp), Adafruit LSM9DS1 sensor (acceleration, gyro, magnometer, and temp), Adafruit LPS22 sensor (pressure and temp), and Adafruit ADXL377 sensor (high G acceleration) via it's onboard SD card. Records data at around 100Hz. Continues until power depletion.


## Authors

- [@sokolovnatal](https://github.com/sokolovnatal)
- [@JanosBanocziRuof](https://github.com/JanosBanocziRuof)
- [@desuarez17](https://github.com/desuarez17)
