#include <M5StickCPlus.h>
/**
 *  The MLX90640 requires some hefty calculations and larger arrays. You will need a microcontroller with
 *  20,000 bytes or more of RAM.
 *  This relies on the driver written by Melexis and can be found at:
 *  https://github.com/melexis/mlx90640-library
 */
#include <Wire.h>

#include "Arduino.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

TFT_eSprite img = TFT_eSprite(&M5.Lcd);
TFT_eSprite msg = TFT_eSprite(&M5.Lcd);

#include "Battery.h"
Battery battery = Battery();

#define TA_SHIFT 8	// Default shift for MLX90640 in open air
#define COLS 32
#define ROWS 24
#define DIAMETER 5	// サーモ画像の拡大倍率
#define COLS_4 (COLS * DIAMETER)
#define ROWS_4 (ROWS * DIAMETER)
#define pixelsArraySize (COLS * ROWS)
#define INTERPOLATED_COLS 32
#define INTERPOLATED_ROWS 32

float pixels[COLS * ROWS];
float reversePixels[COLS * ROWS];
float pixels_4[COLS_4 * ROWS_4];

#define get_pixels(x, y) (pixels[y * COLS + x])
#define get_pixels_5(x, y) (pixels_4[(y)*DIAMETER * COLS_4 + x])

byte speed_setting = 2;	 // High is 1 , Low is 2
bool reverseScreen = false;
const byte MLX90640_address = 0x33;	 // Default 7-bit unshifted address of the MLX90640

paramsMLX90640 mlx90640;

// low range of the sensor (this will be blue on the screen)
float mintemp = 24;		// For color mapping
float min_v = 24;		// Value of current min temp
float min_cam_v = -40;	// Spec in datasheet

// high range of the sensor (this will be red on the screen)
float maxtemp = 35;		// For color mapping
float max_v = 35;		// Value of current max temp
float max_cam_v = 300;	// Spec in datasheet

// the colors we will be using
const uint16_t camColors[] = {
	0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810, 0x3810, 0x3010, 0x3010, 0x3010, 0x2810, 0x2810,
	0x2810, 0x2810, 0x2010, 0x2010, 0x2010, 0x1810, 0x1810, 0x1811, 0x1811, 0x1011, 0x1011, 0x1011, 0x0811, 0x0811,
	0x0811, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0031, 0x0031, 0x0051, 0x0072, 0x0072, 0x0092, 0x00B2, 0x00B2,
	0x00D2, 0x00F2, 0x00F2, 0x0112, 0x0132, 0x0152, 0x0152, 0x0172, 0x0192, 0x0192, 0x01B2, 0x01D2, 0x01F3, 0x01F3,
	0x0213, 0x0233, 0x0253, 0x0253, 0x0273, 0x0293, 0x02B3, 0x02D3, 0x02D3, 0x02F3, 0x0313, 0x0333, 0x0333, 0x0353,
	0x0373, 0x0394, 0x03B4, 0x03D4, 0x03D4, 0x03F4, 0x0414, 0x0434, 0x0454, 0x0474, 0x0474, 0x0494, 0x04B4, 0x04D4,
	0x04F4, 0x0514, 0x0534, 0x0534, 0x0554, 0x0554, 0x0574, 0x0574, 0x0573, 0x0573, 0x0573, 0x0572, 0x0572, 0x0572,
	0x0571, 0x0591, 0x0591, 0x0590, 0x0590, 0x058F, 0x058F, 0x058F, 0x058E, 0x05AE, 0x05AE, 0x05AD, 0x05AD, 0x05AD,
	0x05AC, 0x05AC, 0x05AB, 0x05CB, 0x05CB, 0x05CA, 0x05CA, 0x05CA, 0x05C9, 0x05C9, 0x05C8, 0x05E8, 0x05E8, 0x05E7,
	0x05E7, 0x05E6, 0x05E6, 0x05E6, 0x05E5, 0x05E5, 0x0604, 0x0604, 0x0604, 0x0603, 0x0603, 0x0602, 0x0602, 0x0601,
	0x0621, 0x0621, 0x0620, 0x0620, 0x0620, 0x0620, 0x0E20, 0x0E20, 0x0E40, 0x1640, 0x1640, 0x1E40, 0x1E40, 0x2640,
	0x2640, 0x2E40, 0x2E60, 0x3660, 0x3660, 0x3E60, 0x3E60, 0x3E60, 0x4660, 0x4660, 0x4E60, 0x4E80, 0x5680, 0x5680,
	0x5E80, 0x5E80, 0x6680, 0x6680, 0x6E80, 0x6EA0, 0x76A0, 0x76A0, 0x7EA0, 0x7EA0, 0x86A0, 0x86A0, 0x8EA0, 0x8EC0,
	0x96C0, 0x96C0, 0x9EC0, 0x9EC0, 0xA6C0, 0xAEC0, 0xAEC0, 0xB6E0, 0xB6E0, 0xBEE0, 0xBEE0, 0xC6E0, 0xC6E0, 0xCEE0,
	0xCEE0, 0xD6E0, 0xD700, 0xDF00, 0xDEE0, 0xDEC0, 0xDEA0, 0xDE80, 0xDE80, 0xE660, 0xE640, 0xE620, 0xE600, 0xE5E0,
	0xE5C0, 0xE5A0, 0xE580, 0xE560, 0xE540, 0xE520, 0xE500, 0xE4E0, 0xE4C0, 0xE4A0, 0xE480, 0xE460, 0xEC40, 0xEC20,
	0xEC00, 0xEBE0, 0xEBC0, 0xEBA0, 0xEB80, 0xEB60, 0xEB40, 0xEB20, 0xEB00, 0xEAE0, 0xEAC0, 0xEAA0, 0xEA80, 0xEA60,
	0xEA40, 0xF220, 0xF200, 0xF1E0, 0xF1C0, 0xF1A0, 0xF180, 0xF160, 0xF140, 0xF100, 0xF0E0, 0xF0C0, 0xF0A0, 0xF080,
	0xF060, 0xF040, 0xF020, 0xF800,
};

