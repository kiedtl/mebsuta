static void
command_input(size_t argc, char **argv, char *rawargs)
{
	UNUSED(argc);
	UNUSED(argv);

	CURLU *c_url = curl_url_dup(g->url);
	curl_url_set(c_url, CURLUPART_QUERY, rawargs, CURLU_URLENCODE);

	follow_link(c_url, 0);
}

static void
command_follow(size_t argc, char **argv, char *rawargs)
{
	UNUSED(argc);
	UNUSED(rawargs);

	char *end = NULL;
	size_t link = strtol(argv[1], &end, 0);
	CURLU *c_url = NULL;

	if (end == argv[1]) {
		char url[4096];
		memset(url, 0x0, sizeof(url));

		/* add the missing gemini:// */
		if (strncmp("gemini://", argv[1], sizeof("gemini://")-1)) {
			strcpy(url, format("gemini://%s", argv[1]));
		} else {
			strcpy(url, argv[1]);
		}

		c_url = curl_url();
		CURLUcode rc = curl_url_set(c_url, CURLUPART_URL, url, 0);
		if (rc) {
			curl_url_cleanup(c_url);
			ui_message(UI_STOP, "Invalid URL '%s'", url);
			return;
		}
	} else {
		char *text = NULL;

		if (!gemdoc_find_link(g, link, &text, &c_url)) {
			ui_message(UI_STOP, "No such link '%zu'", link);
			return;
		}
	}

	follow_link(c_url, 0);

	/* free, since follow_link takes a copy */
	curl_url_cleanup(c_url);
}

static void
command_vimmer(size_t argc, char **argv, char *rawargs)
{
	UNUSED(argc);
	UNUSED(argv);
	UNUSED(rawargs);

	char *url = format("gemini://tilde.team/~kiedtl/mebs/vim.gmi?%d,%d",
			tb_height(), tb_width());
	CURLU *c_url = curl_url();
	curl_url_set(c_url, CURLUPART_URL, url, 0);

	follow_link(c_url, 0);
}

typedef void(*command_func_t)(size_t argc, char **argv, char *rawargs);

struct Command {
	char *name;
	command_func_t func;
	size_t args;
	char *usage;
} commands[] = {
	{ "go",    &command_follow, 1, "<link/url>" },
	{ "input", &command_input,  1,    "<input>" },
	{ "wq",    &command_vimmer, 0,           "" },
};

/* TODO: use uint32_t instead of char for strings, and leverage
 * c11/c17 features when doing so */
static void
command_run(char *buf)
{
	char *argv[255], rawargs[4096], *end;
	size_t argc = 0;

	assert(buf[0] == ':');
	++buf;

	memset(rawargs, 0x0, sizeof(rawargs));
	if ((end = memchr(buf, ' ', strlen(buf))))
		strcpy(rawargs, end + 1);

	char *arg;
	while ((arg = strsep(&buf, " "))) {
		argv[argc] = arg;
		++argc;
	}

	for (size_t i = 0; i < SIZEOF(commands); ++i) {
		if (strcmp(commands[i].name, argv[0])) continue;

		if (argc < (commands[i].args+1)) {
			ui_message(UI_WARN, "Usage: :%s %s",
				commands[i].name, commands[i].usage);
			return;
		}

		(commands[i].func)(++argc, argv, rawargs);
		return;
	}

	ui_message(UI_STOP, "No such command '%s'", argv[0]);
}
