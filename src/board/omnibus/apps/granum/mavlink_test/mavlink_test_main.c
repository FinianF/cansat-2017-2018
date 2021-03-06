#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>

#include <nuttx/config.h>

#include <nuttx/sensors/mpu6000.h>
#include <nuttx/sensors/lsm303c.h>
#include <nuttx/sensors/bmp280.h>
#include <nuttx/sensors/gy_us42.h>
#include <nuttx/wireless/nrf24l01.h>
#include <mavlink/granum/mavlink.h>
#include "../../include/gpsutils/minmea.h"
#include "madgwick/MadgwickAHRS.h"

#define DEBUG (void)//printf

#define SEC2NSEC(SEC) (SEC * 1000000000)
#define PERIOD_NSEC SEC2NSEC(1)

#define DTORAD(DEG) ((DEG) * M_PI / 180.0f)

#define MSG_BUF_SIZE 256

static size_t _msg_carret = 0;
static char _msg_buffer[MSG_BUF_SIZE];

bool parseGPS(int fd, int cycles) {
	while(cycles--) {
		if(!_msg_carret) {
			read(fd, _msg_buffer, 1);
			if(_msg_buffer[0] == '$') _msg_carret++;
		}
		else {
			read(fd, _msg_buffer + (_msg_carret++), 1);

			if (_msg_carret >= MSG_BUF_SIZE)
			{
				// что-то не так
				printf("Buffer overflow error!\n");
				_msg_carret = 0;
				break;
			}

			if('\r' == _msg_buffer[_msg_carret-2]
			   &&	'\n' == _msg_buffer[_msg_carret-1]) {
				_msg_buffer[_msg_carret] = '\x00';
				_msg_carret = 0;
				return true;
			}
		}
	}

	return false;
}

struct fds {
	int mpu, lsm, nrf, file;
};

