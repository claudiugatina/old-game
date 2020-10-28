#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <stdlib.h>
#include "keyboard_listener.c"

#define NMAX 20
#define MMAX 30
#define RES_Y 1080
#define RES_X 1920
#define ACCEL 0.1f
#define INDECISIVE 0
#define LOST 1
#define WON 2

struct pos
{
	float x, y;
};

struct ball
{
	struct pos pos;
	float radius;
	// the speed is not actually a position, but it's still an (x, y) tuple, so this works
	struct pos speed;
};

struct pad
{
	int level;
	int width;
	int length;
	float pos;
	float speed;
};

struct
{
	char is_block[NMAX][MMAX];
	struct ball ball;
	struct pad pad;
	char state;
} game;

struct timeval game_start;

float current_time()
{
	struct timeval present_moment;
	gettimeofday(&present_moment, NULL);
	return (float)(present_moment.tv_sec - game_start.tv_sec) + (present_moment.tv_usec - game_start.tv_usec) / 1000000.0;
}

void accelerate_left()
{
	game.pad.speed -= ACCEL;
}

void accelerate_right()
{
	game.pad.speed += ACCEL;
}

void init_game()
{
	gettimeofday(&game_start, NULL);
	int i, j;
	for (i = 0; i <= NMAX; ++i)
		for (j = 0; j <= MMAX; ++j)
			if (i >= 6 && i <= 10)
				game.is_block[i][j] = 1;
			else
				game.is_block[i][j] = 0;

	game.ball = (struct ball) {
		.pos = (struct pos) {
			.x = RES_X * 0.5f,
			.y = RES_Y - 200
		},
		.radius = 15.0f,
		.speed = (struct pos) {
			.x = 0,
			.y = 80.0f
		}
	};

	game.pad = (struct pad) {
		.level = RES_Y - 30,
		.width = 25,
		.length = 400,
		.pos = RES_X * 0.5 - 200,
		.speed = 0.0f
	};

	game.state = INDECISIVE;
}

int framebuffer;

struct color
{
	unsigned char b, g, r, a;
} background_color = { .b = 30, .g = 35, .r = 30, .a = 0 }
, buf[RES_Y][RES_X]
, block_color = { .b = 255, .g = 255, .r = 255, .a = 0 }
, pad_color = { .b = 255, .g = 255, .r = 255, .a = 0 }
, ball_color = { .b = 255, .g = 230, .r = 230, .a = 0 }
, *fb;

int pixel_number(int y, int x)
{
	return y * RES_X + x;
}

void draw_uniform_color(struct color * clr)
{
	int i;
	for (i = 0; i < RES_Y * RES_X; ++i)
		fb[i] = *clr;
}

void draw_rectangle(struct pos top_left, struct pos bot_right, struct color * clr, int noise_amplitude)
{
	int i, j;
	if(noise_amplitude <= 0)
		noise_amplitude = 1;
	for (i = top_left.y + 1; i < bot_right.y; ++i)
		for (j = top_left.x + 1; j < bot_right.x; ++j)
		{
			struct color noise_added = *clr;
			noise_added.b += ((rand() % noise_amplitude) / 2 - noise_amplitude);
			noise_added.g += ((rand() % noise_amplitude) / 2 - noise_amplitude);
			noise_added.r += ((rand() % noise_amplitude) / 2 - noise_amplitude);
			fb[pixel_number(i, j)] = noise_added;
		}
}

void draw_pad(struct color * clr)
{
	struct pos bottom_right_pad_corner = { .x = game.pad.pos + game.pad.length, .y = game.pad.level + game.pad.width };	
	struct pos top_left_pad_corner = { .x = game.pad.pos, .y = game.pad.level};

	draw_rectangle(top_left_pad_corner, bottom_right_pad_corner, clr, 0);
}

void draw_ball(struct pos c, float R, struct color * clr)
{
	int i, j;
	int r = (int) R;
	int half_width = 0;

	for (i = -r; i < 0; ++i)
	{
		while(half_width * half_width + i * i < r * r)
			++half_width;
		for(j = -half_width; j <= half_width; ++j)
			fb[pixel_number(c.y + i, c.x + j)] = *clr;
	}

	for (i = 0; i <= r; ++i)
	{
		while(half_width * half_width + i * i >= r * r)
			--half_width;
		for(j = -half_width; j <= half_width; ++j)
			fb[pixel_number(c.y + i, c.x + j)] = *clr;
	}
}

struct pos grid_to_screen(int y, int x)
{
	struct pos ret = { .x = x * RES_X / MMAX, .y = y * RES_Y / NMAX };
	return ret;
}

struct pos screen_to_grid(int y, int x)
{
	struct pos ret = { .x = x * MMAX / RES_X, .y = y * NMAX / RES_Y };
	return ret;
}

