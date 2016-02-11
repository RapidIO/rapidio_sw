/*
 * Copyright (c) 2014, Prodrive Technologies
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * @file pe.c
 * Processing element helper functions
 */
#define _XOPEN_SOURCE 500

#include <stdint.h>
#include <string.h>

#include "inc/riocp_pe_internal.h"

#include "pe.h"

#include "maint.h"
#include "event.h"
#include "handle.h"
#include "switch.h"
#include "comptag.h"
#include "rio_regs.h"
#include "rio_devs.h"

/*fk: not working #define CONFIG_LINK_SYNC_FAR_LM_REQ 1 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Read the capabilities into the handle
 * @param pe Target PE
 * @returns
 *    - 0 on success
 *    - >0 on failure
 */
int riocp_pe_read_capabilities(struct riocp_pe *pe)
{
	int ret = 0;

	RIOCP_TRACE("Read capabilities\n");

	ret = riocp_pe_maint_read(pe, RIO_DEV_ID_CAR, &pe->cap.dev_id);
	if (ret)
		return ret;

	ret = riocp_pe_maint_read(pe, RIO_DEV_INFO_CAR, &pe->cap.dev_info);
	if (ret)
		return ret;

	ret = riocp_pe_maint_read(pe, RIO_ASM_ID_CAR, &pe->cap.asbly_id);
	if (ret)
		return ret;

	ret = riocp_pe_maint_read(pe, RIO_ASM_INFO_CAR, &pe->cap.asbly_info);
	if (ret)
		return ret;

	ret = riocp_pe_maint_read(pe, RIO_PEF_CAR, &pe->cap.pe_feat);
	if (ret)
		return ret;

	ret = riocp_pe_maint_read(pe, RIO_SRC_OPS_CAR, &pe->cap.src_op);
	if (ret)
		return ret;

	ret = riocp_pe_maint_read(pe, RIO_DST_OPS_CAR, &pe->cap.dst_op);
	if (ret)
		return ret;

	if (pe->cap.pe_feat & RIO_PEF_SWITCH) {
		ret = riocp_pe_maint_read(pe, RIO_SWP_INFO_CAR, &pe->cap.sw_port);
		if (ret)
			return ret;

		ret = riocp_pe_maint_read(pe, RIO_SWITCH_RT_LIMIT, &pe->cap.lut_size);
		if (ret)
			return ret;
	} else {
		pe->cap.sw_port = 0;
		pe->cap.lut_size = 0;
	}

	RIOCP_TRACE("Read capabilities ok\n");

	return 0;
}

/**
 * Get pointer to next extended features block
 * @param pe    Target PE
 * @param from  Offset of current Extended Feature block header
 *  (if 0 starts from ExtFeaturePtr)
 * @param value Value read at offset
 */
static int RIOCP_WU riocp_pe_get_efb(struct riocp_pe *pe, uint32_t from, uint32_t *value)
{
	int ret;
	uint32_t val;

	if (from == 0) {
		ret = riocp_pe_maint_read(pe, RIO_ASM_INFO_CAR, &val);
		if (ret)
			return ret;
		val = val & RIO_EXT_FTR_PTR_MASK;
	} else {
		ret = riocp_pe_maint_read(pe, from, &val);
		if (ret)
			return ret;
		val = RIO_GET_BLOCK_ID(val);
	}

	*value = val;

	return ret;
}

/**
 * Get RapidIO Physical extended feature pointer
 * @param pe Target PE
 * @param[out] efptr Extended feature pointer
 */
