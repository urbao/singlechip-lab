#include "mpu9250.h"  //mpu9250函式庫
#include "math.h"     //數學運算函式庫
// SCL 22
// SDA 21
/* Mpu9250 object */
bfs::Mpu9250 imu;


void setup() {
  //開啟序列埠，鮑率115200
  Serial.begin(115200);

  //開啟I2C
  Wire.begin();
  Wire.setClock(400000);  //設定時脈
  //I2C匯流排，位址0x68
  imu.Config(&Wire, bfs::Mpu9250::I2C_ADDR_PRIM);
  //初始化mpu9250
  if (!imu.Begin()) {
    Serial.println("Error initializing communication with IMU");
    while (1) {}
  }
}


// 變量定義

float Yaw, Pitch, Roll;  //偏航角，俯仰角，翻滾角
#define Kp 100.0f        // 比例增益支配率收斂到加速度計/磁強計
#define Ki 0.002f        // 積分增益支配率的陀螺儀偏見的銜接
#define halfT 0.001f     // 采樣周期的一半
// 四元數的元素，代表估計方向
float q[4] = { 1.0f, 0.0f, 0.0f, 0.0f };  //四元數初始宣告
float eInt[3] = { 0.0f, 0.0f, 0.0f };
float exInt = 0, eyInt = 0, ezInt = 0;
float deltat = 0.01f, sum = 0.0f;
void MahonyQuaternionUpdate(float ax, float ay, float az, float gx, float gy, float gz, float mx, float my, float mz) {


  float q1 = q[0], q2 = q[1], q3 = q[2], q4 = q[3];  // 好讀取的局部變量
  float norm;
  float hx, hy, bx, bz;
  float vx, vy, vz, wx, wy, wz;
  float ex, ey, ez;
  float pa, pb, pc;

  // 避免重複運算的輔助變數
  float q1q1 = q1 * q1;
  float q1q2 = q1 * q2;
  float q1q3 = q1 * q3;
  float q1q4 = q1 * q4;
  float q2q2 = q2 * q2;
  float q2q3 = q2 * q3;
  float q2q4 = q2 * q4;
  float q3q3 = q3 * q3;
  float q3q4 = q3 * q4;
  float q4q4 = q4 * q4;

  // 標準化加速度計資料
  norm = sqrtf(ax * ax + ay * ay + az * az);

  ax /= norm;
  ay /= norm;
  az /= norm;

  // 標準化陀螺儀資料
  norm = sqrtf(mx * mx + my * my + mz * mz);

  mx /= norm;
  my /= norm;
  mz /= norm;

  // 地球磁場的參考方向
  hx = 2.0f * mx * (0.5f - q3q3 - q4q4) + 2.0f * my * (q2q3 - q1q4) + 2.0f * mz * (q2q4 + q1q3);
  hy = 2.0f * mx * (q2q3 + q1q4) + 2.0f * my * (0.5f - q2q2 - q4q4) + 2.0f * mz * (q3q4 - q1q2);
  bx = sqrtf((hx * hx) + (hy * hy));
  bz = 2.0f * mx * (q2q4 - q1q3) + 2.0f * my * (q3q4 + q1q2) + 2.0f * mz * (0.5f - q2q2 - q3q3);

  // 評估地球重力與磁場方向
  vx = 2.0f * (q2q4 - q1q3);
  vy = 2.0f * (q1q2 + q3q4);
  vz = q1q1 - q2q2 - q3q3 + q4q4;
  wx = 2.0f * bx * (0.5f - q3q3 - q4q4) + 2.0f * bz * (q2q4 - q1q3);
  wy = 2.0f * bx * (q2q3 - q1q4) + 2.0f * bz * (q1q2 + q3q4);
  wz = 2.0f * bx * (q1q3 + q2q4) + 2.0f * bz * (0.5f - q2q2 - q3q3);

  //誤差是被評估的方向與量測到的重力方向的外積
  ex = (ay * vz - az * vy) + (my * wz - mz * wy);
  ey = (az * vx - ax * vz) + (mz * wx - mx * wz);
  ez = (ax * vy - ay * vx) + (mx * wy - my * wx);
  if (Ki > 0.0f) {
    eInt[0] += ex;  // 累加積分誤差
    eInt[1] += ey;
    eInt[2] += ez;
  } else {
    eInt[0] = 0.0f;  // 防止積分結束
    eInt[1] = 0.0f;
    eInt[2] = 0.0f;
  }

  // 加入回饋
  gx = gx + Kp * ex + Ki * eInt[0];
  gy = gy + Kp * ey + Ki * eInt[1];
  gz = gz + Kp * ez + Ki * eInt[2];

  // 四元數變化的整合速度
  pa = q2;
  pb = q3;
  pc = q4;
  q1 = q1 + (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * deltat);
  q2 = pa + (q1 * gx + pb * gz - pc * gy) * (0.5f * deltat);
  q3 = pb + (q1 * gy - pa * gz + pc * gx) * (0.5f * deltat);
  q4 = pc + (q1 * gz + pa * gy - pb * gx) * (0.5f * deltat);

  // 標準化四元數
  norm = sqrtf(q1 * q1 + q2 * q2 + q3 * q3 + q4 * q4);
  norm = 1.0f / norm;
  q[0] = q1 * norm;
  q[1] = q2 * norm;
  q[2] = q3 * norm;
  q[3] = q4 * norm;

  Yaw = atan2(2.0f * (q[1] * q[2] + q[0] * q[3]), 1 - 2 * q[2] * q[2] - 2 * q[3] * q[3]);
  Pitch = -asin(2.0f * (q[1] * q[3] - q[0] * q[2]));
  Roll = atan2(2.0f * (q[0] * q[1] + q[2] * q[3]), 1 - 2 * q[2] * q[2] - 2 * q[1] * q[1]);
  Pitch *= 180.0f / PI;
  Yaw *= 180.0f / PI;
  Roll *= 180.0f / PI;
}

void loop() {
  //mpu9250
  float gx, gy, gz, ax, ay, az, mx, my, mz;
  if (imu.Read()) {
    if (imu.new_mag_data()) {

      //獲得九軸資訊
      ax = imu.accel_x_mps2();
      ay = imu.accel_y_mps2();
      az = imu.accel_z_mps2();
      gx = imu.gyro_x_radps();
      gy = imu.gyro_y_radps();
      gz = imu.gyro_z_radps();
      mx = imu.mag_x_ut();
      my = imu.mag_y_ut();
      mz = imu.mag_z_ut();
      //由橢球擬和計算出的偏移量，將圓心移回原點

      //do something//
      mx = mx - 69.8071f;
      my = my - 37.1124f;
      mz = mz + 66.4265f;


      //Mahony Quterion算法做資料融合，得出四元數
      MahonyQuaternionUpdate(ax, ay, az, gx * PI / 180.0f, gy * PI / 180.0f, gz * PI / 180.0f, mx, my, mz);
      //由四元數求出歐拉角

      Serial.print("pitch: ");
      Serial.print(Pitch);
      Serial.print("  roll: ");
      Serial.print(Roll);
      Serial.print("  yaw: ");
      Serial.println(Yaw);
    }
  }
}