#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#define st true

//Declaring Gyro variables
int gyro_x, gyro_y, gyro_z;
long gyro_x_cal, gyro_y_cal, gyro_z_cal;
boolean set_gyro_angles;

long acc_x, acc_y, acc_z, acc_total_vector;
float angle_roll_acc, angle_pitch_acc;

float angle_pitch, angle_roll;
int angle_pitch_buffer, angle_roll_buffer;
float angle_pitch_output, angle_roll_output;
int temp, toff;
double t, tx;

SoftwareSerial esp(10, 11);

//Declaring Additional variables
String x, content, post, com;
String mac = "";
char mac1[18];
int temp35 = 30;
unsigned long currentMillis;
unsigned long previousMillis = 0;
unsigned long interval = 10000;

//Declaring functions
String sendData(String command, const int timeout, boolean debug);
void setup_mpu_6050_registers();
void read_mpu_6050_data();

void setup()
{
  Serial.begin(9600);
  esp.begin(9600);
  sendData("AT+RST\r\n", 10000, st);
  sendData("AT+CWQAP\r\n", 2000, st);
  sendData("AT+CWMODE_CUR=1\r\n", 2000, st);
  sendData("AT+CWJAP=\"CWC-3905447\",\"qfwc8XsNnzb5\"\r\n", 20000, st);
  mac = sendData("AT+CIPSTAMAC?\r\n", 5000, st);
  int x = mac.length() + 1;
  char mACC[x];
  mac.toCharArray(mACC, x);
  memset(mac1, 0, 18);
  memcpy(mac1, (strstr(mACC, ":\"") + 2), (sizeof(char) * 17));
  Serial.println("E");

  Wire.begin();               //Start I2C as master
  setup_mpu_6050_registers(); //Setup the registers of the MPU-6050
  for (int cal_int = 0; cal_int < 1000; cal_int++)
  { //Read the raw acc and gyro data from the MPU-6050 for 1000 times
    read_mpu_6050_data();
    gyro_x_cal += gyro_x; //Add the gyro x offset to the gyro_x_cal variable
    gyro_y_cal += gyro_y; //Add the gyro y offset to the gyro_y_cal variable
    gyro_z_cal += gyro_z; //Add the gyro z offset to the gyro_z_cal variable
    delay(3);             //Delay 3us to have 250Hz for-loop
  }

  // divide by 1000 to get avarage offset
  gyro_x_cal /= 1000;
  gyro_y_cal /= 1000;
  gyro_z_cal /= 1000;
  toff = -1600;
}

