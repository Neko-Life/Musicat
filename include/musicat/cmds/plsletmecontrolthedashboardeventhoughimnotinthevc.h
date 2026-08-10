#ifndef MUSICAT_COMMAND_PLSLETMECONTROLTHEDASHBOARDEVENTHOUGHIMNOTINTHEVC_H
#define MUSICAT_COMMAND_PLSLETMECONTROLTHEDASHBOARDEVENTHOUGHIMNOTINTHEVC_H

#include <dpp/dpp.h>

namespace musicat::command::plsletmecontrolthedashboardeventhoughimnotinthevc
{

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);

void slash_run (const dpp::slashcommand_t &event);

} // musicat::command::plsletmecontrolthedashboardeventhoughimnotinthevc

#endif // MUSICAT_COMMAND_PLSLETMECONTROLTHEDASHBOARDEVENTHOUGHIMNOTINTHEVC_H
