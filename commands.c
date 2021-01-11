static void
command_follow(size_t argc, char **argv)
{
	UNUSED(argc);

	size_t link = strtol(argv[1], NULL, 0);

	char *text = NULL;
	CURLU *url = NULL;

	if (!gemdoc_find_link(g, link, &text, &url)) {
		strcpy(ui_message, format("No such link '%zu'", link));
	} else {
		follow_link(url);
	}
}

typedef void(*command_func_t)(size_t argc, char **argv);

struct Command {
	char *name;
	command_func_t func;
	size_t args;
	char *usage;
} commands[] = {
	{ "follow", &command_follow, 1, "<link>" },
};

static void
command_run(char *buf)
{
	char *argv[255];
	size_t argc = 0;

	assert(buf[0] == ':');
	++buf;

	char *arg;
	while ((arg = strsep(&buf, " "))) {
		argv[argc] = arg;
		++argc;
	}

	for (size_t i = 0; i < SIZEOF(commands); ++i) {
		if (strcmp(commands[i].name, argv[0])) continue;

		if (argc < (commands[i].args+1)) {
			strcpy(ui_message, format("Usage: :%s %s",
					commands[i].name, commands[i].usage));
			return;
		}

		(commands[i].func)(++argc, argv);
		return;
	}

	/* TODO: warn */
	strcpy(ui_message, format("No such command '%s'", argv[0]));
}