void init_renderer()
{
	framebuffer = open("/dev/fb0", O_RDWR);
	fb = mmap(NULL, sizeof(buf), PROT_WRITE | PROT_READ, MAP_SHARED, framebuffer, 0);
	int zero = 0;
	ioctl(framebuffer, FBIO_WAITFORVSYNC, &zero);
	draw_uniform_color(&background_color);
	int i, j;
	// noise amplitude
	int na = 20;
	int rna = 255 - na;
	for (i = 0; i < NMAX; ++i)
		for (j = 0; j < MMAX; ++j)
			if (game.is_block[i][j])
			{
				struct color random_color = { .b = na + rand() % rna , .g = na + rand() % rna, .r = na + rand() % rna, .a = 0 };
				draw_rectangle(grid_to_screen(i, j), grid_to_screen(i + 1, j + 1), &random_color, na);
			}

	draw_ball(game.ball.pos, game.ball.radius, &ball_color);
	draw_pad(&pad_color);
}


void init()
{
	init_game();
	init_keyboard_reader();
//	init_position_updater();
//	init_collision_checker();
	init_renderer();
}



void process_keyboard_input()
{
	__u16 key = get_key_code();
	if(key == KEY_A)
		accelerate_left();
	if(key == KEY_D)
		accelerate_right();
}

struct pos last_ball_pos;
float last_pad_pos;

void update_positions()
{
	static float last_recorded_time = 0.0f;
	float current_time_tmp = current_time();
	float time_elapsed = current_time_tmp - last_recorded_time; 
//	printf("Current time: %f time elapsed %f\n", current_time_tmp, time_elapsed);
	last_recorded_time = current_time_tmp;
	
	// I'm ashamed to be doing it like this, but I will need the previous positions when rendering
	last_ball_pos = game.ball.pos;
	last_pad_pos = game.pad.pos;

	game.ball.pos.x += game.ball.speed.x * time_elapsed;
	game.ball.pos.y += game.ball.speed.y * time_elapsed;

	game.pad.pos += game.pad.speed * time_elapsed;
}

void check_win_or_lose()
{
	if (game.ball.pos.y + game.ball.radius + 2 > RES_Y)
		game.state = LOST;
}

void erase_ball_trace()
{
	int i, j;
	// TODO Intellectually get the points to be erased
	for (i = last_ball_pos.y - game.ball.radius; i <= last_ball_pos.y + game.ball.radius; ++i)
		for (j = last_ball_pos.x - game.ball.radius; j <= last_ball_pos.x + game.ball.radius; ++j)
		{
			int ry = i - game.ball.pos.y;
			int rx = j - game.ball.pos.x;
			if (rx * rx + ry * ry >= game.ball.radius * game.ball.radius)
				fb[pixel_number(i, j)] = background_color;
		}
}

void render()
{
	// erase previous apparition
	erase_ball_trace();
//	draw_ball(last_ball_pos, game.ball.radius, &background_color);
//	draw_pad(&background_color);

	draw_ball(game.ball.pos, game.ball.radius, &ball_color);
	draw_pad(&pad_color);
	struct fb_var_screeninfo fbvar;
	ioctl(framebuffer, FBIOGET_VSCREENINFO, &fbvar);
	ioctl(framebuffer, FBIOPAN_DISPLAY, &fbvar);
}

void collide_with_pad()
{
	game.ball.speed.y *= -1;
}

void collide_with_block(int i, int j)
{
	game.is_block[i][j] = 0;
	struct pos top_left = grid_to_screen(i, j);
	struct pos bot_right = grid_to_screen(i + 1, j + 1);
	draw_rectangle(top_left, bot_right, &background_color, 0);
	game.ball.speed.y *= -1;

}

void check_collisions()
{
	if (game.ball.pos.x >= game.pad.pos && game.ball.pos.x <= game.pad.pos + game.pad.length && game.ball.pos.y + game.ball.radius > game.pad.level)
		collide_with_pad();
	struct pos top = screen_to_grid(game.ball.pos.y - game.ball.radius, game.ball.pos.x);
	struct pos bot = screen_to_grid(game.ball.pos.y + game.ball.radius, game.ball.pos.x);

	int i = top.y;
	int j = top.x;
	
	if(game.is_block[i][j])
		collide_with_block(i, j);
}

void run()
{
	while(game.state == INDECISIVE)
	{
		//process_keyboard_input();
		update_positions();
//		printf("\n");
		check_collisions();
		check_win_or_lose();
		render();
		usleep(10000);
	}
//	render();
}

int main(int argn, char **argc)
{
	init();
	run();
//	teardown();
	return 0;
}