float get_point(float *p, uint8_t rows, uint8_t cols, uint8_t x, uint8_t y);

long loopTime, startTime, endTime, fps, prev_loopTime;
long update_interval = 2000;
bool view_temp_autoAdjust_interval_call_flg = true;

// cover 1 --> 4, 32 * 24 --> 128 * 96
// cover 1 --> 5, 32 * 24 -*5-> 160 * 120
void cover5() {
	uint8_t x, y;
	uint16_t pos = 0;
	float x_step = 0.0;
	float y_step = 0.0;
	float pixel_value = 0.0;
	float max_step = 0;

	// X軸の拡大
	for (y = 0; y < ROWS; y++) {
		for (x = 0; x < COLS; x++) {
			pixel_value = get_pixels(x, y);
			x_step = (get_pixels(x + 1, y) - pixel_value) / (float)DIAMETER;
			pos = DIAMETER * x + COLS_4 * (y * DIAMETER);
			pixels_4[pos] = pixel_value + x_step;
			pixels_4[pos + 1] = pixels_4[pos] + x_step;
			pixels_4[pos + 2] = pixels_4[pos + 1] + x_step;
			pixels_4[pos + 3] = pixels_4[pos + 2] + x_step;
			pixels_4[pos + 4] = pixels_4[pos + 3] + x_step;
		}
	}

	// y軸の拡大
	for (y = 0; y < ROWS; y++) {
		for (x = 0; x < COLS_4; x++) {
			pixel_value = get_pixels_5(x, y);
			y_step = (get_pixels_5(x, y + 1) - pixel_value) / (float)DIAMETER;
			pixels_4[(DIAMETER * y + 1) * COLS_4 + x] = pixel_value + y_step;
			pixels_4[(DIAMETER * y + 2) * COLS_4 + x] = pixel_value + 2 * y_step;
			pixels_4[(DIAMETER * y + 3) * COLS_4 + x] = pixel_value + 3 * y_step;
			pixels_4[(DIAMETER * y + 4) * COLS_4 + x] = pixel_value + 4 * y_step;
			// pixels_4[(DIAMETER * y + 5) * COLS_4 + x] = pixel_value + 5 * y_step;
		}
	}
}

