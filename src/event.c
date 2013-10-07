#include "config.h"
#include "event.h"
#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

struct packet_cb_list_entry {
	packet_cb cb;
	struct packet_cb_list_entry* next;
};
struct packet_cb_list_entry* packet_cb_list = NULL;

struct handle_cb_list_entry {
	handle_cb cb;
	int h;
	void *ctx;
	struct handle_cb_list_entry* next;
};
struct handle_cb_list_entry* handle_cb_list = NULL;

struct signal_cb_list_entry {
	signal_cb cb;
	int s;
	int called;
	struct signal_cb_list_entry* next;
};
struct signal_cb_list_entry* signal_cb_list = NULL;

int signalCalled = 0;

void cb_add_packet_cb(packet_cb cb) {
	struct packet_cb_list_entry* entry = malloc(sizeof(struct packet_cb_list_entry));
	if (!entry) {
		eprintf(DEBUG_ERROR, "out of memory at %s:%d in %s", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		exit(1);
	}
	memset(entry, 0, sizeof(struct packet_cb_list_entry));
	entry->cb = cb;
	entry->next = packet_cb_list;
	packet_cb_list = entry;
};

void cb_call_packet_cb(const int ptype, const uint8_t *packet, const int len, const char* ifname) {
	for (struct packet_cb_list_entry* entry = packet_cb_list; entry; entry = entry->next) {
		entry->cb(ptype, packet, len, ifname);
	}
};

void cb_add_handle(int h, void* ctx, handle_cb cb) {
	struct handle_cb_list_entry* entry = malloc(sizeof(struct handle_cb_list_entry));
	if (!entry) {
		eprintf(DEBUG_ERROR, "out of memory at %s:%d in %s", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		exit(1);
	}
	if(!h) {
		eprintf(DEBUG_ERROR, "no handle given at %s:%d in %s", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		exit(1);
	}
	if (!cb) {
		eprintf(DEBUG_ERROR, "no cb given at %s:%d in %s", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		exit(1);
	}
	memset(entry, 0, sizeof(struct handle_cb_list_entry));
	entry->h = h;
	entry->cb = cb;
	entry->ctx = ctx;
	entry->next = handle_cb_list;
	handle_cb_list = entry;
};

void signal_cb_int(int s) {
	for (struct signal_cb_list_entry* entry = signal_cb_list; entry; entry = entry->next) {
		if (entry->s == s) {
			entry->called++;
			signalCalled = 1;
		}
	}
};

void cb_add_signal(int s, signal_cb cb) {
	struct signal_cb_list_entry* entry = malloc(sizeof(struct signal_cb_list_entry));
	if (!entry) {
		eprintf(DEBUG_ERROR, "out of memory at %s:%d in %s", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		exit(1);
	}
	memset(entry, 0, sizeof(struct signal_cb_list_entry));
	if(!s) {
		eprintf(DEBUG_ERROR, "no signal given at %s:%d in %s", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		exit(1);
	}
	if (!cb) {
		eprintf(DEBUG_ERROR, "no cb given at %s:%d in %s", __FILE__, __LINE__, __PRETTY_FUNCTION__);
		exit(1);
	}
	entry->s = s;
	entry->cb = cb;
	entry->next = signal_cb_list;
	signal_cb_list = entry;
	signal(s, signal_cb_int);
};

void event_runloop() {
	fd_set rfds;
	int maxfd, retval;

        // Block SIGALRM and SIGUSR1
        sigset_t sigset, oldset;
        sigemptyset(&sigset);
	for (struct signal_cb_list_entry* entry = signal_cb_list; entry; entry = entry->next) {
        	sigaddset (&sigset, entry->s);
	}
        sigprocmask(SIG_BLOCK, &sigset, &oldset);

	while (1) {
		FD_ZERO(&rfds);
		maxfd = -1;
		for (struct handle_cb_list_entry* entry = handle_cb_list; entry; entry = entry->next) {
			FD_SET(entry->h, &rfds);
			if (maxfd < entry->h) {
				maxfd = entry->h;
			}
		}
		signalCalled = 0;
		retval = pselect(maxfd+1, &rfds, NULL, NULL, NULL, &oldset);
		if (retval < 0 && errno != EINTR)
			break;
		if (retval > 0) {
			for (struct handle_cb_list_entry* entry = handle_cb_list; entry; entry = entry->next) {
				if (FD_ISSET(entry->h, &rfds)) {
					entry->cb(entry->h, entry->ctx);
				}
			}
		}
		if (signalCalled > 0) {
			for (struct signal_cb_list_entry* entry = signal_cb_list; entry; entry = entry->next) {
				if (entry->called > 0) {
					entry->cb(entry->s);
				}
			}
		}
	}
	eprintf(DEBUG_ERROR, "exit due to: %s\n", strerror(errno));
};