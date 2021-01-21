static void
command_input(size_t argc, char **argv, char *rawargs)
{
	UNUSED(argc);
	UNUSED(argv);

	CURLU *c_url = curl_url_dup(CURTAB().doc->url);
	curl_url_set(c_url, CURLUPART_QUERY, rawargs, CURLU_URLENCODE);

	follow_link(c_url, 0);
}

static void
command_follow(size_t argc, char **argv, char *rawargs)
{
	UNUSED(argc);
	UNUSED(rawargs);

	/*
	 * Both :go and :newgo are handled by this function.
	 * If the command is :newgo, open the link in a new tab.
	 */
	_Bool opennew = !strcmp(argv[0], "newgo");

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

		if (!gemdoc_find_link(CURTAB().doc, link, &text, &c_url)) {
			ui_message(UI_STOP, "No such link '%zu'", link);
			return;
		}
	}

	if (opennew)
		newtab(c_url);
	else
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
	{ "newgo", &command_follow, 1, "<link/url>" },
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

	ENSURE(buf[0] == ':');
	if (strlen(++buf) == 0) return;

	memset(rawargs, 0x0, sizeof(rawargs));
	if ((end = memchr(buf, ' ', strlen(buf))))
		strcpy(rawargs, end + 1);

	char *arg;
	while ((arg = strsep(&buf, " "))) {
		argv[argc] = arg;
		++argc;
	}

	for (size_t i = 0; i < SIZEOF(commands); ++i) {
		if (strncmp(commands[i].name, argv[0], strlen(argv[0])))
			continue;

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

static void
command_complete(char *buf, size_t curs, char *completebuf)
{
	/* get rid of prefixed ':' */
	++buf;

	size_t words = 0;
	char *wordstart = NULL;

	_Bool in_word = false;
	for (size_t i = 0; buf[i] && i < curs; ++i) {
		if (!isblank(buf[i])) {
			if (!in_word) {
				wordstart = &buf[i];
				in_word = true;
			}
		} else if (in_word) {
			++words, in_word = false;
		}
	}

	/* add \0 to end of wordstart pointer */
	buf[curs] = '\0';

	if (!wordstart) return;

	if (words == 0) {
		for (size_t i = 0; i < SIZEOF(commands); ++i) {
			if (!strncmp(commands[i].name, wordstart, strlen(wordstart))) {
				size_t overlap = stroverlap(wordstart, commands[i].name) + 1;
				strcpy(completebuf, commands[i].name + overlap);
				return;
			}
		}
	} else {
		char *urlbuf;
		struct History *hist = &tabs[tab_cur].hist;
		for (size_t i = hist_len(hist) - 1; i >= 0; --i) {
			curl_url_get(hist->visited[i]->url, CURLUPART_URL, &urlbuf, 0);
			if (!urlbuf) continue;

			if (!strncmp(urlbuf, wordstart, strlen(wordstart))) {
				size_t overlap = stroverlap(wordstart, urlbuf) + 1;
				strcpy(completebuf, urlbuf + overlap);
				return;
			}
			free(urlbuf);
		}
	}
}
