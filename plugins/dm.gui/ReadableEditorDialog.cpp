#include "ReadableEditorDialog.h"

#include "ientity.h"
#include "iregistry.h"
#include "selectionlib.h"
#include "imainframe.h"
#include "gtkutil/MultiMonitor.h"
#include "gtkutil/LeftAlignedLabel.h"
#include "gtkutil/RightAlignment.h"
#include "gtkutil/FramedWidget.h"
#include "gtkutil/dialog.h"
#include "gtkutil/ScrolledFrame.h"
#include "gtkutil/LeftAlignment.h"

#include <gtk/gtk.h>

#include "string/string.h"

#include "XdFileChooserDialog.h"
#include "imap.h"

namespace ui
{

namespace 
{
	const std::string WINDOW_TITLE("Readable Editor");

	const std::string RKEY_READABLE_BASECLASS("game/readables/readableBaseClass");

	const char* const NO_ENTITY_ERROR = "Cannot run Readable Editor on this selection.\n"
		"Please select a single readable entity."; 

	const std::string LABEL_PAGE_RELATED("Page related statements");

	const gint LABEL_WIDTH = 20;

	const gint LABEL_HEIGHT = 15;

	// Widget handles for use in the _widgets std::map
	enum
	{
		WIDGET_EDIT_PANE,
		WIDGET_READABLE_NAME,
		WIDGET_XDATA_NAME,
		WIDGET_SAVEBUTTON,
		WIDGET_NUMPAGES,
		WIDGET_CURRENT_PAGE,
		WIDGET_PAGETURN_SND,
		WIDGET_GUI_ENTRY,
		WIDGET_PAGE_LEFT,
		WIDGET_PAGE_RIGHT,
		WIDGET_PAGE_TITLE,
		WIDGET_PAGE_RIGHT_TITLE,
		WIDGET_PAGE_BODY,
		WIDGET_PAGE_RIGHT_BODY,
	};

}

ReadableEditorDialog::ReadableEditorDialog(Entity* entity) :
	gtkutil::BlockingTransientWindow(WINDOW_TITLE, GlobalMainFrame().getTopLevelWindow()),
	_guiView(new gui::GuiView),
	_result(RESULT_CANCEL),
	_entity(entity)
{
	// Set the default border width in accordance to the HIG
	gtk_container_set_border_width(GTK_CONTAINER(getWindow()), 12);
	gtk_window_set_type_hint(GTK_WINDOW(getWindow()), GDK_WINDOW_TYPE_HINT_DIALOG);

	// Set size of the window, default size is too narrow for path entries
	/*GdkRectangle rect = gtkutil::MultiMonitor::getMonitorForWindow(
		GlobalMainFrame().getTopLevelWindow()
	);*/

	//gtk_window_set_default_size(GTK_WINDOW(getWindow()), static_cast<gint>(rect.width*0.4f), -1);

	// Add a vbox for the dialog elements
	GtkWidget* vbox = gtk_vbox_new(FALSE, 6);

	// The vbox is split horizontally, left are the controls, right is the preview
	GtkWidget* hbox = gtk_hbox_new(FALSE, 6);

	// The hbox contains the controls
	gtk_box_pack_start(GTK_BOX(hbox), createEditPane(), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), gtkutil::FramedWidget(_guiView->getWidget()), FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), createButtonPanel(), FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(getWindow()), vbox);

	// Load the initial values from the entity
	initControlsFromEntity();
}

