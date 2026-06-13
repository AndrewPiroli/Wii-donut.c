#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <gccore.h>
#include <gctypes.h>

#define BUFFER_WIDTH 80
#define BUFFER_HEIGHT 27
#define BUFFER_SIZE (BUFFER_WIDTH*BUFFER_HEIGHT)

#define IMAGE_SCALE 15
#define TWO_PI 6.28

// donut point traversal increment (inverse of density)
#define DELTA_J 0.07
#define DELTA_I 0.02

// Fwd Decls
static void blit_buffer(char const* const);
static char intensity_char(float);
static inline void print_goodbye(char const* const);
static void donut_loop(void);

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

// rotation increment around each axis
static double DELTA_A = 0.04;
static double DELTA_B = 0.02;

int main(void) {
	VIDEO_Init();
	PAD_Init(); 
	// setup video and console parameters
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
	donut_loop();
}

static void donut_loop(void){
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
	}
}


static void blit_buffer(char const* const pixel_buffer) {
	int buffer_pos;
	// put cursor at home position
	printf("\x1b[H");
	//For some reason this is too high on the Wii?
	//Try offsetting a few newlines
	printf("\n");
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

static char intensity_char(float const intensity) {
	char gradient[] = ".,-~:;=!*#$@";
	int gradient_position = 12 * (intensity / 1.5);
	if (0 < gradient_position && gradient_position < 12) {
		return gradient[gradient_position];
	}
	return gradient[0];
}

static inline void print_goodbye(char const* const message){
	//Assuming a 25 line 80 col terminal
	//And a message that fits comfortably on 1 line without wrapping
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
