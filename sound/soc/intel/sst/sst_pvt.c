/*
 *  sst_pvt.c - Intel SST Driver for audio engine
 *
 *  Copyright (C) 2008-10	Intel Corp
 *  Authors:	Vinod Koul <vinod.koul@intel.com>
 *		Harsha Priya <priya.harsha@intel.com>
 *		Dharageswari R <dharageswari.r@intel.com>
 *		KP Jeeja <jeeja.kp@intel.com>
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This file contains all private functions
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kobject.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/firmware.h>
#include <linux/sched.h>
#include <sound/asound.h>
#include <sound/pcm.h>
#include "../sst_platform.h"
#include "../platform_ipc_v2.h"
#include "sst.h"

#define SST_EXCE_DUMP_BASE	0xFFFF2c00
#define SST_EXCE_DUMP_WORD	4
#define SST_EXCE_DUMP_LEN	32
#define SST_EXCE_DUMP_SIZE	((SST_EXCE_DUMP_LEN)*(SST_EXCE_DUMP_WORD))
#define SST_EXCE_DUMP_OFFSET	0xA00
/*
 * sst_wait_interruptible - wait on event
 *
 * @sst_drv_ctx: Driver context
 * @block: Driver block to wait on
 *
 * This function waits without a timeout (and is interruptable) for a
 * given block event
 */
int sst_wait_interruptible(struct intel_sst_drv *sst_drv_ctx,
				struct sst_block *block)
{
	int retval = 0;

	if (!wait_event_interruptible(sst_drv_ctx->wait_queue,
				block->condition)) {
		/* event wake */
		if (block->ret_code < 0) {
			pr_err("stream failed %d\n", block->ret_code);
			retval = -EBUSY;
		} else {
			pr_debug("event up\n");
			retval = 0;
		}
	} else {
		pr_err("signal interrupted\n");
		retval = -EINTR;
	}
	return retval;

}

unsigned long long read_shim_data(struct intel_sst_drv *sst, int addr)
{
	unsigned long long val = 0;

	switch (sst->pci_id) {
	case SST_CLV_PCI_ID:
		val = sst_shim_read(sst->shim, addr);
		break;
	case SST_MRFLD_PCI_ID:
	case SST_BYT_PCI_ID:
		val = sst_shim_read64(sst->shim, addr);
		break;
	}
	return val;
}

void write_shim_data(struct intel_sst_drv *sst, int addr,
				unsigned long long data)
{
	switch (sst->pci_id) {
	case SST_CLV_PCI_ID:
		sst_shim_write(sst->shim, addr, (u32) data);
		break;
	case SST_MRFLD_PCI_ID:
	case SST_BYT_PCI_ID:
		sst_shim_write64(sst->shim, addr, (u64) data);
		break;
	}
}


void dump_sst_shim(struct intel_sst_drv *sst)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&sst->ipc_spin_lock, irq_flags);
	pr_err("audio shim registers:\n"
		"CSR: %.8llx\n"
		"PISR: %.8llx\n"
		"PIMR: %.8llx\n"
		"ISRX: %.8llx\n"
		"ISRD: %.8llx\n"
		"IMRX: %.8llx\n"
		"IMRD: %.8llx\n"
		"IPCX: %.8llx\n"
		"IPCD: %.8llx\n"
		"ISRSC: %.8llx\n"
		"ISRLPESC: %.8llx\n"
		"IMRSC: %.8llx\n"
		"IMRLPESC: %.8llx\n"
		"IPCSC: %.8llx\n"
		"IPCLPESC: %.8llx\n"
		"CLKCTL: %.8llx\n"
		"CSR2: %.8llx\n",
		read_shim_data(sst, SST_CSR),
		read_shim_data(sst, SST_PISR),
		read_shim_data(sst, SST_PIMR),
		read_shim_data(sst, SST_ISRX),
		read_shim_data(sst, SST_ISRD),
		read_shim_data(sst, SST_IMRX),
		read_shim_data(sst, SST_IMRD),
		read_shim_data(sst, sst->ipc_reg.ipcx),
		read_shim_data(sst, sst->ipc_reg.ipcd),
		read_shim_data(sst, SST_ISRSC),
		read_shim_data(sst, SST_ISRLPESC),
		read_shim_data(sst, SST_IMRSC),
		read_shim_data(sst, SST_IMRLPESC),
		read_shim_data(sst, SST_IPCSC),
		read_shim_data(sst, SST_IPCLPESC),
		read_shim_data(sst, SST_CLKCTL),
		read_shim_data(sst, SST_CSR2));
	spin_unlock_irqrestore(&sst->ipc_spin_lock, irq_flags);
}

