#include "history.h"
#include "list.h"
#include "ui.h"

struct Tab {
	struct Gemdoc *doc;
	struct History hist;

	size_t ui_vscroll, ui_hscroll;
	char ui_messagebuf[255];
	enum UiMessageType ui_message_type;
	enum UiDocumentMode ui_doc_mode;
};

extern struct lnklist *tabs;
extern struct lnklist *curtab;

void tabs_init(void);
size_t tabs_len(void);
void tabs_add(struct lnklist *after);
void tabs_rm(struct lnklist *tab);
void tabs_free(void);

#define CURTAB() ((struct Tab *)curtab->data)
