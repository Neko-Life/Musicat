#include <dpp/dpp.h>

class Sha_Base_Command {
public:
    dpp::snowflake guild_id;
    dpp::snowflake user_id;
    dpp::discord_client* from;
    dpp::interaction* interaction;

    Sha_Base_Command(dpp::discord_client* _from, dpp::interaction* _interaction);
    Sha_Base_Command();
    ~Sha_Base_Command();
}