void reset_sst_shim(struct intel_sst_drv *sst)
{
	pr_err("Resetting few Shim registers\n");
	write_shim_data(sst, sst->ipc_reg.ipcx, 0x0);
	write_shim_data(sst, sst->ipc_reg.ipcd, 0x0);
	write_shim_data(sst, SST_ISRX, 0x0);
	write_shim_data(sst, SST_ISRD, 0x0);
	write_shim_data(sst, SST_IPCSC, 0x0);
	write_shim_data(sst, SST_IPCLPESC, 0x0);
	write_shim_data(sst, SST_ISRSC, 0x0);
	write_shim_data(sst, SST_ISRLPESC, 0x0);
	write_shim_data(sst, SST_PISR, 0x0);
}

static void dump_sst_crash_area(void)
{
	void __iomem *fw_dump_area;
	u32 dump_word;
	u8 i;

	/* dump the firmware SRAM where the exception details are stored */
	fw_dump_area = ioremap_nocache(SST_EXCE_DUMP_BASE, SST_EXCE_DUMP_SIZE);

	pr_err("Firmware exception dump begins:\n");
	pr_err("Exception start signature:%#x\n", readl(fw_dump_area + SST_EXCE_DUMP_WORD));
	pr_err("EXCCAUSE:\t\t\t%#x\n", readl(fw_dump_area + SST_EXCE_DUMP_WORD*2));
	pr_err("EXCVADDR:\t\t\t%#x\n", readl(fw_dump_area + (SST_EXCE_DUMP_WORD*3)));
	pr_err("Firmware additional data:\n");

	/* dump remaining FW debug data */
	for (i = 1; i < (SST_EXCE_DUMP_LEN-4+1); i++) {
		dump_word = readl(fw_dump_area + (SST_EXCE_DUMP_WORD*3)
						+ (i*SST_EXCE_DUMP_WORD));
		pr_err("Data[%d]=%#x\n", i, dump_word);
	}
	iounmap(fw_dump_area);
	pr_err("Firmware exception dump ends\n");
}

/**
 * dump_ram_area - dumps the iram/dram into a local buff
 *
 * @sst			: pointer to driver context
 * @recovery		: pointer to the struct containing buffers
 * @iram		: true if iram dump else false
 * This function dumps the iram dram data into the respective buffers
 */
#if IS_ENABLED(CONFIG_SND_INTEL_SST_RECOVERY)
static void dump_ram_area(struct intel_sst_drv *sst,
			struct sst_dump_buf *dump_buf, enum sst_ram_type type)
{
	if (type == SST_IRAM) {
		pr_err("Iram dumped in buffer\n");
		memcpy_fromio(dump_buf->iram_buf.buf, sst->iram,
				dump_buf->iram_buf.size);
	} else {
		pr_err("Dram dumped in buffer\n");
		memcpy_fromio(dump_buf->dram_buf.buf, sst->dram,
				dump_buf->dram_buf.size);
	}
}

/*FIXME Disabling IRAM/DRAM dump for timeout issues */
static void sst_stream_recovery(struct intel_sst_drv *sst)
{
	struct stream_info *str_info;
	u8 i;
	for (i = 1; i <= sst->info.max_streams; i++) {
		pr_err("Audio: Stream %d, state %d\n", i, sst->streams[i].status);
		if (sst->streams[i].status != STREAM_UN_INIT) {
			str_info = &sst_drv_ctx->streams[i];
			if (str_info->pcm_substream)
				snd_pcm_stop(str_info->pcm_substream, SNDRV_PCM_STATE_SETUP);
		}
	}
}

