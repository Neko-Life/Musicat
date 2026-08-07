#ifdef MUSICAT_WITH_PYTHON

#define PY_SSIZE_T_CLEAN
#include "nlohmann/json.hpp"
#include <Python.h>

#include "musicat/util.h"
#include <string>
#include <unistd.h>

static std::deque<std::pair<uint64_t, nlohmann::json> > outputs;
static std::mutex outputs_m;
static std::condition_variable outputs_cv;

static bool
has_output (uint64_t id)
{
    std::lock_guard lk (outputs_m);
    auto i = outputs.begin ();
    while (i != outputs.end ())
        {
            if (i->first == id)
                return true;
            i++;
        }
    return false;
}

static int
get_output (uint64_t id, nlohmann::json &data)
{
    bool got_data = false;
    do
        {
            std::unique_lock lk (outputs_m);
            auto i = outputs.begin ();
            while (i != outputs.end ())
                {
                    if (i->first != id)
                        {
                            i++;
                            continue;
                        }
                    got_data = true;
                    data = i->second;
                    outputs.erase (i);
                    return 0;
                }

            outputs_cv.wait (lk);
        }
    while (!got_data);
    return -1;
}

static int
set_output (uint64_t id, const nlohmann::json &data)
{
    {
        std::lock_guard lk (outputs_m);
        outputs.push_back ({ id, data });
    }
    outputs_cv.notify_all ();

    return 0;
}

namespace musicat::ytdlp
{

namespace musicat_module
{

static int
musicat_module_exec (PyObject *m)
{
    // allows reinitialization for sub-interpreter
    PyObject *ModuleError = NULL;

    ModuleError = PyErr_NewException ("Musicat.error", NULL, NULL);
    if (PyModule_AddObjectRef (m, "MusicatError", ModuleError) < 0)
        return -1;

    return 0;
}

static PyObject *
musicat_callback (PyObject *self, PyObject *args)
{
    uint64_t id = -1;
    const char *result = nullptr;

    PyArg_ParseTuple (args, "Ks", &id, &result);

    if (result && strlen (result) > 0)
        set_output (id, nlohmann::json::parse (result));
    else
        set_output (id, nullptr);

    Py_RETURN_NONE;
}

static PyMethodDef musicat_methods[] = { { "callback", musicat_callback, METH_VARARGS, "Musicat callback." }, { NULL, NULL, 0, NULL } };

static PyModuleDef_Slot musicat_module_slots[]
    = { { Py_mod_exec, (void *)musicat_module_exec }, { Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED }, { 0, NULL } };

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

} // namespace musicat_module

inline constexpr const char module[] = "utils.ytdlp_run_musicat";

static std::string program_name;
static std::string pwd;
static std::string lib_path;
static bool global_initialized = false;

void
set_init_params (const std::string &_program_name, const std::string &_pwd, const std::string &_lib_path)
{
    program_name = _program_name;
    pwd = _pwd;
    lib_path = _lib_path;
}

struct py_interpreter_t
{
    PyInterpreterState *state;
    PyThreadState *thread_state;

    bool is_main;
    bool is_acquired;

    py_interpreter_t () : state (nullptr), thread_state (nullptr), is_main (false), is_acquired (false) {}
    py_interpreter_t (PyInterpreterState *_state, PyThreadState *_thread_state, bool _is_main, bool _is_acquired)
        : state (_state), thread_state (_thread_state), is_main (_is_main), is_acquired (_is_acquired)
    {
    }

    ~py_interpreter_t () { destroy (); }

    void
    acquire ()
    {
        if (!thread_state || is_acquired)
            return;

        PyEval_AcquireThread (thread_state);
        is_acquired = true;
    }

    void
    release ()
    {
        if (!thread_state || !is_acquired)
            return;
        PyEval_ReleaseThread (thread_state);
        is_acquired = false;
    }