void drawpixels(float *p, uint8_t rows, uint8_t cols) {
	int colorTemp;
	Serial.printf("%f, %f\r\n", mintemp, maxtemp);

	for (uint8_t y = 0; y < rows; y++) {
		for (uint8_t x = 0; x < cols; x++) {
			float val = get_point(p, rows, cols, x, y);

			if (val >= maxtemp) {
				colorTemp = maxtemp;
			} else if (val <= mintemp) {
				colorTemp = mintemp;
			} else {
				colorTemp = val;
			}

			uint8_t colorIndex = map(colorTemp, mintemp, maxtemp, 0, 255);
			colorIndex = constrain(colorIndex, 0, 255);	 // 0 ~ 255
			// draw the pixels!
			img.drawPixel(x, y, camColors[colorIndex]);
		}
	}

	img.drawCircle(COLS_4 / 2, ROWS_4 / 2, 5, TFT_WHITE);							  // update center spot icon
	img.drawLine(COLS_4 / 2 - 8, ROWS_4 / 2, COLS_4 / 2 + 8, ROWS_4 / 2, TFT_WHITE);  // vertical line
	img.drawLine(COLS_4 / 2, ROWS_4 / 2 - 8, COLS_4 / 2, ROWS_4 / 2 + 8, TFT_WHITE);  // horizontal line
	img.setCursor(COLS_4 / 2 + 6, ROWS_4 / 2 - 12);
	img.setTextColor(TFT_WHITE);
	img.printf("%.1fC", get_point(p, rows, cols, cols / 2, rows / 2));
	img.pushSprite(0, 0);

	msg.fillScreen(TFT_BLACK);
	msg.setCursor(11, 3);
	msg.setTextColor(TFT_WHITE);
	msg.printf("Bat:%3d %%", battery.calcBatteryPercent());
	msg.setTextColor(TFT_YELLOW);
	msg.setCursor(11, 3 + 12 * 1);
	msg.print("min tmp");
	msg.setCursor(13, 3 + 12 * 2);
	msg.printf("%.1fC", min_v);
	msg.setCursor(11, 3 + 12 * 3);
	msg.print("max tmp");
	msg.setCursor(13, 3 + 12 * 4);
	msg.printf("%.1fC", max_v);
	msg.setCursor(11, 3 + 12 * 5);
	msg.printf("tmp mode: %d", view_temp_autoAdjust_interval_call_flg);

	msg.pushSprite(COLS_4, 10);
}

// 横
int now_rotation = 1;
void display_rotation_horizontal() {
	float pitch, roll, yaw;
	M5.IMU.getAhrsData(&pitch, &roll, &yaw);
	int rotation = (pitch < 0) ? 1 : 3;
	if (now_rotation != rotation) {
		M5.Lcd.setRotation(rotation);
		M5.Lcd.fillScreen(TFT_BLACK);
		now_rotation = rotation;
		reverseScreen = reverseScreen ? false : true;
		colorList_add();
	}
}

void colorList_add() {
	M5.Lcd.fillScreen(TFT_BLACK);
	for (int icol = 0; icol <= 127; icol++) {
		M5.Lcd.drawRect(icol * 2, 127, 2, 12, camColors[icol * 2]);
	}
}

void setup() {
	M5.begin();
	M5.IMU.Init();	// ジャイロモジュール初期化

	// Increase I2C clock speed to 450kHz
	Wire.begin(0, 26, (uint32_t)400000);
	M5.Lcd.setRotation(1);

	// use show tmp bitmap
	img.createSprite(COLS_4, ROWS_4);
	// use show tmp data
	msg.createSprite(240 - COLS_4, ROWS_4 - 10);

	Serial.println("M5StickC PLus MLX90640 IR Camera");

	// Get device parameters - We only have to do this once
	int status;
	uint16_t eeMLX90640[832];  // 32 * 24 = 768

	status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
	if (status != 0) Serial.println("Failed to load system parameters");

	status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
	if (status != 0) Serial.println("Parameter extraction failed");

	// Setting MLX90640 device at slave address 0x33 to work with 16Hz refresh
	MLX90640_SetRefreshRate(0x33, 0x05);

	MLX90640_SetResolution(0x33, 0x03);

	// Display bottom side colorList and info
	colorList_add();
}

