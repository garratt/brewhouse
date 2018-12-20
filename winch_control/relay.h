// Copyright 2018 Garratt Gallagher. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: must build with -lftdi

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <ftdi.h>

void ListDevs() {
    struct ftdi_context ftdic;
    // set up driver context
    ftdi_init(&ftdic);
    struct ftdi_device_list *devlist;

    if(ftdi_usb_find_all(&ftdic, &devlist, 0x0403, 0x6001) < 0) {
        fprintf(stderr, "no device\n");
        return;
    }
    char manufacturer[512], description[512], serial[512];
    while (devlist) {
      ftdi_usb_get_strings(&ftdic, devlist->dev, manufacturer, 512,
                           description, 512, serial, 512);
      printf("dev:\n  manufacturer: %s\n  description %s\n  serial %s\n",
             manufacturer, description, serial);
      devlist=devlist->next;
    }
}


int OpenDevice(ftdi_context &ftdic, const char *dev_serial) {
    struct ftdi_device_list *devlist;
    if(ftdi_usb_find_all(&ftdic, &devlist, 0x0403, 0x6001) < 0) {
        fprintf(stderr, "no devices\n");
        return -1;
    }
    char manufacturer[512], description[512], serial[512];
    while (devlist) {
      ftdi_usb_get_strings(&ftdic, devlist->dev, manufacturer, 512,
                           description, 512, serial, 512);
      // printf("dev:\n  manufacturer: %s\n  description %s\n  serial %s\n",
             // manufacturer, description, serial);
      if (strcmp(dev_serial, serial) == 0) {
        return ftdi_usb_open_dev(&ftdic, devlist->dev);
      }
      devlist=devlist->next;
    }
    fprintf(stderr, "no device %s\n", dev_serial);
    return -1;
}


int SetRelay(uint8_t state, const char *serial = "") {
    struct ftdi_context ftdic;
    // set up driver context
    ftdi_init(&ftdic);

    int ret;
    if (strlen(serial)) {
      ret = OpenDevice(ftdic, serial);
    } else {
      ret = ftdi_usb_open(&ftdic, 0x0403, 0x6001);
    }
    if (ret < 0) {
        fprintf(stderr, "no device\n");
        return 1;
    }

    ftdi_set_bitmode(&ftdic, 0xFF, BITMODE_BITBANG);
    ftdi_write_data(&ftdic, &state, 1);
    ftdi_usb_close(&ftdic);
    return 0;
}




int ConnectUSB() { return SetRelay(0xff);}
int DisconnectUSB() { return SetRelay(0x00);}


