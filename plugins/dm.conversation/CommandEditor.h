#ifndef CONVERSATION_COMMAND_EDITOR_H_
#define CONVERSATION_COMMAND_EDITOR_H_

#include <gtk/gtkliststore.h>
#include "gtkutil/window/BlockingTransientWindow.h"

#include "Conversation.h"
#include "ConversationCommand.h"

namespace ui {

class CommandEditor :
	public gtkutil::BlockingTransientWindow
{
public:
	// Whether the user clicked on cancel or OK
	enum Result {
		RESULT_CANCEL,
		RESULT_OK,
		NUM_RESULTS
	};

private:
	// The conversation (read-only)
	const conversation::Conversation& _conversation;

	// The command we're editing
	conversation::ConversationCommand& _command;

	Result _result;

	// All available actors
	GtkListStore* _actorStore;
	GtkWidget* _actorDropDown;

	// All available commands
	GtkListStore* _commandStore;

public:
	// Pass the parent window, the command and the conversation to edit
	CommandEditor(GtkWindow* parent, conversation::ConversationCommand& command, conversation::Conversation conv);

	// Determine which action the user did take to close the dialog
	Result getResult();

private:
	void populateWindow();
	void updateWidgets();

	void save();

	GtkWidget* createButtonPanel();

	static void onSave(GtkWidget* button, CommandEditor* self);
	static void onCancel(GtkWidget* button, CommandEditor* self);
};

} // namespace ui

#endif /* CONVERSATION_COMMAND_EDITOR_H_ */