pthread_addr_t mpu_thread(pthread_addr_t arg) {
	int mpu_fd = ((struct fds*)arg)->mpu;
	int lsm_fd = ((struct fds*)arg)->lsm;
	int nrf_fd = ((struct fds*)arg)->nrf;
	int file_fd = ((struct fds*)arg)->file;

	mpu6000_record_t record_mpu;
	lsm303c_record_t record_lsm;
	mavlink_scaled_imu_t imu_msg;

	mavlink_attitude_quaternion_t quat_msg = {
			.rollspeed = 0, .yawspeed = 0, .pitchspeed = 0
	};

	mavlink_message_t msg;
	uint8_t buffer[1024];

	struct timespec period = {
			.tv_sec = 0, .tv_nsec = SEC2NSEC(0.09)
	};

	float gyro_err_x = 0;
	float gyro_err_y = 0;
	float gyro_err_z = 0;


	for(int i = 0; i < 10; i++){
		read(mpu_fd, &record_mpu, sizeof(mpu6000_record_t) );
		clock_nanosleep(CLOCK_REALTIME, 0, &period, NULL);
		clock_nanosleep(CLOCK_REALTIME, 0, &period, NULL);
		clock_nanosleep(CLOCK_REALTIME, 0, &period, NULL);
	}

	for(int i = 0; i < 10; i++){
		read(mpu_fd, &record_mpu, sizeof(mpu6000_record_t) );
		gyro_err_x += record_mpu.gyro.x;
		gyro_err_y += record_mpu.gyro.y;
		gyro_err_z += record_mpu.gyro.z;
		clock_nanosleep(CLOCK_REALTIME, 0, &period, NULL);
		clock_nanosleep(CLOCK_REALTIME, 0, &period, NULL);
		clock_nanosleep(CLOCK_REALTIME, 0, &period, NULL);
	}

	gyro_err_x /= 10.0f;
	gyro_err_y /= 10.0f;
	gyro_err_z /= 10.0f;

	timer_t timer;
	struct sigevent evt = {
		.sigev_notify = SIGEV_SIGNAL,
		.sigev_signo = SIGALRM,
		.sigev_value = 0,
	};
	volatile int ret = timer_create(CLOCK_REALTIME, &evt, &timer);

	struct itimerspec timerspec = {
		.it_value = period,
		.it_interval = period,
	};
	ret = timer_settime(timer, 0, &timerspec, NULL);

	sigset_t waitset = (sigset_t)0;
	sigaddset(&waitset, SIGALRM);

	ioctl(mpu_fd, MPU6000_CMD_FLUSHFIFO, 0);
	while(true) {
		sigsuspend(&waitset);

		ssize_t isok = read(mpu_fd, &record_mpu, sizeof(record_mpu));
		DEBUG("MPU6000: got %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		if(isok < 0) continue;

		isok = read(lsm_fd, &record_lsm, sizeof(record_lsm));
		DEBUG("LSM303C: got %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		if(isok < 0) continue;

		MadgwickAHRSupdate(DTORAD(record_mpu.gyro.x - gyro_err_x), \
				DTORAD(record_mpu.gyro.x - gyro_err_y), \
				DTORAD(record_mpu.gyro.z - gyro_err_z), \
				record_mpu.acc.x, record_mpu.acc.y, record_mpu.acc.z, \
				record_lsm.field.x, record_lsm.field.y, record_lsm.field.z); //Fixme remap according to real attitude

		beta = 0.066;

		imu_msg.time_boot_ms = record_mpu.time.tv_sec * 1000 + record_mpu.time.tv_nsec / 1000000;
		imu_msg.xacc = (int)(record_mpu.acc.x * 1000.0f); //convert to mG
		imu_msg.yacc = (int)(record_mpu.acc.y * 1000.0f);
		imu_msg.zacc = (int)(record_mpu.acc.z * 1000.0f);
		imu_msg.xgyro = (int)(DTORAD((record_mpu.gyro.x - gyro_err_x)) * 1000.0f); //convert to mDPS
		imu_msg.ygyro = (int)(DTORAD((record_mpu.gyro.y - gyro_err_x)) * 1000.0f);
		imu_msg.zgyro = (int)(DTORAD((record_mpu.gyro.z - gyro_err_x)) * 1000.0f);
		imu_msg.xmag = (int)(record_lsm.field.x / 10.0f); //convert to mT
		imu_msg.ymag = (int)(record_lsm.field.y / 10.0f);
		imu_msg.zmag = (int)(record_lsm.field.z / 10.0f);

		mavlink_msg_scaled_imu_encode(0, MAV_COMP_ID_IMU, &msg, &imu_msg);
		uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);

		isok = write(nrf_fd, buffer, len);
		DEBUG("NRF: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = write(file_fd, buffer, len);
		DEBUG("FILE: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = fsync(file_fd);
		DEBUG("FILE: synced, error %d\n", isok >=0 ? 0 : -get_errno());

		quat_msg.q1 = q0;
		quat_msg.q2 = q1;
		quat_msg.q3 = q2;
		quat_msg.q4 = q3;

		mavlink_msg_attitude_quaternion_encode(0, MAV_COMP_ID_IMU, &msg, &quat_msg);
		len = mavlink_msg_to_send_buffer(buffer, &msg);

		isok = write(nrf_fd, buffer, len);
		DEBUG("NRF: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = write(file_fd, buffer, len);
		DEBUG("FILE: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = fsync(file_fd);
		DEBUG("FILE: synced, error %d\n", isok >=0 ? 0 : -get_errno());
		DEBUG("_________________________________________________________________\n");
	}

	return NULL;
}

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int mavlink_test_main(int argc, char *argv[])
#endif
{
	printf("This is MAVLink test app!\n");
	sleep(3);
	printf("End of sleep\n");
	int mpu_fd = open("/dev/mpu0", O_RDONLY | O_NONBLOCK);
	if (mpu_fd < 0)
	{
		perror("Can't open /dev/mpu0");
		return 1;
	}

	int nrf_fd = open("/dev/nrf24l01", O_RDWR | O_NONBLOCK);
	if (nrf_fd < 0)
	{
	  perror("Can't open /dev/nrf24l01");
	  return 1;
	}

	char filename[16];
	int file_fd = -1;
	struct stat stats;

	for(int i = 0; i < 100; i++) {
		snprintf(filename, 22, "/mnt/sd0/GRANUM%d.bin", i);
		int ret = stat(filename, &stats);

		if(ret == -1 && get_errno() == ENOENT) {
			file_fd = open(filename, O_WRONLY | O_CREAT);
			break;
		}

		if(i == 99) {
			perror("There are 99 telemetry files already");
			return 1;
		}
	}

	if (file_fd < 0)
	{
	  perror("Can't open telemetry file");
	  perror(filename);
	  return 1;
	}

	int baro_fd = open("/dev/baro0", O_RDWR | O_NONBLOCK);
	if (baro_fd < 0)
	{
	  perror("Can't open /dev/baro0");
	  return 1;
	}

	int gps_fd = open("/dev/ttyS1", O_RDONLY);

	if (gps_fd < 0)
	{
		perror("Can't open /dev/ttyS1");
		return 1;
	}

	/*int sonarfd = open("/dev/sonar0", O_RDONLY);
	if (sonarfd < 0)
	{
	  perror("cant open sonar device");
	  return 1;
	}*/

//Настройки MPU6000
	ioctl(mpu_fd, MPU6000_CMD_SET_CONVERT, true);
	ioctl(mpu_fd, MPU6000_CMD_SET_FILTER, 6);
	ioctl(mpu_fd, MPU6000_CMD_SET_SAMPLERATE_DIVIDER, 9);


//Настройки UART4 для GPS
	struct termios termios;

	ioctl(gps_fd, TCGETS, (unsigned long) &termios);

	termios.c_cflag = 	(CS8 	 	//8 bits
						| CREAD) 	//Enable read
						& ~CSTOPB	//1 stop bit
						& ~PARENB	//Disable parity check
						& ~CRTSCTS; //No hardware flow control

	ioctl(gps_fd, TCSETS, (unsigned long) &termios);


//Настройки NRF24L01+
	uint32_t tmp = 2489;
	ioctl(nrf_fd, WLIOC_SETRADIOFREQ, (long unsigned int)&tmp);

	tmp = 5;
	ioctl(nrf_fd, NRF24L01IOC_SETADDRWIDTH, (long unsigned int)&tmp);

	uint8_t addr[] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB};
	ioctl(nrf_fd, NRF24L01IOC_SETTXADDR, (long unsigned int)addr);

	tmp = 0;
	ioctl(nrf_fd, WLIOC_SETTXPOWER, (long unsigned int)&tmp);

	nrf24l01_retrcfg_t retrcfg = {.delay = DELAY_250us, .count = 4};
	ioctl(nrf_fd, NRF24L01IOC_SETRETRCFG, (long unsigned int)&retrcfg);

	nrf24l01_pipecfg_t pipe0 = {.en_aa = true,\
							  .payload_length = NRF24L01_DYN_LENGTH};
	pipe0.rx_addr[0] = 0xAA;
	pipe0.rx_addr[1] = 0xAA;
	pipe0.rx_addr[2] = 0xAA;
	pipe0.rx_addr[3] = 0xAA;
	pipe0.rx_addr[4] = 0xAA;

	nrf24l01_pipecfg_t pipeDummy = {.en_aa = true,\
							  .payload_length = NRF24L01_DYN_LENGTH};
	pipeDummy.rx_addr[0] = 0xAA;
	pipeDummy.rx_addr[1] = 0xAA;
	pipeDummy.rx_addr[2] = 0xAA;
	pipeDummy.rx_addr[3] = 0xAA;
	pipeDummy.rx_addr[4] = 0xAA;

	nrf24l01_pipecfg_t * pipes[] = {&pipe0, &pipeDummy, &pipeDummy, &pipeDummy, &pipeDummy, &pipeDummy};

	ioctl(nrf_fd, NRF24L01IOC_SETPIPESCFG, (long unsigned int)pipes);

	uint8_t pipecfg = 0x01;
	ioctl(nrf_fd, NRF24L01IOC_SETPIPESENABLED, (long unsigned int)&pipecfg);

	nrf24l01_datarate_t datarate = RATE_1Mbps;
	ioctl(nrf_fd, NRF24L01IOC_SETDATARATE, (long unsigned int)&datarate);

	nrf24l01_crcmode_t crcmode = CRC_2BYTE;
	ioctl(nrf_fd, NRF24L01IOC_SETCRCMODE, (long unsigned int)&crcmode);

	nrf24l01_state_t state = ST_STANDBY;
	ioctl(nrf_fd, NRF24L01IOC_SETSTATE, (long unsigned int)&state);



	mavlink_scaled_pressure_t baro_msg;
	bmp280_data_t result = {0, 0};

	mavlink_sonar_t sonar_msg;

	mavlink_hil_gps_t gps_msg;
	gps_msg.cog = 0;
	gps_msg.eph = 0;
	gps_msg.epv = 0;
	gps_msg.vd = 0;
	gps_msg.ve = 0;
	gps_msg.vel = 0;
	gps_msg.vn = 0;
	gps_msg.satellites_visible = 0xff;
	struct timespec gps_time;
	struct minmea_date gps_basedate = {
			.year = 18,
			.month = 5,
			.day = 1,
	};

	mavlink_message_t msg;
	uint8_t buffer[1024];

	pthread_t mpu_thread_id;
	struct fds fds = {.mpu = mpu_fd, .nrf = nrf_fd, .file = file_fd};
	pthread_create(&mpu_thread_id, 0, mpu_thread, &fds);

	while(1)
	{
//BMP280
		ssize_t isok = read(baro_fd, &result, sizeof(result) );
		DEBUG("BMP280: got %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());

		struct timespec current_time;
		clock_gettime(CLOCK_MONOTONIC, &current_time);
		baro_msg.time_boot_ms = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
		baro_msg.press_abs = result.pressure / 100.0;				//pressure in hPa
		baro_msg.temperature = result.temperature * 100.0;		//temperature in 0.01 degC

		mavlink_msg_scaled_pressure_encode(0, MAV_COMP_ID_PERIPHERAL, &msg, &baro_msg);

		uint16_t len = mavlink_msg_to_send_buffer(buffer, &msg);

		isok = write(nrf_fd, buffer, len);
		DEBUG("NRF: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = write(file_fd, buffer, len);
		DEBUG("FILE: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = fsync(file_fd);
		DEBUG("FILE: synced, error %d\n", isok >=0 ? 0 : -get_errno());
		DEBUG("_________________________________________________________________\n");

//SONAR
		/*read(sonarfd, &sonar_msg.distance, 2);
		clock_gettime(CLOCK_MONOTONIC, &current_time);
		ioctl(sonarfd, GY_US42_IOCTL_CMD_MEASURE, (unsigned int)NULL);
		sonar_msg.time_boot_ms = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
		mavlink_msg_sonar_encode(0, MAV_COMP_ID_PERIPHERAL, &msg, &sonar_msg);

		len = mavlink_msg_to_send_buffer(buffer, &msg);

		isok = write(nrf_fd, buffer, len);
		DEBUG("NRF: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = write(file_fd, buffer, len);
		DEBUG("FILE: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
		isok = fsync(file_fd);
		DEBUG("FILE: synced, error %d\n", isok >=0 ? 0 : -get_errno());
		DEBUG("_________________________________________________________________\n");*/

//GPS
		if( parseGPS(gps_fd, 100) ) {
			// накопили, теперь разбираем
			if (!minmea_check(_msg_buffer, false))
				continue;

			struct minmea_sentence_gga frame;
			if (!minmea_parse_gga(&frame, _msg_buffer))
				continue; // опс, что-то пошло не так

			if (frame.fix_quality == 0)
				continue;

			minmea_gettime(&gps_time, &gps_basedate, &frame.time);

			gps_msg.time_usec = gps_time.tv_sec * 1000000.0 + gps_time.tv_nsec / 1000.0;
			gps_msg.lon = (int32_t)(minmea_tocoord(&frame.longitude) * 10000000);
			gps_msg.lat = (int32_t)(minmea_tocoord(&frame.latitude) * 10000000);
			gps_msg.alt = (int32_t)(minmea_tofloat(&frame.altitude) * 1000);
			gps_msg.fix_type = frame.fix_quality;

			mavlink_msg_hil_gps_encode(0, MAV_COMP_ID_PERIPHERAL, &msg, &gps_msg);
			len = mavlink_msg_to_send_buffer(buffer, &msg);

			isok = write(nrf_fd, buffer, len);
			DEBUG("NRF: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
			isok = write(file_fd, buffer, len);
			DEBUG("FILE: wrote %d bytes, error %d\n", isok >= 0 ? isok : 0, isok >=0 ? 0 : -get_errno());
			isok = fsync(file_fd);
			DEBUG("FILE: synced, error %d\n", isok >=0 ? 0 : -get_errno());
			DEBUG("_________________________________________________________________\n");
		}

		usleep(100000);
	}

 	return 0;
}