void add_maxtemp() { maxtemp = maxtemp + 1; }
void subtract_mintemp() {
	if (mintemp <= 0) {
		mintemp = maxtemp - 1;
	} else {
		mintemp--;
	}
}
void view_temp_autoAdjust() {
	mintemp = min_v - 1;
	maxtemp = max_v + 1;
}

void btn_event() {
	// 電源ボタンによる接続のリセット
	if (M5.Axp.GetBtnPress() == 0x02) {
		esp_restart();
	}

	// 計測温度によるReset settings
	if (M5.BtnA.pressedFor(1000)) {
		view_temp_autoAdjust();
	}
	// Set Min Value - SortPress // 表示時の最大温度-1
	if (M5.BtnA.wasPressed()) {
		subtract_mintemp();
	}

	// 自動で温度色調の変更を実行するかの切り替え
	if (M5.BtnB.wasReleasefor(1000)) {	// ms以上押して離したか
		view_temp_autoAdjust_interval_call_flg = view_temp_autoAdjust_interval_call_flg ? false : true;
	}
	if (M5.BtnB.wasPressed()) {	 // 表示時の最大温度+1
		add_maxtemp();
	}
}

void loop() {
	loopTime = millis();
	startTime = loopTime;

	M5.update();
	display_rotation_horizontal();

	btn_event();  // ボタン動作

	//
	if (loopTime - prev_loopTime >= update_interval) {
		if (view_temp_autoAdjust_interval_call_flg) {
			view_temp_autoAdjust();
		}
		prev_loopTime = millis();
	}

	uint16_t mlx90640Frame[834];

	// those fun get tmp array, 32*24, 5fps
	for (byte x = 0; x < speed_setting; x++) {
		int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
		if (status < 0) {
			Serial.print("GetFrame Error: ");
			Serial.println(status);
		}

		float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
		float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
		float tr = Ta - TA_SHIFT;  // Reflected temperature based on the sensor
								   // ambient temperature
		float emissivity = 0.95;
		MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr,
							 reversePixels);  // save pixels temp to array (pixels)
		int mode = MLX90640_GetCurMode(MLX90640_address);
		MLX90640_BadPixelsCorrection(mlx90640.brokenPixels, reversePixels, mode, &mlx90640);
	}

	// if want to send tmp array to other device, you can write fun in here:
	// xxx.send(reversePixels, 32*24);

	// Reverse image (order of Integer array)
	if (reverseScreen == false) {
		for (int x = 0; x < pixelsArraySize; x++) {
			if (x % COLS == 0) {
				for (int j = 0 + x, k = (COLS - 1) + x; j < COLS + x; j++, k--) {
					pixels[j] = reversePixels[k];
				}
			}
		}
	} else {
		for (int x = pixelsArraySize - 1; x >= 0; x--) {
			if (x % COLS == 0) {
				for (int j = pixelsArraySize - x - COLS, k = x; j < pixelsArraySize - x; j++, k++) {
					pixels[j] = reversePixels[k];
				}
			}
		}
	}

	max_v = mintemp;
	min_v = maxtemp;

	for (int itemp = 0; itemp < sizeof(pixels) / sizeof(pixels[0]); itemp++) {
		if (pixels[itemp] > max_v) {
			max_v = pixels[itemp];
		}
		if (pixels[itemp] < min_v) {
			min_v = pixels[itemp];
		}
	}

	// cover pixels to pixels_4
	cover5();

	// show tmp image
	drawpixels(pixels_4, ROWS_4, COLS_4);

	loopTime = millis();
	endTime = loopTime;
	fps = 1000 / (endTime - startTime);
}
