#ifdef MUSICAT_WITH_PYTHON

#define PY_SSIZE_T_CLEAN
#include "nlohmann/json.hpp"
#include <Python.h>

#include "musicat/util.h"
#include <string>
#include <unistd.h>

namespace musicat::ytdlp
{

#define INIT_SCRIPT(PWD_LIB, LIB_PATH)

inline constexpr const char module[] = "utils.ytdlp_run";

static std::string program_name;
static std::string pwd;
static std::string lib_path;

void
set_init_params (const std::string &_program_name, const std::string &_pwd, const std::string &_lib_path)
{
    program_name = _program_name;
    pwd = _pwd;
    lib_path = _lib_path;
}

static util::throttler_t ctx_throttler{ 1 };

// !TODO: this can't be multithreaded, NEED MULTITHREAD!
class py_ctx
{
    PyStatus status;
    PyConfig config;

    PyObject *pName, *pModule, *pFunc;
    PyObject *pArgs, *pValue;
    bool has_pArgs = false;
    bool has_module = false;
    bool has_pFunc = false;

  public:
    int argc = 0;
    bool initialized = false;

    bool error;

    py_ctx (int _argc) : error (false)
    {
        argc = _argc;
        if (init (program_name.c_str (), pwd, lib_path) != 0)
            error = true;
    }
    ~py_ctx () { shutdown (); }

    py_ctx (const py_ctx &) = delete;
    py_ctx (py_ctx &&) = delete;
    py_ctx &operator= (const py_ctx &) = delete;
    py_ctx &operator= (py_ctx &&) = delete;

  private:
    int
    init (const char *program_name, const std::string &pwd, const std::string &lib_path)
    {
        if (initialized)
            return 0;

        const std::string init_script = "import sys\n"
                                        "sys.path.insert(0, '"
                                        + pwd
                                        + "')\n"
                                          "sys.path.insert(0, '"
                                        + lib_path + "')\n";

        PyConfig_InitIsolatedConfig (&config);

        /* optional but recommended */
        status = PyConfig_SetBytesString (&config, &config.program_name, program_name);
        if (PyStatus_Exception (status))
            goto exception;

        status = Py_InitializeFromConfig (&config);
        if (PyStatus_Exception (status))
            goto exception;
        PyConfig_Clear (&config);
        initialized = true;

        PyRun_SimpleString (init_script.c_str ());
        pName = PyUnicode_DecodeFSDefault (module);
        pModule = PyImport_Import (pName);
        Py_DECREF (pName);

        if (!pModule)
            {
                PyErr_Print ();
                fprintf (stderr, "[py_ctx::init ERROR] Failed to load \"%s\"\n", module);
                goto err;
            }
        has_module = true;

        pFunc = PyObject_GetAttrString (pModule, "run");

        if (!pFunc || !PyCallable_Check (pFunc))
            {
                fprintf (stderr, "[py_ctx::init ERROR] Module \"%s\" missing run function\n", module);
                goto err;
            }
        has_pFunc = true;

        pArgs = PyTuple_New (argc);
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
    shutdown ()
    {
        if (!initialized)
            return -1;

        int ret = 0;
        if (has_pArgs)
            Py_DECREF (pArgs);
        if (has_pFunc)
            Py_DECREF (pFunc);
        if (has_module)
            Py_DECREF (pModule);

        ret = Py_FinalizeEx ();

        initialized = false;
        return ret;
    }

  public:
    int
    set_arg (Py_ssize_t idx, PyObject *val)
    {
        pValue = val;
        if (!pValue)
            {
                fprintf (stderr, "[py_ctx::set_arg ERROR] Cannot convert argument for index (%ld)\n", idx);
                return -2;
            }
        PyTuple_SetItem (pArgs, idx, pValue);
        return 0;
    }

    int
    run (std::function<void (PyObject *)> &&cb)
    {
        pValue = PyObject_CallObject (pFunc, pArgs);
        if (pValue == NULL)
            {
                PyErr_Print ();
                fprintf (stderr, "[py_ctx::run ERROR] Calling run failed\n");
                return -2;
            }

        cb (pValue);

        Py_DECREF (pValue);
        return 0;
    }
};

int
fetch (const std::string &query, int max_entries, nlohmann::json &out, const std::string &outfile)
{
    auto throttler = ctx_throttler.throttle ();
    py_ctx ctx{ outfile.empty () ? 3 : 4 };
    if (ctx.error)
        return -1;

    // set url
    ctx.set_arg (0, PyUnicode_DecodeFSDefaultAndSize (query.c_str (), query.size ()));
    // set max_entries
    ctx.set_arg (1, PyLong_FromLong (max_entries));
    // set print_stdout
    ctx.set_arg (2, PyLong_FromLong (0));

    if (outfile.empty ())
        ctx.run (
            [&out] (PyObject *val)
                {
                    const char *res = PyUnicode_AsUTF8AndSize (val, NULL);
                    out = nlohmann::json::parse (res);
                });
    else
        {
            // set outfile
            ctx.set_arg (3, PyUnicode_DecodeFSDefaultAndSize (outfile.c_str (), outfile.size ()));
            ctx.run ([] (PyObject *val) {});
        }

    return 0;
}

} // namespace musicat::ytdlp

#endif // MUSICAT_WITH_PYTHON
