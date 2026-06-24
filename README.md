# Stop Talking and Everybody Explodes
Stop talking and everybody explodes is an open-source take on the popular computer game [Keep Talking and Everybody Explodes](https://keeptalkinggame.com/), transforming it into a physical game to play with your friends!

You must work closley with the other player to solve a series of puzles and work around events all before the time runs out, resulting in a regrettable increase in room temperature!

This game is best enjoyed by using a physical copy of the manual (download and print manual.pdf), however an online mobile-friendly version is also avalible at [manual.ericmcc.com](https://manual.ericmcc.com/) (or the raw pdf at [staee.ericmcc.com](https://staee.ericmcc.com/))

This game was put together by a team of 6 of us for a university project, while there may be some future updates made we do not gaurantee any further maintinance

## Build Process
All components used are very generic and inexpencive. I have linked each item below to where i purchased it on aliexpress, however I recomend looking for potentially better prices as they change often! 

### Hardware
* [Arduino Mega 2560](https://www.aliexpress.com/item/32837393855.html)
* [LED Bar](https://www.aliexpress.com/item/1005007198230035.html) (Note: You must solder a 220Ω or 330Ω resistor on EVERY positive terminal for it to function. Pre wired ones basically dont exist, [there are some](https://www.aliexpress.com/item/1005007804923693.html) however they require seperate packages and work a bit differently)
* [TM1637 4 bit display](https://www.aliexpress.com/item/1005009269502486.html)
* 2x [Rotary Encoders](https://www.aliexpress.com/item/1005009578504178.html)
* [Microphone switch](https://www.aliexpress.com/item/1005009742731611.html)
* [Small buzzer](https://www.aliexpress.com/item/1005007287329656.html)
* [WS2182B-64 8x8 RGB LED Matrix](https://www.aliexpress.com/item/1005010747838637.html)
* [I<sup>2</sup>C LCD Display](https://www.aliexpress.com/item/1005011648206201.html)
* The + and - rail from a [400 pin bread board](https://www.aliexpress.com/item/1005006713173854.html)
* [12mm Key Switch](https://www.aliexpress.com/item/1005009886103118.html)
* 2x [12mm Momentary Push Switch](https://www.aliexpress.com/item/32814739163.html) (Note: A pre wired one may be wise as the raw connectors are very difficult to solder)
* A bunch of 10mm and 20mm [DuPont Cables](https://www.aliexpress.com/item/4000204858217.html)
* A small power bank, any should work but i use [this one](https://www.aliexpress.com/item/1005008094289372.html)

Not directly avalible for purchase is the enclosure which is custom made. All required files have been included so you can print your own!
  
  **Note:** For standard 3d prints use the .STL files, if you would like to print the text in a different colour use the .3mf file. There is also the .f3d if you would like to tweak the design!
  
  **Note:** Make sure you print **10** of the DuPont holders, my recomendation is to print using 5 colours (printing 2 in each colour) however you *could* get away without this

### Software
* Install the [Arduino IDE](https://docs.arduino.cc/software/ide/), download the code in the repository, unzip it and open it using the IDE
* There are a few external packages you must install in the IDE to get everything working, these are all listed at the top of the .ino file!
* After everything is installed connect up the arduino to your computer and upload the code

### Setup
Now that the ardino has the software uploaded to it, disconnect it from the computer before plugging in all the hardware

The .ino file contains a pin map at the top detailing which pin must be connceted to which component. Make sure you also connect up all the VCC and GND pins to the breadboard, connecting each rail to the arduino!

I would also sugest putting a bit of hot glue on the connectors as the DuPont cables can sometimes slide off the components, however you can probably get away without glue.