static int riocp_pe_get_efptr_phys(struct riocp_pe *pe, uint32_t *efptr)
{
	int ret;
	uint32_t _efptr;
	uint32_t _efptr_hdr;

	ret = riocp_pe_get_efb(pe, 0, &_efptr);
	if (ret)
		return ret;

	while (_efptr) {
		ret = riocp_pe_maint_read(pe, _efptr, &_efptr_hdr);
		if (ret)
			return ret;

		_efptr_hdr = RIO_GET_BLOCK_ID(_efptr_hdr);
		switch (_efptr_hdr) {
		case RIO_EFB_SER_EP_ID_V13P:
		case RIO_EFB_SER_EP_REC_ID_V13P:
		case RIO_EFB_SER_EP_FREE_ID_V13P:
		case RIO_EFB_SER_EP_ID:
		case RIO_EFB_SER_EP_REC_ID:
		case RIO_EFB_SER_EP_FREE_ID:
		case RIO_EFB_SER_EP_FREC_ID:
			*efptr = _efptr;
			return 0;
		default:
			break;
		}

		ret = riocp_pe_get_efb(pe, RIO_GET_BLOCK_PTR(_efptr), &_efptr);
		if (ret)
			return ret;
	}

	return -EIO;
}

#if 0
/**
 * Get PE extended feature
 * @note The PE should already have efptr attribute initialized
 * @param pe      Target PE
 * @param feature Extended feature code
 * @param value   Value read for requested feature
 * @retval -ENOENT Could not find feature
 * @retval -EIO Error in maintenance access
 */
static int riocp_pe_get_ef(struct riocp_pe *pe, uint32_t feature, uint32_t *value)
{
	int ret;
	uint32_t efptr = pe->efptr;
	uint32_t efptr_hdr;

	while (efptr) {
		ret = riocp_pe_maint_read(pe, efptr, &efptr_hdr);
		if (ret) {
			RIOCP_ERROR("Error reading efptr_hdr\n");
			return ret;
		}

		if (feature == RIO_GET_BLOCK_ID(efptr_hdr)) {
			RIOCP_DEBUG("Feature[0x%08x] found with value 0x%08x\n",
				feature, *value);
			*value = efptr;
			return 0;
		}

		efptr = RIO_GET_BLOCK_PTR(efptr_hdr);
		if (!efptr)
			break;
	}

	RIOCP_DEBUG("Feature[0x%08x] found with value 0x%08x\n",
		feature, *value);

	return -ENOENT;
}
#endif

/**
 * Read and initialize handle extended feature pointers when available
 * @note The pe should already have the cap attribute read
 * @param pe Target PE
 * @retval < 0 Error
 */
int riocp_pe_read_features(struct riocp_pe *pe)
{
	int ret = 0;

	/* Get extended feature pointers when available */
	if (pe->cap.pe_feat & RIO_PEF_EXT_FEATURES) {
		pe->efptr      = pe->cap.asbly_info & RIO_EXT_FTR_PTR_MASK;

		ret = riocp_pe_get_efptr_phys(pe, &pe->efptr_phys);
		if (ret)
			return ret;

//		ret = riocp_pe_get_ef(pe, RIO_EFB_ERR_MGMNT, &pe->efptr_em);
//		if (ret)
//			return ret;

		RIOCP_TRACE("PE has extended features\n");
		RIOCP_TRACE(" - p->efptr      = 0x%04x\n", pe->efptr);
		RIOCP_TRACE(" - p->efptr_phys = 0x%08x\n", pe->efptr_phys);
//		RIOCP_TRACE(" - p->efptr_em   = 0x%08x\n", pe->efptr_em);
	}

	return ret;
}

/**
 * Check if source port of PE is active using extended feature pointer
 * @retval 0 When PORT_OK is not set
 * @retval 1 When PORT_OK is set
 * @retval < 0 Error in reading if port is active
 */
int riocp_pe_is_port_active(struct riocp_pe *pe, uint32_t port)
{
	int ret;
	uint32_t val = 0;
	uint32_t efptr = pe->efptr;

	RIOCP_TRACE("[pe 0x%08x] Check if port %u is active (efptr: 0x%08x)\n",
		pe->comptag, port, efptr);

	if (efptr) {
		ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_ERR_STS_CSR(port), &val);
		if (ret) {
			RIOCP_ERROR("Unable to read PORT_N_ERR_STS_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_ERR_STS_CSR(port), port);
			return ret;
		}
	}

	if (val & RIO_PORT_N_ERR_STS_PORT_OK)
		return 1;

	return 0;
}