static void sst_do_recovery(struct intel_sst_drv *sst)
{
	struct ipc_post *m, *_m;
	unsigned long irq_flags;
	char iram_event[30], dram_event[30], ddr_imr_event[65];
	char *envp[4];
	int env_offset = 0;

	/*
	 * setting firmware state as uninit so that the firmware will get
	 * redownloaded on next request.This is because firmare not responding
	 * for 1 sec is equalant to some unrecoverable error of FW.
	 */
	pr_err("Audio: Intel SST engine encountered an unrecoverable error\n");
	pr_err("Audio: trying to reset the dsp now\n");

	if (sst->sst_state == SST_FW_RUNNING &&
		sst_drv_ctx->pci_id == SST_CLV_PCI_ID)
		dump_sst_crash_area();

	mutex_lock(&sst->sst_lock);
	sst->sst_state = SST_UN_INIT;
	sst_stream_recovery(sst);

	mutex_unlock(&sst->sst_lock);

	dump_stack();
	dump_sst_shim(sst);
	reset_sst_shim(sst);

	if (sst_drv_ctx->ops->set_bypass) {

		sst_drv_ctx->ops->set_bypass(true);
		dump_ram_area(sst, &(sst->dump_buf), SST_IRAM);
		dump_ram_area(sst, &(sst->dump_buf), SST_DRAM);
		sst_drv_ctx->ops->set_bypass(false);

	}

	snprintf(iram_event, sizeof(iram_event), "IRAM_DUMP_SIZE=%d",
					sst->dump_buf.iram_buf.size);
	envp[env_offset++] = iram_event;
	snprintf(dram_event, sizeof(dram_event), "DRAM_DUMP_SIZE=%d",
					sst->dump_buf.dram_buf.size);
	envp[env_offset++] = dram_event;

	if (sst->ddr != NULL) {
		snprintf(ddr_imr_event, sizeof(ddr_imr_event),
		"DDR_IMR_DUMP_SIZE=%d DDR_IMR_ADDRESS=%p", (sst->ddr_end - sst->ddr_base), sst->ddr);
		envp[env_offset++] = ddr_imr_event;
	}
	envp[env_offset] = NULL;
	kobject_uevent_env(&sst->dev->kobj, KOBJ_CHANGE, envp);
	pr_err("Recovery Uevent Sent!!\n");

	spin_lock_irqsave(&sst->ipc_spin_lock, irq_flags);
	if (list_empty(&sst->ipc_dispatch_list))
		pr_err("List is Empty\n");
	spin_unlock_irqrestore(&sst->ipc_spin_lock, irq_flags);

	list_for_each_entry_safe(m, _m, &sst->ipc_dispatch_list, node) {
		pr_err("pending msg header %#x\n", m->header.full);
		list_del(&m->node);
		kfree(m->mailbox_data);
		kfree(m);
	}
}
#else
static void sst_do_recovery(struct intel_sst_drv *sst)
{
	struct ipc_post *m, *_m;
	unsigned long irq_flags;

	if (sst->pci_id == SST_MRFLD_PCI_ID) {
		dump_stack();
		return;
	}

	dump_stack();
	dump_sst_shim(sst);

	if (sst->sst_state == SST_FW_RUNNING &&
		sst_drv_ctx->pci_id == SST_CLV_PCI_ID)
		dump_sst_crash_area();

	spin_lock_irqsave(&sst->ipc_spin_lock, irq_flags);
	if (list_empty(&sst->ipc_dispatch_list))
		pr_err("List is Empty\n");
	spin_unlock_irqrestore(&sst->ipc_spin_lock, irq_flags);
	list_for_each_entry_safe(m, _m, &sst->ipc_dispatch_list, node)
		pr_err("pending msg header %#x\n", m->header.full);
}
#endif

/*
 * sst_wait_timeout - wait on event for timeout
 *
 * @sst_drv_ctx: Driver context
 * @block: Driver block to wait on
 *
 * This function waits with a timeout value (and is not interruptible) on a
 * given block event
 */
