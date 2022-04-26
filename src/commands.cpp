#include "commands.h"

Sha_Base_Command::Sha_Base_Command(dpp::discord_client* _from, dpp::interaction* _interaction)
    : guild_id(0), user_id(0), from(_from), interaction(_interaction) {};

Sha_Base_Command::Sha_Base_Command()
    : guild_id(0), user_id(0), from(nullptr), interaction(nullptr) {};