/**
 * Read the Port gen control CSR
 * @param pe Target PE
 * @retval < 0 Error from maintenance read
 * @retval 0 When PE is not set as discovered
 * @retval 1 When PE is set as discovered
 */
int riocp_pe_is_discovered(struct riocp_pe *pe)
{
	int ret;
	uint32_t val = 0;

	ret = riocp_pe_maint_read(pe, pe->efptr_phys + RIO_PORT_GEN_CTL_CSR, &val);
	if (ret)
		return ret;

	return (val & RIO_PORT_GEN_DISCOVERED) ? 1 : 0;
}

/**
 * Set the Port gen control CSR to MASTER and DISCOVERED bit using the extended
 *  feature pointer address of the PE.
 * @param pe Target PE
 * @retval 0 When read/write was successfull
 * @retval >0 When read/write was unsuccessfull
 */
int riocp_pe_set_discovered(struct riocp_pe *pe)
{
	uint32_t val;
	int ret;

	ret = riocp_pe_maint_read(pe, pe->efptr_phys + RIO_PORT_GEN_CTL_CSR, &val);
	val |= RIO_PORT_GEN_DISCOVERED | RIO_PORT_GEN_MASTER;
	ret += riocp_pe_maint_write(pe, pe->efptr_phys + RIO_PORT_GEN_CTL_CSR, val);

	return ret;
}

/**
 * Add peer (from peer port) to pe (pe port)
 * @param pe        Target PE
 * @param peer      Peer PE
 * @param pe_port   Port on PE
 * @param peer_port Port on peer PE
 * @retval 0 Peer successfull added to PE
 * @retval -EINVAL Invalid parameter(s)
 */
int riocp_pe_add_peer(struct riocp_pe *pe, struct riocp_pe *peer,
		uint8_t pe_port, uint8_t peer_port)
{
	if (pe == NULL || peer == NULL)
		return -EINVAL;
	if (pe_port >= RIOCP_PE_PORT_COUNT(pe->cap))
		return -EINVAL;
	if (peer_port >= RIOCP_PE_PORT_COUNT(peer->cap))
		return -EINVAL;

	/* Connect peer to pe */
	pe->peers[pe_port].peer = peer;
	pe->peers[pe_port].remote_port = peer_port;

	/* Connect pe to peer */
	peer->peers[peer_port].peer = pe;
	peer->peers[peer_port].remote_port = pe_port;

	return 0;
}

/**
 * Remove peer from pe port
 * @param pe        Target PE
 * @param port   Port on PE
 * @retval 0 Peer successfull added to PE
 * @retval -EINVAL Invalid parameter(s)
 */
int riocp_pe_remove_peer(struct riocp_pe *pe, uint8_t port)
{
	struct riocp_pe *peer;
	uint8_t peer_port;

	if (pe == NULL)
		return -EINVAL;
	if (port >= RIOCP_PE_PORT_COUNT(pe->cap))
		return -EINVAL;

	peer = pe->peers[port].peer;
	peer_port = pe->peers[port].remote_port;

	/* Remove peer from pe */
	pe->peers[port].peer = NULL;
	pe->peers[port].remote_port = 0;

	if (peer == NULL)
		return 0;

	/* Remove pe from peer */
	peer->peers[peer_port].peer = NULL;
	peer->peers[peer_port].remote_port = 0;

	return 0;
}

/**
 * Prepare probe behind PEs port (when pe is switch)
 * - Check if port is active
 * - Program anyid route to PE handle
 * - Set ANY_ID route to port on PE
 * @param pe Target switch PE
 * @param port Port to prepare for probe
 * @retval -EIO Error in maintenance access
 * @retval -ENODEV Supplied port is inactive
 */
