/*
 * Driver for Digigram VXpocket soundcards
 *
 * Xilinx boot image for VXpocket 440.
 *
 * Copyright (c) 2002 by Digigram S.A.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

static unsigned char xilinx_boot_vxpocket[492] = {
	0x00, 0x00, 0xa2, 0x00, 0x01, 0x00, 0x00, 0x03, 0xf8, 0x0a, 0x82, 0x03,
	0x0a, 0x82, 0x04, 0x08, 0xf4, 0xb9, 0x00, 0x04, 0x39, 0x08, 0xf4, 0xb8,
	0x00, 0x04, 0x21, 0x0a, 0xfa, 0x6e, 0x08, 0xf4, 0xbd, 0x34, 0x00, 0x08,
	0x08, 0xf4, 0xbb, 0x01, 0x20, 0x21, 0x07, 0xf4, 0x3f, 0x00, 0x00, 0x00,
	0x07, 0xf4, 0x35, 0x20, 0x10, 0x00, 0x07, 0xf4, 0x36, 0x8b, 0x30, 0x00,
	0x07, 0xf4, 0x3d, 0x00, 0x00, 0x03, 0x07, 0xf4, 0x3e, 0x00, 0x00, 0x03,
	0x07, 0xf4, 0x3f, 0x00, 0x00, 0x3c, 0x07, 0xf4, 0x2f, 0x00, 0x00, 0x00,
	0x07, 0xf4, 0x2d, 0x00, 0x00, 0x01, 0x07, 0xf4, 0x2e, 0x00, 0x00, 0x03,
	0x07, 0xf4, 0x2f, 0x00, 0x00, 0x00, 0x07, 0xf4, 0x1d, 0x00, 0x00, 0x00,
	0x07, 0xf4, 0x1e, 0x00, 0x00, 0x00, 0x07, 0xf4, 0x1f, 0x00, 0x00, 0x05,
	0x07, 0xf4, 0x0f, 0x00, 0x00, 0x00, 0x07, 0xf4, 0x0b, 0x00, 0x08, 0x00,
	0x07, 0xf4, 0x07, 0x00, 0x00, 0x00, 0x08, 0xf4, 0x84, 0x00, 0x10, 0x18,
	0x0a, 0x84, 0x26, 0x30, 0x00, 0x00, 0x05, 0xf4, 0x20, 0x00, 0x5f, 0xff,
	0x31, 0x00, 0x00, 0x32, 0x00, 0x00, 0x0a, 0x82, 0x23, 0x44, 0xf4, 0x00,
	0x00, 0x00, 0x01, 0x01, 0x6d, 0x00, 0x0a, 0x83, 0x84, 0x00, 0x01, 0x37,
	0x01, 0x4b, 0x0d, 0x0a, 0x83, 0x84, 0x00, 0x01, 0x44, 0x0a, 0x83, 0x80,
	0x00, 0x01, 0x3a, 0x08, 0x60, 0xc6, 0x0a, 0x83, 0x81, 0x00, 0x01, 0x40,
	0x08, 0xd8, 0xc7, 0x0c, 0x01, 0x3a, 0x0a, 0x82, 0x24, 0x08, 0xd0, 0x07,
	0x0a, 0x83, 0x81, 0x00, 0x01, 0x46, 0x08, 0xd0, 0x07, 0x01, 0x7d, 0x01,
	0x0a, 0x83, 0x83, 0x00, 0x01, 0x49, 0x01, 0x0b, 0x0d, 0x01, 0x2d, 0x01,
	0x01, 0x3d, 0x21, 0x01, 0x3d, 0x00, 0x44, 0xf4, 0x00, 0x00, 0x27, 0x10,
	0x0b, 0xf0, 0x80, 0x00, 0x01, 0x9c, 0x01, 0x3d, 0x20, 0x44, 0xf4, 0x00,
	0x00, 0x9c, 0x40, 0x0b, 0xf0, 0x80, 0x00, 0x01, 0x9c, 0x01, 0x47, 0x2c,
	0x0e, 0x01, 0x59, 0x07, 0xf4, 0x1e, 0x00, 0x00, 0x02, 0x01, 0x1d, 0x01,
	0x07, 0xf4, 0x2e, 0x00, 0x00, 0x33, 0x31, 0x00, 0x00, 0x22, 0x0f, 0x00,
	0x20, 0x00, 0x0b, 0x0a, 0xf0, 0xaa, 0x00, 0x01, 0x95, 0x06, 0xcd, 0x00,
	0x00, 0x01, 0x7a, 0x5e, 0xd9, 0x00, 0x06, 0x08, 0x80, 0x00, 0x01, 0x78,
	0x01, 0x2d, 0x05, 0x20, 0x00, 0x27, 0x0a, 0xf0, 0xa0, 0x00, 0x01, 0x6f,
	0x01, 0x2d, 0x25, 0x44, 0xf4, 0x00, 0x00, 0x00, 0x01, 0x0b, 0xf0, 0x80,
	0x00, 0x01, 0x9c, 0x01, 0x2d, 0x21, 0x0b, 0xf0, 0x80, 0x00, 0x01, 0x9c,
	0x01, 0x2d, 0x01, 0x00, 0x00, 0x00, 0x20, 0x5a, 0x00, 0x20, 0x5b, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2d, 0x25, 0x06, 0x04, 0x80,
	0x00, 0x01, 0x85, 0x01, 0x2d, 0x21, 0x0b, 0xf0, 0x80, 0x00, 0x01, 0x9c,
	0x01, 0x2d, 0x01, 0x0b, 0xf0, 0x80, 0x00, 0x01, 0x9c, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0xd2, 0x07, 0x44, 0xf4, 0x00, 0x00, 0x00, 0x01,
	0x0b, 0xf0, 0x80, 0x00, 0x01, 0x9c, 0x07, 0xf4, 0x2e, 0x00, 0x00, 0x00,
	0x07, 0xf4, 0x1e, 0x00, 0x00, 0x00, 0x44, 0xf4, 0x00, 0x00, 0x00, 0x01,
	0x0b, 0xf0, 0x80, 0x00, 0x01, 0x9c, 0x0c, 0x01, 0x90, 0x01, 0x6d, 0x00,
	0x01, 0x4b, 0x0d, 0x44, 0xf4, 0x00, 0x00, 0x00, 0x04, 0x0b, 0xf0, 0x80,
	0x00, 0x01, 0x9c, 0x0c, 0x01, 0x95, 0x06, 0xc4, 0x00, 0x00, 0x01, 0x9f,
	0x01, 0x6d, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c,
};
