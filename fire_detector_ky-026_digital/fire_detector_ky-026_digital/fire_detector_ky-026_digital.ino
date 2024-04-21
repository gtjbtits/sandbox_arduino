static const uint8_t D0   = 16;
static const uint8_t D1   = 5;
static const uint8_t D2   = 4;
static const uint8_t D3   = 0;
static const uint8_t D4   = 2;
static const uint8_t D5   = 14;
static const uint8_t D6   = 12;
static const uint8_t D7   = 13;
static const uint8_t D8   = 15;

int led = D1;
int digitalPin = D2; // KY-026 digital pin
int analogPin = A0; // KY-026 analog pin
int digitalVal; // digital readings
int analogVal; //analog readings


void setup()
{
  pinMode(led, OUTPUT);
  pinMode(digitalPin, INPUT);
  //pinMode(analogPin, OUTPUT);
  Serial.begin(9600);
}

void loop()
{
  digitalVal = digitalRead(digitalPin); 
  if(digitalVal == HIGH) // if flame is detected
  {
    // digitalWrite(led, HIGH); // turn on LED
    Serial.println(1);
  }
  else
  {
    // digitalWrite(led, LOW); // turn off LED
    Serial.println(0);
  }

  // // Read the analog
  // analogVal = analogRead(analogPin); 
  // Serial.println(analogVal);

  delay(100);
}