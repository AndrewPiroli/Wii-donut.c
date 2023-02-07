#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

#include <gccore.h>
#include <gctypes.h>
#ifdef HW_RVL
#include <wiiuse/wpad.h>
#endif //HW_RVL

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
static void check_controllers(void);
static void reset_btn();
#ifdef HW_RVL
static void power_btn();
#endif // HW_RVL
static inline void print_goodbye(char const* const);
static void* input_thread(void*);
static void donut_loop(void);

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static volatile bool do_reset = false;
static volatile bool do_die = false;
static volatile bool debug = false;
static volatile bool dirty = false;

// rotation increment around each axis
static volatile double DELTA_A = 0.04;
static volatile double DELTA_B = 0.02;

static lwp_t controller_thread_handle = LWP_THREAD_NULL;

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
	// setup callbacks for console buttons and wiimote power button
	SYS_SetResetCallback(reset_btn); //Console reset button support
#ifdef HW_RVL
	SYS_SetPowerCallback(power_btn); //Console power button support
	WPAD_Init();
	WPAD_SetPowerButtonCallback(power_btn); //Wiimote power button support
#endif //HW_RVL
	// Set the main thread (this thread) to high priority because it does the calculations and blits the buffer to screen
	lwp_t mainthread = LWP_THREAD_NULL;
	mainthread = LWP_GetSelf();
	if (mainthread != LWP_THREAD_NULL){
		LWP_SetThreadPriority(mainthread, LWP_PRIO_HIGHEST);
	}
	// Create another thread to poll the controller.
	// How do I pick a stack size??
	// Is there a rule of thumb? 
	// 1KB seems to be working /shrug
	LWP_CreateThread(&controller_thread_handle, input_thread, NULL, NULL, 1024, LWP_PRIO_IDLE);
	// Setup done - start the main program loop.
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
		// We could use a lock around these since they could be updated by the other thread in-between
		// but it's a spinning donut, so who cares if the rotation is a few radians off what the user technically wanted
		A += DELTA_A;
		B += DELTA_B;
		// Ugly to do this here, but I would have to hoist a bunch of things out of this function to move it properly, so it stays :)
		if (dirty){
			printf("\x1b[2J");
			dirty = false;
		}
		if (do_reset || do_die){
			LWP_JoinThread(controller_thread_handle, NULL);
			if (do_reset){
				print_goodbye("Returning to loader!");
				exit(0);
			}
			if (do_die){
				print_goodbye("Shutting down system!");
				SYS_ResetSystem(SYS_POWEROFF_STANDBY,0,0);
			}
		}
	}
}

static void *input_thread(void*){
	while(!do_die && !do_reset){
		check_controllers();
		usleep(50000);
	}
	// Supress return type error/warning
	// I'm sure some future version of libogc will turn this into a null pointer dereference
	return (void*)NULL;
}

static void check_controllers(void) {
	u32 pressed;
	// Check Wiimote
#ifdef HW_RVL
	WPAD_ScanPads();
	pressed = WPAD_ButtonsDown(0);
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
		debug = !debug;
		dirty = true;
	}
	if (pressed & WPAD_BUTTON_2){
		DELTA_A = 0.04;
		DELTA_B = 0.02;
		dirty = true;
	}
#endif //HW_RVL
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
		debug = !debug;
		dirty = true;
	}
	if (pressed & PAD_BUTTON_X){
		DELTA_A = 0.04;
		DELTA_B = 0.02;
		dirty = true;
	}

}

static void reset_btn(){
	do_reset = true;
}

#ifdef HW_RVL
static void power_btn(){
	do_die = true;
}
#endif // HW_RVL

static void blit_buffer(char const* const pixel_buffer) {
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
