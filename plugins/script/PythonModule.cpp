#include "PythonModule.h"

// Stringify and concatenate the PYBIND11_VERSION_xxx preprocessor symbols
#define Str(x) #x
#define QUOTE(x) Str(x)
#define PYBIND11_VERSION_STR (QUOTE(PYBIND11_VERSION_MAJOR) "." QUOTE(PYBIND11_VERSION_MINOR) "." QUOTE(PYBIND11_VERSION_PATCH))

#include <pybind11/embed.h>
#include "itextstream.h"

namespace script
{

namespace
{
    constexpr const char* ModuleName = "darkradiant";
}

PythonModule::PythonModule(const NamedInterfaces& interfaceList) :
    _namedInterfaces(interfaceList),
    _outputWriter(false, _outputBuffer),
    _errorWriter(true, _errorBuffer)
{
    registerModule();
}

PythonModule::~PythonModule()
{
    // Release the references to trigger the internal cleanup before Py_Finalize
    _module.dec_ref();
    _module.release();

    _globals.dec_ref();
    _globals.release();

    py::finalize_interpreter();
}

void PythonModule::initialise()
{
    py::initialize_interpreter();

    try
    {
        // Import the darkradiant module
        // This is triggering the call to InitModule as passed to the inittab
        // so we need to set the static _instance pointer right before and clear it afterwards
        _instance = this;

        py::module::import(ModuleName);

        // Construct the console writer interface
        PythonConsoleWriterClass consoleWriter(getModule(), "PythonConsoleWriter");
        consoleWriter.def(py::init<bool, std::string&>());
        consoleWriter.def("write", &PythonConsoleWriter::write);
        consoleWriter.def("flush", &PythonConsoleWriter::flush);

        // Redirect stdio output to our local ConsoleWriter instances
        py::module::import("sys").attr("stderr") = &_errorWriter;
        py::module::import("sys").attr("stdout") = &_outputWriter;

        // String vector is used in multiple places
        py::bind_vector< std::vector<std::string> >(getModule(), "StringVector");
    }
    catch (const py::error_already_set& ex)
    {
        rError() << ex.what() << std::endl;
    }

    // Not needed anymore
    _instance = nullptr;
}

void PythonModule::registerModule()
{
    rMessage() << "Registering darkradiant module to Python using pybind11 version " <<
        PYBIND11_VERSION_STR << std::endl;

    // Register the darkradiant module to Python, the InitModule function
    // will be called as soon as the module is imported
    int result = PyImport_AppendInittab(ModuleName, InitModule);

    if (result == -1)
    {
        rError() << "Could not initialise Python module" << std::endl;
        return;
    }
}

ExecutionResultPtr PythonModule::executeString(const std::string& scriptString)
{
    ExecutionResultPtr result = std::make_shared<ExecutionResult>();

    result->errorOccurred = false;

    // Clear the output buffers before starting to execute
    _outputBuffer.clear();
    _errorBuffer.clear();

    try
    {
        std::string fullScript = "import " + std::string(ModuleName) + " as DR\n"
            "from " + std::string(ModuleName) + " import *\n";
        fullScript.append(scriptString);

        // Attempt to run the specified script
        py::eval<py::eval_statements>(fullScript, getGlobals());
    }
    catch (py::error_already_set& ex)
    {
        _errorBuffer.append(ex.what());
        result->errorOccurred = true;

        rError() << "Error executing script: " << ex.what() << std::endl;
    }

    result->output += _outputBuffer + "\n";
    result->output += _errorBuffer + "\n";

    _outputBuffer.clear();
    _errorBuffer.clear();

    return result;
}

py::module& PythonModule::getModule()
{
    return _module;
}

py::dict& PythonModule::getGlobals()
{
    return _globals;
}

#if PY_MAJOR_VERSION >= 3
PyObject* PythonModule::InitModule()
{
	return InitModuleImpl();
}
#else
void PythonModule::InitModule()
{
	InitModuleImpl();
}
#endif

PyObject* PythonModule::InitModuleImpl()
{
	try
	{
		if (!_instance)
		{
            throw new std::runtime_error("_instance reference not set");
		}

        _instance->_module = py::module::create_extension_module(ModuleName, "DarkRadiant Main Module", &_moduleDef);

        // Add the registered interfaces
        for (const auto& i : _instance->_namedInterfaces)
        {
            // Handle each interface in its own try/catch block
            try
            {
                i.second->registerInterface(_instance->getModule(), _instance->getGlobals());
            }
            catch (const py::error_already_set& ex)
            {
                rError() << "Error while initialising interface " << i.first << ": " << std::endl;
                rError() << ex.what() << std::endl;
            }
        }

		auto main = py::module::import("__main__");
		auto globals = main.attr("__dict__").cast<py::dict>();

		for (auto i = globals.begin(); i != globals.end(); ++i)
		{
            _instance->getGlobals()[(*i).first] = (*i).second;
		}

		return _instance->getModule().ptr();
	}
	catch (py::error_already_set& e)
	{
		//e.clear();
		PyErr_SetString(PyExc_ImportError, e.what());
		return nullptr;
	}
	catch (const std::exception& e)
	{
		PyErr_SetString(PyExc_ImportError, e.what());
		return nullptr;
	}
}

PythonModule* PythonModule::_instance = nullptr;
py::module::module_def PythonModule::_moduleDef;

}