void ReadableEditorDialog::initControlsFromEntity()
{
	// Inv_name
	gtk_entry_set_text(GTK_ENTRY(_widgets[WIDGET_READABLE_NAME]), _entity->getKeyValue("inv_name").c_str());

	// Xdata contents
	gtk_entry_set_text(GTK_ENTRY(_widgets[WIDGET_XDATA_NAME]), _entity->getKeyValue("xdata_contents").c_str());

	// Load xdata
	if (_entity->getKeyValue("xdata_contents") == "")
	{
		//Key has not been set yet. Set _xdFilename to the mapfilename.
		_xdFilename = GlobalMapModule().getMapName();
		std::size_t nameStartPos = _xdFilename.rfind("/")+1;
		_xdFilename = _xdFilename.substr( nameStartPos, _xdFilename.rfind(".")+1-nameStartPos ) + "xd";	//might lead to weird behavior if mapname has not been defined yet.
	}
	else
	{
		readable::XDataLoaderPtr xdLoader(new readable::XDataLoader());
		readable::XDataMap xdMap;
		if ( xdLoader->importDef(_entity->getKeyValue("xdata_contents"),xdMap) )
		{
			if (xdMap.size() > 1)
			{
				// The requested definition has been defined in multiple files. Use the XdFileChooserDialog to pick a file.
				// Optimally, the preview renderer would already show the selected definition.
				readable::XDataMap::iterator ChosenIt = xdMap.end();
				XdFileChooserDialog fcDialog(&ChosenIt, &xdMap);
				fcDialog.show();
				if (ChosenIt == xdMap.end())
				{
					//User clicked cancel. The window will be destroyed in _postShow()... (unclean!!!!!!!!!!!!!)
					_xdFilename = "";
					return;
				}
				_xdFilename = ChosenIt->first;
				_xData = ChosenIt->second;
			}
			else
			{
				_xdFilename = xdMap.begin()->first;
				_xData = xdMap.begin()->second;			
			}
		}
		else
		{
			//Import failed. Display the errormessage and use the default filename.
			gtkutil::errorDialog( xdLoader->getImportSummary()[xdLoader->getImportSummary().size()-1] , GlobalMainFrame().getTopLevelWindow());
			_xdFilename = GlobalMapModule().getMapName();
			std::size_t nameStartPos = _xdFilename.rfind("/")+1;
			_xdFilename = _xdFilename.substr( nameStartPos, _xdFilename.rfind(".")+1-nameStartPos ) + "xd";	//might lead to weird behavior if mapname has not been defined yet.
		}
	}
}

void ReadableEditorDialog::save()
{
	// Name
	_entity->setKeyValue("inv_name", gtk_entry_get_text(GTK_ENTRY(_widgets[WIDGET_READABLE_NAME])));

	// Xdata contents
	_entity->setKeyValue("xdata_contents", gtk_entry_get_text(GTK_ENTRY(_widgets[WIDGET_XDATA_NAME])));

	// Save xdata
	if (!_xData)	//temp until editfiels properly fleshed out.
	{
		_xData = readable::XDataPtr(new readable::OneSidedXData(gtk_entry_get_text(GTK_ENTRY(_widgets[WIDGET_XDATA_NAME]))));
		_xData->setNumPages(1);
	}
	std::string path_ = GlobalRegistry().get(RKEY_ENGINE_PATH) + "darkmod/xdata/" + _xdFilename;
	readable::FileStatus fst = _xData->xport( path_, readable::Merge);
	if ( fst == readable::DefinitionExists)
	{
		switch ( _xData->xport( path_, readable::MergeOverwriteExisting) )
		{
		case readable::OpenFailed: 
			gtkutil::errorDialog( "Failed to open " + _xdFilename + " for saving." , GlobalMainFrame().getTopLevelWindow());
			break;
		case readable::MergeFailed: 
			gtkutil::errorDialog( "Merging failed, because the length of the definition to be overwritten could not be retrieved.", 
				GlobalMainFrame().getTopLevelWindow()
				);
			break;
		default: break; //success!
		}
	}
	else if (fst == readable::OpenFailed)
		gtkutil::errorDialog( "Failed to open " + _xdFilename + " for saving." , GlobalMainFrame().getTopLevelWindow());
}

