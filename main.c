#include <stdio.h>
#include <time.h>
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
	int pad_level;
	int pad_width;
	int pad_length;
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

float current_time()
{
	return (float) clock() / CLOCKS_PER_SEC;
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
		.radius = 30.0f,
		.speed = (struct pos) {
			.x = 0,
			.y = 500.0f
		}
	};

	game.pad = (struct pad) {
		.pad_level = RES_Y - 30,
		.pad_width = 25,
		.pad_length = 400,
		.pos = RES_X * 0.5 - 200,
		.speed = 0.0f
	};

	game.state = INDECISIVE;
}

FILE * framebuffer;

struct color
{
	unsigned char b, g, r, a;
} background_color = { .b = 30, .g = 35, .r = 30, .a = 0 }
, buf[RES_Y][RES_X]
, block_color = { .b = 255, .g = 255, .r = 255, .a = 0 }
, pad_color = { .b = 255, .g = 255, .r = 255, .a = 0 }
, ball_color = { .b = 255, .g = 230, .r = 230, .a = 0 };

void draw_n_pixels_in_line(int n, struct color * clr)
{
	int i;
	for (i = 0; i < n; ++i)
		fwrite(clr, sizeof(struct color), 1, framebuffer);
}

void draw_uniform_color(struct color * clr)
{
	fseek(framebuffer, 0, SEEK_SET);
	draw_n_pixels_in_line(RES_Y * RES_X, clr);
}

int pixel_number(int y, int x)
{
	return y * RES_X + x;
}

int pixel_offset(int y, int x)
{
	return sizeof(struct color) * pixel_number(y, x);
}

void draw_rectangle(struct pos top_left, struct pos bot_right, struct color * clr)
{
	int i;
	int j = top_left.x + 1;
	int width = bot_right.x - top_left.x;
	for (i = top_left.y + 1; i < bot_right.y; ++i)
	{
		fseek(framebuffer, pixel_offset(i, j), SEEK_SET);
		draw_n_pixels_in_line(width - 2, clr);
	}
}

void draw_pad(struct color * clr)
{
	struct pos bottom_right_pad_corner = { .x = game.pad.pos + game.pad.pad_length, .y = game.pad.pad_level + game.pad.pad_width };	
	struct pos top_left_pad_corner = { .x = game.pad.pos, .y = game.pad.pad_level};

	draw_rectangle(top_left_pad_corner, bottom_right_pad_corner, clr);
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
		fseek(framebuffer, pixel_offset(i + c.y, c.x - half_width), SEEK_SET);
		draw_n_pixels_in_line(half_width << 1, clr);
	}

	for (i = 0; i <= r; ++i)
	{
		while(half_width * half_width + i * i >= r * r)
			--half_width;
		fseek(framebuffer, pixel_offset(i + c.y, c.x - half_width), SEEK_SET);
		draw_n_pixels_in_line(half_width << 1, clr);
	}
}

void write_to_framebuffer()
{
	fseek(framebuffer, 0, SEEK_SET);
	fwrite(buf, sizeof(buf), 1, framebuffer);
}

struct pos grid_to_screen(int y, int x)
{
	struct pos ret = { .x = x * RES_X / MMAX, .y = y * RES_Y / NMAX };
	return ret;
}

struct pos screen_to_grid(struct pos pos)
{
	struct pos ret = { .x = pos.x * MMAX / RES_X, .y = pos.y * NMAX / RES_Y };
	return ret;
}

void init_renderer()
{
	framebuffer = fopen("/dev/fb0", "w");
	draw_uniform_color(&background_color);
	int i, j;
	for (i = 0; i < NMAX; ++i)
		for (j = 0; j < MMAX; ++j)
			if (game.is_block[i][j])
				draw_rectangle(grid_to_screen(i, j), grid_to_screen(i + 1, j + 1), &block_color);

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

void render()
{
	// erase previous apparition
//	draw_ball(last_ball_pos, game.ball.radius, &background_color);
//	draw_pad(&background_color);

	draw_ball(game.ball.pos, game.ball.radius, &ball_color);
	draw_pad(&pad_color);
}

void run()
{
	while(game.state == INDECISIVE)
	{
		//process_keyboard_input();
		update_positions();

//		check_collisions();
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