    void
    destroy ()
    {
        acquire ();
        if (global_initialized && thread_state && thread_state == PyThreadState_GetUnchecked ())
            Py_EndInterpreter (thread_state);

        state = nullptr;
        thread_state = nullptr;
        is_main = false;
        is_acquired = false;
    }
};

class py_ctx
{
    PyObject *pName, *pModule, *pFunc;
    PyObject *pArgs, *pValue;
    bool has_module = false;
    bool has_pFunc = false;

  public:
    bool initialized = false;
    int argc = 0;
    std::unique_ptr<py_interpreter_t> interpreter;
    bool error;

    py_ctx (int _argc) : error (false), argc (_argc) {}
    ~py_ctx () { clear (); }

    py_ctx (const py_ctx &) = delete;
    py_ctx (py_ctx &&) = delete;
    py_ctx &operator= (const py_ctx &) = delete;
    py_ctx &operator= (py_ctx &&) = delete;

    int
    init (const std::string &pwd, const std::string &lib_path)
    {
        if (initialized)
            return 0;

        std::string init_script = "import sys\n"
                                  "sys.path.insert(0, '"
                                  + pwd
                                  + "')\n"
                                    "sys.path.insert(0, '"
                                  + lib_path + "')\n";

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

        initialized = true;
        error = false;
        return 0;
    err:
        error = true;
        return -2;
    }

    int
    clear ()
    {
        if (!initialized || !global_initialized)
            return -1;

        int ret = 0;
        if (has_pFunc)
            {
                Py_DECREF (pFunc);
                has_pFunc = false;
            }
        if (has_module)
            {
                Py_DECREF (pModule);
                has_module = false;
            }

        initialized = false;
        return ret;
    }

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

        if (cb)
            cb (pValue);

        Py_DECREF (pValue);
        return 0;
    }
};

