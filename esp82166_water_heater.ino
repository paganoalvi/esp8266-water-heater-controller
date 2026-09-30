#include <ESP8266WiFi.h>

#include <NTPClient.h>

#include <WiFiUdp.h>

#include "secrets.h"

const uint8_t RELAY_PIN = 0;

const long UTC_OFFSET_SECONDS = -10800;

char daysOfTheWeek[7][12] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};

// Define NTP Client to get time

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS);

int i; byte aux;

byte hh; byte mm; byte ss;

byte dato[20];

/* Ejemplo ajuste horario.

UTC -5.00 : -5 * 60 * 60 : -18000

UTC +1.00 : 1 * 60 * 60 : 3600

UTC +0.00 : 0 * 60 * 60 : 0

*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////

void checkSerial()

{

  byte fin_rec = 0;

  i = 0;

  if (Serial.available() > 0)

    {do{

      dato[i] = Serial.read();

      if (dato[i] == 61)  //= fin de string.

      { fin_rec = 1; }

      ++i;

    }while (fin_rec == 0);

    Serial.print((i - 1), DEC);  //Cantidad de bytes recibidos.

    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void check_activ(){

  if(hh>06&&hh<07)

    {digitalWrite(RELAY_PIN,HIGH);}

  if(hh>18&&hh<19)

    {digitalWrite(RELAY_PIN,HIGH);}

  if(hh<06&&hh>19)

    {digitalWrite(RELAY_PIN,LOW);}  

  if(hh>07&&hh<18)

    {digitalWrite(RELAY_PIN,LOW);}

  /*

  if(mm>39&&mm<45)

    {digitalWrite(RELAY_PIN,LOW);Serial.println("ON");}

  if(mm<39||mm>45)

    {digitalWrite(RELAY_PIN,HIGH);Serial.println("OFF");}  

  */  

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup(){

  Serial.begin(9600);

  pinMode(RELAY_PIN,OUTPUT);

  digitalWrite(RELAY_PIN, HIGH);

  //delay(2000);

  //digitalWrite(RELAY_PIN,LOW);

  //delay(2000);

  //digitalWrite(RELAY_PIN,HIGH);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while ( WiFi.status() != WL_CONNECTED ) {

    delay ( 500 );

    Serial.print ( "." );}

  timeClient.begin();

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {

  timeClient.update();

  /*

  Serial.print(daysOfTheWeek[timeClient.getDay()]);

  Serial.print(", ");

  Serial.print(timeClient.getHours());

  Serial.print(":");

  Serial.print(timeClient.getMinutes());

  Serial.print(":");

  Serial.println(timeClient.getSeconds());

  */

  hh=timeClient.getHours();

  mm=timeClient.getMinutes();

  Serial.print(hh);Serial.print(":");Serial.println(mm);

  //if(Serial.available()>0)

  //  {aux=Serial.read();

  //  Serial.print("RX: ");Serial.print(aux);}

  check_activ();

  delay(5000);

  Serial.println("VERSION TEST 001");

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////