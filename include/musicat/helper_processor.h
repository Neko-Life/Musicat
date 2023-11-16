#ifndef MUSICAT_HELPER_PROCESSOR_H
#define MUSICAT_HELPER_PROCESSOR_H

#include "musicat/audio_processing.h"

#define PROCESSOR_BUFFER_SIZE processor_buffer_size

inline constexpr size_t processor_buffer_size = BUFSIZ / 2;

namespace musicat
{
// a manager instance for each slave child
// only accept s16le input and output
namespace helper_processor
{

struct helper_chain_t
{
    // parent write connected to child_read_fd (child stdin)
    int write_fd;
    // parent read connected to child_write_fd (child stdout)
    int read_fd;
    // child write (child stdout) connected to parent read_fd
    int child_write_fd;
    // child read (child stdin) connected to parent write_fd
    int child_read_fd;
    // child pid
    pid_t pid;

    audio_processing::helper_chain_option_t options;
    // stream state

    sem_t *sem;
    std::string sem_full_key;
};

void close_all_helper_fds ();

int manage_processor (const audio_processing::processor_options_t &options,
                      void (*on_fork) ());

// run buffer through effect processor chain
ssize_t run_through_chain (uint8_t *buffer, ssize_t *size,
                           bool shutdown_discard_output = false);

int shutdown_chain (bool discard_output = false);

} // helper_processor
} // musicat

#endif // MUSICAT_HELPER_PROCESSOR_H
