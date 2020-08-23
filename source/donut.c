#include <stdio.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include "donut.h"

#define BUFFER_WIDTH 80
#define BUFFER_HEIGHT 27
#define BUFFER_SIZE (BUFFER_WIDTH*BUFFER_HEIGHT)

#define IMAGE_SCALE 15
#define TWO_PI 6.28

// donut point traversal increment (inverse of density)
#define DELTA_J 0.07
#define DELTA_I 0.02

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;
bool do_reset = false;
bool do_die = false;
bool debug = false;

// rotation increment around each axis
double DELTA_A = 0.04;
double DELTA_B = 0.02;


int main(void) {
	//Wii garbage start
	VIDEO_Init();
	WPAD_Init(); //Wiimote support
	PAD_Init(); //GC controller support
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb, 0, 0, rmode->fbWidth, rmode->xfbHeight,
		     rmode->fbWidth * VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
	SYS_SetResetCallback(reset_btn); //Console reset button support
	SYS_SetPowerCallback(power_btn); //Console power button support
	WPAD_SetPowerButtonCallback(power_btn); //Wiimote power button support
	//Wii garbage end

	float A = 0;		// rotation around one axis (radians)
	float B = 0;		// rotation around other axis (radians)
	float i;
	float j;
	float z_buffer[BUFFER_SIZE];
	char pixel_buffer[BUFFER_SIZE];

	// clear the screen
	printf("\x1b[2J");

	for (;;) {

		// reset the pixel and z buffers
		memset(pixel_buffer, 32, BUFFER_SIZE);
		memset(z_buffer, 0, sizeof(float) * BUFFER_SIZE);

		float sin_A = sin(A);
		float cos_A = cos(A);
		float cos_B = cos(B);
		float sin_B = sin(B);

		// for every point on the surface of the donut
		for (j = 0; j < TWO_PI; j += DELTA_J) {
			float cos_j = cos(j);
			float sin_j = sin(j);
			for (i = 0; i < TWO_PI; i += DELTA_I) {
				float sin_i = sin(i);
				float cos_i = cos(i);
				float h = cos_j + 2;
				float D =
				    1 / (sin_i * h * sin_A + sin_j * cos_A + 5);
				float t = sin_i * h * cos_A - sin_j * sin_A;

				// project onto buffer surface point (x,y)
				int x =
				    (BUFFER_WIDTH / 2) +
				    2 * IMAGE_SCALE * D * (cos_i * h * cos_B -
							   t * sin_B);
				int y =
				    (BUFFER_HEIGHT / 2) +
				    IMAGE_SCALE * D * (cos_i * h * sin_B +
						       t * cos_B);

				// only consider points that occur on the defined surface
				if (0 < x && x < BUFFER_WIDTH && 0 < y
				    && y < BUFFER_HEIGHT) {

					// calculate position on linear buffer
					int buffer_pos = (BUFFER_WIDTH * y) + x;

					// if this point is closer to the viewer than previous point at this position, overwrite it
					if (D > z_buffer[buffer_pos]) {
						z_buffer[buffer_pos] = D;

						// calculate intensity of this point
						float intensity =
						    (cos_B *
						     (sin_j * sin_A -
						      sin_i * cos_j * cos_A)
						     - sin_i * cos_j * sin_A -
						     sin_j * cos_A -
						     cos_i * cos_j * sin_B);

						// write the intensity to the buffer
						pixel_buffer[buffer_pos] =
						    intensity_char(intensity);
					}
				}
			}
		}

		blit_buffer(pixel_buffer);

		A += DELTA_A;
		B += DELTA_B;
		check_ui();
	}
}

