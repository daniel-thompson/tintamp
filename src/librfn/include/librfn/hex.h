/*
 * hex.h
 *
 * Part of librfn (a general utility library from redfelineninja.org.uk)
 *
 * Copyright (C) 2012 Daniel Thompson <daniel@redfelineninja.org.uk> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef RF_HEX_H_
#define RF_HEX_H_

#include <stdio.h>
#include <stdlib.h>

int rf_hexdump(unsigned char *p, size_t sz);
int rf_fhexdump(FILE *f, unsigned char *p, size_t sz);

#endif // RF_HEX_H_
