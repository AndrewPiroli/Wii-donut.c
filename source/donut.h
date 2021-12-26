#ifndef DONUT_H
#define DONUT_H
int main(void);
void blit_buffer(char[]);
char intensity_char(float);
void check_controllers();
void reset_btn();
void power_btn();
static inline void print_goodbye(const char*);
void *input_thread(void*);
void donut_loop();
#endif
