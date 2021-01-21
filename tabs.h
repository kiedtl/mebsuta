#include "history.h"
#include "ui.h"

#define CURTAB() (tabs[tab_cur])

struct Tab {
	struct Gemdoc *doc;
	struct History hist;

	size_t ui_vscroll, ui_hscroll;
	char ui_messagebuf[255];
	enum UiMessageType ui_message_type;
	enum UiDocumentMode ui_doc_mode;
};

extern struct Tab tabs[255];
extern size_t tab_cur;

void tabs_init(void);
size_t tabs_len(void);
void tabs_free(void);
