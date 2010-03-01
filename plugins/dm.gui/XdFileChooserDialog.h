#include "XDataLoader.h"
#include "gtkutil/window/BlockingTransientWindow.h"

namespace ui
{

class XdFileChooserDialog :
	public gtkutil::BlockingTransientWindow
{
private:
	// Pointer to an iterator to the chosen file
	XData::XDataMap::iterator* _fileIterator;

	// Pointer to an XdataMap
	XData::XDataMap* _xdMap;

	// A container for storing enumerated widgets
	GtkWidget* _treeview;

	// Gets the selection of the _treeview and stores it in the _fileIterator
	void storeSelection();
public:

	XdFileChooserDialog(XData::XDataMap::iterator* fileIterator, XData::XDataMap* xdMap);

	static void onOk(GtkWidget* widget, XdFileChooserDialog* self);
};


}