#include <stdio.h>
#include </usr/include/linux/input.h>

FILE * keyboard;
struct input_event kbd_evt;

void init_keyboard_reader()
{
	keyboard = fopen("/dev/input/by-path/platform-i8042-serio-0-event-kbd", "r");
}

__u16 get_key_code()
{
	fseek(keyboard, 0, SEEK_SET);
	if(fread(&kbd_evt, sizeof(kbd_evt), 1, keyboard) == 1)
		return kbd_evt.code;
	else
		return 0;
}