GtkWidget* ReadableEditorDialog::createEditPane()
{
	// To Do:
	//	1) Need to connect the radiobutton signal

	GtkWidget* vbox = gtk_vbox_new(FALSE, 6);
	_widgets[WIDGET_EDIT_PANE] = vbox;

	// Create the table for the widget alignment
	GtkTable* table = GTK_TABLE(gtk_table_new(5, 2, FALSE));
	gtk_table_set_row_spacings(table, 6);
	gtk_table_set_col_spacings(table, 6);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(table), FALSE, FALSE, 0);

	int curRow = 0;
	
	// Readable Name
	GtkWidget* nameEntry = gtk_entry_new();
	_widgets[WIDGET_READABLE_NAME] = nameEntry;

	GtkWidget* nameLabel = gtkutil::LeftAlignedLabel("Inventory Name:");

	gtk_table_attach(table, nameLabel, 0, 1, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults(table, nameEntry, 1, 2, curRow, curRow+1);

	curRow++;

	// XData Name
	GtkWidget* xdataNameEntry = gtk_entry_new();
	_widgets[WIDGET_XDATA_NAME] = xdataNameEntry;

	GtkWidget* xDataNameLabel = gtkutil::LeftAlignedLabel("XData Name:");

	// Add a browse-button.
	GtkWidget* browseXdButton = gtk_button_new_with_label("");
	gtk_button_set_image(GTK_BUTTON(browseXdButton), gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_SMALL_TOOLBAR) );
	g_signal_connect(
		G_OBJECT(browseXdButton), "clicked", G_CALLBACK(onBrowseXd), this
		);

	GtkWidget* hboxXd = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hboxXd), GTK_WIDGET(xdataNameEntry), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hboxXd), GTK_WIDGET(browseXdButton), FALSE, FALSE, 0);

	gtk_table_attach(table, xDataNameLabel, 0, 1, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults(table, hboxXd, 1, 2, curRow, curRow+1);

	curRow++;

	// Page count
	GtkWidget* numPagesEntry = gtk_entry_new();
	_widgets[WIDGET_NUMPAGES] = numPagesEntry;

	GtkWidget* numPagesLabel = gtkutil::LeftAlignedLabel("Number of Pages:");

	gtk_table_attach(table, numPagesLabel, 0, 1, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults(table, numPagesEntry, 1, 2, curRow, curRow+1);

	curRow++;

	// Pageturn Sound
	GtkWidget* pageTurnEntry = gtk_entry_new();
	_widgets[WIDGET_PAGETURN_SND] = pageTurnEntry;
	GtkWidget* pageTurnLabel = gtkutil::LeftAlignedLabel("Pageturn Sound:");

	gtk_table_attach(table, pageTurnLabel, 0, 1, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults(table, pageTurnEntry, 1, 2, curRow, curRow+1);

	curRow++;

	// Page Layout:
	GtkWidget* PageLayoutLabel = gtkutil::LeftAlignedLabel("Page layout:");
	GtkWidget* OneSidedRadio = gtk_radio_button_new_with_label( NULL, "One-sided" );
	GSList* PageLayoutGroup = gtk_radio_button_get_group( GTK_RADIO_BUTTON(OneSidedRadio) );
	GtkWidget* TwoSidedRadio = gtk_radio_button_new_with_label( PageLayoutGroup, "Two-sided" );
	
	// Add the radiobuttons to an hbox
	GtkWidget* hboxPL = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hboxPL), GTK_WIDGET(OneSidedRadio), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hboxPL), GTK_WIDGET(TwoSidedRadio), FALSE, FALSE, 0);

	// Add the Page Layout interface to the table:
	gtk_table_attach(table, PageLayoutLabel, 0, 1, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults(table, hboxPL, 1, 2, curRow, curRow+1);

	curRow++;

	// Page Switcher
	GtkWidget* prevPage = gtk_button_new_with_label("");
	gtk_button_set_image(GTK_BUTTON(prevPage), gtk_image_new_from_stock(GTK_STOCK_GO_BACK, GTK_ICON_SIZE_SMALL_TOOLBAR) );
	g_signal_connect(
		G_OBJECT(prevPage), "clicked", G_CALLBACK(onPrevPage), this
		);
	GtkWidget* nextPage = gtk_button_new_with_label("");
	gtk_button_set_image(GTK_BUTTON(nextPage), gtk_image_new_from_stock(GTK_STOCK_GO_FORWARD, GTK_ICON_SIZE_SMALL_TOOLBAR) );
	g_signal_connect(
		G_OBJECT(nextPage), "clicked", G_CALLBACK(onNextPage), this
		);
	GtkWidget* currPageLabel = gtkutil::LeftAlignedLabel("Current Page:");
	GtkWidget* currPageDisplay = gtkutil::LeftAlignedLabel("1");
	_widgets[WIDGET_CURRENT_PAGE] = currPageDisplay;

	// Add the elements to an hbox
	GtkWidget* hboxPS = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hboxPS), GTK_WIDGET(prevPage), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hboxPS), GTK_WIDGET(currPageLabel), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hboxPS), GTK_WIDGET(currPageDisplay), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hboxPS), GTK_WIDGET(nextPage), FALSE, FALSE, 0);

	// Align the hbox to the center
	GtkWidget* currPageContainer = gtk_alignment_new(0.5,1,0,0);
	gtk_container_add(GTK_CONTAINER(currPageContainer), hboxPS);

	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(currPageContainer), FALSE, FALSE, 0);

	// Add a label for page related edits and add it to the vbox
	GtkWidget* pageRelatedLabel = gtkutil::LeftAlignedLabel(
		std::string("<span weight=\"bold\">") + LABEL_PAGE_RELATED + "</span>"
		);
	gtk_box_pack_start(GTK_BOX(vbox), pageRelatedLabel, FALSE, FALSE, 0);

	// Add a gui chooser with a browse-button
	GtkWidget* guiLabel = gtkutil::LeftAlignedLabel("Gui Definition:");

	GtkWidget* guiEntry = gtk_entry_new();
	_widgets[WIDGET_GUI_ENTRY] = guiEntry;

	GtkWidget* browseGuiButton = gtk_button_new_with_label("");
	gtk_button_set_image(GTK_BUTTON(browseGuiButton), gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_SMALL_TOOLBAR) );
	g_signal_connect(
		G_OBJECT(browseGuiButton), "clicked", G_CALLBACK(onBrowseGui), this
		);

	// Add the elements to an hbox
	GtkWidget* hboxGui = gtk_hbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hboxGui), GTK_WIDGET(guiLabel), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hboxGui), GTK_WIDGET(guiEntry), TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hboxGui), GTK_WIDGET(browseGuiButton), FALSE, FALSE, 0);

	// Pack it into an alignment so that it's indented.
	GtkWidget* alignment = gtkutil::LeftAlignment(GTK_WIDGET(hboxGui), 18, 1.0); 
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(alignment), FALSE, FALSE, 0);

	// Page title and body edit fields: Create a 3x3 table
	GtkTable* tablePE = GTK_TABLE(gtk_table_new(4, 3, FALSE));
	gtk_table_set_row_spacings(tablePE, 6);
	gtk_table_set_col_spacings(tablePE, 12);

	// Pack it into an alignment and add it to vbox
	GtkWidget* alignmentTable = gtkutil::LeftAlignment(GTK_WIDGET(tablePE), 18, 1.0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(alignmentTable), TRUE, TRUE, 0);
	curRow = 0;

	// Create "left" and "right" labels and add them to the first row of the table
	GtkWidget* pageLeftLabel = gtk_label_new("Left");
	gtk_label_set_justify(GTK_LABEL(pageLeftLabel), GTK_JUSTIFY_CENTER);
	_widgets[WIDGET_PAGE_LEFT] = pageLeftLabel;
	
	GtkWidget* pageRightLabel = gtk_label_new("Right");
	gtk_label_set_justify(GTK_LABEL(pageRightLabel), GTK_JUSTIFY_CENTER);
	_widgets[WIDGET_PAGE_LEFT] = pageLeftLabel;

	gtk_table_attach(tablePE, pageLeftLabel, 1, 2, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(tablePE, pageRightLabel, 2, 3, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);

	curRow++;

	// Create "title" label and title-edit fields and add them to the second row of the table
	GtkWidget* titleLabel = gtkutil::LeftAlignedLabel("Title:");	

	GtkWidget* textViewTitle = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textViewTitle), GTK_WRAP_WORD);
	_widgets[WIDGET_PAGE_TITLE] = textViewTitle;	

	GtkWidget* textViewRightTitle = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textViewRightTitle), GTK_WRAP_WORD);
	_widgets[WIDGET_PAGE_RIGHT_TITLE] = textViewRightTitle;

	gtk_table_attach(tablePE, titleLabel, 0, 1, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(tablePE, gtkutil::ScrolledFrame(GTK_WIDGET(textViewTitle)), 1, 2, curRow, curRow+1, GtkAttachOptions(GTK_FILL | GTK_EXPAND), GTK_FILL, 0, 0);
	gtk_table_attach(tablePE, gtkutil::ScrolledFrame(GTK_WIDGET(textViewRightTitle)), 2, 3, curRow, curRow+1, GtkAttachOptions(GTK_FILL |GTK_EXPAND), GTK_FILL, 0, 0);

	curRow++;

	// Create "body" label and body-edit fields and add them to the third row of the table
	GtkWidget* bodyLabel = gtkutil::LeftAlignedLabel("Body:");

	GtkWidget* textViewBody = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textViewBody), GTK_WRAP_WORD);
	_widgets[WIDGET_PAGE_BODY] = textViewBody;

	GtkWidget* textViewRightBody = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textViewRightBody), GTK_WRAP_WORD);
	_widgets[WIDGET_PAGE_RIGHT_BODY] = textViewRightBody;

	gtk_table_attach(tablePE, bodyLabel, 0, 1, curRow, curRow+1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults(tablePE, gtkutil::ScrolledFrame(GTK_WIDGET(textViewBody)), 1, 2, curRow, curRow+1);
	gtk_table_attach_defaults(tablePE, gtkutil::ScrolledFrame(GTK_WIDGET(textViewRightBody)), 2, 3, curRow, curRow+1);

	curRow++;

	return vbox;
}

