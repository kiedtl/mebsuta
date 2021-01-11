static void
command_follow(size_t argc, char **argv)
{
	UNUSED(argc);

	char *end = NULL;
	size_t link = strtol(argv[1], &end, 0);
	CURLU *c_url = NULL;

	if (end == argv[1]) {
		c_url = curl_url();
		CURLUcode rc = curl_url_set(c_url, CURLUPART_URL, argv[1], 0);
		if (rc) {
			strcpy(ui_message, format("Invalid link '%s'", argv[1]));
			return;
		}
	} else {
		char *text = NULL;

		if (!gemdoc_find_link(g, link, &text, &c_url)) {
			strcpy(ui_message, format("No such link '%zu'", link));
			return;
		}
	}

	follow_link(c_url);
}

static void
command_vimmer(size_t argc, char **argv)
{
	UNUSED(argc);
	UNUSED(argv);

	char *url = format("gemini://tilde.team/~kiedtl/mebs/vim.gmi?%d,%d",
			tb_height(), tb_width());
	CURLU *c_url = curl_url();
	curl_url_set(c_url, CURLUPART_URL, url, 0);

	follow_link(c_url);
}

typedef void(*command_func_t)(size_t argc, char **argv);

struct Command {
	char *name;
	command_func_t func;
	size_t args;
	char *usage;
} commands[] = {
	{ "go", &command_follow, 1, "<link/url>" },
	{ "wq", &command_vimmer, 0,           "" },
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
