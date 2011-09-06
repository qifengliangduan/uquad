#ifndef IMU_COMM_H
#define IMU_COMM_H

#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>

#define ERROR_OK 0
#define ERROR_FAIL -1
#define ERROR_READ_TIMEOUT -2
#define ERROR_READ_SYNC -3

#define IMU_FRAME_INIT_CHAR 'A'
#define IMU_FRAME_END_CHAR 'Z'
#define IMU_FRAME_SIZE_BYTES_DEFAULT 16 // 4 bytes init/end chars, 2 bytes per sensor reading
#define IMU_FRAME_SAMPLE_AVG_COUNT 16
#define IMU_SENSOR_COUNT 6
#define IMU_GRAVITY 9.81

struct timeval detail_time;
gettimeofday(&detail_time,NULL);
printf("%d %d",
detail_time.tv_usec /1000,  /* milliseconds */
detail_time.tv_usec); /* microseconds */

struct imu_frame{
  unsigned char frame[IMU_FRAME_SIZE_BYTES_DEFAULT];
};

struct imu_null_estimates{
  unsigned int xyzrpy[IMU_SENSOR_COUNT];
  struct timeval timestamp;
}

struct imu_settings{
  // sampling frequency
  unsigned int fs;
  // sampling period
  double T;
  // sens index
  unsigned int gyro_sens;
  // 
  unsigned int frame_width_bytes;
};
    
struct imu{
  struct imu_settings settings;
  struct imu_null_estimates null_estimates;
  struct imu_frame frame_buffer[IMU_FRAME_SAMPLE_AVG_COUNT];
  struct timeval frame_avg_init,frame_avg_end;
  int frames_sampled = 0;
  FILE * device;
} imu;

int imu_comm_connect(struct imu * imu, const char * device);

struct imu * imu_comm_init(void);

int imu_comm_close(struct imu * imu);

int imu_comm_get_data(struct imu * imu);

#endif // IMU_COMM_H