int riocp_pe_probe_prepare(struct riocp_pe *pe, uint8_t port)
{
	int ret = 0;

	RIOCP_TRACE("Prepare probe until pe 0x%08x\n", pe->comptag);

	/* Set ANY_ID route to access PE */
	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		ret = riocp_pe_maint_set_anyid_route(pe);
		if (ret) {
			RIOCP_ERROR("Could not program anyid route\n");
			return -EIO;
		}
		ret = riocp_pe_is_port_active(pe, port);
		if (ret < 0) {
			RIOCP_ERROR("Unable to read if port is active\n");
			return -EIO;
		}
		if (ret == 0) {
			RIOCP_ERROR("Try to probe inactive port\n");
			ret = riocp_pe_maint_unset_anyid_route(pe);
			if (ret < 0) {
				RIOCP_ERROR("Failed to cleanup any_id route for pe 0x%08x\n", pe->comptag);
			}
			return -ENODEV;
		}
		ret = riocp_pe_is_lockout(pe, port);
		if (ret < 0) {
			RIOCP_ERROR("Unable to read port lockout state\n");
			return -EIO;
		}
		if (ret == 1) {
			ret = riocp_pe_clear_lockout(pe, port);
			if (ret < 0) {
				RIOCP_ERROR("Unable to clear port lockout state\n");
				return -EIO;
			}
		}
		ret = riocp_pe_switch_set_route_entry(pe, RIOCP_PE_ANY_PORT, RIOCP_PE_ANY_ID(pe), port);
		if (ret) {
			RIOCP_ERROR("Could not program route\n");
			return -EIO;
		}
	}

	RIOCP_TRACE("Prepare probe until pe 0x%08x successfull\n", pe->comptag);

	return ret;
}

/**
 * Verify found handle by writing current PE comptag and verify peer comptag
 * @param pe Target PE (for comptag_alt)
 * @retval 0 New device, no redudant path
 * @retval 1 Redundant path
 */
int riocp_pe_probe_verify_found(struct riocp_pe *pe, uint8_t port, struct riocp_pe *peer)
{
	int ret;
	uint32_t comptag_peer;
	uint32_t comptag_alt;
	uint8_t  hopcount_alt = pe->hopcount + 1;
	uint32_t any_id = RIOCP_PE_ANY_ID(pe);

	RIOCP_TRACE("Probe verify pe: hc: %u, comptag: 0x%08x, port %u\n",
		pe->hopcount, pe->comptag, port);
	RIOCP_TRACE("Probe verify pe_alt: hc: %u, d: %u\n",
		hopcount_alt, any_id);
	RIOCP_TRACE("Probe verify peer: hc: %u, comptag: 0x%08x\n",
		peer->hopcount, peer->comptag, port);

	/* Reset the component tag for alternative route */
	ret = riocp_pe_comptag_write_remote(pe->mport, any_id, hopcount_alt, 0);
	if (ret) {
		RIOCP_ERROR("Error reading comptag from d: %u, hc: %u\n", any_id, hopcount_alt);
		return -EIO;
	}

	/* read same comptag again to make sure write has been performed
		(we read pe comptag from potentially (shorter) different path) */
	ret = riocp_pe_comptag_read_remote(pe->mport, any_id, hopcount_alt, &comptag_alt);
	if (ret) {
		RIOCP_ERROR("Error reading comptag from d: %u, hc: %u\n", any_id, hopcount_alt);
		return -EIO;
	}

	/* Read peer, this programs ANY_ID route to the device */
	ret = riocp_pe_comptag_read(peer, &comptag_peer);
	if (ret) {
		RIOCP_ERROR("Error reading comptag from peer\n");
		return -EIO;
	}

	RIOCP_DEBUG("comptag_peer 0x%08x, comptag_alt 0x%08x\n",
		comptag_peer, comptag_alt);

	if (comptag_peer == comptag_alt) {
		/* Restore route to peer, and restore component tag */
		ret = riocp_pe_restore(peer);
		if (ret)
			return -EIO;
		RIOCP_DEBUG("Same device different route\n");
		return 1;
	}

	RIOCP_DEBUG("Found different PE\n");

	/* Restore route to new peer */
	ret = riocp_pe_probe_prepare(pe, port);
	if (ret)
		return ret;

	return 0;
}

/**
 * Initialize new peer after probe
 * - Peer is switch
 *   - Set route in global LUT to host destid (mport->destid)
 *   - Set switch port enumerated
 * - Set peer discovered bit
 * - Initialize event handling
 * @param peer Target peer
 */
