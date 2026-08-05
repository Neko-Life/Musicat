#ifdef MUSICAT_WITH_PYTHON

#define PY_SSIZE_T_CLEAN
#include "nlohmann/json.hpp"
#include <Python.h>

#include <csignal>
#include <string>
#include <unistd.h>

namespace musicat::ytdlp
{

#define INIT_SCRIPT(PWD_LIB, LIB_PATH)

inline constexpr const char module[] = "utils.ytdlp_run";

static PyStatus status;
static PyConfig config;

static PyObject *pName, *pModule, *pFunc;
static PyObject *pArgs, *pValue;
static bool has_pArgs = false;
static bool has_module = false;
static bool has_pFunc = false;

// should call shutdown if returns -2
int
init (const char *program_name, const std::string &pwd, const std::string &lib_path)
{
    const std::string init_script = "import sys\n"
                                    "sys.path.insert(0, '"
                                    + pwd
                                    + "')\n"
                                      "sys.path.insert(0, '"
                                    + lib_path + "')\n";

    PyConfig_InitPythonConfig (&config);

    /* optional but recommended */
    status = PyConfig_SetBytesString (&config, &config.program_name, program_name);
    if (PyStatus_Exception (status))
        goto exception;

    status = Py_InitializeFromConfig (&config);
    if (PyStatus_Exception (status))
        goto exception;
    PyConfig_Clear (&config);

    PyRun_SimpleString (init_script.c_str ());
    pName = PyUnicode_DecodeFSDefault (module);
    pModule = PyImport_Import (pName);
    Py_DECREF (pName);

    signal (SIGINT, SIG_DFL);

    if (!pModule)
        {
            PyErr_Print ();
            fprintf (stderr, "Failed to load \"%s\"\n", module);
            goto err;
        }
    has_module = true;

    pFunc = PyObject_GetAttrString (pModule, "run");

    if (!pFunc || !PyCallable_Check (pFunc))
        {
            fprintf (stderr, "Module \"%s\" missing run function\n", module);
            goto err;
        }
    has_pFunc = true;

    pArgs = PyTuple_New (2);
    has_pArgs = true;

    return 0;
err:
    return -2;
exception:
    PyConfig_Clear (&config);
    Py_ExitStatusException (status);
    return -1;
}

int
fetch (const std::string &query, int max_entries, nlohmann::json &out)
{
    // set url
    const std::string strurl = std::string ("ytsearch") + std::to_string (max_entries) + ":" + query;
    pValue = PyUnicode_DecodeFSDefaultAndSize (strurl.c_str (), strurl.size ());
    if (!pValue)
        {
            fprintf (stderr, "Cannot convert argument for url\n");
            return -2;
        }
    PyTuple_SetItem (pArgs, 0, pValue);

    // set max_entries here
    pValue = PyLong_FromLong (max_entries);
    if (!pValue)
        {
            fprintf (stderr, "Cannot convert argument for max_entries\n");
            return -2;
        }
    PyTuple_SetItem (pArgs, 1, pValue);

    pValue = PyObject_CallObject (pFunc, pArgs);
    if (pValue == NULL)
        {
            PyErr_Print ();
            fprintf (stderr, "Calling run failed with argument: `%s`\n", strurl.c_str ());
            return -2;
        }

    const char *res = PyUnicode_AsUTF8AndSize (pValue, NULL);
    out = nlohmann::json::parse (res);
    Py_DECREF (pValue);

    return 0;
}

int
shutdown ()
{
    int ret = 0;
    if (has_pArgs)
        Py_DECREF (pArgs);
    if (has_pFunc)
        Py_DECREF (pFunc);
    if (has_module)
        Py_DECREF (pModule);

    ret = Py_FinalizeEx ();

    return ret;
}

} // namespace musicat::ytdlp

#endif // MUSICAT_WITH_PYTHON
