#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <csignal>
#include <string>
#include <thread>
#include <unistd.h>

#define YTDLP_LIB "../../../libs/yt-dlp"
#define PWD_LIB "../../../src/yt-dlp"

#define INIT_SCRIPT(LIB_PATH)                                                                                                              \
    "import sys\n"                                                                                                                         \
    "sys.path.insert(0, '" PWD_LIB "')\n"                                                                                                  \
    "sys.path.insert(0, '" LIB_PATH "')\n"

inline constexpr const char module_ytdlp[] = "utils.ytdlp_run";
inline constexpr const char module_ytdlp_musicat[] = "utils.ytdlp_run_musicat";
inline constexpr const char module_futures[] = "concurrent.futures";

struct py_obj_t
{
    PyObject *obj;
    std::string name;

    py_obj_t () : obj (nullptr) {}
    py_obj_t (PyObject *const _obj) : obj (_obj) {}
    py_obj_t (const std::string &_name) : obj (nullptr), name (_name) {}
    py_obj_t (PyObject *const _obj, const std::string &_name) : obj (_obj), name (_name) {}

    constexpr
    operator PyObject *() const noexcept
    {
        return obj;
    }

    // delete copy
    py_obj_t (const py_obj_t &) = delete;
    py_obj_t &operator= (const py_obj_t &) = delete;

    py_obj_t &
    operator= (PyObject *const o)
    {
        obj = o;
        return *this;
    }

    ~py_obj_t ()
    {
        if (!obj)
            return;
        Py_DECREF (obj);
        obj = nullptr;
    }
};

struct py_module_t
{
    py_obj_t mod;

    py_module_t () = default;
    py_module_t (PyObject *_mod, std::string &_name) : mod (py_obj_t{ _mod, _name }) {}
};

struct py_func_t
{
    py_obj_t func;

    py_func_t () = default;
    py_func_t (PyObject *_func, std::string &_name) : func (py_obj_t{ _func, _name }) {}
};

int
setup_module (py_module_t &mod, const char *module_name)
{
    PyObject *pName = PyUnicode_DecodeFSDefault (module_name);
    PyObject *pModule = PyImport_Import (pName);
    Py_DECREF (pName);

    if (!pModule)
        {
            PyErr_Print ();
            fprintf (stderr, "Failed to load \"%s\"\n", module_name);
            return 1;
        }
    mod.mod = pModule;
    mod.mod.name = module_name;
    return 0;
}

int
get_func (const py_obj_t &obj, py_func_t &func, const char *func_name)
{
    PyObject *pFunc = PyObject_GetAttrString (obj, func_name);

    if (pFunc)
        {
            if (!PyCallable_Check (pFunc))
                {
                    Py_DECREF (pFunc);
                    fprintf (stderr, "Attribute \"%s\" in object \"%s\" is not a function\n", func_name, obj.name.c_str ());
                    return 1;
                }
        }
    else
        {
            PyErr_Print ();
            fprintf (stderr, "Object \"%s\" missing function \"%s\"\n", obj.name.c_str (), func_name);
            return 1;
        }
    func.func = pFunc;
    func.func.name = func_name;
    return 0;
}

int
get_func (const py_module_t &mod, py_func_t &func, const char *func_name)
{
    return get_func (mod.mod, func, func_name);
}

namespace MusicatModule
{
static PyObject *ModuleError = NULL;

static int
musicat_module_exec (PyObject *m)
{
    if (ModuleError != NULL)
        {
            PyErr_SetString (PyExc_ImportError, "cannot initialize Musicat module more than once");
            return -1;
        }

    ModuleError = PyErr_NewException ("Musicat.error", NULL, NULL);
    if (PyModule_AddObjectRef (m, "MusicatError", ModuleError) < 0)
        return -1;

    return 0;
}

static PyObject *
musicat_callback (PyObject *self, PyObject *args)
{
    const char *id = nullptr;
    const char *result = nullptr;

    PyArg_ParseTuple (args, "ss", &id, &result);

    if (result && strlen (result) > 0)
        fprintf (stderr, "RESULT FOR (%s)\n%s\n", id, result);
    else
        fprintf (stderr, "NO RESULT FOR (%s)\n", id);

    // if (sts < 0)
    //     {
    //         PyErr_SetString (ModuleError, "Musicat callback() error");
    //         return NULL;
    //     }

    Py_RETURN_NONE;
}

static PyMethodDef musicat_methods[] = { { "callback", musicat_callback, METH_VARARGS, "Musicat callback." }, { NULL, NULL, 0, NULL } };

static PyModuleDef_Slot musicat_module_slots[] = { { Py_mod_exec, (void *)musicat_module_exec }, { 0, NULL } };

static struct PyModuleDef musicat_module = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "musicat",
    .m_size = 0, // non-negative
    .m_methods = musicat_methods,
    .m_slots = musicat_module_slots,
};