int riocp_pe_probe_initialize_peer(struct riocp_pe *peer)
{
	int ret;

	/* Set route on new switch to port routing to the host destid */
	if (RIOCP_PE_IS_SWITCH(peer->cap)) {
		ret = riocp_pe_switch_set_route_entry(peer, RIOCP_PE_ANY_PORT,
			peer->mport->destid, RIOCP_PE_SW_PORT(peer->cap));
		if (ret) {
			RIOCP_ERROR("Unable to set route on peer\n");
			return ret;
		}
	}

	/* Initialize peer event management (port-write receival) */
	ret = riocp_pe_event_init(peer);
	if (ret) {
		RIOCP_ERROR("Unable to initialize events for new pe\n");
		return ret;
	}

	return 0;
}

/**
 * Check if source port of PE is in lockout state extended feature pointer
 * @retval 0 When lockout is not set
 * @retval 1 When lockout is set
 * @retval < 0 Error in reading if port is active
 */
int riocp_pe_is_lockout(struct riocp_pe *pe, uint8_t port)
{
	int ret;
	uint32_t val = 0;
	uint32_t efptr = pe->efptr;

	RIOCP_TRACE("[pe 0x%08x] Check if port %u is in lockout (efptr: 0x%08x)\n",
		pe->comptag, port, efptr);

	if (efptr) {
		ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_CTL_CSR(port), &val);
		if (ret) {
			RIOCP_ERROR("Unable to read RIO_PORT_N_CTL_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_CTL_CSR(port), port);
			return ret;
		}
	}

	if (val & RIO_PORT_N_CTL_LOCKOUT)
		return 1;

	return 0;
}

/**
 * Set the source port of PE into lockout state extended feature pointer
 * @retval 0 When lockout is set
 * @retval < 0 Error in reading if port is active
 */