GtkWidget* ReadableEditorDialog::createButtonPanel()
{
	GtkWidget* hbx = gtk_hbox_new(TRUE, 6);

	GtkWidget* cancelButton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	_widgets[WIDGET_SAVEBUTTON] = gtk_button_new_from_stock(GTK_STOCK_SAVE);
	
	g_signal_connect(
		G_OBJECT(cancelButton), "clicked", G_CALLBACK(onCancel), this
	);
	g_signal_connect(
		G_OBJECT(_widgets[WIDGET_SAVEBUTTON]), "clicked", G_CALLBACK(onSave), this
	);

	gtk_box_pack_end(GTK_BOX(hbx), _widgets[WIDGET_SAVEBUTTON], TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(hbx), cancelButton, TRUE, TRUE, 0);

	return gtkutil::RightAlignment(hbx);
}

void ReadableEditorDialog::_postShow()
{
	if (_xdFilename == "")
	{
		this->destroy();
		return;
	}
	// Initialise the GL widget after the widgets have been shown
	_guiView->initialiseView();

	BlockingTransientWindow::_postShow();
}

void ReadableEditorDialog::RunDialog(const cmd::ArgumentList& args)
{
	// Check prerequisites
	const SelectionInfo& info = GlobalSelectionSystem().getSelectionInfo();

	if (info.entityCount == 1 && info.totalCount == info.entityCount)
	{
		// Check the entity type
		Entity* entity = Node_getEntity(GlobalSelectionSystem().ultimateSelected());

		if (entity != NULL && entity->getKeyValue("editor_readable") == "1")
		{
			// Show the dialog
			ReadableEditorDialog dialog(entity);
			dialog.show();

			return;
		}
	}

	// Exactly one redable entity must be selected.
	gtkutil::errorDialog(NO_ENTITY_ERROR, GlobalMainFrame().getTopLevelWindow());
}

void ReadableEditorDialog::onCancel(GtkWidget* widget, ReadableEditorDialog* self) 
{
	self->_result = RESULT_CANCEL;

	self->destroy();
}

void ReadableEditorDialog::onSave(GtkWidget* widget, ReadableEditorDialog* self) 
{
	self->_result = RESULT_OK;

	self->save();

	// Done, just destroy the window
	self->destroy();
}

void ReadableEditorDialog::onBrowseXd(GtkWidget* widget, ReadableEditorDialog* self) 
{

}

void ReadableEditorDialog::onNextPage(GtkWidget* widget, ReadableEditorDialog* self) 
{

}

void ReadableEditorDialog::onPrevPage(GtkWidget* widget, ReadableEditorDialog* self) 
{

}

void ReadableEditorDialog::onBrowseGui(GtkWidget* widget, ReadableEditorDialog* self) 
{

}

} // namespace ui
