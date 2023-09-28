#ifndef MUSICAT_HELPER_PROCESSOR_H
#define MUSICAT_HELPER_PROCESSOR_H

#include "musicat/audio_processing.h"

namespace musicat
{
// a manager instance for each slave child
namespace helper_processor
{

int manage_processor (const audio_processing::processor_options_t &options,
                      audio_processing::processor_options_t &current_options);

// run buffer through chain
ssize_t run_through_chain (uint8_t *buffer, ssize_t *size);

} // helper_processor
} // musicat

#endif // MUSICAT_HELPER_PROCESSOR_H