int riocp_pe_set_lockout(struct riocp_pe *pe, uint8_t port)
{
	int ret;
	uint32_t val = 0;
	uint32_t efptr = pe->efptr;

	RIOCP_TRACE("[pe 0x%08x] Set port %u into lockout (efptr: 0x%08x)\n",
		pe->comptag, port, efptr);

	if (efptr) {
		ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_CTL_CSR(port), &val);
		if (ret) {
			RIOCP_ERROR("Unable to read RIO_PORT_N_CTL_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_CTL_CSR(port), port);
			return ret;
		}
		val |= RIO_PORT_N_CTL_LOCKOUT;

		ret = riocp_pe_maint_write(pe, efptr + RIO_PORT_N_CTL_CSR(port), val);
		if (ret) {
			RIOCP_ERROR("Unable to write RIO_PORT_N_CTL_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_CTL_CSR(port), port);
			return ret;
		}

		return 0;
	}

	return -ENOTSUP;
}

/**
 * Clear the source port of PE lockout state extended feature pointer
 * @retval 0 When lockout is cleared
 * @retval < 0 Error in reading if port is active
 */
int riocp_pe_clear_lockout(struct riocp_pe *pe, uint8_t port)
{
	int ret;
	uint32_t val = 0;
	uint32_t efptr = pe->efptr;

	RIOCP_TRACE("[pe 0x%08x] Clear port %u lockout (efptr: 0x%08x)\n",
		pe->comptag, port, efptr);

	if (efptr) {
		ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_CTL_CSR(port), &val);
		if (ret) {
			RIOCP_ERROR("Unable to read RIO_PORT_N_CTL_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_CTL_CSR(port), port);
			return ret;
		}
		val &= ~RIO_PORT_N_CTL_LOCKOUT;

		ret = riocp_pe_maint_write(pe, efptr + RIO_PORT_N_CTL_CSR(port), val);
		if (ret) {
			RIOCP_ERROR("Unable to write RIO_PORT_N_CTL_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_CTL_CSR(port), port);
			return ret;
		}

		return 0;
	}

	return -ENOTSUP;
}

/**
 * Do a peer link sync on PEs port (when pe is switch)
 * - Check if port is active
 * - send link request
 * - check link response
 * - update ackid status
 * - clear port errors
 * - check port error status
 * @param pe Target switch PE
 * @param port Port to do the link sync
 * @retval -EIO Error in maintenance access
 * @retval -ENODEV Supplied port is inactive
 */
int riocp_pe_link_sync_peer(struct riocp_pe *pe, uint8_t port, uint8_t peer_port)
{
	int ret = 0, i, j;
	uint32_t efptr = pe->efptr;
	uint32_t lm_resp, ackid_stat, port_err_stat;
	uint32_t any_id;

	any_id = RIOCP_PE_ANY_ID(pe);

	RIOCP_TRACE("Peer link sync on pe 0x%08x:%u\n", pe->comptag, port);

	if (RIOCP_PE_IS_SWITCH(pe->cap)) {
		ret = riocp_pe_is_port_active(pe, port);
		if (ret < 0) {
			RIOCP_ERROR("Unable to read if port is active\n");
			return -EIO;
		}
		if (ret == 0) {
			RIOCP_ERROR("Try to sync inactive port %u\n", port);
			return -ENODEV;
		}

		/* do maintenance request */
		j = 3;
		do {
			ret = riocp_pe_maint_write(pe, efptr + RIO_PORT_N_MNT_REQ_CSR(port), RIO_MNT_REQ_CMD_IS);
			if (ret) {
				RIOCP_ERROR("Unable to write RIO_PORT_N_MNT_REQ_CSR(0x%08x) for port %u\n",
					efptr + RIO_PORT_N_MNT_REQ_CSR(port), port);
				return ret;
			}

			/* wait for valid maintenance response */
			i = 5;
			do {
				ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_MNT_RSP_CSR(port), &lm_resp);
				if (ret) {
					RIOCP_ERROR("Unable to read RIO_PORT_N_MNT_RSP_CSR(0x%08x) for port %u\n",
						efptr + RIO_PORT_N_MNT_RSP_CSR(port), port);
					return ret;
				}
				i--;
				if (i<=0) {
					RIOCP_ERROR("No valid response (0x%08x) for RIO_PORT_N_MNT_RSP_CSR(0x%08x) on port %u\n",
						lm_resp, efptr + RIO_PORT_N_MNT_RSP_CSR(port), port);
					return -EIO;
				}
			} while (!(lm_resp & RIO_PORT_N_MNT_RSP_RVAL));

			j--;
			if (j<=0) {
				RIOCP_ERROR("No link status ok (0x%08x) for RIO_PORT_N_MNT_RSP_CSR(0x%08x) on port %u\n",
					lm_resp, efptr + RIO_PORT_N_MNT_RSP_CSR(port), port);
				return -EIO;
			}
		} while ((lm_resp & RIO_PORT_N_MNT_RSP_LSTAT) != 0x10);

		lm_resp = (lm_resp & RIO_PORT_N_MNT_RSP_ASTAT) >> 5;

		/* update local outgoing ackid with peer expected inbound ackid */
		ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_ACK_STS_CSR(port), &ackid_stat);
		if (ret) {
			RIOCP_ERROR("Unable to read RIO_PORT_N_ACK_STS_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_ACK_STS_CSR(port), port);
			return ret;
		}

		ackid_stat &= ~(RIO_PORT_N_ACK_OUTSTAND | RIO_PORT_N_ACK_OUTBOUND);
		ackid_stat |= (RIO_PORT_N_ACK_CLEAR | (lm_resp << 8) | (lm_resp));

		ret = riocp_pe_maint_write(pe, efptr + RIO_PORT_N_ACK_STS_CSR(port), ackid_stat);
		if (ret) {
			RIOCP_ERROR("Unable to write RIO_PORT_N_ACK_STS_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_ACK_STS_CSR(port), port);
			return ret;
		}

		/* clear local port errors */
		ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_ERR_STS_CSR(port), &port_err_stat);
		if (ret) {
			RIOCP_ERROR("Unable to read RIO_PORT_N_ERR_STS_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_ERR_STS_CSR(port), port);
			return ret;
		}
		ret = riocp_pe_maint_write(pe, efptr + RIO_PORT_N_ERR_STS_CSR(port), port_err_stat);
		if (ret) {
			RIOCP_ERROR("Unable to write RIO_PORT_N_ERR_STS_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_ERR_STS_CSR(port), port);
			return ret;
		}

		/* read ackid again, maybe it changed in the mean time due to pending packets now sent out */
		ret = riocp_pe_maint_read(pe, efptr + RIO_PORT_N_ACK_STS_CSR(port), &ackid_stat);
		if (ret) {
			RIOCP_ERROR("Unable to read RIO_PORT_N_ACK_STS_CSR(0x%08x) for port %u\n",
				efptr + RIO_PORT_N_ACK_STS_CSR(port), port);
			return ret;
		}

		lm_resp = ackid_stat;

		/* prepare far end ackid with well known values */
		if (port_err_stat & RIO_PORT_N_ERR_STS_IDLE2_USED) {
			ackid_stat = ((((lm_resp&RIO_PORT_N_ACK_OUTBOUND_IDLE2)+1) << 24) & RIO_PORT_N_ACK_INBOUND_IDLE2) |
					(((lm_resp&RIO_PORT_N_ACK_INBOUND_IDLE2)>>24) << 8) | ((lm_resp&RIO_PORT_N_ACK_INBOUND_IDLE2)>>24);
		} else {
			ackid_stat = ((((lm_resp&RIO_PORT_N_ACK_OUTBOUND)+1) << 24) & RIO_PORT_N_ACK_INBOUND) |
					(((lm_resp&RIO_PORT_N_ACK_INBOUND)>>24) << 8) | ((lm_resp&RIO_PORT_N_ACK_INBOUND)>>24);
		}

		/* set route to host */
		ret = riocp_pe_switch_set_route_entry(pe, port, pe->mport->destid,
				(uint8_t)RIO_GET_PORT_NUM(pe->cap.sw_port));
		if (ret) {
			RIOCP_ERROR("Unable to set host route for 0x04x (0x%08x) port %u->%u\n",
				pe->destid, pe->comptag, port, (uint8_t)RIO_GET_PORT_NUM(pe->cap.sw_port));
			return ret;
		}

		/* assume that far end efptr is 0x100 */

		/* update far end ackid */
		ret = riocp_pe_maint_write_remote(pe->mport, any_id, pe->hopcount+1,
				0x100 + RIO_PORT_N_ACK_STS_CSR(peer_port), ackid_stat);
		if (ret < 0) {
			RIOCP_WARN("Unable to update peer port %u for port %u with ackid_stat 0x%08x\n",
				peer_port, port, ackid_stat);
		}

		/* clear far end port pending errors */
#ifdef CONFIG_LINK_SYNC_FAR_LM_REQ
		for (i=0;i<2;i++) {
			ret = riocp_pe_maint_write_remote(pe->mport, any_id, pe->hopcount+1,
					0x100 + RIO_PORT_N_MNT_REQ_CSR(port), RIO_MNT_REQ_CMD_IS);
			if (ret < 0) {
				RIOCP_ERROR("Unable to clear errors on peer port %u for port %u by LM_REQ\n",
					peer_port, port);
				return ret;
			}
		}
#else
		ret = riocp_pe_maint_write_remote(pe->mport, any_id, pe->hopcount+1,
				0x100 + RIO_PORT_N_ERR_STS_CSR(peer_port),
				(port_err_stat & RIO_PORT_N_ERR_STS_IDLE2_MASK) | RIO_PORT_N_ERR_STS_CLEAR_ERR);
		if (ret < 0) {
			RIOCP_ERROR("Unable to clear errors on peer port %u for port %u\n",
				peer_port, port);
			return ret;
		}
#endif
	}

	RIOCP_TRACE("Peer link sync on pe 0x%08x:%u successful\n", pe->comptag, port);
	return ret;
}

#ifdef __cplusplus
}
#endif
