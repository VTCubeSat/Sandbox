    /* RFM69 library and code by Felix Rusu - felix@lowpowerlab.com
    // Get libraries at: https://github.com/LowPowerLab/
    // Make sure you adjust the settings in the configuration section below !!!
    // **********************************************************************************
    // Copyright Felix Rusu, LowPowerLab.com
    // Library and code by Felix Rusu - felix@lowpowerlab.com
    // **********************************************************************************
    // License
    // **********************************************************************************
    // This program is free software; you can redistribute it 
    // and/or modify it under the terms of the GNU General    
    // Public License as published by the Free Software       
    // Foundation; either version 3 of the License, or        
    // (at your option) any later version.                    
    //                                                        
    // This program is distributed in the hope that it will   
    // be useful, but WITHOUT ANY WARRANTY; without even the  
    // implied warranty of MERCHANTABILITY or FITNESS FOR A   
    // PARTICULAR PURPOSE. See the GNU General Public        
    // License for more details.                              
    //                                                        
    // You should have received a copy of the GNU General    
    // Public License along with this program.
    // If not, see <http://www.gnu.org/licenses></http:>.
    //                                                        
    // Licence can be viewed at                               
    // http://www.gnu.org/licenses/gpl-3.0.txt
    //
    // Please maintain this license information along with authorship
    // and copyright notices in any redistribution of this code
    // **********************************************************************************/

    #include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
    #include <SPI.h>
    #include <Adafruit_GPS.h>
    //*********************************************************************************************
    // *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
    //*********************************************************************************************
    #define NETWORKID     100  // The same on all nodes that talk to each other
    #define NODEID        6    // The unique identifier of this node
    #define RECEIVER      255  // The recipient of packets
     
    //Match frequency to the hardware version of the radio on your Feather
    #define FREQUENCY     RF69_433MHZ
    #define IS_RFM69HCW   true // set to 'true' if you are using an RFM69HCW module
    double rf_freq = 433000000;
    //*********************************************************************************************
    
    #define SERIAL_BAUD   9600
    #define GPS_BAUD      9600
    #define TX_RATE       1000 //number of milliseconds between TX window
    #define TX_OFFSET     0  //Absolute Value of random TX Rate offset
    #define CALLSIGN      "BEACON1"
    
    // Feather 32u4 w/wing
    #define RFM69_RST     A4   // RFM69 Reset pin
    #define RFM69_CS      12   // "E" RFM69 Chip Select Pin
    #define RFM69_IRQ     2    // "SDA" (only SDA/SCL/RX/TX have IRQ!) RFM69 Interrupt 0
    #define RFM69_IRQN    digitalPinToInterrupt(RFM69_IRQ)
    #define RFM69_DIO1    11  // RFM69 Interrupt 1
    #define RFM69_DIO2    10  // RFM69 Interrupt 2
    #define RFM69_DIO3    6   // RFM69 Interrupt 3
    #define RFM69_DIO4    5   // RFM69 Interrupt 4
    #define RFM69_DIO5    A5  // RFM69 Interrupt 5
    
    #define RED_LED       13  // onboard blinky
    #define BATT_MON      9   // Battery Monitor, analog input
    
    #define GPS_RESET     A0  // GPS Reset Pin. ACTIVE LOW
    #define GPS_FIX       A1  // GPS Fix Pin
    #define GPS_EN        A2  // GPS Enable Pin ACTIVE LOW
    #define GPS_TX        0  // GPS TX Pin, Serial RX Pin 
    #define GPS_RX        1  // GPS RX Pin, Serial TX Pin
    #define GPS_PPS       3  // "SCL" HW Interrupt pin

    #define SD_CS         4   // uSD Card 'Chip Select' Pin
    #define SD_CD         7   // uSD Card 'Card Detect' Pin
    #define GREEN_LED     8   // uSD Card blinky
    
    #define GPSSerial Serial1
    
    //*********************************************************************************************
    uint16_t packetnum = 0;  // packet counter, we increment per xmission
    RFM69 radio = RFM69(RFM69_CS, RFM69_IRQ, IS_RFM69HCW, RFM69_IRQN);
    
    // Connect to the GPS on the hardware port
    Adafruit_GPS GPS(&GPSSerial);
    
    volatile bool state = false;
    volatile uint32_t timer = millis();

    String msg;
    long random_delay = random(-TX_OFFSET, TX_OFFSET);
     
    void setup() {
      //configure Interrupts
      attachInterrupt(digitalPinToInterrupt(GPS_PPS), PPS_ISR, RISING);

      //Configure Pin Modes
      pinMode(GREEN_LED, OUTPUT);
      pinMode(RED_LED, OUTPUT);
      
      pinMode(GPS_RESET,OUTPUT);
      digitalWrite(GPS_RESET, HIGH); //Active HIGH, Pull LOW to reset GPS
      pinMode(GPS_FIX,  INPUT);
      pinMode(GPS_EN,   OUTPUT);
      digitalWrite(GPS_EN, LOW); //Active LOW, Pull HIGH to Disable GPS
      pinMode(GPS_PPS,  INPUT);

      
      //while (!Serial); // wait until serial console is open, remove if not tethered to computer
      Serial.begin(SERIAL_BAUD);
      Serial.println("GPS Beacon");

      //*****Setup Radio*****
      // Hard Reset the RFM module
      pinMode(RFM69_RST, OUTPUT);
      digitalWrite(RFM69_RST, HIGH);
      delay(100);
      digitalWrite(RFM69_RST, LOW);
      delay(100);
     
      // Initialize radio
      radio.initialize(FREQUENCY,NODEID,NETWORKID);
      if (IS_RFM69HCW) {
        radio.setHighPower();    // Only for RFM69HCW & HW!
      }
      radio.setPowerLevel(31); // power output ranges from 0 (5dBm) to 31 (20dBm)
      radio.setFrequency(rf_freq);
      Serial.print("\nTransmitting at ");
      Serial.print(FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
      Serial.println(" MHz");
      
      //*****Setup GPS*****
      GPS.begin(GPS_BAUD);
      // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
      GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
      // Set the update rate
      GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ); // 1 Hz update rate
      // For the parsing code to work nicely and have time to sort thru the data, and
      // print it out we don't suggest using anything higher than 1 Hz
         
      delay(1000);
    }
     
     
    void loop() {
      if (Serial1.available()) {
        char c = GPS.read();
        //Serial.write(c);
      }
      if (GPS.newNMEAreceived()) {
        // a tricky thing here is if we print the NMEA sentence, or data
        // we end up not listening and catching other sentences!
        // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
        //Serial.println(GPS.lastNMEA()); // this also sets the newNMEAreceived() flag to false
        if (!GPS.parse(GPS.lastNMEA())) // this also sets the newNMEAreceived() flag to false
          return; // we can fail to parse a sentence in which case we should just wait for another
      }
      //if (millis() - timer > TX_RATE) {
      if (millis() - timer > (TX_RATE + random_delay)) {
      //if (state) {

        uint16_t batt_adc = analogRead(BATT_MON);
        //Serial.print("ADC Bat: " ); Serial.println(batt_adc_val, HEX);
        //measuredvbat *= 2;    // we divided by 2, so multiply back
        //measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
        //measuredvbat /= 1024; // convert to voltage
        //Serial.print("VBat: " ); Serial.println(measuredvbat);
        
        if (GPS.fix) {
          uint8_t SRC  = NODEID; //Node ID of transmitting packet
          uint8_t DST  = RECEIVER; //Intended recipient of PAcket
          uint8_t FLAG = 126; //HEX: 0x7E, BIN: 0111110, HDLC Flag used to mark end of payload.
          
          packetnum++;
          
          uint8_t hour          = GPS.hour;
          uint8_t minute        = GPS.minute;
          uint8_t second        = GPS.seconds;
          uint16_t millisecond  = GPS.milliseconds;
          
          float lat = GPS.latitudeDegrees;//units of degrees
          float lon = GPS.longitudeDegrees;//units of degrees
          float alt = GPS.altitude * 3.28084; //convert meters to feet
          float spd = GPS.speed * 1.150779; //convert knots to mph
          float crs = GPS.angle; //units of degrees

          unsigned char msg[32];
          memset(msg, 0, sizeof(msg));
          
          memcpy(msg   , &SRC, 1);
          memcpy(msg+1 , &DST, 1);
          memcpy(msg+2 , &packetnum, 2);
          memcpy(msg+4 , &hour, 1);
          memcpy(msg+5 , &minute, 1);
          memcpy(msg+6 , &second, 1);
          memcpy(msg+7 , &millisecond, 2);
          memcpy(msg+9 , &lat, 4);
          memcpy(msg+13, &lon, 4);
          memcpy(msg+17, &alt, 4);
          memcpy(msg+21, &spd, 4);
          memcpy(msg+25, &crs, 4);
          memcpy(msg+29, &batt_adc, 2);
          memcpy(msg+31, &FLAG, 1);



          /* *****DON'T DELETE, YOU NEED THIS IN THE RECEIVER*******
          float lat2 = 0;
          memcpy(&lat2, msg+8, 4);
          */

          //for (int i=0; i<sizeof(msg); i++){
          //  Serial.print(msg[i], HEX); Serial.print(" ");
          //}
          
          //Serial.println(lat2, 6);
          
          Serial.print("  Delay: "); Serial.println(TX_RATE+random_delay);
          Serial.print(" Length: "); Serial.println(sizeof(msg));
          Serial.print("Message: "); //Serial.println(msg);
          PrintHex8(msg, sizeof(msg));
          Serial.println("");
          Serial.print(SRC); Serial.print(" ");
          Serial.print(DST); Serial.print(" ");
          Serial.print(packetnum); Serial.print(" ");
          Serial.print(hour); Serial.print(" ");
          Serial.print(minute); Serial.print(" ");
          Serial.print(second); Serial.print(" ");
          Serial.print(millisecond); Serial.print(" ");
          Serial.print(lat, 6); Serial.print(" ");
          Serial.print(lon, 6); Serial.print(" ");
          Serial.print(alt, 6); Serial.print(" ");
          Serial.print(spd, 6); Serial.print(" ");
          Serial.print(crs, 6); Serial.print(" ");
          Serial.print(FLAG); Serial.println(" ");
          
          radio.send(RECEIVER, msg, sizeof(msg));
          Blink(RED_LED,50,2);
        }

        random_delay = random(-TX_OFFSET,TX_OFFSET); //reset random tx rate
        timer = millis(); // reset the timer
      }
      
      /*if (state) {
        Blink(RED_LED,50,2);
        state = false;
      }*/
    }
    void PrintHex8(uint8_t *data, uint8_t length){ // prints 8-bit data in hex with leading zeroes
    //Serial.print("0x"); 
      for (int i=0; i<length; i++) { 
        if (data[i]<0x10) {Serial.print("0");} 
        Serial.print(data[i],HEX); 
        //Serial.print(" "); 
      }
    }

    void PPS_ISR(){
      state= true;
      //timer = millis();
    }
     
    void Blink(byte PIN, byte DELAY_MS, byte loops)
    {
      for (byte i=0; i<loops; i++)
      {
        digitalWrite(PIN,HIGH);
        delay(DELAY_MS);
        digitalWrite(PIN,LOW);
        delay(DELAY_MS);
      }
    }
