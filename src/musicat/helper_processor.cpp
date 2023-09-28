#include "musicat/helper_processor.h"

namespace musicat
{
namespace helper_processor
{

int
manage_processor (const audio_processing::processor_options_t &options,
                  audio_processing::processor_options_t &current_options)
{
    size_t required_chain_size = options.helper_chain.size (),
           current_chain_size = current_options.helper_chain.size ();

    if (required_chain_size > current_chain_size)
        {
            // !TODO: create new chain
        }
    else if (required_chain_size < current_chain_size)
        {
            // !TODO: remove unused chain
        }

    return 0;
}

ssize_t
run_through_chain (uint8_t *buffer, ssize_t *size)
{
    if (*size == 0)
        return 0;

    // TODO

    return 0;
};

} // helper_processor
} // musicat