PyMODINIT_FUNC
PyInit_musicat (void)
{
    return PyModuleDef_Init (&musicat_module);
}
} // namespace MusicatModule

int
shared_main (int argc, char *argv[], std::function<void ()> &&run)
{
    PyStatus status;
    PyConfig config;
    PyConfig_InitPythonConfig (&config);
    config.isolated = 1;

    /* Add a built-in module, before Py_Initialize */
    if (PyImport_AppendInittab ("musicat", MusicatModule::PyInit_musicat) == -1)
        {
            fprintf (stderr, "Error: could not extend in-built modules table\n");
            return -1;
        }

    /* optional but recommended */
    status = PyConfig_SetBytesString (&config, &config.program_name, argv[0]);
    if (PyStatus_Exception (status))
        goto exception;

    status = Py_InitializeFromConfig (&config);
    if (PyStatus_Exception (status))
        goto exception;

    // done with configuration here
    PyConfig_Clear (&config);

    PyRun_SimpleString (INIT_SCRIPT (YTDLP_LIB));

    run ();

    if (Py_FinalizeEx () < 0)
        exit (120);

    return 0;

exception:
    PyConfig_Clear (&config);
    Py_ExitStatusException (status);

    return -1;
}

namespace ThreadPoolExecutor
{

int
setup_pArgs (PyObject **pArgs, PyObject **pValue, bool &has_pArgs, PyObject *pFn)
{
    *pArgs = PyTuple_New (4);
    has_pArgs = true;

    // set self here
    /* pValue reference stolen here: */
    PyTuple_SetItem (*pArgs, 0, pFn);

    // set max_entries here
    *pValue = PyLong_FromLong (2);
    if (!*pValue)
        {
            fprintf (stderr, "Cannot convert argument for max_entries\n");
            return 1;
        }
    /* pValue reference stolen here: */
    PyTuple_SetItem (*pArgs, 2, *pValue);

    *pValue = PyLong_FromLong (0);
    if (!*pValue)
        {
            fprintf (stderr, "Cannot convert argument for print_stdout\n");
            return 1;
        }
    /* pValue reference stolen here: */
    PyTuple_SetItem (*pArgs, 3, *pValue);
    return 0;
}

void
run ()
{
    py_module_t ytdlp_module;
    py_func_t ytdlp_run_func;

    py_module_t futures_module;
    py_func_t futures_ThreadPoolExecutor_func;

    py_obj_t executor{ "executor" };
    py_func_t executor_submit;
    py_func_t executor_result;

    py_obj_t dict = PyDict_New ();

    PyObject *pArgs, *pValue, *pName;
    bool has_pArgs = false;

    char c[1024 + 1];
    size_t s = 0;

    std::string executor_init = "TPE(max_workers=" + std::to_string (std::thread::hardware_concurrency () / 2) + ")\n";

    bool end = false;
    // end = true;

    if (setup_module (ytdlp_module, module_ytdlp))
        goto end;
    if (get_func (ytdlp_module, ytdlp_run_func, "run"))
        goto end;
    if (setup_module (futures_module, module_futures))
        goto end;
    if (get_func (futures_module, futures_ThreadPoolExecutor_func, "ThreadPoolExecutor"))
        goto end;

    // setup ThreadPoolExecutor
    pName = PyUnicode_DecodeFSDefault ("TPE");
    PyDict_SetItem (dict, pName, futures_ThreadPoolExecutor_func.func);
    executor = PyRun_String (executor_init.c_str (), Py_eval_input, dict, NULL);
    if (!executor)
        {
            PyErr_Print ();
            goto end;
        }

    if (get_func (executor, executor_submit, "submit"))
        goto end;

    if (setup_pArgs (&pArgs, &pValue, has_pArgs, ytdlp_run_func.func))
        goto end;

    while (!end && (s = read (STDIN_FILENO, c, 1024)))
        {
            if (end || s <= 0 || !c[0])
                break;

            c[s] = '\0';
            if (c[s - 1] == '\n')
                c[s - 1] = '\0';

            std::string strurl = std::string ("ytsearch2:") + std::string (c);
            memset (c, 0, 1024 + 1);

            pValue = PyUnicode_FromStringAndSize (strurl.c_str (), strurl.size ());
            if (!pValue)
                {
                    fprintf (stderr, "Cannot convert argument for url\n");
                    end = true;
                    break;
                }
            /* pValue reference stolen here: */
            PyTuple_SetItem (pArgs, 1, pValue);

            pValue = PyObject_CallObject (executor_submit.func, pArgs);

            // get future result
            if (pValue != NULL)
                {
                    if (get_func ({ pValue, "pValue" }, executor_result, "result"))
                        {
                            end = true;
                            break;
                        }

                    pValue = PyObject_CallObject (executor_result.func, NULL);
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
                            end = true;
                            break;
                        }
                }
            else
                {
                    PyErr_Print ();
                    fprintf (stderr, "Call run failed with argument: `%s`\n", strurl.c_str ());
                    end = true;
                    break;
                }
        }

end:
    if (has_pArgs)
        Py_DECREF (pArgs);
}

