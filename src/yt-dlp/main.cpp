#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <csignal>
#include <string>
#include <unistd.h>

#define YTDLP_LIB "../../../libs/yt-dlp"
#define PWD_LIB "../../../src/yt-dlp"

#define INIT_SCRIPT(LIB_PATH)                                                                                                              \
    "import sys\n"                                                                                                                         \
    "sys.path.insert(0, '" PWD_LIB "')\n"                                                                                                  \
    "sys.path.insert(0, '" LIB_PATH "')\n"

static const char module[] = "utils.ytdlp_run";

int
main (int argc, char *argv[])
{
    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig (&config);

    PyObject *pName, *pModule, *pFunc;
    PyObject *pArgs, *pValue;
    bool has_pArgs = false;
    bool has_module = false;
    bool has_pFunc = false;

    char c[1024 + 1];
    size_t s = 0;

    /* optional but recommended */
    status = PyConfig_SetBytesString (&config, &config.program_name, argv[0]);
    if (PyStatus_Exception (status))
        goto exception;

    status = Py_InitializeFromConfig (&config);
    if (PyStatus_Exception (status))
        goto exception;
    PyConfig_Clear (&config);

    PyRun_SimpleString (INIT_SCRIPT (YTDLP_LIB));
    pName = PyUnicode_DecodeFSDefault (module);
    pModule = PyImport_Import (pName);
    Py_DECREF (pName);

    signal (SIGINT, SIG_DFL);

    if (!pModule)
        {
            PyErr_Print ();
            fprintf (stderr, "Failed to load \"%s\"\n", module);
            goto end;
        }
    has_module = true;

    pFunc = PyObject_GetAttrString (pModule, "run");

    if (!pFunc || !PyCallable_Check (pFunc))
        {
            fprintf (stderr, "Module \"%s\" missing run function\n", module);
            goto end;
        }
    has_pFunc = true;

    pArgs = PyTuple_New (2);
    has_pArgs = true;

    // set max_entries here
    pValue = PyLong_FromLong (2);
    if (!pValue)
        {
            fprintf (stderr, "Cannot convert argument for max_entries\n");
            goto end;
        }
    /* pValue reference stolen here: */
    PyTuple_SetItem (pArgs, 1, pValue);

    while ((s = read (STDIN_FILENO, c, 1024)))
        {
            c[s] = '\0';
            if (c[s - 1] == '\n')
                c[s - 1] = '\0';

            std::string strurl = std::string ("ytsearch2:") + std::string (c);
            pValue = PyUnicode_DecodeFSDefaultAndSize (strurl.c_str (), strurl.size ());
            if (!pValue)
                {
                    fprintf (stderr, "Cannot convert argument for url\n");
                    goto end;
                }
            /* pValue reference stolen here: */
            PyTuple_SetItem (pArgs, 0, pValue);

            pValue = PyObject_CallObject (pFunc, pArgs);
            if (pValue != NULL)
                {
                    const char *res = PyUnicode_AsUTF8AndSize (pValue, NULL);
                    fprintf (stderr, "%s\n", res);
                    Py_DECREF (pValue);
                }
            else
                {
                    PyErr_Print ();
                    fprintf (stderr, "Call run failed with argument: `%s`\n", strurl.c_str ());
                    goto end;
                }
        }

end:
    if (has_pArgs)
        Py_DECREF (pArgs);
    if (has_pFunc)
        Py_DECREF (pFunc);
    if (has_module)
        Py_DECREF (pModule);

    if (Py_FinalizeEx () < 0)
        {
            exit (120);
        }

    return 0;

exception:
    PyConfig_Clear (&config);
    Py_ExitStatusException (status);
}
