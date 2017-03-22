#include <SPI.h>

// constant variables, including pin outputs
#define OUTINIT 0x00 //pin config to HF2 at init
#define CONVT 1024 //conversion factor from analog read to T
#define CONVH 1024 //conversion factor from analog read to H
#define INPUT_SIZE 15
#define RATE 20
#define FREQ 100000 //SPI communication at 100 kHz
#define NRELECTRODES 20 //nr of electrodes in the chip
#define MASK 0b00000001

 //global variables
boolean readTandHumFlag=false; //from the initialization, see whether to measure Temp and Humidity
String strErr="Command not recognized";
String strOut="";


const int pinHF2[]={5, 6, 7, 8, 9, 10, 11, 12}; //digital pins connected to HF2
const int SSs[]={0, 1, 2}; // slave select pins for electrode, reference, counter
const int pinT=0; //analog pin for temperature sensor
const int pinH=1; //analog pin for humidity sensor



void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.setTimeout(RATE);

  SPI.begin(); //initialize the SPI protocol


  //Set analog reference to 3.3V to improve resolution on the ADC. The aref pin is connected through a resistor to 5V
  //analogReference(EXTERNAL); //Uncomment once on the board!

  //Initialize pins
  for(int ii=0; ii<sizeof(SSs)/2; ii++){ //the division by 2 is necessary as each int is 2bytes. Otherwise, sizeof would return a value which is double
    pinMode(SSs[ii], OUTPUT);
    digitalWrite(SSs[ii], HIGH); //slave select is activated by falling edge
  }

  for(int ii=0; ii<sizeof(pinHF2)/2; ii++){
    pinMode(pinHF2[ii], OUTPUT);
  }
  disconnectAll();
  
  

}

void loop() {

    if (Serial.available() > 0) {
      strOut="";
    //If there's a command, interpret that and execute accordingly. Otherwise, return the Temp and Humidity if the sensors are connected. Run at ~50Hz
    char incomingCommand[INPUT_SIZE+1]={'\0'};
    Serial.readBytes(incomingCommand, INPUT_SIZE);
    //Serial.println(incomingCommand);

  
    switch(incomingCommand[0]){
      case 'I'://initialize. It also disconnects all the inputs
        initialize(incomingCommand);
        break;
      case 'E': //change electrode, reference or counter electrode
      case 'R':
      case 'C':
        changeElectrode(incomingCommand);
        break;
      case 'T': //read/stop temperature and humidity
        if(readTandHumFlag){readTandHumFlag=false;}else{readTandHumFlag=true;};
        break;
      default: //command not recognized
        strOut=strErr;
        break;
   }
   Serial.println(strOut);
  }
  else{
    delay(RATE); //50 Hz
  }

  //
  if(readTandHumFlag){
    readTandHum();
    Serial.println(strOut);
  }
}


// It initializes the board by disconnecting all the switches,
// sending a 0 value to the HF2 and setting the flag for the temperature and humidity readout
void initialize(char initCommand[]){
  String strResult = "Initialized";
  //Serial.println(strResult);
  char* command=strtok(initCommand, " ");
  readTandHumFlag=false;
  while(command!=NULL){
    if(strncmp(command,"TH", 2)==0){
      readTandHumFlag=true; //if in initialization the program requires Temp and Humidity, then change the flag
      strResult+= " and read Temp and Humidity";
      
    }
    //Serial.println(command);
    command=strtok(NULL, " ");
  }
  
  strOut=strResult;
  disconnectAll();
  return;
}


// FUNCTION TO CHANGE THE SWITCHES FOR THE ELECTRODE, REFERENCE AND COUNTER
void changeElectrode(char elecCommand[]){
  char* command=strtok(elecCommand, " "); //used to parse the command and see whether to assign electrode, reference and counter
  int elecNr=33;
  int refNr=34;
  int ctNr=35;
  int tempNr;
  int SSsNr = 0;
  while(command!=NULL){
    //Serial.println(command);
    char* separator=strchr(command, ':');
    
    switch(command[0]){
      case 'E': {SSsNr=SSs[0]; if(separator!=NULL){elecNr=atoi(separator+1); tempNr=elecNr; break;}} //assign electrode
      case 'R': {SSsNr=SSs[1]; if(separator!=NULL){refNr=atoi(separator+1);  tempNr=refNr; break;}} //assign reference
      case 'C': {SSsNr=SSs[2]; if(separator!=NULL){ctNr=atoi(separator+1);   tempNr=ctNr; break;}} //assign counter
    }
    //Serial.println(tempNr);
    if(elecNr!=refNr && elecNr!=ctNr && refNr!=ctNr && tempNr<=NRELECTRODES &&tempNr>0){ //avoid shorts and check the nr of electrode is valid
      writeSPI(SSsNr, lowByte(tempNr-1));
      //Serial.println(lowByte(tempNr-1), BIN);
    }
    else{
      Serial.println("Risk of short circuit, reenter the values");
      disconnectAll();
      return;
    }
    command=strtok(NULL, " ");
  }
  if(elecNr<NRELECTRODES){ //if the electrode has been assigned/changed, send the value to the HF2
    outHF2(elecNr);
  }
  strOut="Electrode assigned";
  return;
}

//It disconnects the electrode, reference and counter
void disconnectAll(){
  outHF2(OUTINIT); //set the HF2 electrode number to 0
  for(int ii=0; ii<sizeof(SSs)/2; ii++){
    writeSPI(SSs[ii], 0b01000000); //turn all switches to inactive
  }
  
}


//Read the temperature and humidity from the sensors and returns a string
void readTandHum(){
  //read temperature lmt87dckt, 3.3V supply
  float temp=(2.637-float(analogRead(pinT))/1023.0*3.3)/0.0136;
  String Temp=String(temp, 2);

  //read humidity HIH-5030 device, 3.3V supply
  float sensorRH=157.23*(float(analogRead(pinH))/1023 - 0.1515);
  float hum=sensorRH/(1.0546-0.00216*temp); //temperature compensated
  String Hum=String(hum,1);
  strOut=String("T"+Temp+" H"+Hum);
}


//write the active electrode to the digital input of HF2
void outHF2(int electrodeNr){ 
  byte electrodeHF2= lowByte(electrodeNr);
  //Serial.println(sizeof(pinHF2));
  //Serial.println(lowByte(electrodeNr), BIN);
  
  for(int ii=0; ii<sizeof(pinHF2)/2; ii++){
    //Serial.print(electrodeHF2&MASK, BIN);
    digitalWrite(pinHF2[ii], electrodeHF2&MASK);
    electrodeHF2>>=1;
  }
  
  
  
}

//SPI communication to the switches
void writeSPI(int slavePin, byte command){ 
      SPI.beginTransaction(SPISettings(FREQ, MSBFIRST, SPI_MODE0));
      digitalWrite(slavePin, LOW); //set the sync low
      SPI.transfer(command); //convert the int to a byte
      digitalWrite(slavePin, HIGH);
      SPI.endTransaction();
      delay(1);
}