int
main (int argc, char *argv[])
{
    return shared_main (argc, argv, run);
}
} // namespace ThreadPoolExecutor

namespace MusicatExecutor
{
int
setup_pArgs (PyObject **pArgs, PyObject **pValue, bool &has_pArgs)
{
    *pArgs = PyTuple_New (4);
    has_pArgs = true;

    // set max_entries here
    *pValue = PyLong_FromLong (2);
    if (!*pValue)
        {
            fprintf (stderr, "Cannot convert argument for max_entries\n");
            return 1;
        }
    /* pValue reference stolen here: */
    PyTuple_SetItem (*pArgs, 2, *pValue);

    *pValue = PyLong_FromLong (0);
    if (!*pValue)
        {
            fprintf (stderr, "Cannot convert argument for print_stdout\n");
            return 1;
        }
    /* pValue reference stolen here: */
    PyTuple_SetItem (*pArgs, 3, *pValue);
    return 0;
}

void
run ()
{
    py_module_t ytdlp_module;
    py_func_t ytdlp_run_func;

    PyObject *pArgs, *pValue, *pName;
    bool has_pArgs = false;

    char c[1024 + 1];
    size_t s = 0;

    bool end = false;
    // end = true;

    if (setup_module (ytdlp_module, module_ytdlp_musicat))
        goto end;
    if (get_func (ytdlp_module, ytdlp_run_func, "run"))
        goto end;
    if (setup_pArgs (&pArgs, &pValue, has_pArgs))
        goto end;

    while (!end && (s = read (STDIN_FILENO, c, 1024)))
        {
            if (end || s <= 0 || !c[0])
                break;

            c[s] = '\0';
            if (c[s - 1] == '\n')
                c[s - 1] = '\0';

            std::string strurl = std::string ("ytsearch2:") + std::string (c);
            memset (c, 0, 1024 + 1);

            pValue = PyUnicode_FromStringAndSize (strurl.c_str (), strurl.size ());
            if (!pValue)
                {
                    fprintf (stderr, "Cannot convert argument for url\n");
                    end = true;
                    break;
                }
            /* pValue reference stolen here: */
            PyTuple_SetItem (pArgs, 1, pValue);
            PyTuple_SetItem (pArgs, 0, PyUnicode_FromStringAndSize (strurl.c_str (), strurl.size ()));

            pValue = PyObject_CallObject (ytdlp_run_func.func, pArgs);

            if (pValue != NULL)
                Py_DECREF (pValue);
            else
                {
                    PyErr_Print ();
                    fprintf (stderr, "Call run failed with argument: `%s`\n", strurl.c_str ());
                    end = true;
                    break;
                }
        }

end:
    if (has_pArgs)
        Py_DECREF (pArgs);
}

int
main (int argc, char *argv[])
{
    return shared_main (argc, argv, run);
}
} // namespace MusicatExecutor

int
main (int argc, char *argv[])
{
    // return ThreadPoolExecutor::main (argc, argv);
    return MusicatExecutor::main (argc, argv);
}