void check_ui() {
	//In rare cases messing with some values can leave artifacts on screen
	//So set a flag to clear the screen if the user changes internal values
	bool dirty = false;
	// Check Wiimote
	WPAD_ScanPads();
	u32 pressed = WPAD_ButtonsDown(0);
	if (pressed & WPAD_BUTTON_HOME) {
		do_reset = true;
	}
	if (pressed & WPAD_BUTTON_LEFT){
		DELTA_A -= 0.0025;
		dirty = true;
	}
	if (pressed & WPAD_BUTTON_RIGHT){
		DELTA_A += 0.0025;
		dirty = true;
	}
	if (pressed & WPAD_BUTTON_UP){
		DELTA_B += 0.0025;
		dirty = true;
	}
	if (pressed & WPAD_BUTTON_DOWN){
		DELTA_B -= 0.0025;
		dirty = true;
	}
	if (pressed & WPAD_BUTTON_1){
		if (debug){
			debug = false;
		}else{
			debug = true;
		}
		dirty = true;
	}
	if (pressed & WPAD_BUTTON_2){
		DELTA_A = 0.04;
		DELTA_B = 0.02;
		dirty = true;
	}
	//Check GC Controller
	PAD_ScanPads();
	pressed = PAD_ButtonsDown(0);
	if (pressed & PAD_BUTTON_LEFT){
		DELTA_A -= 0.0025;
		dirty = true;
	}
	if (pressed & PAD_BUTTON_RIGHT){
		DELTA_A += 0.0025;
		dirty = true;
	}
	if (pressed & PAD_BUTTON_UP){
		DELTA_B += 0.0025;
		dirty = true;
	}
	if (pressed & PAD_BUTTON_DOWN){
		DELTA_B -= 0.0025;
		dirty = true;
	}

	if (pressed & PAD_BUTTON_START) {
		do_reset = true;
	}
	if (pressed & PAD_BUTTON_Y){
		if (debug){
			debug = false;
		}else{
			debug = true;
		}
		dirty = true;
	}
	if (pressed & PAD_BUTTON_X){
		DELTA_A = 0.04;
		DELTA_B = 0.02;
		dirty = true;
	}
	if (dirty){
		printf("\x1b[2J");
	}
	if (do_reset){
		print_goodbye("Returning to loader!");
		exit(0);
	}
	if (do_die){
		print_goodbye("Shutting down system!");
		SYS_ResetSystem(SYS_POWEROFF_STANDBY,0,0);
	}

}

void reset_btn(){
	do_reset = true;
}

void power_btn(){
	do_die = true;
}

void blit_buffer(char pixel_buffer[]) {
	int buffer_pos;
	// put cursor at home position
	printf("\x1b[H");
	//For some reason this is too high on the Wii?
	//Try offsetting a few newlines
	printf("\n");
	if (debug){
		printf("DELTA_A: %f DELTA_B: %f", DELTA_A, DELTA_B);
	}
	// for every char in the buffer
	for (buffer_pos = 0; buffer_pos < 1 + BUFFER_SIZE; buffer_pos++) {
		// newline if necessary
		if (buffer_pos % BUFFER_WIDTH == 0) {
			putchar('\n');
		} else {
			putchar(pixel_buffer[buffer_pos]);
		}
	}

	VIDEO_WaitVSync();
}

char intensity_char(float intensity) {
	char gradient[] = ".,-~:;=!*#$@";
	int gradient_position = 12 * (intensity / 1.5);
	if (0 < gradient_position && gradient_position < 12) {
		return gradient[gradient_position];
	}
	return gradient[0];
}

static inline void print_goodbye(const char* message){
	//Assuming a 25 line 80 col terminal
	//And a message that fits comfortly on 1 line without wrapping
	//Pad the message vertically and horizontally
	printf("\x1b[2J"); //Clear screen
	printf("\x1b[H"); //Reset cursor
	if (message == NULL){
		return;
	}
	unsigned int half_message_len = floor(strlen(message)/2);
	if (half_message_len > 39){
		return;
	}
	printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n" //Vertical padding
	"%*s%s\n", 40-half_message_len, "", message);
	usleep(500000);
	// Program is about to die, so no cleanup needed.
}
