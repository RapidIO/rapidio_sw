#define __STDC_FORMAT_MACROS
#include <cinttypes>

#include <stdint.h>

#include "liblog.h"

#include "rdmad_unix_msg.h"
#include "librdma_db.h"

/* From librdma.cpp */
extern "C" int destroy_msubs_in_msh(ms_h msh);

void force_close_mso(uint32_t msoid)
{
	HIGH("FORCE_CLOSE_MSO received\n");
	/* Find the mso in the local database by its msoid */
	mso_h msoh = find_mso(msoid);
	if (!msoh) {
		CRIT("Could not find msoid(0x%X) in database\n", msoid);
	} else {
		/* Remove mso with specified msoid, msoh from database */
		if (remove_loc_mso(msoh)) {
			WARN("Failed removing msoid(0x%X) msoh(0x%" PRIx64 ")\n",
								msoid, msoh);
		} else {
			HIGH("msoid(0x%X) force-closed\n", msoid, msoh);
		}
	}
} /* force_close_mso() */

void force_close_ms(uint32_t msid)
{
	auto msh = find_loc_ms(msid);
	if (!msh) {
		CRIT("Could not find ms(0x%X)\n", msid);
	} else if (destroy_msubs_in_msh(msh)) {
		WARN("Failed to destroy msubs in msid(0x%X)\n", msid);
	} else if (remove_loc_ms(msh)) {
		WARN("Failed for msid(0x%X)\n", msid);
	} else {
		INFO("msid(0x%X) removed from database\n", msid);
	}
} /* force_close_ms() */

