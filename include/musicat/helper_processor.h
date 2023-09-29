#ifndef MUSICAT_HELPER_PROCESSOR_H
#define MUSICAT_HELPER_PROCESSOR_H

#include "musicat/audio_processing.h"

namespace musicat
{
// a manager instance for each slave child
// only accept s16le input and output
namespace helper_processor
{

struct helper_chain_t
{
    int write_fd;
    int read_fd;
    int child_write_fd;
    int child_read_fd;
    pid_t pid;

    audio_processing::helper_chain_option_t options;
    // stream state
};

void close_all_helper_fds ();

int manage_processor (const audio_processing::processor_options_t &options,
                      void (*on_fork) ());

// run buffer through effect processor chain
ssize_t run_through_chain (uint8_t *buffer, ssize_t *size);

int shutdown_chain ();

} // helper_processor
} // musicat

#endif // MUSICAT_HELPER_PROCESSOR_H