void loop()
{
  currentMillis = millis();

  read_mpu_6050_data();
  //Subtract the offset values from the raw gyro values
  gyro_x -= gyro_x_cal;
  gyro_y -= gyro_y_cal;
  gyro_z -= gyro_z_cal;

  //Gyro angle calculations . Note 0.0000611 = 1 / (250Hz x 65.5)
  angle_pitch += gyro_x * 0.0000611; //Calculate the traveled pitch angle and add this to the angle_pitch variable
  angle_roll += gyro_y * 0.0000611;  //Calculate the traveled roll angle and add this to the angle_roll variable
  //0.000001066 = 0.0000611 * (3.142(PI) / 180degr) The Arduino sin function is in radians
  angle_pitch += angle_roll * sin(gyro_z * 0.000001066); //If the IMU has yawed transfer the roll angle to the pitch angel
  angle_roll -= angle_pitch * sin(gyro_z * 0.000001066); //If the IMU has yawed transfer the pitch angle to the roll angel

  //Accelerometer angle calculations
  acc_total_vector = sqrt((acc_x * acc_x) + (acc_y * acc_y) + (acc_z * acc_z)); //Calculate the total accelerometer vector
  //57.296 = 1 / (3.142 / 180) The Arduino asin function is in radians
  angle_pitch_acc = asin((float)acc_y / acc_total_vector) * 57.296; //Calculate the pitch angle
  angle_roll_acc = asin((float)acc_x / acc_total_vector) * -57.296; //Calculate the roll angle

  angle_pitch_acc -= 0.0; //Accelerometer calibration value for pitch
  angle_roll_acc -= 0.0;  //Accelerometer calibration value for roll

  if (set_gyro_angles)
  {                                                                //If the IMU is already started
    angle_pitch = angle_pitch * 0.9996 + angle_pitch_acc * 0.0004; //Correct the drift of the gyro pitch angle with the accelerometer pitch angle
    angle_roll = angle_roll * 0.9996 + angle_roll_acc * 0.0004;    //Correct the drift of the gyro roll angle with the accelerometer roll angle
  }
  else
  {                                //At first start
    angle_pitch = angle_pitch_acc; //Set the gyro pitch angle equal to the accelerometer pitch angle
    angle_roll = angle_roll_acc;   //Set the gyro roll angle equal to the accelerometer roll angle
    set_gyro_angles = true;        //Set the IMU started flag
  }

  //To dampen the pitch and roll angles a complementary filter is used
  angle_pitch_output = angle_pitch_output * 0.9 + angle_pitch * 0.1; //Take 90% of the output pitch value and add 10% of the raw pitch value
  angle_roll_output = angle_roll_output * 0.9 + angle_roll * 0.1;    //Take 90% of the output roll value and add 10% of the raw roll value
  // Serial.print(" | Angle  = ");
  // Serial.println(angle_pitch_output);

  if (currentMillis - previousMillis >= interval)
  {
    sendData("AT+CIPSTART=\"TCP\",\"192.168.0.6\",5000\r\n", 3000, st);

    content = "";
    content = "{	\"patient_id\": \"";
    content += String(mac1);
    content += String("\", \"position\": ");
    content += String(angle_pitch_output);
    content += String("\", \"temperature\": ");
    content += String(t);
    content += String(" }");
    post = String("");
    post = String("POST /api/record HTTP/1.1\r\nHost: 192.168.0.6\r\nContent-Type: application/json\r\nContent-Length: ");
    post += String(content.length());
    post += String("\r\n\r\n");
    post += content;
    post += String("\r\n\r\n");
    com = String("");
    com = String("AT+CIPSEND=");
    com += String(post.length());
    //com += "\r\n";
    sendData(com, 2000, st);
    delay(100);
    //Serial.println("sigh");
    sendData(post, 10000, st);
    previousMillis = currentMillis;
  }
  delay(2000);
}

void setup_mpu_6050_registers()
{
  //Activate the MPU-6050
  Wire.beginTransmission(0x68); //Start communicating with the MPU-6050
  Wire.write(0x6B);             //Send the requested starting register
  Wire.write(0x00);             //Set the requested starting register
  Wire.endTransmission();
  //Configure the accelerometer (+/-8g)
  Wire.beginTransmission(0x68); //Start communicating with the MPU-6050
  Wire.write(0x1C);             //Send the requested starting register
  Wire.write(0x10);             //Set the requested starting register
  Wire.endTransmission();
  //Configure the gyro (500dps full scale)
  Wire.beginTransmission(0x68); //Start communicating with the MPU-6050
  Wire.write(0x1B);             //Send the requested starting register
  Wire.write(0x08);             //Set the requested starting register
  Wire.endTransmission();
}

void read_mpu_6050_data()
{                               //Subroutine for reading the raw gyro and accelerometer data
  Wire.beginTransmission(0x68); //Start communicating with the MPU-6050
  Wire.write(0x3B);             //Send the requested starting register
  Wire.endTransmission();       //End the transmission
  Wire.requestFrom(0x68, 14);   //Request 14 bytes from the MPU-6050
  while (Wire.available() < 14)
    ; //Wait until all the bytes are received
  acc_x = Wire.read() << 8 | Wire.read();
  acc_y = Wire.read() << 8 | Wire.read();
  acc_z = Wire.read() << 8 | Wire.read();
  temp = (Wire.read() << 8 | Wire.read()) + toff;
  gyro_x = Wire.read() << 8 | Wire.read();
  gyro_y = Wire.read() << 8 | Wire.read();
  gyro_z = Wire.read() << 8 | Wire.read();
  tx = temp;
  t = tx / 340 + 36.53;
}

String sendData(String command, const int timeout, boolean debug)
{
  String response = "";

  esp.print(command); // send the read character to the esp8266

  unsigned long int time = millis();

  while ((time + timeout) > millis())
  {
    while (esp.available())
    {

      // The esp has data so display its output to the serial window
      char c = esp.read(); // read the next character.
      response += c;
    }
  }

  if (debug)
  {
    Serial.println(response);
  }

  return response;
}