namespace managed
{
// main_interpreter doesn't necessarily run on the main thread
// only the first ever thread that created it,
// doesn't have associated py_ctx
// and is responsible for other interpreters creation
static std::unique_ptr<py_interpreter_t> main_interpreter;

static std::map<std::__thread_id, std::unique_ptr<py_ctx> > contexts;
static std::mutex contexts_m;

// must be called in the main thread only
static void
cache_main_interpreter ()
{
    if (main_interpreter && main_interpreter->state)
        return;

    main_interpreter = std::make_unique<py_interpreter_t> ();

    main_interpreter->state = PyInterpreterState_Get ();
    main_interpreter->thread_state = PyThreadState_GetUnchecked ();
    main_interpreter->is_main = true;
    main_interpreter->is_acquired = true;
}

static int
global_init (const char *program_name)
{
    if (global_initialized)
        return 0;

    PyStatus status;
    PyConfig config;

    PyConfig_InitPythonConfig (&config);
    config.isolated = 1;

    /* Add a built-in module, before Py_Initialize */
    if (PyImport_AppendInittab ("musicat", musicat_module::PyInit_musicat) == -1)
        {
            fprintf (stderr, "Error: could not extend in-built modules table\n");
            return -1;
        }

    /* optional but recommended */
    status = PyConfig_SetBytesString (&config, &config.program_name, program_name);
    if (PyStatus_Exception (status))
        goto exception;

    status = Py_InitializeFromConfig (&config);
    if (PyStatus_Exception (status))
        goto exception;

    global_initialized = true;
    PyConfig_Clear (&config);
    cache_main_interpreter ();

    return 0;
err:
    return -2;
exception:
    PyConfig_Clear (&config);
    Py_ExitStatusException (status);
    return -1;
}

static void
acquire_main_thread ()
{
    main_interpreter->acquire ();
}

static void
release_main_thread ()
{
    main_interpreter->release ();
}

static PyThreadState *
acquire_thread ()
{
    PyThreadState *tstate = PyThreadState_New (main_interpreter->state);
    release_main_thread ();
    PyThreadState_Swap (tstate);
    return tstate;
}

static void
release_thread (PyThreadState *tstate)
{
    if (PyThreadState_GetUnchecked () != tstate)
        {
            // tstate is not attached, attach it first before clearing
            PyThreadState_Swap (tstate);
        }
    PyThreadState_Clear (tstate);
    PyThreadState_DeleteCurrent ();
}

// create new interpreter from any thread
static std::mutex create_interpreter_m;
static std::unique_ptr<py_interpreter_t>
create_interpreter ()
{
    std::lock_guard lk (create_interpreter_m);
    PyThreadState *cur_tstate = acquire_thread ();

    PyInterpreterConfig config = {
        .use_main_obmalloc = 0,
        .allow_fork = 1,
        .allow_exec = 1,
        .allow_threads = 1,
        .allow_daemon_threads = 1,
        .check_multi_interp_extensions = 1,
        .gil = PyInterpreterConfig_OWN_GIL,
    };
    PyThreadState *tstate = NULL;
    PyStatus status = Py_NewInterpreterFromConfig (&tstate, &config);
    if (PyStatus_Exception (status))
        {
            Py_ExitStatusException (status);

            // fail
            release_thread (cur_tstate);
            return nullptr;
        }

    // this must be done before detaching tstate
    PyInterpreterState *interp_state = PyInterpreterState_Get ();

    // cur_state detached here, swapped by tstate
    // restore and delete it, after this call tstate is detached
    release_thread (cur_tstate);

    py_interpreter_t *p = new py_interpreter_t{ interp_state, tstate, false, false };
    auto i = std::unique_ptr<py_interpreter_t> (p);
    i->acquire ();

    return std::move (i);
}

static py_ctx *
get_context_unlocked ()
{
    auto i = contexts.find (std::this_thread::get_id ());
    if (i == contexts.end ())
        return nullptr;
    return i->second.get ();
}

static py_ctx *
get_context ()
{
    std::lock_guard lk (contexts_m);
    global_init (program_name.c_str ());
    auto ctx = get_context_unlocked ();
    if (ctx)
        return ctx;

    // create new context for this thread
    auto nctx = contexts.insert ({ std::this_thread::get_id (), std::make_unique<py_ctx> (5) });
    ctx = nctx.first->second.get ();
    ctx->interpreter = create_interpreter ();
    ctx->init (pwd, lib_path);
    return ctx;
}

void
on_thread_done ()
{
    std::lock_guard lk (contexts_m);
    auto i = contexts.find (std::this_thread::get_id ());
    if (i == contexts.end ())
        return;
    // destroy
    i->second->interpreter->destroy ();
    contexts.erase (i);

    if (contexts.empty ())
        {
            main_interpreter.reset ();
            Py_FinalizeEx ();
            global_initialized = false;
        }
}

} // namespace managed

// allows max of 8 concurrent query/downloads
static util::throttler_t ctx_throttler{ 8 };

static int
do_fetch (uint64_t id, const std::string &query, int max_entries, nlohmann::json &out, const std::string &outfile)
{
    auto *ctx = managed::get_context ();
    if (!ctx)
        return -1;

    ctx->interpreter->acquire ();

    // set id
    ctx->set_arg (0, PyLong_FromLong (id));
    // set url
    ctx->set_arg (1, PyUnicode_FromStringAndSize (query.c_str (), query.size ()));
    // set max_entries
    ctx->set_arg (2, PyLong_FromLong (max_entries));
    // set print_stdout
    ctx->set_arg (3, PyLong_FromLong (0));
    // set outfile
    ctx->set_arg (4, outfile.empty () ? Py_False : PyUnicode_FromStringAndSize (outfile.c_str (), outfile.size ()));

    int ret = ctx->run (nullptr);

    ctx->interpreter->release ();
    return ret;
}

int
fetch (const std::string &query, int max_entries, nlohmann::json &out, const std::string &outfile)
{
    int ret = 0;
    // IT'S FINALLY MULTITHREADED!!!
    auto id = util::get_random_number ();
    auto throttler = ctx_throttler.throttle ();
    if (do_fetch (id, query, max_entries, out, outfile))
        return -1;
    // !TODO: make this async
    ret = get_output (id, out);
    return ret;
}

} // namespace musicat::ytdlp

#endif // MUSICAT_WITH_PYTHON