int sst_wait_timeout(struct intel_sst_drv *sst_drv_ctx, struct sst_block *block)
{
	int retval = 0;

	/* NOTE:
	Observed that FW processes the alloc msg and replies even
	before the alloc thread has finished execution */
	pr_debug("sst: waiting for condition %x\n",
		       block->condition);
	if (wait_event_timeout(sst_drv_ctx->wait_queue,
				block->condition,
				msecs_to_jiffies(SST_BLOCK_TIMEOUT))) {
		/* event wake */
		pr_debug("sst: Event wake %x\n", block->condition);
		pr_debug("sst: message ret: %d\n", block->ret_code);
		retval = block->ret_code;
	} else {
		block->on = false;
		pr_err("sst: Wait timed-out condition:%#x, msg_id:%#x\n",
				block->condition, block->msg_id);

		sst_do_recovery(sst_drv_ctx);
		/* settign firmware state as uninit so that the
		firmware will get redownloaded on next request
		this is because firmare not responding for 5 sec
		is equalant to some unrecoverable error of FW */
		retval = -EBUSY;
	}
	return retval;
}

/*
 * sst_create_ipc_msg - create a IPC message
 *
 * @arg: ipc message
 * @large: large or short message
 *
 * this function allocates structures to send a large or short
 * message to the firmware
 */
int sst_create_ipc_msg(struct ipc_post **arg, bool large)
{
	struct ipc_post *msg;

	msg = kzalloc(sizeof(struct ipc_post), GFP_ATOMIC);
	if (!msg) {
		pr_err("kzalloc ipc msg failed\n");
		return -ENOMEM;
	}
	if (large) {
		msg->mailbox_data = kzalloc(SST_MAILBOX_SIZE, GFP_ATOMIC);
		if (!msg->mailbox_data) {
			kfree(msg);
			pr_err("kzalloc mailbox_data failed");
			return -ENOMEM;
		}
	} else {
		msg->mailbox_data = NULL;
	}
	msg->is_large = large;
	*arg = msg;
	return 0;
}

/*
 * sst_create_block_and_ipc_msg - Creates IPC message and sst block
 * @arg: passed to sst_create_ipc_message API
 * @large: large or short message
 * @sst_drv_ctx: sst driver context
 * @block: return block allocated
 * @msg_id: IPC
 * @drv_id: stream id or private id
 */
int sst_create_block_and_ipc_msg(struct ipc_post **arg, bool large,
		struct intel_sst_drv *sst_drv_ctx, struct sst_block **block,
		u32 msg_id, u32 drv_id)
{
	int retval = 0;
	retval = sst_create_ipc_msg(arg, large);
	if (retval)
		return retval;
	*block = sst_create_block(sst_drv_ctx, msg_id, drv_id);
	if (*block == NULL) {
		kfree(*arg);
		return -ENOMEM;
	}
	return retval;
}

/*
 * sst_clean_stream - clean the stream context
 *
 * @stream: stream structure
 *
 * this function resets the stream contexts
 * should be called in free
 */
void sst_clean_stream(struct stream_info *stream)
{
	stream->status = STREAM_UN_INIT;
	stream->prev = STREAM_UN_INIT;
	mutex_lock(&stream->lock);
	stream->cumm_bytes = 0;
	mutex_unlock(&stream->lock);
}

/*
 * sst_create_and_send_uevent - dynamically create, send and destroy uevent
 * @name - Name of the uevent
 * @envp - Event parameters
 */
int sst_create_and_send_uevent(char *name, char *envp[])
{
	struct kset *set;
	struct kobject *obj;
	int ret = 0;

	set = kset_create_and_add("SSTEVENTS", NULL, &sst_drv_ctx->dev->kobj);
	if (!set) {
		pr_err("kset creation failed\n");
		return -ENOMEM;
	}
	obj = kobject_create_and_add(name, &sst_drv_ctx->dev->kobj);
	if (!obj) {
		pr_err("kboject creation failed\n");
		ret = -ENOMEM;
		goto free_kset;
	}
	obj->kset = set;
	ret = kobject_uevent_env(obj, KOBJ_ADD, envp);
	if (ret)
		pr_err("sst uevent send failed - %d\n", ret);
	kobject_put(obj);
free_kset:
	kset_unregister(set);
	return ret;
}